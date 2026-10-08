#include "linker.hpp"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iterator>
#include <map>
#include <set>
#include <sstream>
#include <tuple>

#include "hash.hpp"
#include "file_io.hpp"
#include "module.hpp"
#include "parser.hpp"
#include "scanner.hpp"

namespace vista {
namespace {

void diagnose(LinkResult& result, const std::string& code,
              const std::string& message, const SourceSpan& span) {
    result.diagnostics.push_back({code, message, span.first_line, span.first_column});
}

// Reuse the file-only scanner without adding a second lexer or changing its API.
struct SourceFile {
    std::filesystem::path directory;
    std::filesystem::path path;
    ~SourceFile() {
        std::error_code error;
        if (!path.empty()) std::filesystem::remove(path, error);
        if (!directory.empty()) std::filesystem::remove(directory, error);
    }
    bool write(const std::string& source) {
        std::error_code error;
        const auto root = std::filesystem::temp_directory_path(error);
        if (error) return false;
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        for (unsigned attempt = 0; attempt < 100; ++attempt) {
            const auto candidate = root / ("vista-link-" + std::to_string(stamp) + "-" +
                                           std::to_string(attempt));
            if (std::filesystem::create_directory(candidate, error)) {
                directory = candidate;
                break;
            }
            if (error) return false;
        }
        if (directory.empty()) return false;
        path = directory / "module.vista";
        std::ofstream output(path, std::ios::binary);
        output << source;
        output.close();
        return !output.fail();
    }
};

struct LoadedModule {
    ModuleObject object;
    ParseResult parsed;
    SemanticResult semantics;
    std::map<std::string, std::string> names;
    std::vector<Symbol> imports;
};

bool load_module(const WorkflowUse& use, const std::filesystem::path& base,
                 LoadedModule& loaded, LinkResult& result) {
    const auto path = base / use.source_path;
    std::string bytes;
    if (!read_binary_file(path, bytes)) {
        result.operational_failure = true;
        diagnose(result, "LNK001", "cannot read module '" + path.string() + "'", use.span);
        return false;
    }
    std::string source;
    const bool object_input = path.extension() == ".vobj";
    if (object_input) {
        auto object = load_module_object(bytes);
        if (!object.succeeded()) {
            for (const auto& diagnostic : object.diagnostics)
                diagnose(result, diagnostic.code, use.alias + ": " + diagnostic.message, use.span);
            return false;
        }
        loaded.object = std::move(object.object);
        source = loaded.object.canonical_source;
    } else if (path.extension() == ".vista") {
        // Same newline normalization as module compilation, before scanning comments.
        for (std::size_t index = 0; index < bytes.size(); ++index) {
            source += bytes[index] == '\r' ? '\n' : bytes[index];
            if (bytes[index] == '\r' && index + 1 < bytes.size() && bytes[index + 1] == '\n') ++index;
        }
    } else {
        diagnose(result, "LNK001", "module use requires .vista or .vobj", use.span);
        return false;
    }
    SourceFile temporary;
    if (!temporary.write(source)) {
        result.operational_failure = true;
        diagnose(result, "LNK001", "cannot prepare module source for scanner", use.span);
        return false;
    }
    const auto scan = scan_file(temporary.path.string());
    if (!scan.input_opened || !scan.diagnostics.empty()) {
        if (!scan.input_opened) result.operational_failure = true;
        diagnose(result, "LNK001", "cannot scan module '" + use.alias + "'", use.span);
        for (const auto& diagnostic : scan.diagnostics)
            diagnose(result, diagnostic.code, use.alias + ": " + diagnostic.message, use.span);
        return false;
    }
    loaded.parsed = parse_tokens(scan.tokens);
    if (!loaded.parsed.succeeded() || loaded.parsed.module == nullptr) {
        diagnose(result, "LNK001", "use '" + use.alias + "' must contain a valid module", use.span);
        for (const auto& diagnostic : loaded.parsed.diagnostics)
            diagnose(result, diagnostic.code, use.alias + ": " + diagnostic.message, use.span);
        return false;
    }
    auto compiled = compile_module(*loaded.parsed.module, source);
    if (!compiled.succeeded()) {
        for (const auto& diagnostic : compiled.diagnostics)
            diagnose(result, diagnostic.code, use.alias + ": " + diagnostic.message, use.span);
        return false;
    }
    // A valid digest does not by itself prove the header matches the embedded source.
    if (object_input && serialize_module_object(compiled.object) != serialize_module_object(loaded.object)) {
        diagnose(result, "MOD004", "module contract differs from embedded source", use.span);
        return false;
    }
    loaded.object = std::move(compiled.object);
    return true;
}

using Graph = std::map<std::string, std::vector<std::string>>;
bool has_cycle(const Graph& graph) {
    std::map<std::string, unsigned> state;
    std::function<bool(const std::string&)> visit = [&](const std::string& name) {
        if (state[name] == 1) return true;
        if (state[name] == 2) return false;
        state[name] = 1;
        const auto found = graph.find(name);
        if (found != graph.end())
            for (const auto& next : found->second)
                if (visit(next)) return true;
        state[name] = 2;
        return false;
    };
    for (const auto& item : graph) if (visit(item.first)) return true;
    return false;
}

IrExpression boolean(bool value) {
    return {ExpressionKind::BooleanLiteral, ValueType::Boolean, value ? "true" : "false", {}, false};
}

void combine(IrExpression& aggregate, IrExpression condition, const std::string& operation) {
    const std::string identity = operation == "and" ? "true" : "false";
    if (aggregate.kind == ExpressionKind::BooleanLiteral && aggregate.value == identity) {
        aggregate = std::move(condition);
    } else {
        aggregate = {ExpressionKind::Binary, ValueType::Boolean, operation,
                     {std::move(aggregate), std::move(condition)}, false};
    }
}

std::string type_name(ValueType type) {
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

// Escape every control byte: one physical line per record, even for multiline messages.
std::string quote(const std::string& value) {
    std::ostringstream output;
    output << '"';
    for (unsigned char character : value) {
        if (character == '"' || character == '\\') output << '\\' << character;
        else if (character < 32 || character == 127)
            output << "\\x" << std::hex << std::setw(2) << std::setfill('0')
                   << static_cast<unsigned>(character) << std::dec;
        else output << character;
    }
    output << '"';
    return output.str();
}

std::string expression_text(const IrExpression& expression) {
    static const char* kinds[] = {"name", "string", "integer", "decimal", "boolean", "unary", "binary"};
    std::string text = "(" + std::string(expression.choice_literal ? "choice-literal" :
                       kinds[static_cast<unsigned>(expression.kind)]) + " " +
                       type_name(expression.type) + " " + quote(expression.value);
    for (const auto& operand : expression.operands) text += " " + expression_text(operand);
    return text + ")";
}

std::string constraint_name(PropertyKind kind) {
    switch (kind) {
        case PropertyKind::Minimum: return "minimum";
        case PropertyKind::Maximum: return "maximum";
        case PropertyKind::MinLength: return "minlength";
        case PropertyKind::MaxLength: return "maxlength";
        default: return "";
    }
}

LinkedModule lower_module(const std::string& alias, const LoadedModule& loaded) {
    LinkedModule linked;
    linked.alias = alias;
    linked.name = loaded.object.name;
    linked.source_sha256 = loaded.object.source_sha256;
    linked.object_sha256 = loaded.object.object_sha256;
    linked.entry_rule_id = alias + ".entry";
    linked.entry = lower_expression(loaded.parsed.module->enter_condition, loaded.semantics, loaded.names);
    for (const auto& import : loaded.object.imports) {
        const auto* symbol = loaded.semantics.symbols.find(import.alias);
        linked.imports.push_back({alias + "." + import.alias, loaded.names.at(import.alias),
                                  symbol->type, symbol->options});
    }
    for (const auto& exported : loaded.object.exports) linked.exports.push_back(alias + "." + exported.field);
    unsigned check_index = 0;
    for (const auto* declaration : loaded.parsed.module->declarations) {
        if (declaration->kind == DeclarationKind::Check) {
            linked.checks.push_back({lower_expression(declaration->check->condition, loaded.semantics, loaded.names),
                                     declaration->check->message, alias + ".check." + std::to_string(++check_index)});
            continue;
        }
        const auto& field = *declaration->field;
        LinkedField lowered;
        lowered.qualified_name = alias + "." + field.name;
        lowered.type = field.type;
        lowered.options = field.options;
        lowered.visibility_rule_id = lowered.qualified_name + ".visible";
        lowered.requiredness_rule_id = lowered.qualified_name + ".required";
        for (const auto* property : field.properties) {
            switch (property->kind) {
                case PropertyKind::Label: lowered.label = property->text; break;
                case PropertyKind::Section: lowered.section = property->text; break;
                case PropertyKind::Help: lowered.help = property->text; break;
                case PropertyKind::Placeholder: lowered.placeholder = property->text; break;
                case PropertyKind::Required: combine(lowered.requiredness, boolean(true), "or"); break;
                case PropertyKind::ShowWhen:
                    combine(lowered.visibility, lower_expression(property->condition, loaded.semantics, loaded.names), "and"); break;
                case PropertyKind::RequiredWhen:
                    combine(lowered.requiredness, lower_expression(property->condition, loaded.semantics, loaded.names), "or"); break;
                default:
                    lowered.constraints.push_back({property->kind, property->text,
                        lowered.qualified_name + "." + constraint_name(property->kind)});
            }
        }
        std::sort(lowered.constraints.begin(), lowered.constraints.end(), [](const auto& a, const auto& b) {
            return a.rule_id < b.rule_id;
        });
        linked.fields.push_back(std::move(lowered));
    }
    // Keep field declaration order for display; canonical serialization sorts its copy.
    return linked;
}

std::string workflow_prefix(const LinkedWorkflow& workflow) {
    std::ostringstream output;
    output << "VISTA-WORKFLOW 1\nworkflow " << workflow.name << "\nstart " << workflow.start_alias << '\n';
    auto modules = workflow.modules;
    std::sort(modules.begin(), modules.end(), [](const auto& a, const auto& b) { return a.alias < b.alias; });
    output << "modules " << modules.size() << '\n';
    for (auto& module : modules) {
        output << "module " << module.alias << ' ' << module.name << '\n'
               << "source-sha256 " << module.source_sha256 << '\n'
               << "object-sha256 " << module.object_sha256 << '\n';
        std::sort(module.imports.begin(), module.imports.end(), [](const auto& a, const auto& b) { return a.alias < b.alias; });
        for (const auto& import : module.imports) {
            output << "import " << import.alias << " -> " << import.qualified_name << ' ' << field_type_name(import.type);
            for (const auto& option : import.options) output << ' ' << quote(option);
            output << '\n';
        }
        std::sort(module.exports.begin(), module.exports.end());
        for (const auto& name : module.exports) output << "export " << name << '\n';
        output << "rule " << module.entry_rule_id << ' ' << expression_text(module.entry) << '\n';
        std::sort(module.fields.begin(), module.fields.end(), [](const auto& a, const auto& b) { return a.qualified_name < b.qualified_name; });
        for (const auto& field : module.fields) {
            output << "field " << field.qualified_name << ' ' << field_type_name(field.type);
            for (const auto& option : field.options) output << ' ' << quote(option);
            output << "\nlabel " << quote(field.label) << "\nsection " << quote(field.section)
                   << "\nhelp " << quote(field.help) << "\nplaceholder " << quote(field.placeholder) << '\n';
            output << "rule " << field.visibility_rule_id << ' ' << expression_text(field.visibility) << '\n'
                   << "rule " << field.requiredness_rule_id << ' ' << expression_text(field.requiredness) << '\n';
            for (const auto& constraint : field.constraints)
                output << "rule " << constraint.rule_id << ' ' << constraint_name(constraint.kind) << ' ' << quote(constraint.value) << '\n';
        }
        for (const auto& check : module.checks)
            output << "rule " << check.rule_id << ' ' << expression_text(check.condition) << ' ' << quote(check.message) << '\n';
    }
    auto routes = workflow.routes;
    std::sort(routes.begin(), routes.end(), [](const auto& a, const auto& b) {
        return std::tie(a.source, a.destination, a.rule_id) < std::tie(b.source, b.destination, b.rule_id);
    });
    output << "routes " << routes.size() << '\n';
    for (const auto& route : routes)
        output << "route " << route.source << " -> " << route.destination << " rule " << route.rule_id
               << ' ' << expression_text(route.condition) << '\n';
    return output.str();
}

}  // namespace

IrExpression lower_expression(const Expression* expression, const SemanticResult& semantics,
                              const std::map<std::string, std::string>& names) {
    if (expression == nullptr) return boolean(true);
    const auto& info = semantics.expressions.at(expression);
    IrExpression lowered{expression->kind, info.type, expression->value, {}, info.choice_literal};
    if (expression->kind == ExpressionKind::Name && !info.choice_literal)
        lowered.value = names.at(info.referenced_field);
    if (expression->left) lowered.operands.push_back(lower_expression(expression->left, semantics, names));
    if (expression->right) lowered.operands.push_back(lower_expression(expression->right, semantics, names));
    return lowered;
}

LinkResult link_workflow(const WorkflowAst& ast, const std::filesystem::path& workflow_path) {
    LinkResult result;
    result.workflow.name = ast.name;
    result.workflow.start_alias = ast.start_module;
    std::map<std::string, LoadedModule> modules;
    for (const auto* use : ast.uses)
        if (!modules.try_emplace(use->alias).second)
            diagnose(result, "LNK004", "duplicate module alias '" + use->alias + "'", use->span);
    if (!result.succeeded()) return result; // Duplicate aliases must precede any module IO.
    for (const auto* use : ast.uses)
        load_module(*use, workflow_path.parent_path(), modules.at(use->alias), result);
    if (!result.succeeded()) return result;

    if (modules.count(ast.start_module) == 0)
        diagnose(result, "LNK005", "invalid start module '" + ast.start_module + "'", ast.span);
    Graph dependencies, routes, unconditional;
    std::map<std::string, unsigned> outgoing;
    for (auto& item : modules) {
        auto& loaded = item.second;
        for (const auto* declaration : loaded.parsed.module->declarations)
            if (declaration->kind == DeclarationKind::Field)
                loaded.names[declaration->field->name] = item.first + "." + declaration->field->name;
        for (const auto& import : loaded.object.imports) {
            dependencies[item.first].push_back(import.provider);
            const auto provider = modules.find(import.provider);
            const ExportContract* exported = nullptr;
            if (provider != modules.end())
                for (const auto& candidate : provider->second.object.exports)
                    if (candidate.field == import.field) exported = &candidate;
            if (exported == nullptr) {
                diagnose(result, "LNK002", item.first + ": unresolved export '" + import.provider + "." + import.field + "'", import.span);
                continue;
            }
            if (!loaded.names.emplace(import.alias, import.provider + "." + import.field).second) {
                diagnose(result, "LNK002", item.first + ": duplicate or local imported alias '" + import.alias + "'", import.span);
                continue;
            }
            loaded.imports.push_back({import.alias, exported->type, exported->options, "", import.span, true});
        }
    }
    if (has_cycle(dependencies)) diagnose(result, "LNK006", "module import cycle", ast.span);
    for (const auto* route : ast.routes) {
        if (modules.count(route->source) == 0 || modules.count(route->destination) == 0) {
            diagnose(result, "LNK002", "unresolved route endpoint", route->span);
            continue;
        }
        routes[route->source].push_back(route->destination);
        if (route->condition == nullptr ||
            (route->condition->kind == ExpressionKind::BooleanLiteral && route->condition->value == "true")) {
            unconditional[route->source].push_back(route->destination);
            if (++outgoing[route->source] > 1)
                diagnose(result, "LNK008", "ambiguous unconditional routes from '" + route->source + "'", route->span);
        }
    }
    if (has_cycle(unconditional)) diagnose(result, "LNK006", "unconditional route cycle", ast.span);
    if (modules.count(ast.start_module)) {
        std::set<std::string> reached;
        std::vector<std::string> pending{ast.start_module};
        while (!pending.empty()) {
            const auto current = pending.back();
            pending.pop_back();
            if (!reached.insert(current).second) continue;
            for (const auto& next : routes[current]) pending.push_back(next);
        }
        for (const auto* use : ast.uses)
            if (!reached.count(use->alias)) diagnose(result, "LNK007", "unreachable module '" + use->alias + "'", use->span);
    }
    if (!result.succeeded()) return result;

    for (auto& item : modules) {
        auto& loaded = item.second;
        loaded.semantics = analyze_semantics(*loaded.parsed.module, loaded.imports);
        for (const auto& diagnostic : loaded.semantics.diagnostics)
            result.diagnostics.push_back({"LNK003", item.first + ": " + diagnostic.message,
                                          diagnostic.line, diagnostic.column});
        if (loaded.semantics.succeeded()) result.workflow.modules.push_back(lower_module(item.first, loaded));
    }
    std::vector<Symbol> route_symbols;
    std::map<std::string, std::string> route_names;
    std::map<std::string, unsigned> route_occurrences;
    for (const auto& item : modules)
        for (const auto& exported : item.second.object.exports) {
            const auto qualified = item.first + "." + exported.field;
            route_symbols.push_back({qualified, exported.type, exported.options, "", ast.span, true});
            route_names[qualified] = qualified;
        }
    for (const auto* route : ast.routes) {
        ModuleAst condition_scope(ast.name, {}, {}, route->condition, {}, route->span);
        const auto semantics = analyze_semantics(condition_scope, route_symbols);
        for (const auto& diagnostic : semantics.diagnostics)
            result.diagnostics.push_back({diagnostic.code == "SEM002" ? "LNK002" : "LNK003",
                                          "route: " + diagnostic.message, diagnostic.line, diagnostic.column});
        if (!semantics.succeeded()) continue;
        auto condition = lower_expression(route->condition, semantics, route_names);
        const std::string identity = route->source + " -> " + route->destination + " " + expression_text(condition);
        result.workflow.routes.push_back({route->source, route->destination, std::move(condition),
            ast.name + ".route." + sha256_hex(identity) + "." +
                std::to_string(++route_occurrences[identity])});
    }
    // Routes have no priority. Consumers must evaluate every outgoing condition.
    std::sort(result.workflow.routes.begin(), result.workflow.routes.end(), [](const auto& a, const auto& b) {
        return std::tie(a.source, a.destination, a.rule_id) < std::tie(b.source, b.destination, b.rule_id);
    });
    if (result.succeeded()) result.workflow.canonical_sha256 = sha256_hex(workflow_prefix(result.workflow));
    return result;
}

std::string format_linked_workflow(const LinkedWorkflow& workflow) {
    const auto prefix = workflow_prefix(workflow);
    return prefix + "workflow-sha256 " + sha256_hex(prefix) + '\n';
}

}  // namespace vista
