#ifndef VISTA_CODEGEN_HPP
#define VISTA_CODEGEN_HPP

#include <string>
#include <string_view>

#include "analysis.hpp"
#include "ast.hpp"
#include "semantic.hpp"
#include "workflow.hpp"

namespace vista {

std::string generate_html(const FormAst& form,
                          const SemanticResult& semantics,
                          const AnalysisResult& analysis);

std::string generate_workflow_javascript(const LinkedWorkflow&);
std::string generate_workflow_html(const LinkedWorkflow&, std::string_view generated_javascript);

}  // namespace vista

#endif
