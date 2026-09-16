#!/usr/bin/env bash
set -u

binary="./build/vista"
failures=0

expect_contains() {
    local text="$1"
    local expected="$2"
    local label="$3"
    if ! grep -Fq -- "$expected" <<<"$text"; then
        echo "FAIL: $label"
        echo "  Expected to find: $expected"
        failures=$((failures + 1))
    fi
}

check_valid() {
    local template="$1"
    local data="$2"
    local output
    output="$($binary "$template" --validate-data "$data" 2>&1)"
    local status=$?
    if [[ $status -ne 0 ]]; then
        echo "FAIL: $data returned $status instead of 0"
        echo "$output"
        failures=$((failures + 1))
    fi
    expect_contains "$output" "VISTA DATA VALIDATION" "$data report heading"
    expect_contains "$output" "Result          : PASS" "$data pass result"
    expect_contains "$output" "No validation errors." "$data clean result"
}

check_invalid() {
    local template="$1"
    local data="$2"
    shift 2
    local output
    output="$($binary "$template" --validate-data "$data" 2>&1)"
    local status=$?
    if [[ $status -ne 1 ]]; then
        echo "FAIL: $data returned $status instead of 1"
        echo "$output"
        failures=$((failures + 1))
    fi
    expect_contains "$output" "Result          : FAIL" "$data fail result"
    for expected in "$@"; do
        expect_contains "$output" "$expected" "$data diagnostic: $expected"
    done
}

check_valid templates/scholarship_application.vista samples/scholarship-valid.data
check_valid templates/college_admission.vista samples/college-valid.data
check_valid templates/event_registration.vista samples/event-valid.data

scholarship_report="$($binary templates/scholarship_application.vista --validate-data samples/scholarship-valid.data 2>&1)"
expect_contains "$scholarship_report" "Rules skipped   : 1" "hidden-field check is skipped"

check_invalid templates/scholarship_application.vista samples/scholarship-invalid.data \
    "VAL005" "VAL006" "required field 'other_course'" \
    "required field 'previous_degree_proof'" "required field 'category_certificate'" \
    "course name when Other is selected" "household income of 800000 or less"
check_invalid templates/scholarship_application.vista samples/scholarship-ineligible.data \
    "GPA of at least 6.0" "household income of 800000 or less"
check_invalid templates/college_admission.vista samples/college-invalid.data \
    "required field 'other_program'" "required field 'lateral_entry_document'" \
    "final score of at least 50"
check_invalid templates/event_registration.vista samples/event-invalid.data \
    "required field 'guest_name'" "required field 'workshop_track'" \
    "required field 'online_timezone'" "at most 4"
check_invalid templates/scholarship_application.vista samples/data-format-invalid.data \
    "VAL001" "VAL002" "VAL003"

missing_output="$($binary templates/scholarship_application.vista --validate-data samples/missing.data 2>&1)"
missing_status=$?
if [[ $missing_status -ne 2 ]]; then
    echo "FAIL: missing data file returned $missing_status instead of 2"
    failures=$((failures + 1))
fi
expect_contains "$missing_output" "VAL000" "missing data-file diagnostic"

if [[ $failures -ne 0 ]]; then
    echo "$failures CLI data-validation test(s) failed."
    exit 1
fi

echo "CLI runtime data-validation tests passed."
