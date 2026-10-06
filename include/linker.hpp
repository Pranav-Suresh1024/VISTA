#ifndef VISTA_LINKER_HPP
#define VISTA_LINKER_HPP

#include <filesystem>
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

}  // namespace vista
#endif
