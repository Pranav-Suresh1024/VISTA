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
        case FieldType::Email: return "email";
        case FieldType::Phone: return "tel";
        case FieldType::Textarea: return "text";
        case FieldType::Integer:
        case FieldType::Decimal: return "number";
        case FieldType::Boolean: return "checkbox";
        case FieldType::Date: return "date";
        case FieldType::File: return "file";
        case FieldType::Choice: return "";
    }
    return "text";
}

const std::string* constraint_value(const FieldDecl& field, PropertyKind kind) {
    for (const FieldProperty* property : field.properties) {
        if (property->kind == kind) {
            return &property->text;
        }
    }
    return nullptr;
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

void collect_field_references(const Expression* expression,
                              const SemanticResult& semantics,
                              std::vector<std::string>& references) {
    if (expression == nullptr) {
        return;
    }
    const auto information = semantics.expressions.find(expression);
    const bool choice_literal = information != semantics.expressions.end() &&
                                information->second.choice_literal;
    if (expression->kind == ExpressionKind::Name &&
        !choice_literal &&
        semantics.symbols.find(expression->value) != nullptr) {
        bool already_present = false;
        for (const std::string& reference : references) {
            already_present = already_present || reference == expression->value;
        }
        if (!already_present) {
            references.push_back(expression->value);
        }
    }
    collect_field_references(expression->left, semantics, references);
    collect_field_references(expression->right, semantics, references);
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
    } else if (field.type == FieldType::Textarea) {
        output << indent << "  <textarea id=\"" << field.name << "\" name=\""
               << field.name << "\" data-type=\"textarea\" rows=\"6\""
               << (has_help ? " aria-describedby=\"help-" + field.name + "\"" : "");
        if (placeholder != nullptr && !placeholder->empty()) {
            output << " placeholder=\"" << html_escape(*placeholder) << "\"";
        }
        const std::string* minimum_length =
            constraint_value(field, PropertyKind::MinLength);
        const std::string* maximum_length =
            constraint_value(field, PropertyKind::MaxLength);
        if (minimum_length != nullptr) {
            output << " minlength=\"" << *minimum_length << "\"";
        }
        if (maximum_length != nullptr) {
            output << " maxlength=\"" << *maximum_length << "\"";
        }
        if (is_always_required(field)) {
            output << " required";
        }
        output << "></textarea>\n";
    } else {
        output << indent << "  <input id=\"" << field.name << "\" name=\""
               << field.name << "\" type=\"" << input_type(field.type)
               << "\" data-type=\"" << field_type_name(field.type) << "\""
               << (has_help ? " aria-describedby=\"help-" + field.name + "\"" : "");
        if (field.type == FieldType::Phone) {
            output << " inputmode=\"tel\" autocomplete=\"tel\""
                   << " pattern=\"[+() 0-9-]{7,25}\"";
        } else if (field.type == FieldType::Email) {
            output << " autocomplete=\"email\"";
        }
        if (placeholder != nullptr && !placeholder->empty()) {
            output << " placeholder=\"" << html_escape(*placeholder) << "\"";
        }
        if (field.type == FieldType::Integer) {
            output << " step=\"1\"";
        } else if (field.type == FieldType::Decimal) {
            output << " step=\"any\"";
        }
        const std::string* minimum = constraint_value(field, PropertyKind::Minimum);
        const std::string* maximum = constraint_value(field, PropertyKind::Maximum);
        if (minimum != nullptr) {
            output << " min=\"" << *minimum << "\"";
        }
        if (maximum != nullptr) {
            output << " max=\"" << *maximum << "\"";
        }
        const std::string* minimum_length =
            constraint_value(field, PropertyKind::MinLength);
        const std::string* maximum_length =
            constraint_value(field, PropertyKind::MaxLength);
        if (minimum_length != nullptr) {
            output << " minlength=\"" << *minimum_length << "\"";
        }
        if (maximum_length != nullptr) {
            output << " maxlength=\"" << *maximum_length << "\"";
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
           << "    .field input, .field select, .field textarea { grid-column: 1 / -1; width: 100%; min-height: 2.9rem; padding: .72rem .85rem; color: #182235; background: #fff; border: 1px solid #bac5d5; border-radius: 9px; font: inherit; transition: border-color .15s, box-shadow .15s; }\n"
           << "    .field textarea { min-height: 9rem; resize: vertical; line-height: 1.55; }\n"
           << "    .field input::placeholder { color: #7b8799; }\n"
           << "    .field input:focus-visible, .field select:focus-visible, .field textarea:focus-visible, button:focus-visible { outline: 3px solid #2457d64d; outline-offset: 2px; border-color: #2457d6; }\n"
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
        std::vector<std::string> references;
        collect_field_references(
            declaration->check->condition, semantics, references);
        output << "      { references: [";
        for (std::size_t index = 0; index < references.size(); ++index) {
            if (index != 0) {
                output << ", ";
            }
            output << javascript_string(references[index]);
        }
        output << "], valid: () => "
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
           << "      const errors = checks.filter((check) =>\n"
           << "        check.references.every((name) => !document.getElementById(name).disabled) &&\n"
           << "        !check.valid()).map((check) => check.message);\n"
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

namespace {
// Byte strings are Latin-1 containers, never a lossy decoding of source UTF-8.
std::string workflow_string(const std::string& text, bool bytes = false) {
    std::string out = "\"";
    for (unsigned char c : text) {
        if (bytes || c < 32 || c == '<' || c == '>' || c == '&') {
            out += "\\u00";
            out += "0123456789abcdef"[c >> 4];
            out += "0123456789abcdef"[c & 15];
        } else if (c == '"' || c == '\\') { out += '\\'; out += c; }
        else out += c;
    }
    return out + '"';
}
void workflow_expression(std::ostream& out, const IrExpression& e) {
    out << '[' << static_cast<int>(e.kind) << ',' << static_cast<int>(e.type) << ','
        << workflow_string(e.value, true) << ',' << (e.choice_literal ? "true" : "false") << ",[";
    for (std::size_t i = 0; i < e.operands.size(); ++i) {
        if (i) out << ',';
        workflow_expression(out, e.operands[i]);
    }
    out << "]]";
}
void workflow_strings(std::ostream& out, const std::vector<std::string>& values, bool bytes = false) {
    out << '[';
    for (std::size_t i = 0; i < values.size(); ++i) {
        if (i) out << ',';
        out << workflow_string(values[i], bytes);
    }
    out << ']';
}
}  // namespace

std::string generate_workflow_javascript(const LinkedWorkflow& workflow) {
    std::ostringstream out;
    out << "'use strict';\nconst vistaWorkflow = {start:" << workflow_string(workflow.start_alias) << ",modules:[";
    for (std::size_t i = 0; i < workflow.modules.size(); ++i) {
        const auto& m = workflow.modules[i];
        if (i) out << ',';
        out << "{alias:" << workflow_string(m.alias) << ",entry:";
        workflow_expression(out, m.entry);
        out << ",entryRule:" << workflow_string(m.entry_rule_id) << ",exports:";
        workflow_strings(out, m.exports);
        out << ",imports:[";
        for (std::size_t j = 0; j < m.imports.size(); ++j) {
            if (j) out << ',';
            out << workflow_string(m.imports[j].qualified_name);
        }
        out << "],fields:[";
        for (std::size_t j = 0; j < m.fields.size(); ++j) {
            const auto& f = m.fields[j];
            if (j) out << ',';
            out << "{name:" << workflow_string(f.qualified_name) << ",type:" << static_cast<int>(f.type)
                << ",options:";
            workflow_strings(out, f.options, true);
            out << ",visible:"; workflow_expression(out, f.visibility);
            out << ",required:"; workflow_expression(out, f.requiredness);
            out << ",visibleRule:" << workflow_string(f.visibility_rule_id)
                << ",requiredRule:" << workflow_string(f.requiredness_rule_id) << ",constraints:[";
            for (std::size_t k = 0; k < f.constraints.size(); ++k) {
                if (k) out << ',';
                const auto& c = f.constraints[k];
                out << '[' << static_cast<int>(c.kind) << ',' << workflow_string(c.value, true)
                    << ',' << workflow_string(c.rule_id) << ']';
            }
            out << "]}";
        }
        out << "],checks:[";
        for (std::size_t j = 0; j < m.checks.size(); ++j) {
            if (j) out << ',';
            const auto& c = m.checks[j];
            out << "{condition:"; workflow_expression(out, c.condition);
            out << ",message:" << workflow_string(c.message) << ",rule:" << workflow_string(c.rule_id) << '}';
        }
        out << "]}";
    }
    out << "],routes:[";
    for (std::size_t i = 0; i < workflow.routes.size(); ++i) {
        if (i) out << ',';
        const auto& r = workflow.routes[i];
        out << "{source:" << workflow_string(r.source) << ",destination:" << workflow_string(r.destination)
            << ",rule:" << workflow_string(r.rule_id) << ",condition:";
        workflow_expression(out, r.condition); out << '}';
    }
    out << R"JS(]};
// Internal strings contain one character per byte. Browser strings enter here once.
function vistaBytes(text) {
  let result = '';
  for (const byte of new TextEncoder().encode(text)) result += String.fromCharCode(byte);
  return result;
}
function vistaDecimal(text, integer = false) {
  if (!text.length || text.length > 4096) return null;
  if (integer && !/^-?[0-9]+$/.test(text)) return null;
  let i = 0, sign = 1, digits = '', fractional = 0, dot = false;
  if (text[i] === '-' || text[i] === '+') sign = text[i++] === '-' ? -1 : 1;
  while (i < text.length) {
    const c = text[i];
    if (c >= '0' && c <= '9') { digits += c; if (dot) fractional++; }
    else if (c === '.' && !dot) dot = true;
    else break;
    i++;
  }
  if (!digits.length) return null;
  let exponent = 0;
  if (text[i] === 'e' || text[i] === 'E') {
    i++; let exponentSign = 1;
    if (text[i] === '+' || text[i] === '-') exponentSign = text[i++] === '-' ? -1 : 1;
    const start = i;
    while (i < text.length && text[i] >= '0' && text[i] <= '9') {
      const digit = text.charCodeAt(i++) - 48;
      if (exponent > (100000 - digit) / 10) return null;
      exponent = exponent * 10 + digit;
    }
    if (i === start) return null;
    exponent *= exponentSign;
  }
  if (i !== text.length) return null;
  let scale = fractional - exponent;
  if (Math.abs(scale) > 100000) return null;
  digits = digits.replace(/^0+/, '');
  if (!digits) return {sign:0,digits:'0',scale:0};
  while (digits.length > 1 && digits.endsWith('0')) { digits = digits.slice(0,-1); scale--; }
  return Math.abs(scale) > 100000 ? null : {sign,digits,scale};
}
function vistaCompare(a,b) {
  if (a.sign !== b.sign) return a.sign < b.sign ? -1 : 1;
  if (!a.sign) return 0;
  const x = a.digits.length - a.scale, y = b.digits.length - b.scale;
  if (x !== y) return (x < y ? -1 : 1) * a.sign;
  for (let i = 0; i < Math.max(a.digits.length,b.digits.length); i++) {
    const l = a.digits[i] || '0', r = b.digits[i] || '0';
    if (l !== r) return (l < r ? -1 : 1) * a.sign;
  }
  return 0;
}
function vistaDate(text) {
  if (text.length !== 10 || !/^[0-9]{4}-[0-9]{2}-[0-9]{2}$/.test(text)) return false;
  const year = Number(text.slice(0,4)), month = Number(text.slice(5,7)), day = Number(text.slice(8));
  const leap = year % 400 === 0 || (year % 4 === 0 && year % 100 !== 0);
  return year > 0 && month >= 1 && month <= 12 && day >= 1 && day <= [31,leap?29:28,31,30,31,30,31,31,30,31,30,31][month-1];
}
function vistaValue(field,text) {
  const v = {type:field.type,text:text === undefined ? (field.type === 3 ? 'false' : '') : text,
    number:{sign:0,digits:'0',scale:0},boolean:false,present:text !== undefined && text !== '',valid:true,code:''};
  if (!v.present) return v;
  switch (field.type) {
    case 1: case 2: v.number = vistaDecimal(text,field.type === 1); v.valid = v.number !== null; break;
    case 3: v.valid = text === 'true' || text === 'false'; v.boolean = text === 'true'; break;
    case 4: v.valid = vistaDate(text); break;
    case 5: v.valid = field.options.includes(text); v.code = 'VAL007'; break;
    // Full byte-string match: ASCII POSIX whitespace only; $ alone accepts final LF.
    case 7: { const match = /^[^@\x09-\x0d ]+@[^@\x09-\x0d ]+\.[^@\x09-\x0d ]+$/.exec(text); v.valid = !!match && match[0].length === text.length; break; }
    case 8: { const digits = [...text].filter(c => c >= '0' && c <= '9').length;
      v.valid = !/[^+() 0-9-]/.test(text) && digits >= 7 && digits <= 15; break; }
  }
  if (!v.valid && !v.code) v.code = 'VAL005';
  return v;
}
function vistaExpression(e,scope) {
  const [kind,type,text,choice,args] = e;
  const v = {type:0,text,number:{sign:0,digits:'0',scale:0},boolean:false,present:true,valid:true};
  if (choice) { v.type = 5; return v; }
  switch (kind) {
    case 0: return scope.get(text) || {...v,valid:false};
    case 1: v.type = type === 4 ? 4 : 0; v.valid = v.type !== 4 || vistaDate(text); return v;
    case 2: case 3: v.type = kind === 2 ? 1 : 2; v.number = vistaDecimal(text); v.valid = v.number !== null; return v;
    case 4: v.type = 3; v.valid = text === 'true' || text === 'false'; v.boolean = text === 'true'; return v;
  }
  v.type = 3; v.valid = false;
  if (kind === 5 && args.length === 1) {
    const a = vistaExpression(args[0],scope);
    if (text === 'present') { v.valid = a.valid && a.type === 6; v.boolean = v.valid && a.present; }
    else if (text === 'not') { v.valid = a.valid && a.type === 3; v.boolean = v.valid && !a.boolean; }
    return v;
  }
  if (kind !== 6 || args.length !== 2) return v;
  const a = vistaExpression(args[0],scope), b = vistaExpression(args[1],scope);
  if (!a.valid || !b.valid) return v;
  if (text === 'and' || text === 'or') {
    v.valid = a.type === 3 && b.type === 3;
    v.boolean = v.valid && (text === 'and' ? a.boolean && b.boolean : a.boolean || b.boolean);
    return v;
  }
  let order;
  if ([1,2].includes(a.type) && [1,2].includes(b.type)) order = vistaCompare(a.number,b.number);
  else if (a.type === 3 && b.type === 3) order = a.boolean === b.boolean ? 0 : a.boolean ? 1 : -1;
  else order = a.text === b.text ? 0 : a.text < b.text ? -1 : 1;
  const comparisons = {'==':order===0,'!=':order!==0,'<':order<0,'<=':order<=0,'>':order>0,'>=':order>=0};
  v.valid = Object.hasOwn(comparisons,text); v.boolean = v.valid && comparisons[text]; return v;
}
function vistaByteOrder(a,b) {
  const left = vistaBytes(a), right = vistaBytes(b);
  return left === right ? 0 : left < right ? -1 : 1;
}
function vistaNormalize(o) {
  o.fields.sort((a,b)=>vistaByteOrder(a.field,b.field));
  o.rule_ids = [...new Set(o.rule_ids)].sort(vistaByteOrder); o.diagnostic_codes.sort(vistaByteOrder);
  o.diagnostic_details.sort((a,b)=>{
    for (const k of ['code','field','rule_id']) { const order = vistaByteOrder(a[k],b[k]); if (order) return order; } return 0;
  }); return o;
}
function vistaError(o,code,field='',rule_id='',message=code) {
  o.diagnostic_codes.push(code); o.diagnostic_details.push({code,field,rule_id});
  o.diagnostics.push({code,field,rule_id,message});
}
function vistaHidden(e,visibility) {
  return (e[0] === 0 && !e[3] && visibility.get(e[2]) === false) || e[4].some(a=>vistaHidden(a,visibility));
}
function vistaStep(input,active) {
  const o = {active_module:active,fields:[],selected_route:'',complete:false,diagnostic_codes:[],diagnostic_details:[],rule_ids:[],transition_trace:[],diagnostics:[]};
  const all = new Map(), exported = new Map();
  for (const m of vistaWorkflow.modules) {
    for (const f of m.fields) all.set(f.name,vistaValue(f,input.get(f.name)));
    for (const name of m.exports) if (all.has(name)) exported.set(name,all.get(name));
  }
  for (const name of [...input.keys()].sort(vistaByteOrder)) if (!all.has(name)) vistaError(o,'VAL003',name,'',`Unknown field ${name}`);
  const m = vistaWorkflow.modules.find(m=>m.alias === active);
  if (!m) { vistaError(o,'LNK005','','',`Invalid active module ${active}`); return vistaNormalize(o); }
  const scope = new Map(m.fields.map(f=>[f.name,all.get(f.name)]));
  for (const name of m.imports) if (exported.has(name)) scope.set(name,exported.get(name));
  const predicate = e => { const v = vistaExpression(e,scope); return {valid:v.valid && v.type === 3,value:v.boolean}; };
  o.rule_ids.push(m.entryRule);
  const entered = predicate(m.entry);
  if (!entered.valid || !entered.value) { vistaError(o,'VAL010','',m.entryRule,`Cannot enter ${active}`); return vistaNormalize(o); }
  const visibility = new Map();
  for (const f of m.fields) {
    o.rule_ids.push(f.visibleRule,f.requiredRule,f.name+'.value');
    const visible = predicate(f.visible), required = predicate(f.required);
    if (!visible.valid) vistaError(o,'VAL011',f.name,f.visibleRule,'Invalid visibility condition');
    if (!required.valid) vistaError(o,'VAL011',f.name,f.requiredRule,'Invalid requiredness condition');
    visibility.set(f.name,visible.valid && visible.value);
    const value = all.get(f.name); let valid = value.valid;
    if (!valid) vistaError(o,value.code,f.name,f.name+'.value',`Invalid value for ${f.name}`);
    if (visible.value && required.value && (!value.present || (f.type === 3 && value.valid && !value.boolean))) {
      valid = false; vistaError(o,'VAL004',f.name,f.requiredRule,`Required field ${f.name} is missing`);
    } else if (visible.value && value.present && value.valid) {
      for (const [kind,bound,rule] of f.constraints) {
        o.rule_ids.push(rule); let failed = false, invalid = false;
        if ((kind === 7 || kind === 8) && [1,2].includes(f.type)) {
          const n = vistaDecimal(bound); invalid = n === null;
          if (!invalid) { const order = vistaCompare(value.number,n); failed = kind === 7 ? order < 0 : order > 0; }
        } else if ((kind === 7 || kind === 8) && f.type === 4) failed = kind === 7 ? value.text < bound : value.text > bound;
        else if (kind === 9 || kind === 10) {
          const n = vistaDecimal(bound,true); invalid = n === null;
          if (!invalid) { const order = vistaCompare(vistaDecimal(String(value.text.length),true),n); failed = kind === 9 ? order < 0 : order > 0; }
        }
        if (invalid || failed) { valid = false; vistaError(o,invalid?'VAL011':'VAL006',f.name,rule,`Constraint failed for ${f.name}`); }
      }
    }
    o.fields.push({field:f.name,visible:visible.value,required:visible.value && required.value,valid});
  }
  for (const check of m.checks) {
    if (vistaHidden(check.condition,visibility)) continue;
    o.rule_ids.push(check.rule); const passed = predicate(check.condition);
    if (!passed.valid || !passed.value) vistaError(o,'VAL009','',check.rule,check.message);
  }
  const enabled = [];
  for (const route of vistaWorkflow.routes) {
    if (route.source !== active) continue;
    o.rule_ids.push(route.rule); const passed = vistaExpression(route.condition,exported);
    if (!passed.valid || passed.type !== 3) vistaError(o,'VAL011','',route.rule,'Invalid route condition');
    else if (passed.boolean) enabled.push(route);
  }
  if (enabled.length > 1) vistaError(o,'LNK008','','',`Ambiguous routes from ${active}`);
  else if (!o.diagnostic_codes.length) {
    if (!enabled.length) o.complete = true;
    else { o.selected_route = active+'->'+enabled[0].destination; o.transition_trace.push(o.selected_route); }
  }
  return vistaNormalize(o);
}
// activeModule: exact one-state mode. inputMode:'bytes': lossless Latin-1 byte containers.
function vistaEvaluate(input,activeModule,options = {}) {
  if (!input || typeof input !== 'object' || Array.isArray(input)) throw new TypeError('Expected a field-value object');
  if (options.inputMode !== undefined && options.inputMode !== 'bytes' && options.inputMode !== 'unicode') throw new TypeError('Unknown inputMode');
  const values = new Map();
  for (const [name,text] of Object.entries(input)) {
    if (typeof text !== 'string') throw new TypeError('Field values must be strings');
    if (options.inputMode === 'bytes') {
      for (let i=0;i<text.length;i++) if (text.charCodeAt(i)>255) throw new TypeError('Byte input must contain only Latin-1 code units');
      values.set(name,text);
    } else values.set(name,vistaBytes(text));
  }
  if (activeModule !== undefined) return vistaStep(values,activeModule);
  let active = vistaWorkflow.start, selected = ''; const visited = new Set(), trace = [], rules = [];
  for (;;) {
    const o = vistaStep(values,active); rules.push(...o.rule_ids);
    if (visited.has(active)) { o.complete = false; o.selected_route = ''; o.transition_trace = []; vistaError(o,'LNK006','','',`Nonterminating route cycle at ${active}`); }
    visited.add(active);
    if (o.selected_route) { selected = o.selected_route; trace.push(selected); }
    const next = o.selected_route; o.selected_route = selected; o.transition_trace = trace.slice(); o.rule_ids = rules.slice(); vistaNormalize(o);
    if (o.diagnostic_codes.length || o.complete || !next) return o;
    active = next.slice(next.indexOf('->')+2);
  }
}
function vistaCanonicalObservation(source) {
  const o = vistaNormalize({...source,fields:source.fields.slice(),rule_ids:source.rule_ids.slice(),diagnostic_codes:source.diagnostic_codes.slice(),diagnostic_details:source.diagnostic_details.slice()});
  const text = s => new TextEncoder().encode(s).length+':'+s;
  let out = 'VISTA-OBSERVATION 1\n'+text(o.active_module)+'\n'+o.fields.length+'\n';
  for (const f of o.fields) out += text(f.field)+' '+Number(f.visible)+' '+Number(f.required)+' '+Number(f.valid)+'\n';
  out += text(o.selected_route)+'\n'+Number(o.complete)+'\n'+o.diagnostic_codes.length+'\n';
  for (const c of o.diagnostic_codes) out += text(c)+'\n';
  out += o.diagnostic_details.length+'\n';
  for (const d of o.diagnostic_details) out += text(d.code)+text(d.field)+text(d.rule_id)+'\n';
  out += o.rule_ids.length+'\n'; for (const r of o.rule_ids) out += text(r)+'\n';
  out += o.transition_trace.length+'\n'; for (const t of o.transition_trace) out += text(t)+'\n';
  return out;
}
if (typeof module !== 'undefined' && module.exports) module.exports = {vistaEvaluate,vistaCanonicalObservation};
)JS";
    return out.str();
}

std::string generate_workflow_html(const LinkedWorkflow& workflow, std::string_view javascript) {
    std::ostringstream out;
    out << "<!doctype html>\n<html lang=\"en\"><head><meta charset=\"utf-8\">"
        << "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\"><title>"
        << html_escape(workflow.name) << "</title><style>"
        << "body{font-family:system-ui,sans-serif;max-width:48rem;margin:2rem auto;padding:1rem}"
        << ".field{margin:1rem 0}label,small{display:block}input,select,textarea,button{font:inherit;padding:.5rem}"
        << "[hidden]{display:none} :focus-visible{outline:3px solid #2457d6;outline-offset:2px}"
        << "[aria-invalid=true]{border:2px solid #a11919}#validation-errors{color:#a11919}"
        << "</style></head><body><main><h1>" << html_escape(workflow.name) << "</h1>\n"
        << "<form id=\"vista-workflow\" novalidate><p>Required fields are marked with an asterisk.</p>\n";
    for (const auto& m : workflow.modules) {
        const auto alias = html_escape(m.alias);
        out << "<section data-module=\"" << alias << "\" aria-labelledby=\"module-" << alias << "-title\""
            << (m.alias == workflow.start_alias ? "" : " hidden") << "><h2 id=\"module-" << alias
            << "-title\" tabindex=\"-1\">" << alias << "</h2>\n";
        for (const auto& f : m.fields) {
            const auto id = html_escape(f.qualified_name);
            out << "<div class=\"field\"><label for=\"" << id << "\">" << html_escape(f.label.empty() ? f.qualified_name : f.label)
                << " <span id=\"required-" << id << "\" aria-hidden=\"true\" hidden>*</span></label>\n";
            const auto attrs = " id=\"" + id + "\" name=\"" + id + "\" data-type=\"" + field_type_name(f.type) +
                "\" aria-describedby=\"help-" + id + " error-" + id + "\"";
            if (f.type == FieldType::Choice) {
                out << "<select" << attrs << "><option value=\"\">Select an option</option>";
                for (const auto& option : f.options) out << "<option value=\"" << html_escape(option) << "\">" << html_escape(option) << "</option>";
                out << "</select>\n";
            } else if (f.type == FieldType::Textarea) {
                out << "<textarea" << attrs << " placeholder=\"" << html_escape(f.placeholder) << "\"></textarea>\n";
            } else {
                // Text controls preserve decimal exponent/precision and invalid supplied spellings.
                const auto type = f.type == FieldType::Integer || f.type == FieldType::Decimal ? "text" : input_type(f.type);
                out << "<input" << attrs << " type=\"" << type << "\"";
                if (f.type == FieldType::Integer || f.type == FieldType::Decimal) out << " inputmode=\"decimal\"";
                if (f.type != FieldType::File && f.type != FieldType::Boolean) out << " placeholder=\"" << html_escape(f.placeholder) << "\"";
                out << ">\n";
            }
            out << "<small id=\"help-" << id << "\">" << html_escape(f.help) << "</small>"
                << "<small id=\"error-" << id << "\"></small></div>\n";
        }
        out << "</section>\n";
    }
    out << "<ul id=\"validation-errors\" role=\"alert\" aria-label=\"Workflow errors\" aria-atomic=\"true\"></ul>"
        << "<p id=\"status\" role=\"status\" aria-live=\"polite\"></p><button type=\"submit\">Continue</button>"
        << "</form><noscript>JavaScript is required to evaluate this workflow.</noscript></main>\n<script>\n"
        << javascript << "</script>\n<script>\n";
    out << "let vistaActiveModule = " << workflow_string(workflow.start_alias) << ";\n";
    out << R"JS(const vistaForm = document.getElementById('vista-workflow');
const vistaControls = [...document.querySelectorAll('[data-type]')];
const vistaSections = [...document.querySelectorAll('[data-module]')];
function vistaReadInput() {
  return Object.fromEntries(vistaControls.map(c=>[c.id,c.dataset.type === 'boolean' ? String(c.checked) :
    c.dataset.type === 'file' ? (c.files[0]?.name || '') : c.value]));
}
function vistaRender(result,showErrors) {
  for (const section of vistaSections) section.hidden = section.dataset.module !== vistaActiveModule;
  for (const control of vistaControls) {
    const state = result.fields.find(f=>f.field === control.id);
    const visible = !!state?.visible;
    control.closest('.field').hidden = !visible; control.disabled = !visible;
    control.required = !!state?.required; control.setAttribute('aria-required',String(control.required));
    control.setAttribute('aria-invalid',String(showErrors && !!state && !state.valid));
    const marker = document.getElementById('required-'+control.id); if (marker) marker.hidden = !control.required;
    const error = document.getElementById('error-'+control.id);
    if (error) error.textContent = showErrors ? result.diagnostics.filter(d=>d.field === control.id).map(d=>d.message).join('; ') : '';
  }
  const errors = document.getElementById('validation-errors'); errors.replaceChildren();
  if (showErrors) for (const d of result.diagnostics) {
    const li = document.createElement('li'); li.textContent = d.message; errors.appendChild(li);
  }
  document.getElementById('status').textContent = showErrors && result.complete ? 'Workflow complete.' : 'Active module: '+vistaActiveModule;
}
function vistaUpdate() { vistaRender(vistaEvaluate(vistaReadInput(),vistaActiveModule),false); }
vistaForm.addEventListener('input',vistaUpdate);
vistaForm.addEventListener('change',vistaUpdate);
vistaForm.addEventListener('submit',event=>{
  event.preventDefault();
  const result = vistaEvaluate(vistaReadInput(),vistaActiveModule);
  if (result.selected_route) {
    vistaActiveModule = result.selected_route.slice(result.selected_route.indexOf('->')+2);
    vistaRender(vistaEvaluate(vistaReadInput(),vistaActiveModule),true);
    vistaSections.find(s=>s.dataset.module === vistaActiveModule).querySelector('h2').focus();
  } else {
    vistaRender(result,true);
    if (result.diagnostic_codes.length) vistaControls.find(c=>!c.disabled && c.getAttribute('aria-invalid') === 'true')?.focus();
  }
});
vistaUpdate();
</script>
</body></html>
)JS";
    return out.str();
}

}  // namespace vista
