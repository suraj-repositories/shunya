#include "shunya/core/base/expression_evaluator.hpp"
#include "shunya/core/base/units.hpp"

#include <cmath>
#include <cctype>
#include <sstream>
#include <algorithm>
#include <unordered_set>

namespace shunya::core::base {

namespace {

constexpr double PI_VAL = 3.14159265358979323846;
constexpr double E_VAL  = 2.71828182845904523536;

enum class TokenType {
    Number,
    Identifier,
    Plus,
    Minus,
    Multiply,
    Divide,
    Modulo,
    Power,
    LParen,
    RParen,
    Comma,
    End
};

struct Token {
    TokenType type = TokenType::End;
    double number_value = 0.0;
    std::string text;
    size_t position = 0;
};

class Lexer {
public:
    explicit Lexer(std::string_view input) : input_(input) {}

    Result<std::vector<Token>> tokenize() {
        std::vector<Token> tokens;
        // Strip optional leading '='
        size_t start_idx = 0;
        while (start_idx < input_.size() && (input_[start_idx] == ' ' || input_[start_idx] == '\t')) {
            ++start_idx;
        }
        if (start_idx < input_.size() && input_[start_idx] == '=') {
            cursor_ = start_idx + 1;
        } else {
            cursor_ = 0;
        }

        while (cursor_ < input_.size()) {
            char c = input_[cursor_];
            if (std::isspace(static_cast<unsigned char>(c))) {
                ++cursor_;
                continue;
            }

            size_t token_pos = cursor_;

            if (std::isdigit(static_cast<unsigned char>(c)) || (c == '.' && cursor_ + 1 < input_.size() && std::isdigit(static_cast<unsigned char>(input_[cursor_ + 1])))) {
                auto tok_res = lex_number_or_quantity();
                if (tok_res.is_err()) return Result<std::vector<Token>>::err(tok_res.error());
                tokens.push_back(tok_res.value());
                continue;
            }

            if (std::isalpha(static_cast<unsigned char>(c)) || c == '_' || c == '$') {
                std::string ident;
                while (cursor_ < input_.size() && (std::isalnum(static_cast<unsigned char>(input_[cursor_])) || input_[cursor_] == '_' || input_[cursor_] == '$')) {
                    ident.push_back(input_[cursor_++]);
                }
                tokens.push_back(Token{TokenType::Identifier, 0.0, std::move(ident), token_pos});
                continue;
            }

            switch (c) {
                case '+': tokens.push_back({TokenType::Plus, 0.0, "+", token_pos}); ++cursor_; break;
                case '-': tokens.push_back({TokenType::Minus, 0.0, "-", token_pos}); ++cursor_; break;
                case '*': tokens.push_back({TokenType::Multiply, 0.0, "*", token_pos}); ++cursor_; break;
                case '/': tokens.push_back({TokenType::Divide, 0.0, "/", token_pos}); ++cursor_; break;
                case '%': tokens.push_back({TokenType::Modulo, 0.0, "%", token_pos}); ++cursor_; break;
                case '^': tokens.push_back({TokenType::Power, 0.0, "^", token_pos}); ++cursor_; break;
                case '(': tokens.push_back({TokenType::LParen, 0.0, "(", token_pos}); ++cursor_; break;
                case ')': tokens.push_back({TokenType::RParen, 0.0, ")", token_pos}); ++cursor_; break;
                case ',': tokens.push_back({TokenType::Comma, 0.0, ",", token_pos}); ++cursor_; break;
                default:
                    return Result<std::vector<Token>>::err({
                        .code = 3020,
                        .domain = ErrorDomain::Validation,
                        .severity = Severity::Error,
                        .message = std::string("Unexpected character '") + c + "' at position " + std::to_string(token_pos),
                        .details = ""
                    });
            }
        }

        tokens.push_back(Token{TokenType::End, 0.0, "", cursor_});
        return Result<std::vector<Token>>::ok(std::move(tokens));
    }

private:
    Result<Token> lex_number_or_quantity() {
        size_t start = cursor_;
        while (cursor_ < input_.size() && std::isdigit(static_cast<unsigned char>(input_[cursor_]))) {
            ++cursor_;
        }
        if (cursor_ < input_.size() && input_[cursor_] == '.') {
            ++cursor_;
            while (cursor_ < input_.size() && std::isdigit(static_cast<unsigned char>(input_[cursor_]))) {
                ++cursor_;
            }
        }
        // Exponent check (e.g., 1e-3, 2.5E4)
        if (cursor_ < input_.size() && (input_[cursor_] == 'e' || input_[cursor_] == 'E')) {
            size_t next = cursor_ + 1;
            if (next < input_.size() && (input_[next] == '+' || input_[next] == '-')) {
                ++next;
            }
            if (next < input_.size() && std::isdigit(static_cast<unsigned char>(input_[next]))) {
                cursor_ = next;
                while (cursor_ < input_.size() && std::isdigit(static_cast<unsigned char>(input_[cursor_]))) {
                    ++cursor_;
                }
            }
        }

        std::string num_part(input_.substr(start, cursor_ - start));
        double val = 0.0;
        try {
            val = std::stod(num_part);
        } catch (...) {
            return Result<Token>::err({
                .code = 3021,
                .domain = ErrorDomain::Validation,
                .severity = Severity::Error,
                .message = "Failed to parse number '" + num_part + "'",
                .details = ""
            });
        }

        // Check for attached unit suffix (e.g. 5mm, 3.5kOhm, 100psi)
        size_t unit_start = cursor_;
        while (cursor_ < input_.size()) {
            unsigned char uc = static_cast<unsigned char>(input_[cursor_]);
            if (std::isalpha(uc) || uc >= 128 || (cursor_ > unit_start && (input_[cursor_] == '/' || input_[cursor_] == '^'))) {
                ++cursor_;
            } else {
                break;
            }
        }

        if (cursor_ > unit_start) {
            std::string unit_candidate(input_.substr(unit_start, cursor_ - unit_start));
            const auto* unit_def = units::UnitRegistry::instance().find_unit(unit_candidate);
            if (unit_def) {
                // Convert to canonical SI unit value
                auto canonical_res = units::UnitRegistry::instance().to_canonical(val, unit_candidate);
                if (canonical_res.is_ok()) {
                    val = canonical_res.value().value;
                }
            } else {
                // If not a recognized unit, rewind cursor so identifier lexing handles it
                cursor_ = unit_start;
            }
        }

        return Result<Token>::ok(Token{TokenType::Number, val, num_part, start});
    }

    std::string_view input_;
    size_t cursor_ = 0;
};

class Parser {
public:
    Parser(std::vector<Token> tokens, ExpressionEvaluator::SymbolResolver resolver)
        : tokens_(std::move(tokens)), resolver_(std::move(resolver)) {}

    Result<double> parse() {
        if (tokens_.empty() || (tokens_.size() == 1 && tokens_[0].type == TokenType::End)) {
            return Result<double>::err({
                .code = 3022,
                .domain = ErrorDomain::Validation,
                .severity = Severity::Error,
                .message = "Empty mathematical expression",
                .details = ""
            });
        }

        auto res = parse_expression();
        if (res.is_err()) return res;

        if (current().type != TokenType::End) {
            return Result<double>::err({
                .code = 3023,
                .domain = ErrorDomain::Validation,
                .severity = Severity::Error,
                .message = "Unexpected token '" + current().text + "' at end of expression",
                .details = ""
            });
        }

        return res;
    }

private:
    const Token& current() const {
        if (pos_ < tokens_.size()) return tokens_[pos_];
        return tokens_.back();
    }

    const Token& advance() {
        if (pos_ < tokens_.size()) ++pos_;
        return current();
    }

    Result<double> parse_expression() {
        auto left_res = parse_term();
        if (left_res.is_err()) return left_res;
        double left = left_res.value();

        while (current().type == TokenType::Plus || current().type == TokenType::Minus) {
            TokenType op = current().type;
            advance();
            auto right_res = parse_term();
            if (right_res.is_err()) return right_res;
            double right = right_res.value();

            if (op == TokenType::Plus) {
                left += right;
            } else {
                left -= right;
            }
        }

        return Result<double>::ok(left);
    }

    Result<double> parse_term() {
        auto left_res = parse_power();
        if (left_res.is_err()) return left_res;
        double left = left_res.value();

        while (current().type == TokenType::Multiply || current().type == TokenType::Divide || current().type == TokenType::Modulo) {
            TokenType op = current().type;
            advance();
            auto right_res = parse_power();
            if (right_res.is_err()) return right_res;
            double right = right_res.value();

            if (op == TokenType::Multiply) {
                left *= right;
            } else if (op == TokenType::Divide) {
                if (right == 0.0) {
                    return Result<double>::err({
                        .code = 3024,
                        .domain = ErrorDomain::Validation,
                        .severity = Severity::Error,
                        .message = "Division by zero in expression evaluation",
                        .details = ""
                    });
                }
                left /= right;
            } else if (op == TokenType::Modulo) {
                if (right == 0.0) {
                    return Result<double>::err({
                        .code = 3025,
                        .domain = ErrorDomain::Validation,
                        .severity = Severity::Error,
                        .message = "Modulo by zero in expression evaluation",
                        .details = ""
                    });
                }
                left = std::fmod(left, right);
            }
        }

        return Result<double>::ok(left);
    }

    Result<double> parse_power() {
        auto base_res = parse_unary();
        if (base_res.is_err()) return base_res;
        double base = base_res.value();

        if (current().type == TokenType::Power) {
            advance();
            // Right-associative exponentiation: 2 ^ 3 ^ 2 = 2 ^ (3 ^ 2)
            auto exp_res = parse_power();
            if (exp_res.is_err()) return exp_res;
            return Result<double>::ok(std::pow(base, exp_res.value()));
        }

        return Result<double>::ok(base);
    }

    Result<double> parse_unary() {
        if (current().type == TokenType::Plus) {
            advance();
            return parse_unary();
        }
        if (current().type == TokenType::Minus) {
            advance();
            auto operand_res = parse_unary();
            if (operand_res.is_err()) return operand_res;
            return Result<double>::ok(-operand_res.value());
        }

        return parse_primary();
    }

    Result<double> parse_primary() {
        if (current().type == TokenType::Number) {
            double val = current().number_value;
            advance();
            return Result<double>::ok(val);
        }

        if (current().type == TokenType::LParen) {
            advance();
            auto expr_res = parse_expression();
            if (expr_res.is_err()) return expr_res;
            if (current().type != TokenType::RParen) {
                return Result<double>::err({
                    .code = 3026,
                    .domain = ErrorDomain::Validation,
                    .severity = Severity::Error,
                    .message = "Expected closing parenthesis ')' at position " + std::to_string(current().position),
                    .details = ""
                });
            }
            advance();
            return expr_res;
        }

        if (current().type == TokenType::Identifier) {
            std::string name = current().text;
            advance();

            // Check if it is a function call
            if (current().type == TokenType::LParen) {
                advance();
                std::vector<double> args;
                if (current().type != TokenType::RParen) {
                    while (true) {
                        auto arg_res = parse_expression();
                        if (arg_res.is_err()) return arg_res;
                        args.push_back(arg_res.value());

                        if (current().type == TokenType::Comma) {
                            advance();
                        } else {
                            break;
                        }
                    }
                }
                if (current().type != TokenType::RParen) {
                    return Result<double>::err({
                        .code = 3027,
                        .domain = ErrorDomain::Validation,
                        .severity = Severity::Error,
                        .message = "Expected closing ')' after function arguments for '" + name + "'",
                        .details = ""
                    });
                }
                advance();
                return evaluate_function(name, args);
            }

            // Check math constants
            std::string lower_name = name;
            std::transform(lower_name.begin(), lower_name.end(), lower_name.begin(), [](unsigned char c) { return std::tolower(c); });
            if (lower_name == "pi") return Result<double>::ok(PI_VAL);
            if (lower_name == "e") return Result<double>::ok(E_VAL);

            // Variable lookup
            if (resolver_) {
                auto var_res = resolver_(name);
                if (var_res.is_ok()) return var_res;
                return Result<double>::err({
                    .code = 3028,
                    .domain = ErrorDomain::Validation,
                    .severity = Severity::Error,
                    .message = "Unknown variable or symbol '" + name + "': " + var_res.error().message,
                    .details = ""
                });
            }

            return Result<double>::err({
                .code = 3029,
                .domain = ErrorDomain::Validation,
                .severity = Severity::Error,
                .message = "Unknown variable '" + name + "' and no symbol resolver configured",
                .details = ""
            });
        }

        return Result<double>::err({
            .code = 3030,
            .domain = ErrorDomain::Validation,
            .severity = Severity::Error,
            .message = "Unexpected token '" + current().text + "' at position " + std::to_string(current().position),
            .details = ""
        });
    }

    Result<double> evaluate_function(const std::string& name, const std::vector<double>& args) {
        std::string fn = name;
        std::transform(fn.begin(), fn.end(), fn.begin(), [](unsigned char c) { return std::tolower(c); });

        // 1-argument functions
        if (args.size() == 1) {
            double x = args[0];
            if (fn == "sin") return Result<double>::ok(std::sin(x));
            if (fn == "cos") return Result<double>::ok(std::cos(x));
            if (fn == "tan") return Result<double>::ok(std::tan(x));
            if (fn == "asin") {
                if (x < -1.0 || x > 1.0) return Result<double>::err({.code = 3031, .domain = ErrorDomain::Validation, .severity = Severity::Error, .message = "asin argument domain error: out of [-1, 1]", .details = ""});
                return Result<double>::ok(std::asin(x));
            }
            if (fn == "acos") {
                if (x < -1.0 || x > 1.0) return Result<double>::err({.code = 3031, .domain = ErrorDomain::Validation, .severity = Severity::Error, .message = "acos argument domain error: out of [-1, 1]", .details = ""});
                return Result<double>::ok(std::acos(x));
            }
            if (fn == "atan") return Result<double>::ok(std::atan(x));
            if (fn == "sinh") return Result<double>::ok(std::sinh(x));
            if (fn == "cosh") return Result<double>::ok(std::cosh(x));
            if (fn == "tanh") return Result<double>::ok(std::tanh(x));
            if (fn == "sqrt") {
                if (x < 0.0) return Result<double>::err({.code = 3032, .domain = ErrorDomain::Validation, .severity = Severity::Error, .message = "sqrt of negative number", .details = ""});
                return Result<double>::ok(std::sqrt(x));
            }
            if (fn == "cbrt") return Result<double>::ok(std::cbrt(x));
            if (fn == "abs") return Result<double>::ok(std::abs(x));
            if (fn == "exp") return Result<double>::ok(std::exp(x));
            if (fn == "log" || fn == "ln") {
                if (x <= 0.0) return Result<double>::err({.code = 3033, .domain = ErrorDomain::Validation, .severity = Severity::Error, .message = "log of non-positive number", .details = ""});
                return Result<double>::ok(std::log(x));
            }
            if (fn == "log10") {
                if (x <= 0.0) return Result<double>::err({.code = 3033, .domain = ErrorDomain::Validation, .severity = Severity::Error, .message = "log10 of non-positive number", .details = ""});
                return Result<double>::ok(std::log10(x));
            }
            if (fn == "log2") {
                if (x <= 0.0) return Result<double>::err({.code = 3033, .domain = ErrorDomain::Validation, .severity = Severity::Error, .message = "log2 of non-positive number", .details = ""});
                return Result<double>::ok(std::log2(x));
            }
            if (fn == "floor") return Result<double>::ok(std::floor(x));
            if (fn == "ceil") return Result<double>::ok(std::ceil(x));
            if (fn == "round") return Result<double>::ok(std::round(x));
        }

        // 2-argument functions
        if (args.size() == 2) {
            double a = args[0];
            double b = args[1];
            if (fn == "pow") return Result<double>::ok(std::pow(a, b));
            if (fn == "atan2") return Result<double>::ok(std::atan2(a, b));
            if (fn == "min") return Result<double>::ok(std::min(a, b));
            if (fn == "max") return Result<double>::ok(std::max(a, b));
        }

        // 3-argument functions
        if (args.size() == 3) {
            if (fn == "clamp") {
                return Result<double>::ok(std::clamp(args[0], args[1], args[2]));
            }
        }

        return Result<double>::err({
            .code = 3034,
            .domain = ErrorDomain::Validation,
            .severity = Severity::Error,
            .message = "Unknown function '" + name + "' or invalid argument count (" + std::to_string(args.size()) + ")",
            .details = ""
        });
    }

    std::vector<Token> tokens_;
    size_t pos_ = 0;
    ExpressionEvaluator::SymbolResolver resolver_;
};

} // namespace

Result<double> ExpressionEvaluator::evaluate(std::string_view expr) const {
    Lexer lexer(expr);
    auto tok_res = lexer.tokenize();
    if (tok_res.is_err()) return Result<double>::err(tok_res.error());

    Parser parser(std::move(tok_res.value()), resolver_);
    return parser.parse();
}

Result<double> ExpressionEvaluator::evaluate_with_context(
    std::string_view expr,
    const std::unordered_map<std::string, double>& context
) const {
    ExpressionEvaluator eval([&context](std::string_view name) -> Result<double> {
        auto it = context.find(std::string(name));
        if (it != context.end()) {
            return Result<double>::ok(it->second);
        }
        return Result<double>::err({
            .code = 3035,
            .domain = ErrorDomain::Validation,
            .severity = Severity::Error,
            .message = "Variable not found in context: " + std::string(name),
            .details = ""
        });
    });
    return eval.evaluate(expr);
}

std::vector<std::string> ExpressionEvaluator::extract_variables(std::string_view expr) {
    Lexer lexer(expr);
    auto tok_res = lexer.tokenize();
    if (tok_res.is_err()) return {};

    static const std::unordered_set<std::string> builtins = {
        "sin", "cos", "tan", "asin", "acos", "atan", "atan2",
        "sinh", "cosh", "tanh", "sqrt", "cbrt", "abs",
        "exp", "log", "ln", "log10", "log2", "pow", "floor", "ceil", "round",
        "min", "max", "clamp", "pi", "e"
    };

    std::vector<std::string> vars;
    std::unordered_set<std::string> seen;

    const auto& tokens = tok_res.value();
    for (size_t i = 0; i < tokens.size(); ++i) {
        if (tokens[i].type == TokenType::Identifier) {
            // Check if next token is '(' -> function call
            if (i + 1 < tokens.size() && tokens[i + 1].type == TokenType::LParen) {
                continue;
            }
            std::string lower = tokens[i].text;
            std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return std::tolower(c); });
            if (builtins.count(lower) == 0 && seen.count(tokens[i].text) == 0) {
                seen.insert(tokens[i].text);
                vars.push_back(tokens[i].text);
            }
        }
    }

    return vars;
}

Result<void> ExpressionEvaluator::validate_syntax(std::string_view expr) {
    ExpressionEvaluator dummy([](std::string_view) -> Result<double> {
        return Result<double>::ok(1.0);
    });
    auto res = dummy.evaluate(expr);
    if (res.is_err()) return Result<void>::err(res.error());
    return Result<void>::ok();
}

} // namespace shunya::core::base
