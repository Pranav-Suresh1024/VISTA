#include "ast.hpp"

#include <sstream>
#include <string_view>

#include "token.hpp"

namespace vista {
namespace {

std::string location_text(const SourceSpan& span) {
    return "@" + std::to_string(span.first_line) + ':' +
           std::to_string(span.first_column);
}

void indent(std::ostringstream& output, int depth) {
    output << std::string(static_cast<std::size_t>(depth * 2), ' ');
}

void print_expression(std::ostringstream& output,
                      const Expression* expression,
                      int depth) {
    if (expression == nullptr) {
        indent(output, depth);
        output << "<missing expression>\n";
        return;
    }

    indent(output, depth);
    switch (expression->kind) {
        case ExpressionKind::Name:
            output << "Name " << expression->value;
            break;
        case ExpressionKind::StringLiteral:
            output << "String " << display_lexeme(expression->value);
            break;
        case ExpressionKind::IntegerLiteral:
            output << "Integer " << expression->value;
            break;
        case ExpressionKind::DecimalLiteral:
            output << "Decimal " << expression->value;
            break;
        case ExpressionKind::BooleanLiteral:
            output << "Boolean " << expression->value;
            break;
        case ExpressionKind::Unary:
            output << "Unary " << expression->value;
            break;
        case ExpressionKind::Binary:
            output << "Binary " << expression->value;
            break;
    }
    output << ' ' << location_text(expression->span) << '\n';

    if (expression->left != nullptr) {
        print_expression(output, expression->left, depth + 1);
    }
    if (expression->right != nullptr) {
        print_expression(output, expression->right, depth + 1);
    }
}

std::string property_name(PropertyKind kind) {
    switch (kind) {
        case PropertyKind::Label: return "Label";
        case PropertyKind::Section: return "Section";
        case PropertyKind::Help: return "Help";
        case PropertyKind::Placeholder: return "Placeholder";
        case PropertyKind::Required: return "Required";
        case PropertyKind::RequiredWhen: return "RequiredWhen";
        case PropertyKind::ShowWhen: return "ShowWhen";
        case PropertyKind::Minimum: return "Minimum";
        case PropertyKind::Maximum: return "Maximum";
        case PropertyKind::MinLength: return "MinLength";
        case PropertyKind::MaxLength: return "MaxLength";
    }
    return "UnknownProperty";
}

void print_declarations(std::ostringstream& output,
                        const std::vector<Declaration*>& declarations) {
    for (const Declaration* declaration : declarations) {
        if (declaration->kind == DeclarationKind::Field) {
            const FieldDecl& field = *declaration->field;
            indent(output, 1);
            output << "Field " << field.name << " : " << field_type_name(field.type);
            if (!field.options.empty()) {
                output << " [";
                for (std::size_t index = 0; index < field.options.size(); ++index) {
                    if (index != 0) {
                        output << ", ";
                    }
                    output << field.options[index];
                }
                output << ']';
            }
            output << ' ' << location_text(field.span) << '\n';

            for (const FieldProperty* property : field.properties) {
                indent(output, 2);
                output << property_name(property->kind);
                if (property->kind == PropertyKind::Label ||
                    property->kind == PropertyKind::Section ||
                    property->kind == PropertyKind::Help ||
                    property->kind == PropertyKind::Placeholder) {
                    output << ' ' << display_lexeme(property->text);
                } else if (property->kind == PropertyKind::Minimum ||
                           property->kind == PropertyKind::Maximum ||
                           property->kind == PropertyKind::MinLength ||
                           property->kind == PropertyKind::MaxLength) {
                    output << ' ' << property->text;
                }
                output << ' ' << location_text(property->span) << '\n';
                if (property->condition != nullptr) {
                    print_expression(output, property->condition, 3);
                }
            }
            continue;
        }

        const CheckDecl& check = *declaration->check;
        indent(output, 1);
        output << "Check " << location_text(check.span) << '\n';
        print_expression(output, check.condition, 2);
        indent(output, 2);
        output << "Message " << display_lexeme(check.message) << '\n';
    }
}

}  // namespace

std::string field_type_name(FieldType type) {
    switch (type) {
        case FieldType::Text: return "text";
        case FieldType::Integer: return "integer";
        case FieldType::Decimal: return "decimal";
        case FieldType::Boolean: return "boolean";
        case FieldType::Date: return "date";
        case FieldType::Choice: return "choice";
        case FieldType::File: return "file";
        case FieldType::Email: return "email";
        case FieldType::Phone: return "phone";
        case FieldType::Textarea: return "textarea";
    }
    return "unknown";
}

std::string format_ast(const FormAst& form) {
    std::ostringstream output;
    output << "Form " << form.name << ' ' << location_text(form.span) << '\n';
    if (form.title != form.name) {
        output << "  Title " << display_lexeme(form.title) << '\n';
    }
    if (!form.description.empty()) {
        output << "  Description " << display_lexeme(form.description) << '\n';
    }

    print_declarations(output, form.declarations);

    return output.str();
}

std::string format_ast(const ModuleAst& module) {
    std::ostringstream output;
    output << "Module " << module.name << ' ' << location_text(module.span) << '\n';
    for (const ImportDecl* import : module.imports) {
        indent(output, 1);
        output << "Import " << import->provider << '.' << import->field
               << " as " << import->alias << ' ' << location_text(import->span) << '\n';
    }
    for (const ExportDecl* export_decl : module.exports) {
        indent(output, 1);
        output << "Export " << export_decl->field << ' '
               << location_text(export_decl->span) << '\n';
    }
    if (module.enter_condition != nullptr) {
        indent(output, 1);
        output << "EnterWhen\n";
        print_expression(output, module.enter_condition, 2);
    }
    print_declarations(output, module.declarations);
    return output.str();
}

std::string format_ast(const WorkflowAst& workflow) {
    std::ostringstream output;
    output << "Workflow " << workflow.name << ' ' << location_text(workflow.span) << '\n';
    for (const WorkflowUse* use : workflow.uses) {
        indent(output, 1);
        output << "Use " << use->alias << ' ' << display_lexeme(use->source_path)
               << ' ' << location_text(use->span) << '\n';
    }
    if (!workflow.start_module.empty()) {
        indent(output, 1);
        output << "Start " << workflow.start_module << '\n';
    }
    for (const RouteDecl* route : workflow.routes) {
        indent(output, 1);
        output << "Route " << route->source << " -> " << route->destination
               << ' ' << location_text(route->span) << '\n';
        if (route->condition != nullptr) {
            print_expression(output, route->condition, 2);
        }
    }
    return output.str();
}

}  // namespace vista
