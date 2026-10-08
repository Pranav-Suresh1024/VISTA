#include "evaluator.hpp"
#include "file_io.hpp"

#include <algorithm>
#include <fstream>
#include <regex>
#include <set>
#include <sstream>
#include <tuple>

namespace vista {
namespace {
std::string trim(const std::string& text) {
    const std::size_t beginning = text.find_first_not_of(" \t\r\n");
    if (beginning == std::string::npos) {
        return "";
    }
    const std::size_t ending = text.find_last_not_of(" \t\r\n");
    return text.substr(beginning, ending - beginning + 1);
}

bool valid_identifier_part(const std::string& text) {
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

bool is_leap_year(int year) {
    return year % 400 == 0 || (year % 4 == 0 && year % 100 != 0);
}

bool iso_date(const std::string& text) {
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


bool valid_identifier(const std::string& name) {
    std::size_t start = 0;
    do {
        const auto end = name.find('.', start);
        if (!valid_identifier_part(name.substr(start, end == std::string::npos ? end : end - start)))
            return false;
        if (end == std::string::npos) return true;
        start = end + 1;
    } while (true);
}
bool numeric(FieldType type) {
    return type == FieldType::Integer || type == FieldType::Decimal;
}
RuntimeValue expression_value(const IrExpression& expression, const RuntimeValues& values);
}  // namespace

bool valid_iso_date(const std::string& text) { return iso_date(text); }

bool parse_decimal(const std::string& text, CanonicalDecimal& result) {
    if (text.empty() || text.size() > 4096) return false;
    std::size_t i = 0;
    int sign = 1;
    if (text[i] == '-' || text[i] == '+') {
        sign = text[i++] == '-' ? -1 : 1;
    }
    std::string digits;
    int fractional = 0;
    bool dot = false, any = false;
    for (; i < text.size(); ++i) {
        const char c = text[i];
        if (c >= '0' && c <= '9') {
            digits += c;
            any = true;
            if (dot) ++fractional;
        } else if (c == '.' && !dot) dot = true;
        else break;
    }
    if (!any) return false;
    int exponent = 0;
    if (i < text.size() && (text[i] == 'e' || text[i] == 'E')) {
        ++i;
        int exponent_sign = 1;
        if (i < text.size() && (text[i] == '+' || text[i] == '-'))
            exponent_sign = text[i++] == '-' ? -1 : 1;
        const auto beginning = i;
        for (; i < text.size() && text[i] >= '0' && text[i] <= '9'; ++i) {
            const int digit = text[i] - '0';
            if (exponent > (100000 - digit) / 10) return false;
            exponent = exponent * 10 + digit;
        }
        if (i == beginning) return false;
        exponent *= exponent_sign;
    }
    if (i != text.size()) return false;
    int scale = fractional - exponent;
    if (scale < -100000 || scale > 100000) return false;
    const auto first = digits.find_first_not_of('0');
    if (first == std::string::npos) {
        result = {};
        return true;
    }
    digits.erase(0, first);
    while (digits.size() > 1 && digits.back() == '0') {
        digits.pop_back();
        --scale;
    }
    if (scale < -100000 || scale > 100000) return false;
    result = {sign, std::move(digits), scale};
    return true;
}

bool parse_integer(const std::string& text, CanonicalDecimal& result) {
    if (text.empty() || text.size() > 4096) return false;
    const std::size_t start = text.front() == '-' ? 1 : 0;
    if (start == text.size() || !std::all_of(text.begin() + start, text.end(),
        [](char c) { return c >= '0' && c <= '9'; })) return false;
    return parse_decimal(text, result);
}

int compare_decimal(const CanonicalDecimal& a, const CanonicalDecimal& b) {
    if (a.sign != b.sign) return a.sign < b.sign ? -1 : 1;
    if (a.sign == 0) return 0;
    const auto left = static_cast<long long>(a.digits.size()) - a.scale;
    const auto right = static_cast<long long>(b.digits.size()) - b.scale;
    if (left != right) return (left < right ? -1 : 1) * a.sign;
    for (std::size_t i = 0; i < std::max(a.digits.size(), b.digits.size()); ++i) {
        const char x = i < a.digits.size() ? a.digits[i] : '0';
        const char y = i < b.digits.size() ? b.digits[i] : '0';
        if (x != y) return (x < y ? -1 : 1) * a.sign;
    }
    return 0;
}

RuntimeValue convert_value(const LinkedField& field,
                           const std::string* supplied,
                           std::vector<Diagnostic>& diagnostics, std::size_t line) {
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
    value.present = !supplied->empty();
    value.text = *supplied;
    if (!value.present) {
        return value;
    }

    auto format_error = [&](const std::string& expectation) {
        value.valid = false;
        diagnostics.push_back({ "VAL005", "field '" + field.qualified_name + "' " + expectation,
                  line, line ? 1U : 0U});
    };
    switch (field.type) {
        case FieldType::Integer:
            if (!parse_integer(value.text, value.number)) {
                format_error("must be a whole number");
            }
            break;
        case FieldType::Decimal:
            if (!parse_decimal(value.text, value.number)) {
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
                diagnostics.push_back({ "VAL007", "field '" + field.qualified_name +
                              "' must be one of the declared choices", line, line ? 1U : 0U});
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


void validate_constraints(const LinkedField& field, const RuntimeValue& value,
                          std::vector<Diagnostic>& diagnostics, std::size_t line) {
    if (!value.present || !value.valid) return;
    for (const auto& constraint : field.constraints) {
        const bool minimum = constraint.kind == PropertyKind::Minimum;
        const bool maximum = constraint.kind == PropertyKind::Maximum;
        bool failed = false;
        std::string expectation;
        if ((minimum || maximum) && numeric(field.type)) {
            CanonicalDecimal bound;
            if (!parse_decimal(constraint.value, bound)) {
                diagnostics.push_back({"VAL011", "invalid numeric constraint", line, line ? 1U : 0U});
                continue;
            }
            const int order = compare_decimal(value.number, bound);
            failed = minimum ? order < 0 : order > 0;
            expectation = minimum ? "must be at least " : "must be at most ";
        } else if ((minimum || maximum) && field.type == FieldType::Date) {
            failed = minimum ? value.text < constraint.value : value.text > constraint.value;
            expectation = minimum ? "must be on or after " : "must be on or before ";
        } else if (constraint.kind == PropertyKind::MinLength || constraint.kind == PropertyKind::MaxLength) {
            CanonicalDecimal length, bound;
            parse_integer(std::to_string(value.text.size()), length);
            if (!parse_integer(constraint.value, bound)) {
                diagnostics.push_back({"VAL011", "invalid length constraint", line, line ? 1U : 0U});
                continue;
            }
            const bool min_length = constraint.kind == PropertyKind::MinLength;
            const int order = compare_decimal(length, bound);
            failed = min_length ? order < 0 : order > 0;
            expectation = min_length ? "must contain at least " : "must contain at most ";
        }
        if (failed) diagnostics.push_back({"VAL006", "field '" + field.qualified_name + "' " +
            expectation + constraint.value +
            ((constraint.kind == PropertyKind::MinLength || constraint.kind == PropertyKind::MaxLength)
                ? " characters" : ""), line, line ? 1U : 0U});
    }
}

namespace {
RuntimeValue expression_value(const IrExpression& expression, const RuntimeValues& values) {
    RuntimeValue value;
    value.text = expression.value;
    value.present = true;
    if (expression.choice_literal) { value.type = FieldType::Choice; return value; }
    switch (expression.kind) {
        case ExpressionKind::Name: {
            const auto found = values.find(expression.value);
            if (found == values.end()) { value.valid = false; return value; }
            return found->second;
        }
        case ExpressionKind::StringLiteral:
            value.type = expression.type == ValueType::Date ? FieldType::Date : FieldType::Text;
            value.valid = value.type != FieldType::Date || valid_iso_date(value.text);
            return value;
        case ExpressionKind::IntegerLiteral:
        case ExpressionKind::DecimalLiteral:
            value.type = expression.kind == ExpressionKind::IntegerLiteral ? FieldType::Integer : FieldType::Decimal;
            value.valid = parse_decimal(value.text, value.number);
            return value;
        case ExpressionKind::BooleanLiteral:
            value.type = FieldType::Boolean;
            value.valid = value.text == "true" || value.text == "false";
            value.boolean = value.text == "true";
            return value;
        case ExpressionKind::Unary:
        case ExpressionKind::Binary:
            value.type = FieldType::Boolean;
            value.valid = evaluate_expression(expression, values, value.boolean);
            return value;
    }
    value.valid = false;
    return value;
}
}  // namespace

bool evaluate_expression(const IrExpression& expression, const RuntimeValues& values, bool& result) {
    if (expression.kind == ExpressionKind::BooleanLiteral || expression.kind == ExpressionKind::Name) {
        const auto value = expression_value(expression, values);
        if (!value.valid || value.type != FieldType::Boolean) return false;
        result = value.boolean;
        return true;
    }
    if (expression.kind == ExpressionKind::Unary && expression.operands.size() == 1) {
        if (expression.value == "present") {
            const auto value = expression_value(expression.operands[0], values);
            if (!value.valid || value.type != FieldType::File) return false;
            result = value.present;
            return true;
        }
        bool operand = false;
        if (expression.value != "not" || !evaluate_expression(expression.operands[0], values, operand))
            return false;
        result = !operand;
        return true;
    }
    if (expression.kind != ExpressionKind::Binary || expression.operands.size() != 2) return false;
    if (expression.value == "and" || expression.value == "or") {
        bool left = false, right = false;
        // Deliberately evaluate both operands: invalid data never short-circuits to success.
        const bool left_valid = evaluate_expression(expression.operands[0], values, left);
        const bool right_valid = evaluate_expression(expression.operands[1], values, right);
        if (!left_valid || !right_valid) return false;
        result = expression.value == "and" ? left && right : left || right;
        return true;
    }
    const auto left = expression_value(expression.operands[0], values);
    const auto right = expression_value(expression.operands[1], values);
    if (!left.valid || !right.valid) return false;
    int ordering = 0;
    if (numeric(left.type) && numeric(right.type)) ordering = compare_decimal(left.number, right.number);
    else if (left.type == FieldType::Boolean && right.type == FieldType::Boolean)
        ordering = left.boolean == right.boolean ? 0 : (left.boolean ? 1 : -1);
    else ordering = left.text < right.text ? -1 : (left.text > right.text ? 1 : 0);
    if (expression.value == "==") result = ordering == 0;
    else if (expression.value == "!=") result = ordering != 0;
    else if (expression.value == "<") result = ordering < 0;
    else if (expression.value == "<=") result = ordering <= 0;
    else if (expression.value == ">") result = ordering > 0;
    else if (expression.value == ">=") result = ordering >= 0;
    else return false;
    return true;
}

InputLoadResult load_input_values(const std::string& path, bool qualified_names) {
    InputLoadResult result;
    std::string bytes;
    auto error = [&](const std::string& code, const std::string& message, std::size_t line = 0) {
        result.diagnostics.push_back({code, message, line, line ? 1U : 0U});
    };
    if (!read_binary_file(path, bytes)) {
        result.operational_failure = true;
        error("VAL000", "cannot read data file '" + path + "'"); return result;
    }
    result.data_opened = true;
    std::istringstream input(bytes);
    std::string line;
    std::size_t number = 0;
    while (std::getline(input, line)) {
        ++number;
        const auto stripped = trim(line);
        if (stripped.empty() || stripped[0] == '#' || stripped.rfind("//", 0) == 0) continue;
        const auto equals = stripped.find('=');
        if (equals == std::string::npos) { error("VAL001", "expected field = value", number); continue; }
        const auto name = trim(stripped.substr(0, equals));
        std::string value;
        if (!(qualified_names ? valid_identifier(name) : valid_identifier_part(name))) {
            error("VAL001", "invalid field name '" + name + "'", number); continue;
        }
        if (!decode_value(stripped.substr(equals + 1), value)) {
            error("VAL001", "invalid quoted value for field '" + name + "'", number); continue;
        }
        if (!result.values.emplace(name, value).second)
            error("VAL002", "field '" + name + "' is assigned more than once", number);
        else { result.lines[name] = number; result.supplied_order.push_back(name); }
    }
    return result;
}

namespace {
void normalize(EvaluationObservation& observation) {
    std::sort(observation.fields.begin(), observation.fields.end(),
        [](const auto& a, const auto& b) { return a.field < b.field; });
    std::sort(observation.rule_ids.begin(), observation.rule_ids.end());
    observation.rule_ids.erase(std::unique(observation.rule_ids.begin(), observation.rule_ids.end()),
                               observation.rule_ids.end());
    std::sort(observation.diagnostic_details.begin(), observation.diagnostic_details.end(),
        [](const auto& a, const auto& b) {
            return std::tie(a.code, a.field, a.rule_id) < std::tie(b.code, b.field, b.rule_id);
        });
    std::sort(observation.diagnostic_codes.begin(), observation.diagnostic_codes.end());
}
void error(EvaluationResult& result, const std::string& code, const std::string& message,
           const std::string& field = "", const std::string& rule = "") {
    result.diagnostics.push_back({code, message, 0, 0});
    result.observation.diagnostic_codes.push_back(code);
    result.observation.diagnostic_details.push_back({code, field, rule});
}
bool hidden_reference(const IrExpression& expression, const std::map<std::string, bool>& visibility) {
    if (expression.kind == ExpressionKind::Name && !expression.choice_literal) {
        const auto found = visibility.find(expression.value);
        if (found != visibility.end() && !found->second) return true;
    }
    for (const auto& operand : expression.operands)
        if (hidden_reference(operand, visibility)) return true;
    return false;
}
}  // namespace

EvaluationResult evaluate_workflow_step(const LinkedWorkflow& workflow, const InputValues& inputs,
                                        const std::string& active) {
    EvaluationResult result;
    auto& observation = result.observation;
    observation.active_module = active;
    const LinkedModule* module = nullptr;
    RuntimeValues all, exported;
    std::vector<Diagnostic> ignored;
    for (const auto& candidate : workflow.modules) {
        if (candidate.alias == active) module = &candidate;
        for (const auto& field : candidate.fields) {
            const auto supplied = inputs.find(field.qualified_name);
            all[field.qualified_name] = convert_value(field,
                supplied == inputs.end() ? nullptr : &supplied->second, ignored);
        }
        for (const auto& name : candidate.exports) {
            const auto found = all.find(name);
            if (found != all.end()) exported[name] = found->second;
        }
    }
    for (const auto& input : inputs)
        if (!all.count(input.first)) error(result, "VAL003", "unknown field '" + input.first + "'", input.first);
    if (!module) {
        error(result, "LNK005", "invalid active module '" + active + "'");
        normalize(observation);
        return result;
    }
    RuntimeValues scope;
    for (const auto& field : module->fields) scope[field.qualified_name] = all.at(field.qualified_name);
    for (const auto& import : module->imports) {
        const auto found = exported.find(import.qualified_name);
        if (found != exported.end()) scope[import.qualified_name] = found->second;
    }
    observation.rule_ids.push_back(module->entry_rule_id);
    bool entered = false;
    if (!evaluate_expression(module->entry, scope, entered) || !entered) {
        error(result, "VAL010", "module '" + active + "' entry condition is false or invalid",
              "", module->entry_rule_id);
        normalize(observation);
        return result;
    }
    std::map<std::string, bool> visibility;
    for (const auto& field : module->fields) {
        bool visible = false, required = false;
        observation.rule_ids.push_back(field.visibility_rule_id);
        if (!evaluate_expression(field.visibility, scope, visible))
            error(result, "VAL011", "invalid visibility condition for '" + field.qualified_name + "'",
                  field.qualified_name, field.visibility_rule_id);
        visibility[field.qualified_name] = visible;
        observation.rule_ids.push_back(field.requiredness_rule_id);
        if (!evaluate_expression(field.requiredness, scope, required))
            error(result, "VAL011", "invalid requiredness condition for '" + field.qualified_name + "'",
                  field.qualified_name, field.requiredness_rule_id);
        const auto supplied = inputs.find(field.qualified_name);
        std::vector<Diagnostic> conversion;
        observation.rule_ids.push_back(field.qualified_name + ".value");
        const auto value = convert_value(field, supplied == inputs.end() ? nullptr : &supplied->second, conversion);
        for (const auto& diagnostic : conversion)
            error(result, diagnostic.code, diagnostic.message, field.qualified_name, field.qualified_name + ".value");
        bool valid = value.valid;
        if (visible && required && (!value.present ||
                (field.type == FieldType::Boolean && value.valid && !value.boolean))) {
            valid = false;
            error(result, "VAL004", "required field '" + field.qualified_name + "' (" +
                (field.label.empty() ? field.qualified_name : field.label) + ") is missing",
                field.qualified_name, field.requiredness_rule_id);
        } else if (visible && value.present && value.valid) {
            for (const auto& constraint : field.constraints) {
                observation.rule_ids.push_back(constraint.rule_id);
                auto scalar = field;
                scalar.constraints = {constraint};
                std::vector<Diagnostic> diagnostics;
                validate_constraints(scalar, value, diagnostics);
                for (const auto& diagnostic : diagnostics) {
                    valid = false;
                    error(result, diagnostic.code, diagnostic.message, field.qualified_name, constraint.rule_id);
                }
            }
        }
        observation.fields.push_back({field.qualified_name, visible, visible && required, valid});
    }
    for (const auto& check : module->checks) {
        if (hidden_reference(check.condition, visibility)) continue;
        observation.rule_ids.push_back(check.rule_id);
        bool passed = false;
        if (!evaluate_expression(check.condition, scope, passed) || !passed)
            error(result, "VAL009", check.message, "", check.rule_id);
    }
    // All outgoing predicates run, even if this module cannot advance.
    std::vector<const LinkedRoute*> enabled;
    for (const auto& route : workflow.routes) {
        if (route.source != active) continue;
        observation.rule_ids.push_back(route.rule_id);
        bool passed = false;
        if (!evaluate_expression(route.condition, exported, passed))
            error(result, "VAL011", "invalid route condition", "", route.rule_id);
        else if (passed) enabled.push_back(&route);
    }
    if (enabled.size() > 1)
        error(result, "LNK008", "ambiguous enabled routes from '" + active + "'");
    else if (result.diagnostics.empty()) {
        if (enabled.empty()) observation.complete = true;
        else {
            observation.selected_route = enabled[0]->source + "->" + enabled[0]->destination;
            observation.transition_trace.push_back(observation.selected_route);
        }
    }
    normalize(observation);
    return result;
}

EvaluationResult evaluate_workflow(const LinkedWorkflow& workflow, const InputValues& inputs) {
    std::string active = workflow.start_alias, selected;
    std::set<std::string> visited;
    std::vector<std::string> trace, rules;
    for (;;) {
        auto result = evaluate_workflow_step(workflow, inputs, active);
        auto& observation = result.observation;
        rules.insert(rules.end(), observation.rule_ids.begin(), observation.rule_ids.end());
        if (!visited.insert(active).second) {
            observation.complete = false;
            observation.selected_route.clear();
            observation.transition_trace.clear();
            error(result, "LNK006", "nonterminating conditional route cycle at '" + active + "'");
        }
        if (!observation.selected_route.empty()) {
            selected = observation.selected_route;
            trace.push_back(selected);
        }
        const std::string next = observation.selected_route;
        observation.selected_route = selected;
        observation.transition_trace = trace;
        observation.rule_ids = rules;
        normalize(observation);
        if (!result.succeeded() || observation.complete || next.empty()) return result;
        active = next.substr(next.find("->") + 2);
    }
}

std::string format_observation(const EvaluationObservation& observation) {
    std::ostringstream output;
    output << "VISTA WORKFLOW EVALUATION\n"
           << "Active module   : " << observation.active_module << "\nVisible fields  :";
    for (const auto& field : observation.fields) if (field.visible) output << ' ' << field.field;
    output << "\nRequired fields :";
    for (const auto& field : observation.fields) if (field.required) output << ' ' << field.field;
    output << "\nSelected route  : " << (observation.selected_route.empty() ? "none" : observation.selected_route)
           << "\nComplete        : " << (observation.complete ? "true" : "false")
           << "\nResult          : " << (observation.diagnostic_codes.empty() ? "PASS" : "FAIL")
           << "\nRule IDs        :";
    for (const auto& rule : observation.rule_ids) output << ' ' << rule;
    output << "\nTransition trace:";
    for (const auto& transition : observation.transition_trace) output << ' ' << transition;
    output << "\nField states:\n";
    for (const auto& field : observation.fields)
        output << "  " << field.field << " visible=" << field.visible << " required=" << field.required
               << " valid=" << field.valid << '\n';
    for (const auto& detail : observation.diagnostic_details)
        output << "  " << detail.code << " field=" << detail.field << " rule=" << detail.rule_id << '\n';
    return output.str();
}

std::string canonical_observation(const EvaluationObservation& source) {
    auto observation = source;
    normalize(observation);
    std::ostringstream output;
    // UTF-8 byte-counted strings: delimiters/control characters cannot lose information.
    auto text = [&](const std::string& value) { output << value.size() << ':' << value; };
    output << "VISTA-OBSERVATION 1\n";
    text(observation.active_module);
    output << '\n' << observation.fields.size() << '\n';
    for (const auto& field : observation.fields) {
        text(field.field);
        output << ' ' << field.visible << ' ' << field.required << ' ' << field.valid << '\n';
    }
    text(observation.selected_route);
    output << '\n' << observation.complete << '\n' << observation.diagnostic_codes.size() << '\n';
    for (const auto& code : observation.diagnostic_codes) { text(code); output << '\n'; }
    output << observation.diagnostic_details.size() << '\n';
    for (const auto& detail : observation.diagnostic_details) {
        text(detail.code); text(detail.field); text(detail.rule_id); output << '\n';
    }
    output << observation.rule_ids.size() << '\n';
    for (const auto& rule : observation.rule_ids) { text(rule); output << '\n'; }
    output << observation.transition_trace.size() << '\n';
    for (const auto& transition : observation.transition_trace) { text(transition); output << '\n'; }
    return output.str();
}

}  // namespace vista
