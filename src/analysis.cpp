#include "analysis.hpp"

#include <algorithm>
#include <cstddef>
#include <functional>
#include <iomanip>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace vista {
namespace {

using Assignment = std::unordered_map<std::string, std::string>;

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
                        const SymbolTable& symbols,
                        std::vector<std::string>& references) {
    if (expression == nullptr) {
        return;
    }
    if (expression->kind == ExpressionKind::Name &&
        symbols.find(expression->value) != nullptr &&
        std::find(references.begin(), references.end(), expression->value) ==
            references.end()) {
        references.push_back(expression->value);
    }
    collect_references(expression->left, symbols, references);
    collect_references(expression->right, symbols, references);
}

bool is_finite_field(const Symbol* symbol) {
    return symbol != nullptr &&
           (symbol->type == FieldType::Boolean || symbol->type == FieldType::Choice);
}

bool expression_is_finite(const Expression* expression,
                          const SemanticResult& semantics) {
    if (expression == nullptr) {
        return false;
    }

    switch (expression->kind) {
        case ExpressionKind::BooleanLiteral:
            return true;
        case ExpressionKind::Name: {
            const auto information = semantics.expressions.find(expression);
            if (information != semantics.expressions.end() &&
                information->second.choice_literal) {
                return true;
            }
            return is_finite_field(semantics.symbols.find(expression->value));
        }
        case ExpressionKind::Unary:
            return expression->value == "not" &&
                   expression_is_finite(expression->left, semantics);
        case ExpressionKind::Binary:
            if (expression->value == "and" || expression->value == "or" ||
                expression->value == "==" || expression->value == "!=") {
                return expression_is_finite(expression->left, semantics) &&
                       expression_is_finite(expression->right, semantics);
            }
            return false;
        case ExpressionKind::StringLiteral:
        case ExpressionKind::IntegerLiteral:
        case ExpressionKind::DecimalLiteral:
            return false;
    }
    return false;
}

std::optional<bool> evaluate_condition(const Expression* expression,
                                       const SemanticResult& semantics,
                                       const Assignment& assignment);

std::optional<std::string> evaluate_scalar(const Expression* expression,
                                           const SemanticResult& semantics,
                                           const Assignment& assignment) {
    if (expression->kind == ExpressionKind::BooleanLiteral) {
        return expression->value;
    }
    if (expression->kind != ExpressionKind::Name) {
        const auto information = semantics.expressions.find(expression);
        if (information != semantics.expressions.end() &&
            information->second.type == ValueType::Boolean) {
            const std::optional<bool> value =
                evaluate_condition(expression, semantics, assignment);
            if (value.has_value()) {
                return *value ? "true" : "false";
            }
        }
        return std::nullopt;
    }

    const auto information = semantics.expressions.find(expression);
    if (information != semantics.expressions.end() && information->second.choice_literal) {
        return expression->value;
    }

    const auto value = assignment.find(expression->value);
    if (value == assignment.end()) {
        return std::nullopt;
    }
    return value->second;
}

std::optional<bool> evaluate_condition(const Expression* expression,
                                       const SemanticResult& semantics,
                                       const Assignment& assignment) {
    if (expression->kind == ExpressionKind::BooleanLiteral) {
        return expression->value == "true";
    }
    if (expression->kind == ExpressionKind::Name) {
        const std::optional<std::string> value =
            evaluate_scalar(expression, semantics, assignment);
        if (!value.has_value() || (*value != "true" && *value != "false")) {
            return std::nullopt;
        }
        return *value == "true";
    }
    if (expression->kind == ExpressionKind::Unary) {
        const std::optional<bool> operand =
            evaluate_condition(expression->left, semantics, assignment);
        if (!operand.has_value()) {
            return std::nullopt;
        }
        return !*operand;
    }
    if (expression->kind != ExpressionKind::Binary) {
        return std::nullopt;
    }

    if (expression->value == "and" || expression->value == "or") {
        const std::optional<bool> left =
            evaluate_condition(expression->left, semantics, assignment);
        const std::optional<bool> right =
            evaluate_condition(expression->right, semantics, assignment);
        if (!left.has_value() || !right.has_value()) {
            return std::nullopt;
        }
        return expression->value == "and" ? *left && *right : *left || *right;
    }

    const std::optional<std::string> left =
        evaluate_scalar(expression->left, semantics, assignment);
    const std::optional<std::string> right =
        evaluate_scalar(expression->right, semantics, assignment);
    if (!left.has_value() || !right.has_value()) {
        return std::nullopt;
    }
    return expression->value == "==" ? *left == *right : *left != *right;
}

std::string witness_text(const std::vector<const Symbol*>& variables,
                         const Assignment& assignment,
                         const std::string& field_name) {
    std::ostringstream output;
    bool first = true;
    for (const Symbol* variable : variables) {
        if (!first) {
            output << ", ";
        }
        first = false;
        output << variable->name << '=' << assignment.at(variable->name);
    }
    if (!first) {
        output << ", ";
    }
    output << field_name << ".visible=false, "
           << field_name << ".required=true";
    return output.str();
}

void analyze_hidden_required(const FieldDecl& field,
                             const SemanticResult& semantics,
                             AnalysisResult& result) {
    std::vector<const Expression*> show_conditions;
    std::vector<const Expression*> required_conditions;
    bool always_required = false;

    for (const FieldProperty* property : field.properties) {
        if (property->kind == PropertyKind::ShowWhen) {
            show_conditions.push_back(property->condition);
        } else if (property->kind == PropertyKind::Required) {
            always_required = true;
        } else if (property->kind == PropertyKind::RequiredWhen) {
            required_conditions.push_back(property->condition);
        }
    }

    if (show_conditions.empty() || (!always_required && required_conditions.empty())) {
        return;
    }

    std::vector<const Expression*> conditions = show_conditions;
    conditions.insert(
        conditions.end(), required_conditions.begin(), required_conditions.end());
    if (std::any_of(conditions.begin(), conditions.end(), [&](const Expression* condition) {
            return !expression_is_finite(condition, semantics);
        })) {
        add_diagnostic(
            result,
            "ANL900",
            "cannot prove hidden-required safety for field '" + field.name +
                "' because its conditions are outside Boolean/choice witness analysis",
            field.span);
        return;
    }

    std::unordered_set<std::string> used_names;
    for (const Expression* condition : conditions) {
        std::vector<std::string> references;
        collect_references(condition, semantics.symbols, references);
        used_names.insert(references.begin(), references.end());
    }

    std::vector<const Symbol*> variables;
    std::size_t assignment_count = 1;
    for (const Symbol& symbol : semantics.symbols.entries()) {
        if (used_names.find(symbol.name) == used_names.end()) {
            continue;
        }
        variables.push_back(&symbol);
        const std::size_t domain_size =
            symbol.type == FieldType::Boolean ? 2U : symbol.options.size();
        if (domain_size == 0 || assignment_count > 65536U / domain_size) {
            add_diagnostic(
                result,
                "ANL900",
                "cannot search hidden-required states for field '" + field.name +
                    "' because its finite state space exceeds 65536 assignments",
                field.span);
            return;
        }
        assignment_count *= domain_size;
    }

    Assignment assignment;
    std::optional<std::string> witness;
    // Enumerate the small, finite domains until required && !visible is found.
    std::function<void(std::size_t)> search = [&](std::size_t index) {
        if (witness.has_value()) {
            return;
        }
        if (index != variables.size()) {
            const Symbol* variable = variables[index];
            if (variable->type == FieldType::Boolean) {
                for (const std::string value : {"false", "true"}) {
                    assignment[variable->name] = value;
                    search(index + 1);
                }
            } else {
                for (const std::string& option : variable->options) {
                    assignment[variable->name] = option;
                    search(index + 1);
                }
            }
            return;
        }

        // Multiple show rules are conjunctive; any false rule hides the field.
        bool visible = true;
        for (const Expression* condition : show_conditions) {
            const std::optional<bool> value =
                evaluate_condition(condition, semantics, assignment);
            visible = visible && value.value_or(false);
        }

        // An unconditional rule or any matching conditional rule requires it.
        bool required = always_required;
        for (const Expression* condition : required_conditions) {
            const std::optional<bool> value =
                evaluate_condition(condition, semantics, assignment);
            required = required || value.value_or(false);
        }
        if (required && !visible) {
            witness = witness_text(variables, assignment, field.name);
        }
    };
    search(0);

    if (witness.has_value()) {
        add_diagnostic(
            result,
            "ANL003",
            "field '" + field.name +
                "' can be required while hidden; witness: " + *witness,
            field.span);
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
            collect_references(property->condition, semantics.symbols, references);
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
        analyze_hidden_required(field, semantics, result);
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
