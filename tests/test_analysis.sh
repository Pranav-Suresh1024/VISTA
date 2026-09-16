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

valid_output="$($binary examples/valid_scholarship.vista --emit dependencies 2>&1)"
valid_status=$?
if [[ $valid_status -ne 0 ]]; then
    echo "FAIL: valid dependency input returned $valid_status"
    echo "$valid_output"
    failures=$((failures + 1))
fi
expect_contains "$valid_output" "certificate       category          show when" "visibility dependency"
expect_contains "$valid_output" "certificate       category          required when" "requirement dependency"

graph_output="$($binary examples/valid_scholarship.vista --emit graph 2>&1)"
graph_status=$?
if [[ $graph_status -ne 0 ]]; then
    echo "FAIL: valid graph input returned $graph_status"
    failures=$((failures + 1))
fi
expect_contains "$graph_output" '"category" -> "certificate" [label="show when"]' "DOT visibility edge"

diagnostics_output="$($binary examples/valid_scholarship.vista --emit diagnostics 2>&1)"
diagnostics_status=$?
if [[ $diagnostics_status -ne 0 ]]; then
    echo "FAIL: valid analysis input returned $diagnostics_status"
    failures=$((failures + 1))
fi
expect_contains "$diagnostics_output" "No diagnostics." "safe form result"

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

check_failure examples/self_dependency.vista ANL001 "field 'consent' has a self-dependency"
check_failure examples/dependency_cycle.vista ANL002 "dependency cycle detected"
check_failure examples/hidden_required.vista ANL003 "category=general, certificate.visible=false, certificate.required=true"
check_failure examples/boolean_hidden_required.vista ANL003 "employed=false, employer.visible=false, employer.required=true"
check_failure examples/numeric_analysis_limit.vista ANL900 "outside Boolean/choice witness analysis"

collision_dependencies="$($binary examples/choice_field_collision.vista --emit dependencies 2>&1)"
collision_status=$?
if [[ $collision_status -ne 0 ]]; then
    echo "FAIL: choice/field collision dependency analysis returned $collision_status"
    echo "$collision_dependencies"
    failures=$((failures + 1))
fi
expect_contains "$collision_dependencies" "program_details   program" "choice literal collision dependency"
if grep -Eq 'program_details +other' <<<"$collision_dependencies"; then
    echo "FAIL: choice literal was incorrectly treated as a field dependency"
    failures=$((failures + 1))
fi

if [[ $failures -ne 0 ]]; then
    echo "$failures Stage 5 test(s) failed."
    exit 1
fi

echo "Stage 5 dependency and witness-analysis tests passed."
