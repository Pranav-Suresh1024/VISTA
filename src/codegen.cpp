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

const std::string* presentation_value(const FieldDecl& field, PropertyKind kind) {
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

void generate_field(std::ostringstream& output,
                    const FieldDecl& field,
                    bool inside_section) {
    const std::string indent = inside_section ? "        " : "      ";
    const std::string* help = presentation_value(field, PropertyKind::Help);
    const std::string* placeholder =
        presentation_value(field, PropertyKind::Placeholder);
    const bool has_help = help != nullptr && !help->empty();

    output << indent << "<div class=\"field\" data-field="
           << javascript_string(field.name) << ">\n";

    std::string label = field.name;
    for (const FieldProperty* property : field.properties) {
        if (property->kind == PropertyKind::Label) {
            label = property->text;
            break;
        }
    }

    output << indent << "  <label for=\"" << field.name << "\">"
           << html_escape(label) << "</label>\n";
    output << indent << "  <span class=\"required-indicator\" data-required-for=\""
           << field.name << "\" aria-hidden=\"true\""
           << (is_always_required(field) ? "" : " hidden") << ">*</span>\n";
    if (field.type == FieldType::Choice) {
        output << indent << "  <select id=\"" << field.name << "\" name=\""
               << field.name << "\" data-type=\"choice\""
               << (has_help ? " aria-describedby=\"help-" + field.name + "\"" : "")
               << (is_always_required(field) ? " required" : "") << ">\n"
               << indent << "    <option value=\"\">"
               << html_escape(placeholder == nullptr || placeholder->empty()
                                  ? "Select an option"
                                  : *placeholder)
               << "</option>\n";
        for (const std::string& option : field.options) {
            output << indent << "    <option value=\"" << html_escape(option) << "\">"
                   << html_escape(option) << "</option>\n";
        }
        output << indent << "  </select>\n";
    } else {
        output << indent << "  <input id=\"" << field.name << "\" name=\""
               << field.name << "\" type=\"" << input_type(field.type)
               << "\" data-type=\"" << field_type_name(field.type) << "\""
               << (has_help ? " aria-describedby=\"help-" + field.name + "\"" : "");
        if (placeholder != nullptr && !placeholder->empty()) {
            output << " placeholder=\"" << html_escape(*placeholder) << "\"";
        }
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
    if (has_help) {
        output << indent << "  <small class=\"field-help\" id=\"help-" << field.name
               << "\">" << html_escape(*help) << "</small>\n";
    }
    output << indent << "</div>\n";
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
           << "  <title>" << html_escape(form.title) << "</title>\n"
           << "  <style>\n"
           << "    :root { color-scheme: light; font-family: Inter, ui-sans-serif, system-ui, sans-serif; font-size: 16px; background: #f3f6fb; color: #182235; }\n"
           << "    * { box-sizing: border-box; }\n"
           << "    body { min-width: 320px; min-height: 100vh; margin: 0; padding: 2.5rem 1.25rem; background: radial-gradient(circle at top left, #e8efff, transparent 34rem), #f3f6fb; }\n"
           << "    main { width: min(100%, 760px); margin: 0 auto; padding: clamp(1.25rem, 4vw, 2.75rem); background: #fff; border: 1px solid #e1e7f0; border-radius: 20px; box-shadow: 0 18px 55px #1d355714; }\n"
           << "    h1 { margin: 0; color: #17233b; font-size: clamp(1.7rem, 5vw, 2.25rem); line-height: 1.15; letter-spacing: -.035em; }\n"
           << "    .form-description { margin: .8rem 0 1.6rem; color: #58677f; line-height: 1.6; }\n"
           << "    .required-note { margin: 1.5rem 0; color: #58677f; font-size: .9rem; }\n"
           << "    .required-note span, .required-indicator { color: #b42318; font-weight: 750; }\n"
           << "    .field { display: grid; grid-template-columns: auto 1fr; align-items: center; column-gap: .25rem; row-gap: .55rem; margin: 1.25rem 0; }\n"
           << "    .field[hidden] { display: none; }\n"
           << "    .field label { color: #28364d; font-weight: 650; }\n"
           << "    .required-indicator[hidden] { display: none; }\n"
           << "    .field input, .field select { grid-column: 1 / -1; width: 100%; min-height: 2.9rem; padding: .72rem .85rem; color: #182235; background: #fff; border: 1px solid #bac5d5; border-radius: 9px; font: inherit; transition: border-color .15s, box-shadow .15s; }\n"
           << "    .field input::placeholder { color: #7b8799; }\n"
           << "    .field input:focus-visible, .field select:focus-visible, button:focus-visible { outline: 3px solid #2457d64d; outline-offset: 2px; border-color: #2457d6; }\n"
           << "    .field input[type=checkbox] { grid-column: 1; width: 1.25rem; min-height: 1.25rem; height: 1.25rem; margin: .25rem 0; accent-color: #2457d6; }\n"
           << "    .field-help { grid-column: 1 / -1; color: #58677f; line-height: 1.45; }\n"
           << "    .form-section { margin: 1.75rem 0 2rem; padding: 1.25rem 1.35rem; background: #fbfcff; border: 1px solid #e5eaf2; border-radius: 14px; }\n"
           << "    .form-section h2 { margin: 0 0 1rem; color: #344563; font-size: 1.05rem; letter-spacing: -.01em; }\n"
           << "    .form-section .field:first-of-type { margin-top: 0; }\n"
           << "    .form-section .field:last-child { margin-bottom: 0; }\n"
           << "    button { min-height: 2.9rem; margin-top: .5rem; padding: .75rem 1.1rem; border: 0; border-radius: 9px; background: #2457d6; color: #fff; font: inherit; font-weight: 700; cursor: pointer; }\n"
           << "    button:hover { background: #1948bd; }\n"
           << "    #validation-errors { margin: 1rem 0; padding: .85rem 1rem .85rem 2.25rem; border-radius: 9px; background: #fff1f0; color: #a11919; }\n"
           << "    #validation-errors:empty { display: none; }\n"
           << "    #status { min-height: 1.5em; color: #176b3a; font-weight: 650; }\n"
           << "    #status:empty { display: none; }\n"
           << "    @media (max-width: 520px) { body { padding: .75rem; } main { padding: 1.25rem; border-radius: 14px; } .form-section { margin-inline: -.25rem; padding: 1rem; } button { width: 100%; } }\n"
           << "  </style>\n"
           << "</head>\n"
           << "<body>\n"
           << "  <main>\n"
           << "    <h1>" << html_escape(form.title) << "</h1>\n";
    if (!form.description.empty()) {
        output << "    <p class=\"form-description\" id=\"form-description\">"
               << html_escape(form.description) << "</p>\n";
    }
    output << "    <form id=\"vista-form\""
           << (form.description.empty() ? "" : " aria-describedby=\"form-description\"")
           << ">\n"
           << "      <p class=\"required-note\"><span aria-hidden=\"true\">*</span> Indicates a required field.</p>\n";

    std::string open_section;
    bool section_open = false;
    for (const Declaration* declaration : form.declarations) {
        if (declaration->kind == DeclarationKind::Field) {
            const FieldDecl& field = *declaration->field;
            const std::string* section = presentation_value(field, PropertyKind::Section);
            const std::string next_section =
                section == nullptr ? std::string() : *section;
            if (next_section != open_section) {
                if (section_open) {
                    output << "      </section>\n";
                    section_open = false;
                }
                open_section = next_section;
                if (!open_section.empty()) {
                    output << "      <section class=\"form-section\" aria-label=\""
                           << html_escape(open_section) << "\">\n"
                           << "        <h2>" << html_escape(open_section) << "</h2>\n";
                    section_open = true;
                }
            }
            generate_field(output, field, section_open);
        }
    }
    if (section_open) {
        output << "      </section>\n";
    }

    output << "      <ul id=\"validation-errors\" role=\"alert\" aria-live=\"assertive\" aria-atomic=\"true\" aria-label=\"Form validation errors\"></ul>\n"
           << "      <p id=\"status\" role=\"status\" aria-live=\"polite\"></p>\n"
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
           << "        control.setAttribute('aria-required', String(control.required));\n"
           << "        const requiredIndicator = container.querySelector('.required-indicator');\n"
           << "        requiredIndicator.hidden = !control.required;\n"
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
