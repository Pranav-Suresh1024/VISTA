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
    }
    return "UnknownProperty";
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

    for (const Declaration* declaration : form.declarations) {
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

    return output.str();
}

}  // namespace vista
