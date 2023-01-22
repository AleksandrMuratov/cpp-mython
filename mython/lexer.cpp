#include "lexer.h"

#include <algorithm>
#include <charconv>
#include <optional>
#include <cctype>

using namespace std;

namespace parse {

bool operator==(const Token& lhs, const Token& rhs) {
    using namespace token_type;

    if (lhs.index() != rhs.index()) {
        return false;
    }
    if (lhs.Is<Char>()) {
        return lhs.As<Char>().value == rhs.As<Char>().value;
    }
    if (lhs.Is<Number>()) {
        return lhs.As<Number>().value == rhs.As<Number>().value;
    }
    if (lhs.Is<String>()) {
        return lhs.As<String>().value == rhs.As<String>().value;
    }
    if (lhs.Is<Id>()) {
        return lhs.As<Id>().value == rhs.As<Id>().value;
    }
    return true;
}

bool operator!=(const Token& lhs, const Token& rhs) {
    return !(lhs == rhs);
}

std::ostream& operator<<(std::ostream& os, const Token& rhs) {
    using namespace token_type;

#define VALUED_OUTPUT(type) \
    if (auto p = rhs.TryAs<type>()) return os << #type << '{' << p->value << '}';

    VALUED_OUTPUT(Number);
    VALUED_OUTPUT(Id);
    VALUED_OUTPUT(String);
    VALUED_OUTPUT(Char);

#undef VALUED_OUTPUT

#define UNVALUED_OUTPUT(type) \
    if (rhs.Is<type>()) return os << #type;

    UNVALUED_OUTPUT(Class);
    UNVALUED_OUTPUT(Return);
    UNVALUED_OUTPUT(If);
    UNVALUED_OUTPUT(Else);
    UNVALUED_OUTPUT(Def);
    UNVALUED_OUTPUT(Newline);
    UNVALUED_OUTPUT(Print);
    UNVALUED_OUTPUT(Indent);
    UNVALUED_OUTPUT(Dedent);
    UNVALUED_OUTPUT(And);
    UNVALUED_OUTPUT(Or);
    UNVALUED_OUTPUT(Not);
    UNVALUED_OUTPUT(Eq);
    UNVALUED_OUTPUT(NotEq);
    UNVALUED_OUTPUT(LessOrEq);
    UNVALUED_OUTPUT(GreaterOrEq);
    UNVALUED_OUTPUT(None);
    UNVALUED_OUTPUT(True);
    UNVALUED_OUTPUT(False);
    UNVALUED_OUTPUT(Eof);

#undef UNVALUED_OUTPUT

    return os << "Unknown token :("sv;
}

std::map<std::string_view, Token> Lexer::key_words_for_tokens = {
    {"class"sv, token_type::Class()},
    {"return"sv, token_type::Return()},
    {"if"sv, token_type::If()},
    {"else"sv, token_type::Else()},
    {"def"sv, token_type::Def()},
    {"print"sv, token_type::Print()},
    {"or"sv, token_type::Or()},
    {"None"sv, token_type::None()},
    {"and"sv, token_type::And()},
    {"not"sv, token_type::Not()},
    {"True"sv, token_type::True()},
    {"False"sv, token_type::False()}
};

std::map<std::string_view, Token> Lexer::symbols_for_comparison = {
    {"=="sv, token_type::Eq()},
    {"!="sv, token_type::NotEq()},
    {"<="sv, token_type::LessOrEq()},
    {">="sv, token_type::GreaterOrEq()}
};

std::vector<char> Lexer::chars = { '<', '>', '=', '+', '-', '*', '/', '(', ')', '.', ',', ':' };

std::vector<std::string_view> Lexer::GetKeyWords() {
    std::vector<std::string_view> res;
    res.reserve(key_words_for_tokens.size());
    for (const auto& [key_word, _] : key_words_for_tokens) {
        res.push_back(key_word);
    }
    return res;
}

std::vector<std::string_view> Lexer::GetSymbolsForComparison() {
    std::vector<string_view> res;
    res.reserve(symbols_for_comparison.size());
    for (const auto& [comparison, _] : symbols_for_comparison) {
        res.push_back(comparison);
    }
    return res;
}

Token Lexer::GetTokenFromKeyWord(std::string_view key_word) {
    auto it = key_words_for_tokens.find(key_word);
    if (it == key_words_for_tokens.end()) {
        throw LexerError("logic error in Lexer::GetTokenFromKeyWord"s);
    }
    return it->second;
}

int Lexer::RemoveSpaceLeft(std::string_view& line) {
    int count_space = 0;
    while (!line.empty() && line.front() == ' ') {
        ++count_space;
        line.remove_prefix(1);
    }
    return count_space;
}

std::optional<Token> Lexer::SplitTokenChar(std::string_view& line) {
    if (!line.empty() && std::find(chars.begin(), chars.end(), line.front()) != chars.end()) {
        char symbol = line.front();
        line.remove_prefix(1);
        return token_type::Char{ symbol };
    }
    else {
        return std::nullopt;
    }
}

std::optional<Token> Lexer::SplitTokenNumber(std::string_view& line) {
    size_t pos = 0;
    while (pos < line.size() && std::isdigit(static_cast<unsigned char>(line[pos]))) {
        ++pos;
    }
    if (pos != 0) {
        std::string_view digits = line.substr(0, pos);
        int num = std::stoi(std::string(digits));
        line.remove_prefix(pos);
        return token_type::Number{ num };
    }
    else {
        return std::nullopt;
    }
}

std::optional<Token> Lexer::SplitTokenString(std::string_view& line) {
    const std::string symbols_range = R"("')"s;
    const std::string symbols_for_backslash_shielding = R"("'nt)"s;
    if (line.empty() || symbols_range.find(line.front()) == std::string::npos) {
        return std::nullopt;
    }
    char start = line.front();
    line.remove_prefix(1);
    size_t pos = 0;
    std::string value;
    while (pos < line.size()) {
        if (line[pos] == '\\') {
            size_t next_pos = pos + 1;
            if (next_pos < line.size() && symbols_for_backslash_shielding.find(line[next_pos]) != std::string::npos) {
                ++pos;
            }
            if (line[pos] == 'n') {
                value += '\n';
            }
            else if (line[pos] == 't') {
                value += '\t';
            }
            else {
                value += line[pos];
            }
            ++pos;
        }
        else if (line[pos] == start) {
            ++pos;
            break;
        }
        else {
            value += line[pos++];
        }
    }
    line.remove_prefix(pos);
    return token_type::String{ std::move(value) };
}

bool Lexer::CheckSymbolForIdBegin(char c) {
    return c == '_'
        || (c >= 'a' && c <= 'z')
        || (c >= 'A' && c <= 'Z');
}

bool Lexer::CheckSymbolForId(char c) {
    return CheckSymbolForIdBegin(c)
        || std::isdigit(static_cast<unsigned char>(c));
}

std::optional<Token> Lexer::SplitTokenId(std::string_view& line) {
    if (line.empty() || !CheckSymbolForIdBegin(line.front())) {
        return std::nullopt;
    }
    size_t pos = 1;
    while (pos < line.size() && CheckSymbolForId(line[pos])) {
        ++pos;
    }
    std::string_view value = line.substr(0, pos);
    line.remove_prefix(pos);
    return token_type::Id{ std::string(value) };
}

Token Lexer::GetTokenFromComparison(std::string_view comparison) {
    auto it = symbols_for_comparison.find(comparison);
    if (it == symbols_for_comparison.end()) {
        throw LexerError("logic error in Lexer::GetTokenFromComparison"s);
    }
    return it->second;
}

std::optional<Token> Lexer::SplitTokenKeyWord(std::string_view& line) {
    static std::vector<std::string_view> key_words = GetKeyWords();
    auto it_to_found_word = std::find_if(key_words.begin(), key_words.end(), [&line](const string_view key_word) {
        return line.size() >= key_word.size()
        && std::equal(key_word.begin(), key_word.end(), line.begin());
        });

    if (it_to_found_word != key_words.end()
        && (line.size() == it_to_found_word->size()
            || std::isspace(line[it_to_found_word->size()])
            || line[it_to_found_word->size()] == ':'
            || line[it_to_found_word->size()] == ',')) {

        line.remove_prefix(it_to_found_word->size());
        return GetTokenFromKeyWord(*it_to_found_word);
    }
    else {
        return std::nullopt;
    }
}

std::optional<Token> Lexer::SplitTokenComparison(std::string_view& line) {
    static std::vector<string_view> comparisons = GetSymbolsForComparison();
    auto it_to_found_comparison = std::find_if(comparisons.begin(), comparisons.end(), [&line](const string_view comparison) {
        return line.size() >= comparison.size()
        && std::equal(comparison.begin(), comparison.end(), line.begin());
        });

    if (it_to_found_comparison != comparisons.end()) {
        line.remove_prefix(it_to_found_comparison->size());
        return GetTokenFromComparison(*it_to_found_comparison);
    }
    else {
        return std::nullopt;
    }
}

std::vector<std::function<std::optional<Token>(std::string_view&)>> Lexer::GetSplitersOfTokens() {
    std::vector<std::function<std::optional<Token>(std::string_view&)>> spliters_of_tokens;
    spliters_of_tokens.push_back(SplitTokenKeyWord);
    spliters_of_tokens.push_back(SplitTokenComparison);
    spliters_of_tokens.push_back(SplitTokenChar);
    spliters_of_tokens.push_back(SplitTokenNumber);
    spliters_of_tokens.push_back(SplitTokenString);
    spliters_of_tokens.push_back(SplitTokenId);
    return spliters_of_tokens;
}

std::optional<Token> Lexer::SplitTokenLeft(std::string_view& line) {
    if (line.empty() || line.front() == '#') {
        line = {};
        return std::nullopt;
    }
    static std::vector<std::function<std::optional<Token>(std::string_view&)>> 
        spliters_of_tokens = GetSplitersOfTokens();

    for (auto& split_token : spliters_of_tokens) {
        if (auto token = split_token(line)) {
            return *token;
        }
    }
    throw LexerError("logic error in Lexer::SplitTokenLeft");
    return Token();
}

Lexer::Lexer(std::istream& input) {
    std::string line;
    int prev_indent = 0;
    while (std::getline(input, line)) {
        std::string_view line_view = line;
        int current_indent = RemoveSpaceLeft(line_view);
        if (line_view.empty()) {
            continue;
        }
        if (current_indent % 2 != 0) {
            throw LexerError("Error of indent"s);
        }
        std::vector<Token> tokens_of_current_line;
        if (current_indent > prev_indent) {
            tokens_of_current_line.push_back(token_type::Indent());
        }
        else if (current_indent < prev_indent) {
            for (int i = 0; i < (prev_indent - current_indent) / 2; ++i) {
                tokens_of_current_line.push_back(token_type::Dedent());
            }
        }
        prev_indent = current_indent;
        while (!line_view.empty()) {
            RemoveSpaceLeft(line_view);
            if (line_view.empty()) {
                break;
            }
            if (auto token = SplitTokenLeft(line_view)) {
                tokens_of_current_line.push_back(std::move(*token));
            }
        }
        if (!tokens_of_current_line.empty()) {
            for (auto& token : tokens_of_current_line) {
                tokens_.push_back(std::move(token));
            }
            tokens_.push_back(token_type::Newline());
        }
    }
    for (int i = 0; i < prev_indent / 2; ++i) {
        tokens_.push_back(token_type::Dedent());
    }
    tokens_.push_back(token_type::Eof());
    it_to_current_token_ = tokens_.begin();
}

const Token& Lexer::CurrentToken() const {
    return *it_to_current_token_;
}

Token Lexer::NextToken() {
    if (it_to_current_token_ != std::prev(tokens_.end())) {
        ++it_to_current_token_;
    }
    return *it_to_current_token_;
}

}  // namespace parse