#ifndef VISTA_EVALUATOR_HPP
#define VISTA_EVALUATOR_HPP

#include <map>
#include "workflow.hpp"

namespace vista {

using InputValues = std::map<std::string, std::string>;
struct InputLoadResult {
    InputValues values;
    std::vector<Diagnostic> diagnostics;
    bool data_opened = false;
    bool operational_failure = false;
    std::map<std::string, std::size_t> lines;
    std::vector<std::string> supplied_order;
    bool succeeded() const { return data_opened && !operational_failure && diagnostics.empty(); }
};

// value = sign * digits * 10^(-scale). Zero has sign 0, digits "0", scale 0.
// At most 4096 input bytes and absolute exponent/scale 100000; no exponent expansion.
struct CanonicalDecimal {
    int sign = 0;
    std::string digits = "0";
    int scale = 0;
};
bool parse_decimal(const std::string&, CanonicalDecimal&);
bool parse_integer(const std::string&, CanonicalDecimal&);
int compare_decimal(const CanonicalDecimal&, const CanonicalDecimal&);
bool valid_iso_date(const std::string&);

struct RuntimeValue {
    FieldType type = FieldType::Text;
    std::string text;
    CanonicalDecimal number;
    bool boolean = false, present = false, valid = true;
};
using RuntimeValues = std::map<std::string, RuntimeValue>;
RuntimeValue convert_value(const LinkedField&, const std::string*,
                           std::vector<Diagnostic>&, std::size_t line = 0);
void validate_constraints(const LinkedField&, const RuntimeValue&,
                          std::vector<Diagnostic>&, std::size_t line = 0);
bool evaluate_expression(const IrExpression&, const RuntimeValues&, bool&);

struct FieldObservation {
    std::string field;
    bool visible = false, required = false, valid = true;
};
struct DiagnosticObservation {
    std::string code, field, rule_id;
};
struct EvaluationObservation {
    std::string active_module;
    std::vector<FieldObservation> fields;
    std::string selected_route;
    bool complete = false;
    std::vector<std::string> diagnostic_codes, rule_ids;
    std::vector<DiagnosticObservation> diagnostic_details;
    std::vector<std::string> transition_trace;
};
struct EvaluationResult {
    EvaluationObservation observation;
    std::vector<Diagnostic> diagnostics;
    bool succeeded() const { return diagnostics.empty(); }
};

InputLoadResult load_input_values(const std::string& path, bool qualified_names = true);
// One state only: selected_route is the enabled outgoing transition; complete
// means terminal workflow success, not merely that this module passed validation.
EvaluationResult evaluate_workflow_step(const LinkedWorkflow&, const InputValues&,
                                        const std::string& active_module);
// Final state's fields; last selected transition survives destination failure.
EvaluationResult evaluate_workflow(const LinkedWorkflow&, const InputValues&);
std::string format_observation(const EvaluationObservation&);
std::string canonical_observation(const EvaluationObservation&);

}  // namespace vista
#endif
