#include "evaluator.hpp"
#include <algorithm>
#include <cassert>
#include <iostream>

using namespace vista;

int main() {
    CanonicalDecimal a, b;
    assert(parse_decimal("-001.2500e+2", a));
    assert(a.sign == -1 && a.digits == "125" && a.scale == 0);
    assert(parse_decimal("-125", b) && compare_decimal(a, b) == 0);
    assert(parse_decimal("-0.000e-20", a));
    assert(a.sign == 0 && a.digits == "0" && a.scale == 0);
    assert(parse_decimal("100000000000000000001", a));
    assert(parse_decimal("100000000000000000000", b));
    assert(compare_decimal(a, b) == 1);
    assert(parse_decimal("-100000000000000000001", a));
    assert(parse_decimal("-100000000000000000000", b));
    assert(compare_decimal(a, b) == -1);
    assert(parse_decimal("1e100000", a) && a.digits == "1" && a.scale == -100000);
    assert(parse_decimal("1e-100000", b) && compare_decimal(a, b) > 0);
    for (const auto& text : {"1e100001", "1e-100001", "1e9999999999999999999999",
                             "10e100000", "NaN", "inf", "0x1p2", "1e", ".", "--1", "1 2"})
        assert(!parse_decimal(text, a));
    assert(!parse_decimal(std::string(4097, '1'), a));
    assert(parse_integer("-00042", a) && a.sign == -1 && a.digits == "42");
    for (const auto& text : {"+42", "4.2", "42e0", "-"}) assert(!parse_integer(text, a));
    assert(valid_iso_date("2000-02-29"));
    assert(!valid_iso_date("1900-02-29") && !valid_iso_date("0000-01-01"));

    std::vector<Diagnostic> diagnostics;
    LinkedField scalar;
    scalar.qualified_name = "S.value";
    struct Case { FieldType type; const char* valid; const char* invalid; };
    for (const auto& test : {Case{FieldType::Integer, "-42", "42.0"},
                            Case{FieldType::Decimal, "+.5e1", "NaN"},
                            Case{FieldType::Boolean, "false", "False"},
                            Case{FieldType::Date, "2024-02-29", "2023-02-29"},
                            Case{FieldType::Email, "a@b.c", "a@b"},
                            Case{FieldType::Phone, "+91 1234567", "123"},
                            Case{FieldType::Choice, "yes", "other"}}) {
        scalar.type = test.type;
        scalar.options = {"yes", "no"};
        std::string text = test.valid;
        diagnostics.clear();
        assert(convert_value(scalar, &text, diagnostics).valid && diagnostics.empty());
        text = test.invalid;
        assert(!convert_value(scalar, &text, diagnostics).valid && diagnostics.size() == 1);
    }
    scalar.type = FieldType::File;
    IrExpression presence{ExpressionKind::Unary, ValueType::Boolean, "present",
        {{ExpressionKind::Name, ValueType::File, "S.value", {}, false}}, false};
    bool present = false;
    for (const auto& text : {std::string("false"), std::string("0"), std::string(" ")}) {
        diagnostics.clear();
        assert(evaluate_expression(presence, {{"S.value", convert_value(scalar, &text, diagnostics)}}, present));
        assert(present);
    }
    const std::string empty;
    assert(evaluate_expression(presence, {{"S.value", convert_value(scalar, &empty, diagnostics)}}, present));
    assert(!present);
    assert(evaluate_expression(presence, {{"S.value", convert_value(scalar, nullptr, diagnostics)}}, present));
    assert(!present);
    scalar.type = FieldType::Text;
    scalar.constraints = {{PropertyKind::MinLength, "2", "S.value.minlength"},
                          {PropertyKind::MaxLength, "2", "S.value.maxlength"}};
    const std::string utf8 = "é";
    diagnostics.clear();
    validate_constraints(scalar, convert_value(scalar, &utf8, diagnostics), diagnostics);
    assert(diagnostics.empty());

    LinkedField field;
    field.qualified_name = "A.amount";
    field.type = FieldType::Decimal;
    field.visibility_rule_id = "A.amount.visible";
    field.requiredness_rule_id = "A.amount.required";
    LinkedModule module;
    module.alias = "A";
    module.entry_rule_id = "A.entry";
    module.fields = {field};
    LinkedWorkflow workflow;
    workflow.start_alias = "A";
    workflow.modules = {module};
    auto invalid = evaluate_workflow(workflow, {{"A.amount", "NaN"}});
    assert(!invalid.succeeded() && !invalid.observation.complete);
    assert(invalid.observation.diagnostic_details.size() == 1);
    const auto& detail = invalid.observation.diagnostic_details[0];
    assert(detail.code == "VAL005" && detail.field == "A.amount");
    // A diagnostic's rule must survive canonical reporting as an observed rule.
    assert(std::find(invalid.observation.rule_ids.begin(), invalid.observation.rule_ids.end(),
                     detail.rule_id) != invalid.observation.rule_ids.end());

    auto observation = invalid.observation;
    const auto canonical = canonical_observation(observation);
    observation.diagnostic_details[0].field = "A.other";
    assert(canonical != canonical_observation(observation));
    observation = invalid.observation;
    observation.transition_trace = {"A->B", "B->A"};
    assert(canonical != canonical_observation(observation));
    const auto traced = canonical_observation(observation);
    std::reverse(observation.transition_trace.begin(), observation.transition_trace.end());
    assert(traced != canonical_observation(observation));
    observation = invalid.observation;
    observation.rule_ids = {"b", "a"};
    const auto sorted = canonical_observation(observation);
    std::reverse(observation.rule_ids.begin(), observation.rule_ids.end());
    assert(sorted == canonical_observation(observation));
    observation.diagnostic_codes.push_back("VAL009");
    assert(sorted != canonical_observation(observation));

    observation = invalid.observation;
    observation.fields.push_back({"A.other", false, true, false});
    observation.diagnostic_details.push_back({"VAL009", "A.other", "A.check"});
    observation.diagnostic_codes.push_back("VAL009");
    const auto unordered = canonical_observation(observation);
    std::reverse(observation.fields.begin(), observation.fields.end());
    std::reverse(observation.diagnostic_details.begin(), observation.diagnostic_details.end());
    std::reverse(observation.diagnostic_codes.begin(), observation.diagnostic_codes.end());
    assert(unordered == canonical_observation(observation));
    auto duplicate = observation;
    duplicate.diagnostic_codes.push_back(duplicate.diagnostic_codes.front());
    assert(unordered != canonical_observation(duplicate));
    duplicate = observation;
    duplicate.diagnostic_details.push_back(duplicate.diagnostic_details.front());
    assert(unordered != canonical_observation(duplicate));

    // Shifting separators between adjacent byte strings must not collide.
    EvaluationObservation bytes;
    bytes.diagnostic_details = {{std::string("x\0:\n", 4), "|y", "r\t"}};
    auto shifted = bytes;
    shifted.diagnostic_details = {{std::string("x\0:\n|", 5), "y", "r\t"}};
    assert(canonical_observation(bytes) != canonical_observation(shifted));
    bytes.fields = {{std::string("a\0:\n|", 5), true, false, true}};
    shifted = bytes;
    shifted.fields[0].field = std::string("a\0:\n", 4);
    assert(canonical_observation(bytes) != canonical_observation(shifted));
    const auto controls = canonical_observation(bytes);
    assert(controls.find(std::string("5:a\0:\n|", 7)) != std::string::npos);

    LinkedModule destination;
    destination.alias = "B";
    destination.entry_rule_id = "B.entry";
    LinkedField upload;
    upload.qualified_name = "B.upload";
    upload.type = FieldType::File;
    upload.visibility_rule_id = "B.upload.visible";
    upload.requiredness.value = "true";
    upload.requiredness_rule_id = "B.upload.required";
    destination.fields = {upload};
    workflow.modules.push_back(destination);
    workflow.routes.push_back({"A", "B", {}, "route.A.B"});
    const auto step = evaluate_workflow_step(workflow, {}, "A");
    assert(step.succeeded() && !step.observation.complete && step.observation.active_module == "A");
    assert(step.observation.selected_route == "A->B");
    const auto whole = evaluate_workflow(workflow, {});
    assert(!whole.succeeded() && whole.observation.active_module == "B");
    assert(whole.observation.selected_route == "A->B" && !whole.observation.complete);
    assert(whole.observation.transition_trace == std::vector<std::string>{"A->B"});
    workflow.routes[0].condition = {ExpressionKind::Binary, ValueType::Boolean, "==",
        {{ExpressionKind::Name, ValueType::Decimal, "A.amount", {}, false},
         {ExpressionKind::IntegerLiteral, ValueType::Integer, "0", {}, false}}, false};
    const auto private_route = evaluate_workflow_step(workflow, {}, "A");
    assert(!private_route.succeeded() && private_route.observation.diagnostic_codes[0] == "VAL011");
    workflow.modules[0].exports = {"A.amount"};
    assert(evaluate_workflow_step(workflow, {}, "A").observation.selected_route == "A->B");
    workflow.modules[0].entry = {ExpressionKind::Binary, ValueType::Boolean, "==",
        {{ExpressionKind::Name, ValueType::Decimal, "A.amount", {}, false},
         {ExpressionKind::IntegerLiteral, ValueType::Integer, "0", {}, false}}, false};
    const auto bad_entry = evaluate_workflow_step(workflow, {{"A.amount", "NaN"}}, "A");
    assert(!bad_entry.succeeded() && !bad_entry.observation.complete);
    assert(bad_entry.observation.diagnostic_codes[0] == "VAL010");
    std::cout << "Evaluator scalar and observation tests passed.\n";
}
