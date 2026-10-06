#include "validation.hpp"
#include "evaluator.hpp"

#include <algorithm>
#include <map>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

namespace vista {
namespace {
struct SuppliedValue { std::string text; std::size_t line = 1; };
void add_error(ValidationResult& result, std::string code, std::string message, std::size_t line = 0) {
    result.diagnostics.push_back({std::move(code), std::move(message), line, line ? 1U : 0U});
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
LinkedField scalar_field(const FieldDecl& field) {
    LinkedField linked;
    linked.qualified_name = field.name;
    linked.type = field.type;
    linked.options = field.options;
    for (const auto* property : field.properties)
        if (property->kind == PropertyKind::Minimum || property->kind == PropertyKind::Maximum ||
            property->kind == PropertyKind::MinLength || property->kind == PropertyKind::MaxLength)
            linked.constraints.push_back({property->kind, property->text, ""});
    // Preserve legacy minimum, maximum, minimum-length, maximum-length error order.
    std::sort(linked.constraints.begin(), linked.constraints.end(), [](const auto& a, const auto& b) {
        return a.kind < b.kind;
    });
    return linked;
}

IrExpression scalar_expression(const Expression* expression, const SemanticResult& semantics) {
    const auto info = semantics.expressions.find(expression);
    IrExpression result{expression->kind, info == semantics.expressions.end() ? ValueType::Error : info->second.type,
                        expression->value, {}, info != semantics.expressions.end() && info->second.choice_literal};
    if (expression->left) result.operands.push_back(scalar_expression(expression->left, semantics));
    if (expression->right) result.operands.push_back(scalar_expression(expression->right, semantics));
    return result;
}

bool evaluate_boolean(const Expression* expression, const SemanticResult& semantics,
                      const RuntimeValues& values, bool& result) {
    return evaluate_expression(scalar_expression(expression, semantics), values, result);
}
RuntimeValue convert_value(const FieldDecl& field, const SuppliedValue* supplied, ValidationResult& result) {
    return convert_value(scalar_field(field), supplied ? &supplied->text : nullptr,
                         result.diagnostics, supplied ? supplied->line : 0);
}
void validate_constraints(const FieldDecl& field, const SuppliedValue& supplied,
                          const RuntimeValue& value, ValidationResult& result) {
    validate_constraints(scalar_field(field), value, result.diagnostics, supplied.line);
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
}  // namespace

ValidationResult validate_data_file(const FormAst& form,
                                    const SemanticResult& semantics,
                                    const std::string& data_path) {
    ValidationResult result;
    const auto input = load_input_values(data_path, false);
    result.data_opened = input.data_opened;
    result.diagnostics = input.diagnostics;
    if (!input.data_opened) return result;
    std::map<std::string, SuppliedValue> supplied_values;
    for (const auto& supplied : input.values)
        supplied_values[supplied.first] = {supplied.second, input.lines.at(supplied.first)};
    const auto& supplied_order = input.supplied_order;
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
