#!/usr/bin/env bash
set -u

binary="./build/vista"
failures=0

expect_contains() {
    local text="$1"
    local expected="$2"
    local label="$3"

    if ! grep -Fq "$expected" <<<"$text"; then
        echo "FAIL: $label"
        echo "  Expected to find: $expected"
        failures=$((failures + 1))
    fi
}

symbols_output="$($binary examples/valid_scholarship.vista --emit symbols 2>&1)"
symbols_status=$?
if [[ $symbols_status -ne 0 ]]; then
    echo "FAIL: valid symbol-table input returned $symbols_status"
    echo "$symbols_output"
    failures=$((failures + 1))
fi
expect_contains "$symbols_output" "NAME" "symbol-table header"
expect_contains "$symbols_output" "name            text" "text symbol"
expect_contains "$symbols_output" "category        choice      general,reserved" "choice symbol and options"
expect_contains "$symbols_output" '"Applicant category"' "symbol label"

diagnostics_output="$($binary examples/valid_scholarship.vista --emit diagnostics 2>&1)"
diagnostics_status=$?
if [[ $diagnostics_status -ne 0 ]]; then
    echo "FAIL: valid diagnostic input returned $diagnostics_status"
    failures=$((failures + 1))
fi
expect_contains "$diagnostics_output" "No diagnostics." "clean diagnostic result"

typed_output="$($binary examples/typed_fields.vista --emit diagnostics 2>&1)"
typed_status=$?
if [[ $typed_status -ne 0 ]]; then
    echo "FAIL: valid typed fields and constraints returned $typed_status"
    echo "$typed_output"
    failures=$((failures + 1))
fi

check_failure() {
    local file="$1"
    local code="$2"
    local message="$3"

    local output
    output="$($binary "$file" --emit diagnostics 2>&1)"
    local status=$?
    if [[ $status -ne 1 ]]; then
        echo "FAIL: $file returned $status instead of 1"
        failures=$((failures + 1))
    fi
    expect_contains "$output" "$code" "$file diagnostic code"
    expect_contains "$output" "$message" "$file diagnostic message"
}

check_failure examples/duplicate_field.vista SEM001 "duplicate field 'name'"
check_failure examples/undefined_reference.vista SEM002 "undefined field or value 'category'"
check_failure examples/type_mismatch.vista SEM003 "cannot compare integer and text"
check_failure examples/invalid_choice.vista SEM005 "is not an option of choice field 'category'"
check_failure examples/missing_label.vista SEM006 "requires a nonempty label"
check_failure examples/non_boolean_condition.vista SEM004 "visibility condition must be boolean"
check_failure examples/invalid_numeric_constraint_type.vista SEM007 "require an integer or decimal field"
check_failure examples/inverted_numeric_constraints.vista SEM008 "minimum value must not exceed maximum value"
check_failure examples/invalid_length_constraint_type.vista SEM009 "length constraints require a textarea field"
check_failure examples/inverted_length_constraints.vista SEM011 "minimum length must not exceed maximum length"
check_failure examples/decimal_integer_bound.vista SEM013 "integer field bounds must use whole-number values"
check_failure examples/duplicate_numeric_constraint.vista SEM012 "declares the same constraint more than once"
check_failure examples/decimal_text_length.vista SEM010 "length constraints must be whole numbers"

if [[ $failures -ne 0 ]]; then
    echo "$failures Stage 4 test(s) failed."
    exit 1
fi

echo "Stage 4 symbol-table and semantic tests passed."
