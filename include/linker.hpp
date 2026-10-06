#ifndef VISTA_LINKER_HPP
#define VISTA_LINKER_HPP

#include <filesystem>
#include <map>
#include "workflow.hpp"

namespace vista {

struct LinkResult {
    LinkedWorkflow workflow;
    std::vector<Diagnostic> diagnostics;
    bool succeeded() const { return diagnostics.empty(); }
};

LinkResult link_workflow(const WorkflowAst& workflow,
                         const std::filesystem::path& workflow_path);
std::string format_linked_workflow(const LinkedWorkflow& workflow);
IrExpression lower_expression(const Expression*, const SemanticResult&,
                              const std::map<std::string, std::string>& qualified_names);

}  // namespace vista
#endif
