#include "codegen.hpp"

#include <sstream>
#include <string>
#include <vector>

namespace vista {
namespace {

std::string html_escape(const std::string& text) {
    std::string escaped;
    for (const char character : text) {
        switch (character) {
            case '&': escaped += "&amp;"; break;
            case '<': escaped += "&lt;"; break;
            case '>': escaped += "&gt;"; break;
            case '"': escaped += "&quot;"; break;
            case '\'': escaped += "&#39;"; break;
            default: escaped += character; break;
        }
    }
    return escaped;
}

std::string javascript_string(const std::string& text) {
    std::ostringstream output;
    output << '"';
    for (const unsigned char character : text) {
        switch (character) {
            case '\\': output << "\\\\"; break;
            case '"': output << "\\\""; break;
            case '\n': output << "\\n"; break;
            case '\r': output << "\\r"; break;
            case '\t': output << "\\t"; break;
            case '<': output << "\\u003C"; break;
            case '>': output << "\\u003E"; break;
            case '&': output << "\\u0026"; break;
            default: output << static_cast<char>(character); break;
        }
    }
    output << '"';
    return output.str();
}

std::string input_type(FieldType type) {
    switch (type) {
        case FieldType::Text: return "text";
        case FieldType::Integer:
        case FieldType::Decimal: return "number";
        case FieldType::Boolean: return "checkbox";
        case FieldType::Date: return "date";
        case FieldType::File: return "file";
        case FieldType::Choice: return "";
    }
    return "text";
}

bool is_always_required(const FieldDecl& field) {
    for (const FieldProperty* property : field.properties) {
        if (property->kind == PropertyKind::Required) {
            return true;
        }
    }
    return false;
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

std::string expression_javascript(const Expression* expression,
                                  const SemanticResult& semantics) {
    switch (expression->kind) {
        case ExpressionKind::Name: {
            const auto information = semantics.expressions.find(expression);
            if (information != semantics.expressions.end() &&
                information->second.choice_literal) {
                return javascript_string(expression->value);
            }
            return "readValue(" + javascript_string(expression->value) + ")";
        }
        case ExpressionKind::StringLiteral:
            return javascript_string(expression->value);
        case ExpressionKind::IntegerLiteral:
        case ExpressionKind::DecimalLiteral:
        case ExpressionKind::BooleanLiteral:
            return expression->value;
        case ExpressionKind::Unary:
            return "(!" + expression_javascript(expression->left, semantics) + ")";
        case ExpressionKind::Binary:
            break;
    }

    std::string operation = expression->value;
    if (operation == "and") {
        operation = "&&";
    } else if (operation == "or") {
        operation = "||";
    } else if (operation == "==") {
        operation = "===";
    } else if (operation == "!=") {
        operation = "!==";
    }
    return "(" + expression_javascript(expression->left, semantics) + " " + operation +
           " " + expression_javascript(expression->right, semantics) + ")";
}

std::string combine_conditions(const std::vector<const Expression*>& conditions,
                               const SemanticResult& semantics,
                               const std::string& operation,
                               const std::string& empty_value) {
    if (conditions.empty()) {
        return empty_value;
    }
    std::ostringstream output;
    for (std::size_t index = 0; index < conditions.size(); ++index) {
        if (index != 0) {
            output << ' ' << operation << ' ';
        }
        output << '(' << expression_javascript(conditions[index], semantics) << ')';
    }
    return output.str();
}

void generate_field(std::ostringstream& output, const FieldDecl& field) {
    output << "      <div class=\"field\" data-field="
           << javascript_string(field.name) << ">\n";

    std::string label = field.name;
    for (const FieldProperty* property : field.properties) {
        if (property->kind == PropertyKind::Label) {
            label = property->text;
            break;
        }
    }

    output << "        <label for=\"" << field.name << "\">"
           << html_escape(label) << "</label>\n";
    if (field.type == FieldType::Choice) {
        output << "        <select id=\"" << field.name << "\" name=\""
               << field.name << "\" data-type=\"choice\""
               << (is_always_required(field) ? " required" : "") << ">\n"
               << "          <option value=\"\">Select an option</option>\n";
        for (const std::string& option : field.options) {
            output << "          <option value=\"" << html_escape(option) << "\">"
                   << html_escape(option) << "</option>\n";
        }
        output << "        </select>\n";
    } else {
        output << "        <input id=\"" << field.name << "\" name=\""
               << field.name << "\" type=\"" << input_type(field.type)
               << "\" data-type=\"" << field_type_name(field.type) << "\"";
        if (field.type == FieldType::Integer) {
            output << " step=\"1\"";
        } else if (field.type == FieldType::Decimal) {
            output << " step=\"any\"";
        }
        if (is_always_required(field)) {
            output << " required";
        }
        output << ">\n";
    }
    output << "      </div>\n";
}

}  // namespace

std::string generate_html(const FormAst& form,
                          const SemanticResult& semantics,
                          const AnalysisResult& analysis) {
    (void)analysis;
    std::ostringstream output;
    output << "<!doctype html>\n"
           << "<html lang=\"en\">\n"
           << "<head>\n"
           << "  <meta charset=\"utf-8\">\n"
           << "  <meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n"
           << "  <title>" << html_escape(form.name) << "</title>\n"
           << "  <style>\n"
           << "    :root { color-scheme: light; font-family: system-ui, sans-serif; }\n"
           << "    body { margin: 0; background: #f3f6fb; color: #182235; }\n"
           << "    main { max-width: 680px; margin: 3rem auto; padding: 2rem; background: white; border-radius: 14px; box-shadow: 0 8px 30px #1d35571a; }\n"
           << "    h1 { margin-top: 0; }\n"
           << "    .field { display: grid; gap: .4rem; margin: 1.1rem 0; }\n"
           << "    .field[hidden] { display: none; }\n"
           << "    label { font-weight: 650; }\n"
           << "    input, select, button { font: inherit; padding: .7rem; border: 1px solid #9aa8bd; border-radius: 7px; }\n"
           << "    input[type=checkbox] { width: 1.25rem; height: 1.25rem; }\n"
           << "    button { margin-top: 1rem; border: 0; background: #2457d6; color: white; cursor: pointer; }\n"
           << "    #validation-errors { color: #a11919; padding-left: 1.25rem; }\n"
           << "    #status { font-weight: 650; color: #176b3a; }\n"
           << "  </style>\n"
           << "</head>\n"
           << "<body>\n"
           << "  <main>\n"
           << "    <h1>" << html_escape(form.name) << "</h1>\n"
           << "    <form id=\"vista-form\">\n";

    for (const Declaration* declaration : form.declarations) {
        if (declaration->kind == DeclarationKind::Field) {
            generate_field(output, *declaration->field);
        }
    }

    output << "      <ul id=\"validation-errors\" aria-live=\"polite\"></ul>\n"
           << "      <p id=\"status\" aria-live=\"polite\"></p>\n"
           << "      <button type=\"submit\">Validate form</button>\n"
           << "    </form>\n"
           << "  </main>\n"
           << "  <script>\n"
           << "    'use strict';\n"
           << "    const form = document.getElementById('vista-form');\n"
           << "    function readValue(name) {\n"
           << "      const control = document.getElementById(name);\n"
           << "      if (control.dataset.type === 'boolean') return control.checked;\n"
           << "      if (control.dataset.type === 'integer' || control.dataset.type === 'decimal') {\n"
           << "        return control.value === '' ? Number.NaN : Number(control.value);\n"
           << "      }\n"
           << "      return control.value;\n"
           << "    }\n"
           << "    const fieldRules = [\n";

    bool first_rule = true;
    for (const Declaration* declaration : form.declarations) {
        if (declaration->kind != DeclarationKind::Field) {
            continue;
        }
        const FieldDecl& field = *declaration->field;
        const std::vector<const Expression*> show_conditions =
            conditions_for(field, PropertyKind::ShowWhen);
        const std::vector<const Expression*> required_conditions =
            conditions_for(field, PropertyKind::RequiredWhen);
        if (!first_rule) {
            output << ",\n";
        }
        first_rule = false;
        output << "      { name: " << javascript_string(field.name)
               << ", visible: () => "
               << combine_conditions(show_conditions, semantics, "&&", "true")
               << ", required: () => ";
        if (is_always_required(field)) {
            output << "true";
        } else {
            output << combine_conditions(required_conditions, semantics, "||", "false");
        }
        output << " }";
    }

    output << "\n    ];\n"
           << "    const checks = [\n";
    bool first_check = true;
    for (const Declaration* declaration : form.declarations) {
        if (declaration->kind != DeclarationKind::Check) {
            continue;
        }
        if (!first_check) {
            output << ",\n";
        }
        first_check = false;
        output << "      { valid: () => "
               << expression_javascript(declaration->check->condition, semantics)
               << ", message: " << javascript_string(declaration->check->message) << " }";
    }
    output << "\n    ];\n"
           << "    function updateForm() {\n"
           << "      for (const rule of fieldRules) {\n"
           << "        const control = document.getElementById(rule.name);\n"
           << "        const container = control.closest('.field');\n"
           << "        const visible = Boolean(rule.visible());\n"
           << "        container.hidden = !visible;\n"
           << "        container.setAttribute('aria-hidden', String(!visible));\n"
           << "        control.disabled = !visible;\n"
           << "        control.required = visible && Boolean(rule.required());\n"
           << "      }\n"
           << "    }\n"
           << "    form.addEventListener('input', updateForm);\n"
           << "    form.addEventListener('change', updateForm);\n"
           << "    form.addEventListener('submit', (event) => {\n"
           << "      event.preventDefault();\n"
           << "      updateForm();\n"
           << "      const errors = checks.filter((check) => !check.valid()).map((check) => check.message);\n"
           << "      const list = document.getElementById('validation-errors');\n"
           << "      const status = document.getElementById('status');\n"
           << "      list.replaceChildren();\n"
           << "      status.textContent = '';\n"
           << "      for (const message of errors) {\n"
           << "        const item = document.createElement('li');\n"
           << "        item.textContent = message;\n"
           << "        list.appendChild(item);\n"
           << "      }\n"
           << "      if (!form.checkValidity()) { form.reportValidity(); return; }\n"
           << "      if (errors.length === 0) status.textContent = 'Form validation passed.';\n"
           << "    });\n"
           << "    updateForm();\n"
           << "  </script>\n"
           << "</body>\n"
           << "</html>\n";
    return output.str();
}

}  // namespace vista
