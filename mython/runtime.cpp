#include "runtime.h"

#include <cassert>
#include <optional>
#include <sstream>
#include <string>
#include <algorithm>

using namespace std;

namespace runtime {

ObjectHolder::ObjectHolder(std::shared_ptr<Object> data)
    : data_(std::move(data)) {
}

void ObjectHolder::AssertIsValid() const {
    assert(data_ != nullptr);
}

ObjectHolder ObjectHolder::Share(Object& object) {
    // Возвращаем невладеющий shared_ptr (его deleter ничего не делает)
    return ObjectHolder(std::shared_ptr<Object>(&object, [](auto* /*p*/) { /* do nothing */ }));
}

ObjectHolder ObjectHolder::None() {
    return ObjectHolder();
}

Object& ObjectHolder::operator*() const {
    AssertIsValid();
    return *Get();
}

Object* ObjectHolder::operator->() const {
    AssertIsValid();
    return Get();
}

Object* ObjectHolder::Get() const {
    return data_.get();
}

ObjectHolder::operator bool() const {
    return Get() != nullptr;
}

bool IsTrue(const ObjectHolder& object) {
    if (auto ptr_value = object.TryAs<String>()) {
        return !(ptr_value->GetValue().empty());
    }
    else if (auto ptr_value = object.TryAs<Number>()) {
        return ptr_value->GetValue() != 0;
    }
    else if (auto ptr_value = object.TryAs<Bool>()) {
        return ptr_value->GetValue();
    }
    return false;
}

void ClassInstance::Print(std::ostream& os, Context& context) {
    using namespace std::literals;
    std::string str = "__str__"s;
    if (HasMethod(str, 0)) {
        Call(str, {}, context)->Print(os, context);
    }
    else {
        os << this;
    }
}

bool ClassInstance::HasMethod(const std::string& method, size_t argument_count) const {
    if (const Method* mth = cls_.GetMethod(method)) {
        return mth->formal_params.size() == argument_count;
    }
    return false;
}

Closure& ClassInstance::Fields() {
    return closure_;
}

const Closure& ClassInstance::Fields() const {
    return closure_;
}

ClassInstance::ClassInstance(const Class& cls)
    : cls_(cls)
{}

ObjectHolder ClassInstance::Call(const std::string& method,
                                 const std::vector<ObjectHolder>& actual_args,
                                 Context& context) {

    if (!HasMethod(method, actual_args.size())) {
        throw std::runtime_error("There is no such method: "s + method);
    }
    const Method* mth = cls_.GetMethod(method);
    Closure closure;
    for (size_t i = 0; i < mth->formal_params.size(); ++i) {
        closure.emplace(mth->formal_params[i], actual_args[i]);
    }
    closure.emplace("self"s, ObjectHolder::Share(*this));
    return mth->body->Execute(closure, context);
}

Class::Class(std::string name, std::vector<Method> methods, const Class* parent)
    : parent_(parent)
    , name_(std::move(name))
    , methods_(std::move(methods)) {
    
    std::sort(methods_.begin(), methods_.end(), [](const Method& lhs, const Method& rhs) {
        return lhs.name < rhs.name;
        });
}

const Method* Class::GetMethod(const std::string& name) const {
    const Method* mth = nullptr;
    const Class* ptr_class = this;
    while (ptr_class && !mth) {
        const auto& methods = ptr_class->GetMethods();
        auto it = std::lower_bound(methods.begin(), methods.end(), name, [](const Method& mth, const string& name) {
            return mth.name < name;
            });
        if (it != methods.end() && it->name == name) {
            mth = &(*it);
        }
        ptr_class = ptr_class->parent_;
    }
    return mth;
}

const std::vector<Method>& Class::GetMethods() const {
    return methods_;
}

[[nodiscard]] const std::string& Class::GetName() const {
    return name_;
}

void Class::Print(ostream& os, [[maybe_unused]] Context& context) {
    using namespace std::literals;
    os << "Class "s << name_;
}

void Bool::Print(std::ostream& os, [[maybe_unused]] Context& context) {
    os << (GetValue() ? "True"sv : "False"sv);
}

bool Equal(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
    if (!lhs.Get() && !rhs.Get()) {
        return true;
    }
    auto l_ptr_str = lhs.TryAs<String>();
    auto r_ptr_str = rhs.TryAs<String>();
    if (l_ptr_str && r_ptr_str) {
        return l_ptr_str->GetValue() == r_ptr_str->GetValue();
    }
    auto l_ptr_number = lhs.TryAs<Number>();
    auto r_ptr_number = rhs.TryAs<Number>();
    if (l_ptr_number && r_ptr_number) {
        return l_ptr_number->GetValue() == r_ptr_number->GetValue();
    }
    auto l_ptr_bool = lhs.TryAs<Bool>();
    auto r_ptr_bool = rhs.TryAs<Bool>();
    if (l_ptr_bool && r_ptr_bool) {
        return l_ptr_bool->GetValue() == r_ptr_bool->GetValue();
    }
    auto ptr_to_obj = lhs.TryAs<ClassInstance>();
    std::string method_eq = "__eq__"s;
    if (ptr_to_obj && ptr_to_obj->HasMethod(method_eq, 1)) {
        return IsTrue(ptr_to_obj->Call(method_eq, { rhs }, context));
    }
    throw std::runtime_error("logic error in Equal"s);
    return false;
}

bool Less(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
    if (lhs.Get() && rhs.Get()) {
        auto l_ptr_str = lhs.TryAs<String>();
        auto r_ptr_str = rhs.TryAs<String>();
        if (l_ptr_str && r_ptr_str) {
            return l_ptr_str->GetValue() < r_ptr_str->GetValue();
        }
        auto l_ptr_number = lhs.TryAs<Number>();
        auto r_ptr_number = rhs.TryAs<Number>();
        if (l_ptr_number && r_ptr_number) {
            return l_ptr_number->GetValue() < r_ptr_number->GetValue();
        }
        auto l_ptr_bool = lhs.TryAs<Bool>();
        auto r_ptr_bool = rhs.TryAs<Bool>();
        if (l_ptr_bool && r_ptr_bool) {
            return l_ptr_bool->GetValue() < r_ptr_bool->GetValue();
        }
        auto ptr_to_obj = lhs.TryAs<ClassInstance>();
        std::string method_lt = "__lt__"s;
        if (ptr_to_obj && ptr_to_obj->HasMethod(method_lt, 1)) {
            return IsTrue(ptr_to_obj->Call(method_lt, { rhs }, context));
        }
    }
    throw std::runtime_error("logic_error in Less"s);
    return false;
}

bool NotEqual(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
    return !Equal(lhs, rhs, context);
}

bool Greater(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
    return !Less(lhs, rhs, context) && !Equal(lhs, rhs, context);
}

bool LessOrEqual(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
    return Less(lhs, rhs, context) || Equal(lhs, rhs, context);
}

bool GreaterOrEqual(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
    return !Less(lhs, rhs, context);
}

}  // namespace runtime