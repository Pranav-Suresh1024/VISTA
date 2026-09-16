#include "validation.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <fstream>
#include <iomanip>
#include <regex>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace vista {
namespace {

struct SuppliedValue {
    std::string text;
    std::size_t line = 1;
};

struct RuntimeValue {
    FieldType type = FieldType::Text;
    std::string text;
    long double number = 0.0L;
    bool boolean = false;
    bool present = false;
    bool valid = true;
};

using SuppliedValues = std::unordered_map<std::string, SuppliedValue>;
using RuntimeValues = std::unordered_map<std::string, RuntimeValue>;

std::string trim(const std::string& text) {
    const std::size_t beginning = text.find_first_not_of(" \t\r\n");
    if (beginning == std::string::npos) {
        return "";
    }
    const std::size_t ending = text.find_last_not_of(" \t\r\n");
    return text.substr(beginning, ending - beginning + 1);
}

bool valid_identifier(const std::string& text) {
    if (text.empty() ||
        !((text[0] >= 'A' && text[0] <= 'Z') ||
          (text[0] >= 'a' && text[0] <= 'z') || text[0] == '_')) {
        return false;
    }
    return std::all_of(text.begin() + 1, text.end(), [](unsigned char character) {
        return (character >= 'A' && character <= 'Z') ||
               (character >= 'a' && character <= 'z') ||
               (character >= '0' && character <= '9') || character == '_';
    });
}

bool decode_value(const std::string& source, std::string& value) {
    const std::string input = trim(source);
    if (input.empty() || input.front() != '"') {
        value = input;
        return true;
    }
    if (input.size() < 2 || input.back() != '"') {
        return false;
    }
    value.clear();
    for (std::size_t index = 1; index + 1 < input.size(); ++index) {
        if (input[index] != '\\') {
            if (input[index] == '"') {
                return false;
            }
            value.push_back(input[index]);
            continue;
        }
        if (index + 1 >= input.size() - 1) {
            return false;
        }
        ++index;
        switch (input[index]) {
            case 'n': value.push_back('\n'); break;
            case 'r': value.push_back('\r'); break;
            case 't': value.push_back('\t'); break;
            case '\\': value.push_back('\\'); break;
            case '"': value.push_back('"'); break;
            default: return false;
        }
    }
    return true;
}

void add_error(ValidationResult& result,
               std::string code,
               std::string message,
               std::size_t line = 0) {
    result.diagnostics.push_back(
        {std::move(code), std::move(message), line, line == 0 ? 0U : 1U});
}

bool parse_number(const std::string& text, long double& value) {
    try {
        std::size_t parsed = 0;
        value = std::stold(text, &parsed);
        return parsed == text.size() && std::isfinite(value);
    } catch (...) {
        return false;
    }
}

bool parse_integer(const std::string& text, long double& value) {
    static const std::regex integer_pattern("^-?[0-9]+$");
    return std::regex_match(text, integer_pattern) && parse_number(text, value);
}

bool is_leap_year(int year) {
    return year % 400 == 0 || (year % 4 == 0 && year % 100 != 0);
}

bool valid_iso_date(const std::string& text) {
    static const std::regex date_pattern("^[0-9]{4}-[0-9]{2}-[0-9]{2}$");
    if (!std::regex_match(text, date_pattern)) {
        return false;
    }
    const int year = std::stoi(text.substr(0, 4));
    const int month = std::stoi(text.substr(5, 2));
    const int day = std::stoi(text.substr(8, 2));
    if (year == 0 || month < 1 || month > 12) {
        return false;
    }
    static const int days_per_month[] = {
        31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
    };
    int maximum_day = days_per_month[month - 1];
    if (month == 2 && is_leap_year(year)) {
        maximum_day = 29;
    }
    return day >= 1 && day <= maximum_day;
}

const std::string* property_value(const FieldDecl& field, PropertyKind kind) {
    for (const FieldProperty* property : field.properties) {
        if (property->kind == kind) {
            return &property->text;
        }
    }
    return nullptr;
}

std::vector<const Expression*> conditions_for(const FieldDecl& field,
                                               PropertyKind kind) {
    std::vector<const Expression*> conditions;
    for (const FieldProperty* property : field.properties) {
        if (property->kind == kind) {
            conditions.push_back(property->condition);
        }
    }
    return conditions;
}

bool always_required(const FieldDecl& field) {
    return std::any_of(field.properties.begin(), field.properties.end(),
                       [](const FieldProperty* property) {
                           return property->kind == PropertyKind::Required;
                       });
}

std::string field_label(const FieldDecl& field) {
    const std::string* label = property_value(field, PropertyKind::Label);
    return label == nullptr || label->empty() ? field.name : *label;
}

RuntimeValue literal_value(const Expression* expression,
                           const SemanticResult& semantics,
                           const RuntimeValues& values);

bool evaluate_boolean(const Expression* expression,
                      const SemanticResult& semantics,
                      const RuntimeValues& values,
                      bool& result) {
    if (expression->kind == ExpressionKind::BooleanLiteral) {
        result = expression->value == "true";
        return true;
    }
    if (expression->kind == ExpressionKind::Name) {
        const RuntimeValue value = literal_value(expression, semantics, values);
        if (!value.valid || value.type != FieldType::Boolean) {
            return false;
        }
        result = value.boolean;
        return true;
    }
    if (expression->kind == ExpressionKind::Unary) {
        bool operand = false;
        if (!evaluate_boolean(expression->left, semantics, values, operand)) {
            return false;
        }
        result = !operand;
        return true;
    }
    if (expression->kind != ExpressionKind::Binary) {
        return false;
    }
    if (expression->value == "and" || expression->value == "or") {
        bool left = false;
        bool right = false;
        if (!evaluate_boolean(expression->left, semantics, values, left) ||
            !evaluate_boolean(expression->right, semantics, values, right)) {
            return false;
        }
        result = expression->value == "and" ? left && right : left || right;
        return true;
    }

    const RuntimeValue left = literal_value(expression->left, semantics, values);
    const RuntimeValue right = literal_value(expression->right, semantics, values);
    if (!left.valid || !right.valid) {
        return false;
    }
    bool equal = false;
    int ordering = 0;
    const bool numeric_left = left.type == FieldType::Integer ||
                              left.type == FieldType::Decimal;
    const bool numeric_right = right.type == FieldType::Integer ||
                               right.type == FieldType::Decimal;
    if (numeric_left && numeric_right) {
        equal = left.number == right.number;
        ordering = left.number < right.number ? -1 : (left.number > right.number ? 1 : 0);
    } else if (left.type == FieldType::Boolean && right.type == FieldType::Boolean) {
        equal = left.boolean == right.boolean;
        ordering = left.boolean == right.boolean ? 0 : (left.boolean ? 1 : -1);
    } else {
        equal = left.text == right.text;
        ordering = left.text < right.text ? -1 : (left.text > right.text ? 1 : 0);
    }
    if (expression->value == "==") result = equal;
    else if (expression->value == "!=") result = !equal;
    else if (expression->value == "<") result = ordering < 0;
    else if (expression->value == "<=") result = ordering <= 0;
    else if (expression->value == ">") result = ordering > 0;
    else if (expression->value == ">=") result = ordering >= 0;
    else return false;
    return true;
}

RuntimeValue literal_value(const Expression* expression,
                           const SemanticResult& semantics,
                           const RuntimeValues& values) {
    RuntimeValue value;
    const auto information = semantics.expressions.find(expression);
    if (information != semantics.expressions.end() && information->second.choice_literal) {
        value.type = FieldType::Choice;
        value.text = expression->value;
        value.present = true;
        return value;
    }
    switch (expression->kind) {
        case ExpressionKind::Name: {
            const auto found = values.find(expression->value);
            if (found == values.end()) {
                value.valid = false;
                return value;
            }
            return found->second;
        }
        case ExpressionKind::StringLiteral:
            value.type = information != semantics.expressions.end() &&
                                 information->second.type == ValueType::Date
                             ? FieldType::Date
                             : FieldType::Text;
            value.text = expression->value;
            value.present = true;
            return value;
        case ExpressionKind::IntegerLiteral:
            value.type = FieldType::Integer;
            value.text = expression->value;
            value.present = true;
            value.valid = parse_number(value.text, value.number);
            return value;
        case ExpressionKind::DecimalLiteral:
            value.type = FieldType::Decimal;
            value.text = expression->value;
            value.present = true;
            value.valid = parse_number(value.text, value.number);
            return value;
        case ExpressionKind::BooleanLiteral:
            value.type = FieldType::Boolean;
            value.text = expression->value;
            value.boolean = expression->value == "true";
            value.present = true;
            return value;
        case ExpressionKind::Unary:
        case ExpressionKind::Binary: {
            value.type = FieldType::Boolean;
            value.present = true;
            value.valid = evaluate_boolean(expression, semantics, values, value.boolean);
            value.text = value.boolean ? "true" : "false";
            return value;
        }
    }
    value.valid = false;
    return value;
}

void collect_field_references(const Expression* expression,
                              const SemanticResult& semantics,
                              std::unordered_set<std::string>& references) {
    if (expression == nullptr) {
        return;
    }
    const auto information = semantics.expressions.find(expression);
    const bool choice_literal = information != semantics.expressions.end() &&
                                information->second.choice_literal;
    if (expression->kind == ExpressionKind::Name && !choice_literal &&
        semantics.symbols.find(expression->value) != nullptr) {
        references.insert(expression->value);
    }
    collect_field_references(expression->left, semantics, references);
    collect_field_references(expression->right, semantics, references);
}

RuntimeValue convert_value(const FieldDecl& field,
                           const SuppliedValue* supplied,
                           ValidationResult& result) {
    RuntimeValue value;
    value.type = field.type;
    if (supplied == nullptr) {
        if (field.type == FieldType::Boolean) {
            value.valid = true;
            value.boolean = false;
            value.text = "false";
        }
        return value;
    }
    value.present = !supplied->text.empty();
    value.text = supplied->text;
    if (!value.present) {
        return value;
    }

    auto format_error = [&](const std::string& expectation) {
        value.valid = false;
        add_error(result, "VAL005", "field '" + field.name + "' " + expectation,
                  supplied->line);
    };
    switch (field.type) {
        case FieldType::Integer:
            if (!parse_integer(value.text, value.number)) {
                format_error("must be a whole number");
            }
            break;
        case FieldType::Decimal:
            if (!parse_number(value.text, value.number)) {
                format_error("must be a finite number");
            }
            break;
        case FieldType::Boolean:
            if (value.text != "true" && value.text != "false") {
                format_error("must be true or false");
            } else {
                value.boolean = value.text == "true";
            }
            break;
        case FieldType::Date:
            if (!valid_iso_date(value.text)) {
                format_error("must be a valid date in YYYY-MM-DD format");
            }
            break;
        case FieldType::Choice:
            if (std::find(field.options.begin(), field.options.end(), value.text) ==
                field.options.end()) {
                value.valid = false;
                add_error(result, "VAL007", "field '" + field.name +
                              "' must be one of the declared choices", supplied->line);
            }
            break;
        case FieldType::Email: {
            static const std::regex email_pattern("^[^@[:space:]]+@[^@[:space:]]+\\.[^@[:space:]]+$");
            if (!std::regex_match(value.text, email_pattern)) {
                format_error("must contain a valid email address");
            }
            break;
        }
        case FieldType::Phone: {
            static const std::regex phone_characters("^[+() 0-9-]+$");
            const std::size_t digits = static_cast<std::size_t>(std::count_if(
                value.text.begin(), value.text.end(),
                [](unsigned char character) { return character >= '0' && character <= '9'; }));
            if (!std::regex_match(value.text, phone_characters) || digits < 7 || digits > 15) {
                format_error("must contain 7 to 15 digits and valid phone characters");
            }
            break;
        }
        case FieldType::Text:
        case FieldType::Textarea:
        case FieldType::File:
            break;
    }
    return value;
}

void validate_constraints(const FieldDecl& field,
                          const SuppliedValue& supplied,
                          const RuntimeValue& value,
                          ValidationResult& result) {
    if (!value.present || !value.valid) {
        return;
    }
    const std::string* minimum = property_value(field, PropertyKind::Minimum);
    const std::string* maximum = property_value(field, PropertyKind::Maximum);
    if (field.type == FieldType::Integer || field.type == FieldType::Decimal) {
        long double bound = 0.0L;
        if (minimum != nullptr && parse_number(*minimum, bound) && value.number < bound) {
            add_error(result, "VAL006", "field '" + field.name + "' must be at least " +
                          *minimum, supplied.line);
        }
        if (maximum != nullptr && parse_number(*maximum, bound) && value.number > bound) {
            add_error(result, "VAL006", "field '" + field.name + "' must be at most " +
                          *maximum, supplied.line);
        }
    } else if (field.type == FieldType::Date) {
        if (minimum != nullptr && value.text < *minimum) {
            add_error(result, "VAL006", "field '" + field.name +
                          "' must be on or after " + *minimum, supplied.line);
        }
        if (maximum != nullptr && value.text > *maximum) {
            add_error(result, "VAL006", "field '" + field.name +
                          "' must be on or before " + *maximum, supplied.line);
        }
    }

    const std::string* minimum_length = property_value(field, PropertyKind::MinLength);
    const std::string* maximum_length = property_value(field, PropertyKind::MaxLength);
    if (minimum_length != nullptr && value.text.size() < std::stoull(*minimum_length)) {
        add_error(result, "VAL006", "field '" + field.name + "' must contain at least " +
                      *minimum_length + " characters", supplied.line);
    }
    if (maximum_length != nullptr && value.text.size() > std::stoull(*maximum_length)) {
        add_error(result, "VAL006", "field '" + field.name + "' must contain at most " +
                      *maximum_length + " characters", supplied.line);
    }
}

}  // namespace

ValidationResult validate_data_file(const FormAst& form,
                                    const SemanticResult& semantics,
                                    const std::string& data_path) {
    ValidationResult result;
    std::ifstream input(data_path);
    if (!input) {
        add_error(result, "VAL000", "cannot open data file '" + data_path + "'");
        return result;
    }
    result.data_opened = true;

    SuppliedValues supplied_values;
    std::vector<std::string> supplied_order;
    std::string line;
    std::size_t line_number = 0;
    while (std::getline(input, line)) {
        ++line_number;
        const std::string stripped = trim(line);
        if (stripped.empty() || stripped[0] == '#' || stripped.rfind("//", 0) == 0) {
            continue;
        }
        const std::size_t equals = stripped.find('=');
        if (equals == std::string::npos) {
            add_error(result, "VAL001", "expected field = value", line_number);
            continue;
        }
        const std::string name = trim(stripped.substr(0, equals));
        std::string value;
        if (!valid_identifier(name)) {
            add_error(result, "VAL001", "invalid field name '" + name + "'", line_number);
            continue;
        }
        if (!decode_value(stripped.substr(equals + 1), value)) {
            add_error(result, "VAL001", "invalid quoted value for field '" + name + "'",
                      line_number);
            continue;
        }
        if (!supplied_values.emplace(name, SuppliedValue{value, line_number}).second) {
            add_error(result, "VAL002", "field '" + name + "' is assigned more than once",
                      line_number);
        } else {
            supplied_order.push_back(name);
        }
    }
    result.supplied_fields = supplied_values.size();

    std::unordered_map<std::string, const FieldDecl*> fields;
    for (const Declaration* declaration : form.declarations) {
        if (declaration->kind == DeclarationKind::Field) {
            fields[declaration->field->name] = declaration->field;
        }
    }
    for (const std::string& name : supplied_order) {
        const SuppliedValue& supplied = supplied_values.at(name);
        if (fields.find(name) == fields.end()) {
            add_error(result, "VAL003", "unknown field '" + name + "'", supplied.line);
        }
    }

    RuntimeValues runtime_values;
    for (const Declaration* declaration : form.declarations) {
        if (declaration->kind != DeclarationKind::Field) {
            continue;
        }
        const FieldDecl& field = *declaration->field;
        const auto supplied = supplied_values.find(field.name);
        runtime_values[field.name] = convert_value(
            field,
            supplied == supplied_values.end() ? nullptr : &supplied->second,
            result);
    }

    std::unordered_map<std::string, bool> visibility;
    for (const Declaration* declaration : form.declarations) {
        if (declaration->kind != DeclarationKind::Field) {
            continue;
        }
        const FieldDecl& field = *declaration->field;
        bool visible = true;
        for (const Expression* condition : conditions_for(field, PropertyKind::ShowWhen)) {
            bool condition_value = false;
            visible = visible &&
                      evaluate_boolean(condition, semantics, runtime_values, condition_value) &&
                      condition_value;
        }
        visibility[field.name] = visible;
    }

    for (const Declaration* declaration : form.declarations) {
        if (declaration->kind != DeclarationKind::Field) {
            continue;
        }
        const FieldDecl& field = *declaration->field;
        if (!visibility[field.name]) {
            continue;
        }
        ++result.checked_fields;
        bool required = always_required(field);
        for (const Expression* condition : conditions_for(field, PropertyKind::RequiredWhen)) {
            bool condition_value = false;
            if (evaluate_boolean(condition, semantics, runtime_values, condition_value)) {
                required = required || condition_value;
            }
        }
        const RuntimeValue& value = runtime_values[field.name];
        if (required &&
            (!value.present ||
             (field.type == FieldType::Boolean && value.valid && !value.boolean))) {
            add_error(result, "VAL004", "required field '" + field.name + "' (" +
                          field_label(field) + ") is missing");
            continue;
        }
        const auto supplied = supplied_values.find(field.name);
        if (supplied != supplied_values.end()) {
            validate_constraints(field, supplied->second, value, result);
        }
    }

    for (const Declaration* declaration : form.declarations) {
        if (declaration->kind != DeclarationKind::Check) {
            continue;
        }
        std::unordered_set<std::string> references;
        collect_field_references(
            declaration->check->condition, semantics, references);
        const bool hidden_reference = std::any_of(
            references.begin(), references.end(), [&](const std::string& name) {
                const auto found = visibility.find(name);
                return found != visibility.end() && !found->second;
            });
        if (hidden_reference) {
            ++result.skipped_checks;
            continue;
        }
        bool passed = false;
        if (evaluate_boolean(
                declaration->check->condition, semantics, runtime_values, passed) && passed) {
            ++result.passed_checks;
        } else {
            add_error(result, "VAL009", declaration->check->message);
        }
    }
    return result;
}

std::string format_validation_report(const ValidationResult& result) {
    std::ostringstream output;
    output << "VISTA DATA VALIDATION\n"
           << "---------------------\n"
           << "Supplied fields : " << result.supplied_fields << '\n'
           << "Visible fields  : " << result.checked_fields << '\n'
           << "Rules passed    : " << result.passed_checks << '\n'
           << "Rules skipped   : " << result.skipped_checks << '\n'
           << "Result          : " << (result.succeeded() ? "PASS" : "FAIL") << "\n\n";
    if (result.diagnostics.empty()) {
        output << "No validation errors.\n";
        return output.str();
    }
    output << "Validation errors:\n";
    for (const Diagnostic& diagnostic : result.diagnostics) {
        output << "  " << diagnostic.code;
        if (diagnostic.line != 0) {
            output << " (data line " << diagnostic.line << ')';
        }
        output << ": " << diagnostic.message << '\n';
    }
    return output.str();
}

}  // namespace vista
