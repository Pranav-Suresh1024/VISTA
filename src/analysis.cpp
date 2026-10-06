#include "analysis.hpp"
#include "linker.hpp"
#include "partition.hpp"

#include <algorithm>
#include <cstddef>
#include <functional>
#include <iomanip>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace vista {
namespace {

void add_diagnostic(AnalysisResult& result,
                    std::string code,
                    std::string message,
                    const SourceSpan& span) {
    result.diagnostics.push_back(
        {std::move(code), std::move(message), span.first_line, span.first_column});
}

std::string dependency_kind_name(DependencyKind kind) {
    return kind == DependencyKind::Visibility ? "show when" : "required when";
}

void collect_references(const Expression* expression,
                        const SemanticResult& semantics,
                        std::vector<std::string>& references) {
    if (expression == nullptr) {
        return;
    }
    const auto information = semantics.expressions.find(expression);
    const bool choice_literal = information != semantics.expressions.end() &&
                                information->second.choice_literal;
    if (expression->kind == ExpressionKind::Name && !choice_literal &&
        semantics.symbols.find(expression->value) != nullptr &&
        std::find(references.begin(), references.end(), expression->value) ==
            references.end()) {
        references.push_back(expression->value);
    }
    collect_references(expression->left, semantics, references);
    collect_references(expression->right, semantics, references);
}

void combine(IrExpression& aggregate, IrExpression condition, const std::string& operation) {
    if (aggregate.kind == ExpressionKind::BooleanLiteral &&
        aggregate.value == (operation == "and" ? "true" : "false")) aggregate = std::move(condition);
    else aggregate = {ExpressionKind::Binary, ValueType::Boolean, operation,
                      {std::move(aggregate), std::move(condition)}, false};
}

void analyze_hidden_required(const FormAst& form, const FieldDecl& target,
                             const SemanticResult& semantics, AnalysisResult& result) {
    bool show = false, require = false;
    std::vector<std::string> references;
    for (const auto* p : target.properties) {
        show = show || p->kind == PropertyKind::ShowWhen;
        require = require || p->kind == PropertyKind::Required || p->kind == PropertyKind::RequiredWhen;
        if (p->condition) collect_references(p->condition, semantics, references);
    }
    if (!show || !require) return;
    std::map<std::string, std::string> names;
    for (const auto& s : semantics.symbols.entries()) names[s.name] = s.name;
    LinkedWorkflow workflow; workflow.name = form.name; workflow.start_alias = form.name;
    LinkedModule module; module.alias = form.name;
    for (const auto* d : form.declarations) {
        if (d->kind != DeclarationKind::Field) continue;
        const auto& f = *d->field;
        if (f.name != target.name && std::find(references.begin(), references.end(), f.name) == references.end()) continue;
        LinkedField linked; linked.qualified_name = f.name; linked.type = f.type; linked.options = f.options;
        // Other fields contribute scalar domains, not unrelated rules.
        if (f.name == target.name) for (const auto* p : f.properties) {
            if (p->kind == PropertyKind::Required) combine(linked.requiredness, {}, "or");
            else if (p->kind == PropertyKind::RequiredWhen)
                combine(linked.requiredness, lower_expression(p->condition, semantics, names), "or");
            else if (p->kind == PropertyKind::ShowWhen)
                combine(linked.visibility, lower_expression(p->condition, semantics, names), "and");
        }
        module.fields.push_back(std::move(linked));
    }
    workflow.modules.push_back(std::move(module));
    const auto partitions = derive_partitions(workflow);
    if (!partitions.complete()) {
        for (const auto& d : partitions.diagnostics)
            add_diagnostic(result, d.code == "CERT001" ? "ANL900" : d.code,
                "cannot prove hidden-required safety for field '" + target.name + "': " + d.message, target.span);
        return;
    }
    const auto cases = enumerate_route_cases(workflow, partitions);
    if (!cases.complete()) {
        for (const auto& d : cases.diagnostics) add_diagnostic(result, d.code, d.message, target.span);
        return;
    }
    const auto& fields = workflow.modules[0].fields;
    const auto& linked_target = *std::find_if(fields.begin(), fields.end(),
        [&](const auto& f) { return f.qualified_name == target.name; });
    for (const auto& c : cases.cases) {
        if (c.active_module.empty()) continue;
        RuntimeValues values; bool valid = true;
        for (const auto& f : fields) {
            const auto supplied = c.values.find(f.qualified_name); std::vector<Diagnostic> ignored;
            values[f.qualified_name] = convert_value(f, supplied == c.values.end() ? nullptr : &supplied->second, ignored);
            if (f.qualified_name == target.name) continue;
            valid = valid && values.at(f.qualified_name).valid;
            // Preserve declared-option Boolean/choice witnesses.
            if (f.type == FieldType::Choice && !values.at(f.qualified_name).present) valid = false;
        }
        bool required = false, visible = false;
        if (!valid || !evaluate_expression(linked_target.requiredness, values, required) ||
            !evaluate_expression(linked_target.visibility, values, visible) || !required || visible) continue;
        const auto step = evaluate_workflow_step(workflow, c.values, form.name);
        const auto observed = std::find_if(step.observation.fields.begin(), step.observation.fields.end(),
            [&](const auto& f) { return f.field == target.name; });
        if (observed == step.observation.fields.end() || observed->visible) continue;
        std::ostringstream witness;
        for (const auto& symbol : semantics.symbols.entries()) {
            if (std::find(references.begin(), references.end(), symbol.name) == references.end()) continue;
            const auto& value = values.at(symbol.name);
            witness << symbol.name << '=';
            if (symbol.type == FieldType::Boolean) witness << (value.boolean ? "true" : "false");
            else if (value.present) witness << value.text;
            else if (symbol.type == FieldType::Integer || symbol.type == FieldType::Decimal) witness << '0';
            witness << ", ";
        }
        witness << target.name << ".visible=false, " << target.name << ".required=true";
        add_diagnostic(result, "ANL003", "field '" + target.name +
            "' can be required while hidden; witness: " + witness.str(), target.span);
        return;
    }
}

void detect_cycles(const SemanticResult& semantics, AnalysisResult& result) {
    std::unordered_map<std::string, std::vector<std::string>> adjacency;
    std::unordered_map<std::string, SourceSpan> edge_spans;
    for (const Dependency& dependency : result.dependencies) {
        if (dependency.source_field == dependency.target_field) {
            continue;
        }
        std::vector<std::string>& targets = adjacency[dependency.source_field];
        if (std::find(targets.begin(), targets.end(), dependency.target_field) ==
            targets.end()) {
            targets.push_back(dependency.target_field);
            edge_spans[dependency.source_field + "\n" + dependency.target_field] =
                dependency.span;
        }
    }

    std::unordered_map<std::string, int> state;
    std::vector<std::string> path;
    std::set<std::string> reported_cycles;
    // A back edge to a node on the active DFS path identifies a real cycle.
    std::function<void(const std::string&)> visit = [&](const std::string& source) {
        state[source] = 1;
        path.push_back(source);
        for (const std::string& target : adjacency[source]) {
            if (state[target] == 0) {
                visit(target);
            } else if (state[target] == 1) {
                const auto beginning = std::find(path.begin(), path.end(), target);
                std::vector<std::string> cycle(beginning, path.end());
                cycle.push_back(target);

                std::vector<std::string> members(cycle.begin(), cycle.end() - 1);
                std::sort(members.begin(), members.end());
                std::ostringstream key;
                for (const std::string& member : members) {
                    key << member << '\n';
                }
                if (reported_cycles.insert(key.str()).second) {
                    std::ostringstream message;
                    message << "dependency cycle detected: ";
                    for (std::size_t index = 0; index < cycle.size(); ++index) {
                        if (index != 0) {
                            message << " -> ";
                        }
                        message << cycle[index];
                    }
                    add_diagnostic(
                        result,
                        "ANL002",
                        message.str(),
                        edge_spans[source + "\n" + target]);
                }
            }
        }
        path.pop_back();
        state[source] = 2;
    };

    for (const Symbol& symbol : semantics.symbols.entries()) {
        if (state[symbol.name] == 0) {
            visit(symbol.name);
        }
    }
}

}  // namespace

AnalysisResult analyze_dependencies(const FormAst& form,
                                    const SemanticResult& semantics) {
    AnalysisResult result;
    for (const Declaration* declaration : form.declarations) {
        if (declaration->kind != DeclarationKind::Field) {
            continue;
        }

        const FieldDecl& field = *declaration->field;
        for (const FieldProperty* property : field.properties) {
            DependencyKind kind;
            if (property->kind == PropertyKind::ShowWhen) {
                kind = DependencyKind::Visibility;
            } else if (property->kind == PropertyKind::RequiredWhen) {
                kind = DependencyKind::Requirement;
            } else {
                continue;
            }

            std::vector<std::string> references;
            collect_references(property->condition, semantics, references);
            for (const std::string& reference : references) {
                result.dependencies.push_back(
                    {reference, field.name, kind, property->span});
                if (reference == field.name) {
                    add_diagnostic(
                        result,
                        "ANL001",
                        "field '" + field.name +
                            "' has a self-dependency in its " +
                            dependency_kind_name(kind) + " rule",
                        property->span);
                }
            }
        }
        analyze_hidden_required(form, field, semantics, result);
    }

    detect_cycles(semantics, result);
    return result;
}

std::string format_dependencies(const AnalysisResult& analysis) {
    std::ostringstream output;
    output << std::left << std::setw(18) << "TARGET FIELD"
           << std::setw(18) << "DEPENDS ON"
           << std::setw(18) << "RULE"
           << "LOCATION\n";
    output << std::string(68, '-') << '\n';
    if (analysis.dependencies.empty()) {
        output << "(no conditional field dependencies)\n";
        return output.str();
    }

    for (const Dependency& dependency : analysis.dependencies) {
        const std::string location = std::to_string(dependency.span.first_line) + ':' +
                                     std::to_string(dependency.span.first_column);
        output << std::left << std::setw(18) << dependency.target_field
               << std::setw(18) << dependency.source_field
               << std::setw(18) << dependency_kind_name(dependency.kind)
               << location << '\n';
    }
    return output.str();
}

std::string format_dependency_graph_dot(const FormAst& form,
                                        const AnalysisResult& analysis) {
    std::ostringstream output;
    output << "digraph \"" << form.name << "\" {\n"
           << "  rankdir=LR;\n";
    for (const Declaration* declaration : form.declarations) {
        if (declaration->kind == DeclarationKind::Field) {
            output << "  \"" << declaration->field->name << "\";\n";
        }
    }
    for (const Dependency& dependency : analysis.dependencies) {
        output << "  \"" << dependency.source_field << "\" -> \""
               << dependency.target_field << "\" [label=\""
               << dependency_kind_name(dependency.kind) << "\"];\n";
    }
    output << "}\n";
    return output.str();
}

}  // namespace vista
