#ifndef VISTA_PARTITION_HPP
#define VISTA_PARTITION_HPP
#include "evaluator.hpp"

namespace vista {
enum class BehaviorClassKind { Missing, Present, Exact, Below, Between, Above, Other, Invalid };
struct BehaviorClass {
    std::string field, id, representative, description;
    BehaviorClassKind kind = BehaviorClassKind::Other;
};
struct FieldPartition { std::string field; std::vector<BehaviorClass> classes; };
// Assignments are coordinated: distinct ranks / equality witnesses share pools.
struct PartitionComponent {
    std::vector<std::string> fields;
    std::vector<InputValues> assignments;
};
struct PartitionResult {
    std::vector<FieldPartition> fields;
    std::vector<PartitionComponent> components;
    std::vector<Diagnostic> diagnostics;
    std::size_t generation_states = 0;
    std::size_t generation_bytes = 0;
    bool complete() const { return diagnostics.empty(); }
};
struct RouteCase {
    std::string id;
    InputValues values;
    std::vector<std::string> expected_route_prefix;
    // Empty selects whole-workflow evaluation; otherwise a reachable step.
    std::string active_module;
};
struct RouteSuite {
    std::vector<RouteCase> cases;
    std::vector<Diagnostic> diagnostics;
    bool complete() const { return diagnostics.empty(); }
};
PartitionResult derive_partitions(const LinkedWorkflow&);
RouteSuite enumerate_route_cases(const LinkedWorkflow&, const PartitionResult&);
std::string format_partitions(const PartitionResult&, const RouteSuite&);
}  // namespace vista
#endif
