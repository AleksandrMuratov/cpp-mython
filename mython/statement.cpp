#include "statement.h"

#include <iostream>
#include <sstream>

using namespace std;

namespace ast {

using runtime::Closure;
using runtime::Context;
using runtime::ObjectHolder;

namespace {
const string ADD_METHOD = "__add__"s;
const string INIT_METHOD = "__init__"s;
}  // namespace

ObjectHolder Assignment::Execute(Closure& closure, Context& context) {
    auto& var = closure[var_];
    var = rv_->Execute(closure, context);
    return var;
}

Assignment::Assignment(std::string var, std::unique_ptr<Statement> rv)
    : var_(std::move(var)) 
    , rv_(std::move(rv)){
}

VariableValue::VariableValue(const std::string& var_name)
    : var_names_({ var_name })
{}

VariableValue::VariableValue(std::vector<std::string> dotted_ids):
    var_names_(std::move(dotted_ids))
{}

ObjectHolder VariableValue::Execute(Closure& closure, [[maybe_unused]] Context& context) {
    ObjectHolder obj;
    Closure* current_closure = &closure;
    for (const auto& var_name : var_names_) {
        auto it = current_closure->find(var_name);
        if (it == current_closure->end()) {
            throw std::runtime_error("Variable "s + var_name + " not found"s);
        }
        obj = it->second;
        if (auto ptr_obj = obj.TryAs<runtime::ClassInstance>()) {
            current_closure = &ptr_obj->Fields();
        }
    }
    return obj;
}

unique_ptr<Print> Print::Variable(const std::string& name) {
    return std::make_unique<Print>(std::make_unique<VariableValue>(name));
}

Print::Print(unique_ptr<Statement> argument) {
    args_.push_back(std::move(argument));
}

Print::Print(vector<unique_ptr<Statement>> args)
    : args_(std::move(args))
{}

ObjectHolder Print::Execute(Closure& closure, Context& context) {
    bool is_first = true;
    for (auto& ex : args_) {
        if (!is_first) {
            context.GetOutputStream() << ' ';
        }
        auto obj = ex->Execute(closure, context);
        if (obj.Get()) {
            obj->Print(context.GetOutputStream(), context);
        }
        else {
            context.GetOutputStream() << "None"s;
        }
        is_first = false;
    }
    context.GetOutputStream() << '\n';
    return ObjectHolder::None();
}

MethodCall::MethodCall(std::unique_ptr<Statement> object, std::string method,
                       std::vector<std::unique_ptr<Statement>> args)
    : method_(std::move(method))
    , object_(std::move(object))
    , args_(std::move(args))
{}

ObjectHolder MethodCall::Execute(Closure& closure, Context& context) {
    auto obj =  object_->Execute(closure, context);
    std::vector<ObjectHolder> args;
    for (auto& ex : args_) {
        args.push_back(ex->Execute(closure, context));
    }
    return obj.TryAs<runtime::ClassInstance>()->Call(method_, args, context);
}

UnaryOperation::UnaryOperation(std::unique_ptr<Statement> argument)
    :argument_(std::move(argument))
{}

ObjectHolder Stringify::Execute(Closure& closure, Context& context) {
    ostringstream ss;
    auto obj = argument_->Execute(closure, context);
    if (obj.Get()) {
        argument_->Execute(closure, context)->Print(ss, context);
    }
    else {
        ss << "None"s;
    }
    return ObjectHolder::Own(runtime::String(ss.str()));
}

BinaryOperation::BinaryOperation(std::unique_ptr<Statement> lhs, std::unique_ptr<Statement> rhs)
    : lhs_(std::move(lhs))
    , rhs_(std::move(rhs))
{}

ObjectHolder Add::Execute(Closure& closure, Context& context) {
    auto lhs = lhs_->Execute(closure, context);
    auto rhs = rhs_->Execute(closure, context);
    auto lhs_ptr_number = lhs.TryAs<runtime::Number>();
    auto rhs_ptr_number = rhs.TryAs<runtime::Number>();
    if (lhs_ptr_number && rhs_ptr_number) {
        return ObjectHolder::Own(runtime::Number{ lhs_ptr_number->GetValue() + rhs_ptr_number->GetValue() });
    }
    auto lhs_ptr_string = lhs.TryAs<runtime::String>();
    auto rhs_ptr_string = rhs.TryAs<runtime::String>();
    if (lhs_ptr_string && rhs_ptr_string) {
        return ObjectHolder::Own(runtime::String{ lhs_ptr_string->GetValue() + rhs_ptr_string->GetValue() });
    }
    auto lhs_ptr_obj = lhs.TryAs<runtime::ClassInstance>();
    std::string method_add = "__add__"s;
    if (lhs_ptr_obj && lhs_ptr_obj->HasMethod(method_add, 1)) {
        return lhs_ptr_obj->Call(method_add, { rhs }, context);
    }
    throw std::runtime_error("no Add operation for such data types"s);
    return {};
}

ObjectHolder Sub::Execute(Closure& closure, Context& context) {
    auto lhs = lhs_->Execute(closure, context);
    auto rhs = rhs_->Execute(closure, context);
    auto lhs_ptr_number = lhs.TryAs<runtime::Number>();
    auto rhs_ptr_number = rhs.TryAs<runtime::Number>();
    if (lhs_ptr_number && rhs_ptr_number) {
        return ObjectHolder::Own(runtime::Number{ lhs_ptr_number->GetValue() - rhs_ptr_number->GetValue() });
    }
    throw std::runtime_error("no Sub operation for such data types"s);
    return {};
}

ObjectHolder Mult::Execute(Closure& closure, Context& context) {
    auto lhs = lhs_->Execute(closure, context);
    auto rhs = rhs_->Execute(closure, context);
    auto lhs_ptr_number = lhs.TryAs<runtime::Number>();
    auto rhs_ptr_number = rhs.TryAs<runtime::Number>();
    if (lhs_ptr_number && rhs_ptr_number) {
        return ObjectHolder::Own(runtime::Number{ lhs_ptr_number->GetValue() * rhs_ptr_number->GetValue() });
    }
    throw std::runtime_error("no Mult operation for such data types"s);
    return {};
}

ObjectHolder Div::Execute(Closure& closure, Context& context) {
    auto lhs = lhs_->Execute(closure, context);
    auto rhs = rhs_->Execute(closure, context);
    auto lhs_ptr_number = lhs.TryAs<runtime::Number>();
    auto rhs_ptr_number = rhs.TryAs<runtime::Number>();
    if (lhs_ptr_number && rhs_ptr_number) {
        if (rhs_ptr_number->GetValue() == 0) {
            throw std::runtime_error("Error div to null"s);
        }
        return ObjectHolder::Own(runtime::Number{ lhs_ptr_number->GetValue() / rhs_ptr_number->GetValue() });
    }
    throw std::runtime_error("no Div operation for such data types"s);
    return {};
}

void Compound::AddStatement(std::unique_ptr<Statement> stmt) {
    args_.push_back(std::move(stmt));
}

ObjectHolder Compound::Execute(Closure& closure, Context& context) {
    for (auto& ex : args_) {
        ex->Execute(closure, context);
    }
    return ObjectHolder::None();
}

Return::Return(std::unique_ptr<Statement> statement)
    : statement_(std::move(statement))
{}

ObjectHolder Return::Execute(Closure& closure, Context& context) {
    throw statement_->Execute(closure, context);
    return ObjectHolder::None();
}

ClassDefinition::ClassDefinition(ObjectHolder cls)
    : cls_(std::move(cls))
{}

ObjectHolder ClassDefinition::Execute(Closure& closure, [[maybe_unused]] Context& context) {
    closure[cls_.TryAs<runtime::Class>()->GetName()] = cls_;
    return cls_;
}

FieldAssignment::FieldAssignment(VariableValue object, std::string field_name,
                                 std::unique_ptr<Statement> rv)
    : object_(std::move(object))
    , field_name_(std::move(field_name))
    , rv_(std::move(rv))
{}

ObjectHolder FieldAssignment::Execute(Closure& closure, Context& context) {
    auto cls_instance_ptr = object_.Execute(closure, context).TryAs<runtime::ClassInstance>();
    auto& fields = cls_instance_ptr->Fields();
    fields[field_name_] = rv_->Execute(closure, context);
    return fields[field_name_];
}

IfElse::IfElse(std::unique_ptr<Statement> condition, std::unique_ptr<Statement> if_body,
               std::unique_ptr<Statement> else_body)
    : condition_(std::move(condition))
    , if_body_(std::move(if_body))
    , else_body_(else_body ? std::move(else_body) : nullptr)
{}

ObjectHolder IfElse::Execute(Closure& closure, Context& context) {
    if (runtime::IsTrue(condition_->Execute(closure, context))) {
        return if_body_->Execute(closure, context);
    }
    else if (else_body_) {
        return else_body_->Execute(closure, context);
    }
    return ObjectHolder::None();
}

ObjectHolder Or::Execute(Closure& closure, Context& context) {
    bool res = runtime::IsTrue(lhs_->Execute(closure, context)) 
        || runtime::IsTrue(rhs_->Execute(closure, context));
    return ObjectHolder::Own(runtime::Bool{ res });
}

ObjectHolder And::Execute(Closure& closure, Context& context) {
    bool res = runtime::IsTrue(lhs_->Execute(closure, context))
        && runtime::IsTrue(rhs_->Execute(closure, context));
    return ObjectHolder::Own(runtime::Bool{ res });
}

ObjectHolder Not::Execute(Closure& closure, Context& context) {
    bool res = !runtime::IsTrue(argument_->Execute(closure, context));
    return ObjectHolder::Own(runtime::Bool{ res });
}

Comparison::Comparison(Comparator cmp, unique_ptr<Statement> lhs, unique_ptr<Statement> rhs)
    : BinaryOperation(std::move(lhs), std::move(rhs))
    , cmp_(cmp)
{}

ObjectHolder Comparison::Execute(Closure& closure, Context& context) {
    bool res = cmp_(lhs_->Execute(closure, context)
                    , rhs_->Execute(closure, context)
                    , context);
    return ObjectHolder::Own(runtime::Bool{ res });
}

NewInstance::NewInstance(const runtime::Class& class_, std::vector<std::unique_ptr<Statement>> args)
    : cls_(class_)
    , args_(std::move(args))
{}

NewInstance::NewInstance(const runtime::Class& class_)
    : cls_(class_)
{}

ObjectHolder NewInstance::Execute(Closure& closure, Context& context) {
    auto obj_holder = ObjectHolder::Own(runtime::ClassInstance(cls_));
    auto& obj = *obj_holder.TryAs<runtime::ClassInstance>();
    obj.Fields()["self"s] = obj_holder;
    std::string method_init = "__init__"s;
    if (obj.HasMethod(method_init, args_.size())) {
        std::vector<ObjectHolder> args;
        for (auto& ex : args_) {
            args.push_back(ex->Execute(closure, context));
        }
        obj.Call(method_init, args, context);
    }
    return obj_holder;
}

MethodBody::MethodBody(std::unique_ptr<Statement>&& body)
    : body_(std::move(body))
{}

ObjectHolder MethodBody::Execute(Closure& closure , Context& context) {
    try {
        body_->Execute(closure, context);
    }
    catch (ObjectHolder obj) {
        return obj;
    }

    return ObjectHolder::None();
}

}  // namespace ast