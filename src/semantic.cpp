#include "semantic.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <string_view>
#include <utility>

#include "token.hpp"

namespace vista {
namespace {

ValueType value_type_for(FieldType type) {
    switch (type) {
        case FieldType::Text: return ValueType::Text;
        case FieldType::Integer: return ValueType::Integer;
        case FieldType::Decimal: return ValueType::Decimal;
        case FieldType::Boolean: return ValueType::Boolean;
        case FieldType::Date: return ValueType::Date;
        case FieldType::Choice: return ValueType::Choice;
        case FieldType::File: return ValueType::File;
        case FieldType::Email:
        case FieldType::Phone:
        case FieldType::Textarea: return ValueType::Text;
    }
    return ValueType::Error;
}

std::string value_type_name(ValueType type) {
    switch (type) {
        case ValueType::Text: return "text";
        case ValueType::Integer: return "integer";
        case ValueType::Decimal: return "decimal";
        case ValueType::Boolean: return "boolean";
        case ValueType::Date: return "date";
        case ValueType::Choice: return "choice";
        case ValueType::File: return "file";
        case ValueType::Error: return "error";
    }
    return "error";
}

bool is_numeric(ValueType type) {
    return type == ValueType::Integer || type == ValueType::Decimal;
}

bool contains_option(const Symbol& symbol, const std::string& option) {
    return std::find(symbol.options.begin(), symbol.options.end(), option) !=
           symbol.options.end();
}

void add_diagnostic(SemanticResult& result,
                    std::string code,
                    std::string message,
                    const SourceSpan& span) {
    result.diagnostics.push_back(
        {std::move(code), std::move(message), span.first_line, span.first_column});
}

const FieldProperty* first_label(const FieldDecl& field) {
    for (const FieldProperty* property : field.properties) {
        if (property->kind == PropertyKind::Label) {
            return property;
        }
    }
    return nullptr;
}

std::size_t constraint_index(PropertyKind kind) {
    switch (kind) {
        case PropertyKind::Minimum: return 0;
        case PropertyKind::Maximum: return 1;
        case PropertyKind::MinLength: return 2;
        case PropertyKind::MaxLength: return 3;
        default: return 4;
    }
}

bool parse_finite_number(const std::string& text, long double& value) {
    try {
        std::size_t parsed = 0;
        value = std::stold(text, &parsed);
        return parsed == text.size() && std::isfinite(value);
    } catch (...) {
        return false;
    }
}

bool parse_length(const std::string& text, unsigned long long& value) {
    if (text.empty() ||
        !std::all_of(text.begin(), text.end(), [](unsigned char character) {
            return character >= '0' && character <= '9';
        })) {
        return false;
    }
    try {
        std::size_t parsed = 0;
        value = std::stoull(text, &parsed);
        return parsed == text.size();
    } catch (...) {
        return false;
    }
}

class SemanticAnalyzer {
public:
    explicit SemanticAnalyzer(const FormAst& source_form) : form(source_form) {}

    SemanticResult run() {
        collect_symbols();
        validate_declarations();
        return std::move(result);
    }

private:
    const FormAst& form;
    SemanticResult result;

    void collect_symbols() {
        for (const Declaration* declaration : form.declarations) {
            if (declaration->kind != DeclarationKind::Field) {
                continue;
            }

            const FieldDecl& field = *declaration->field;
            const FieldProperty* label = first_label(field);
            Symbol symbol{
                field.name,
                field.type,
                field.options,
                label == nullptr ? "" : label->text,
                field.span
            };

            if (!result.symbols.insert(std::move(symbol))) {
                const Symbol* original = result.symbols.find(field.name);
                std::ostringstream message;
                message << "duplicate field '" << field.name << "'";
                if (original != nullptr) {
                    message << "; first declared at " << original->span.first_line << ':'
                            << original->span.first_column;
                }
                add_diagnostic(result, "SEM001", message.str(), field.span);
            }
        }
    }

    void validate_declarations() {
        for (const Declaration* declaration : form.declarations) {
            if (declaration->kind == DeclarationKind::Field) {
                validate_field(*declaration->field);
            } else {
                validate_condition(declaration->check->condition, "check condition");
            }
        }
    }

    void validate_field(const FieldDecl& field) {
        const FieldProperty* label = first_label(field);
        if (label == nullptr || label->text.empty()) {
            add_diagnostic(
                result,
                "SEM006",
                "field '" + field.name + "' requires a nonempty label",
                field.span);
        }

        for (const FieldProperty* property : field.properties) {
            if (property->kind == PropertyKind::RequiredWhen) {
                validate_condition(property->condition, "required condition");
            } else if (property->kind == PropertyKind::ShowWhen) {
                validate_condition(property->condition, "visibility condition");
            }
        }
        validate_constraints(field);
    }

    void validate_constraints(const FieldDecl& field) {
        const FieldProperty* limits[4] = {nullptr, nullptr, nullptr, nullptr};
        for (const FieldProperty* property : field.properties) {
            const std::size_t index = constraint_index(property->kind);
            if (index == 4) {
                continue;
            }
            if (limits[index] != nullptr) {
                add_diagnostic(
                    result,
                    "SEM012",
                    "field '" + field.name + "' declares the same constraint more than once",
                    property->span);
            } else {
                limits[index] = property;
            }
        }

        const bool has_numeric_limit = limits[0] != nullptr || limits[1] != nullptr;
        const bool has_length_limit = limits[2] != nullptr || limits[3] != nullptr;
        const bool numeric_field = field.type == FieldType::Integer ||
                                   field.type == FieldType::Decimal;
        if (has_numeric_limit && !numeric_field) {
            const FieldProperty* property = limits[0] != nullptr ? limits[0] : limits[1];
            add_diagnostic(
                result,
                "SEM007",
                "minimum and maximum constraints require an integer or decimal field",
                property->span);
        } else if (has_numeric_limit) {
            long double minimum = 0.0L;
            long double maximum = 0.0L;
            const bool minimum_is_valid =
                limits[0] == nullptr || parse_finite_number(limits[0]->text, minimum);
            const bool maximum_is_valid =
                limits[1] == nullptr || parse_finite_number(limits[1]->text, maximum);
            if (!minimum_is_valid || !maximum_is_valid) {
                add_diagnostic(result, "SEM008", "numeric constraints must be finite values", field.span);
            } else if (field.type == FieldType::Integer &&
                       ((limits[0] != nullptr && limits[0]->text.find('.') != std::string::npos) ||
                        (limits[1] != nullptr && limits[1]->text.find('.') != std::string::npos))) {
                const FieldProperty* property =
                    limits[0] != nullptr && limits[0]->text.find('.') != std::string::npos
                        ? limits[0]
                        : limits[1];
                add_diagnostic(
                    result,
                    "SEM013",
                    "integer field bounds must use whole-number values",
                    property->span);
            } else if (limits[0] != nullptr && limits[1] != nullptr && minimum > maximum) {
                add_diagnostic(result, "SEM008", "minimum value must not exceed maximum value", limits[1]->span);
            }
        }

        if (!has_length_limit) {
            return;
        }
        if (field.type != FieldType::Textarea) {
            const FieldProperty* property = limits[2] != nullptr ? limits[2] : limits[3];
            add_diagnostic(
                result,
                "SEM009",
                "length constraints require a textarea field",
                property->span);
            return;
        }

        unsigned long long minimum_length = 0;
        unsigned long long maximum_length = 0;
        if ((limits[2] != nullptr && !parse_length(limits[2]->text, minimum_length)) ||
            (limits[3] != nullptr && !parse_length(limits[3]->text, maximum_length))) {
            const FieldProperty* property =
                limits[2] != nullptr && !parse_length(limits[2]->text, minimum_length)
                    ? limits[2]
                    : limits[3];
            add_diagnostic(
                result,
                "SEM010",
                "length constraints must be whole numbers within the supported range",
                property->span);
        } else if (limits[2] != nullptr && limits[3] != nullptr &&
                   minimum_length > maximum_length) {
            add_diagnostic(
                result,
                "SEM011",
                "minimum length must not exceed maximum length",
                limits[3]->span);
        }
    }

    void validate_condition(const Expression* expression, std::string_view description) {
        const ExpressionInfo info = analyze_expression(expression);
        if (info.type != ValueType::Boolean && info.type != ValueType::Error) {
            add_diagnostic(
                result,
                "SEM004",
                std::string(description) + " must be boolean, but found " +
                    value_type_name(info.type),
                expression->span);
        }
    }

    ExpressionInfo remember(const Expression* expression, ExpressionInfo info) {
        result.expressions[expression] = info;
        return info;
    }

    ExpressionInfo analyze_name(const Expression* expression,
                                const Symbol* choice_context = nullptr) {
        const Symbol* symbol = result.symbols.find(expression->value);
        if (symbol != nullptr) {
            return remember(
                expression,
                {value_type_for(symbol->type), symbol->name, false});
        }

        if (choice_context != nullptr && contains_option(*choice_context, expression->value)) {
            return remember(expression, {ValueType::Choice, choice_context->name, true});
        }

        if (choice_context != nullptr) {
            add_diagnostic(
                result,
                "SEM005",
                "'" + expression->value + "' is not an option of choice field '" +
                    choice_context->name + "'",
                expression->span);
        } else {
            add_diagnostic(
                result,
                "SEM002",
                "undefined field or value '" + expression->value + "'",
                expression->span);
        }
        return remember(expression, {ValueType::Error, "", false});
    }

    ExpressionInfo analyze_comparison(const Expression* expression) {
        const Expression* left = expression->left;
        const Expression* right = expression->right;
        const Symbol* left_symbol =
            left->kind == ExpressionKind::Name ? result.symbols.find(left->value) : nullptr;
        const Symbol* right_symbol =
            right->kind == ExpressionKind::Name ? result.symbols.find(right->value) : nullptr;

        ExpressionInfo left_info;
        ExpressionInfo right_info;

        if (left_symbol != nullptr && left_symbol->type == FieldType::Choice &&
            right->kind == ExpressionKind::Name && right_symbol == nullptr) {
            left_info = analyze_name(left);
            right_info = analyze_name(right, left_symbol);
        } else if (right_symbol != nullptr && right_symbol->type == FieldType::Choice &&
                   left->kind == ExpressionKind::Name && left_symbol == nullptr) {
            left_info = analyze_name(left, right_symbol);
            right_info = analyze_name(right);
        } else {
            left_info = analyze_expression(left);
            right_info = analyze_expression(right);
        }

        if (left_info.type == ValueType::Error || right_info.type == ValueType::Error) {
            return remember(expression, {ValueType::Error, "", false});
        }

        const bool equality = expression->value == "==" || expression->value == "!=";
        bool compatible = false;
        if (equality) {
            compatible = left_info.type == right_info.type ||
                         (is_numeric(left_info.type) && is_numeric(right_info.type));
            if (left_info.type == ValueType::File || right_info.type == ValueType::File) {
                compatible = false;
            }
        } else {
            compatible = (is_numeric(left_info.type) && is_numeric(right_info.type)) ||
                         (left_info.type == ValueType::Date &&
                          right_info.type == ValueType::Date);
        }

        if (!compatible) {
            add_diagnostic(
                result,
                "SEM003",
                "operator '" + expression->value + "' cannot compare " +
                    value_type_name(left_info.type) + " and " +
                    value_type_name(right_info.type),
                expression->span);
            return remember(expression, {ValueType::Error, "", false});
        }

        return remember(expression, {ValueType::Boolean, "", false});
    }

    ExpressionInfo analyze_expression(const Expression* expression) {
        switch (expression->kind) {
            case ExpressionKind::Name:
                return analyze_name(expression);
            case ExpressionKind::StringLiteral:
                return remember(expression, {ValueType::Text, "", false});
            case ExpressionKind::IntegerLiteral:
                return remember(expression, {ValueType::Integer, "", false});
            case ExpressionKind::DecimalLiteral:
                return remember(expression, {ValueType::Decimal, "", false});
            case ExpressionKind::BooleanLiteral:
                return remember(expression, {ValueType::Boolean, "", false});
            case ExpressionKind::Unary: {
                const ExpressionInfo operand = analyze_expression(expression->left);
                if (operand.type == ValueType::Error) {
                    return remember(expression, {ValueType::Error, "", false});
                }
                if (operand.type != ValueType::Boolean) {
                    add_diagnostic(
                        result,
                        "SEM003",
                        "operator 'not' requires a boolean operand, but found " +
                            value_type_name(operand.type),
                        expression->span);
                    return remember(expression, {ValueType::Error, "", false});
                }
                return remember(expression, {ValueType::Boolean, "", false});
            }
            case ExpressionKind::Binary:
                break;
        }

        if (expression->value == "and" || expression->value == "or") {
            const ExpressionInfo left = analyze_expression(expression->left);
            const ExpressionInfo right = analyze_expression(expression->right);
            if (left.type == ValueType::Error || right.type == ValueType::Error) {
                return remember(expression, {ValueType::Error, "", false});
            }
            if (left.type != ValueType::Boolean || right.type != ValueType::Boolean) {
                add_diagnostic(
                    result,
                    "SEM003",
                    "operator '" + expression->value +
                        "' requires boolean operands, but found " +
                        value_type_name(left.type) + " and " + value_type_name(right.type),
                    expression->span);
                return remember(expression, {ValueType::Error, "", false});
            }
            return remember(expression, {ValueType::Boolean, "", false});
        }

        return analyze_comparison(expression);
    }
};

}  // namespace

bool SymbolTable::insert(Symbol symbol) {
    if (indexes_.find(symbol.name) != indexes_.end()) {
        return false;
    }
    indexes_[symbol.name] = entries_.size();
    entries_.push_back(std::move(symbol));
    return true;
}

const Symbol* SymbolTable::find(const std::string& name) const {
    const auto iterator = indexes_.find(name);
    if (iterator == indexes_.end()) {
        return nullptr;
    }
    return &entries_[iterator->second];
}

const std::vector<Symbol>& SymbolTable::entries() const {
    return entries_;
}

SemanticResult analyze_semantics(const FormAst& form) {
    return SemanticAnalyzer(form).run();
}

std::string format_symbol_table(const SymbolTable& symbols) {
    std::ostringstream output;
    output << std::left << std::setw(16) << "NAME"
           << std::setw(12) << "TYPE"
           << std::setw(24) << "OPTIONS"
           << std::setw(24) << "LABEL"
           << "LOCATION\n";
    output << std::string(86, '-') << '\n';

    for (const Symbol& symbol : symbols.entries()) {
        std::string options = "-";
        if (!symbol.options.empty()) {
            options.clear();
            for (std::size_t index = 0; index < symbol.options.size(); ++index) {
                if (index != 0) {
                    options += ',';
                }
                options += symbol.options[index];
            }
        }

        const std::string label = symbol.label.empty() ? "-" : display_lexeme(symbol.label);
        const std::string location = std::to_string(symbol.span.first_line) + ':' +
                                     std::to_string(symbol.span.first_column);
        output << std::left << std::setw(16) << symbol.name
               << std::setw(12) << field_type_name(symbol.type)
               << std::setw(24) << options
               << std::setw(24) << label
               << location << '\n';
    }
    return output.str();
}

}  // namespace vista
