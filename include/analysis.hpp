#ifndef VISTA_ANALYSIS_HPP
#define VISTA_ANALYSIS_HPP

#include <string>
#include <vector>

#include "ast.hpp"
#include "diagnostic.hpp"
#include "semantic.hpp"

namespace vista {

enum class DependencyKind {
    Visibility,
    Requirement
};

struct Dependency {
    std::string source_field;
    std::string target_field;
    DependencyKind kind = DependencyKind::Visibility;
    SourceSpan span;
};

struct AnalysisResult {
    std::vector<Dependency> dependencies;
    std::vector<Diagnostic> diagnostics;

    bool succeeded() const {
        return diagnostics.empty();
    }
};

AnalysisResult analyze_dependencies(const FormAst& form,
                                    const SemanticResult& semantics);
std::string format_dependencies(const AnalysisResult& analysis);
std::string format_dependency_graph_dot(const FormAst& form,
                                        const AnalysisResult& analysis);

}  // namespace vista

#endif
