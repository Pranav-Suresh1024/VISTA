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

expect_ast() {
    local source="$1"
    shift
    local output
    output="$($binary "$source" --emit ast 2>&1)"
    local status=$?
    if [[ $status -ne 0 ]]; then
        echo "FAIL: $source returned $status"
        echo "$output"
        failures=$((failures + 1))
    fi
    for expected in "$@"; do
        expect_contains "$output" "$expected" "$source: $expected"
    done
}

expect_failure() {
    local source="$1"
    local expected="$2"
    local output
    output="$($binary "$source" --emit ast 2>&1)"
    local status=$?
    if [[ $status -ne 1 ]]; then
        echo "FAIL: $source returned $status instead of 1"
        echo "$output"
        failures=$((failures + 1))
    fi
    expect_contains "$output" "$expected" "$source: $expected"
}

expect_ast examples/vista_link/applicant.vista \
    "Module Applicant" \
    "Export category"

expect_ast examples/vista_link/certificate.vista \
    "Import Applicant.category as applicant_category" \
    "EnterWhen" \
    "Unary present"

expect_ast examples/vista_link/scholarship.workflow.vista \
    "Workflow ScholarshipApplication" \
    'Use Applicant "applicant.vista"' \
    "Start Applicant" \
    "Route Applicant -> Certificate" \
    "Name Applicant.category"

expect_failure examples/vista_link/errors/duplicate_export.vista "MOD003"
expect_failure examples/vista_link/errors/malformed_route.vista "SYN001"

temp_dir="$(mktemp -d)"
trap 'rm -rf -- "$temp_dir"' EXIT
cat >"$temp_dir/duplicate-enter.vista" <<'EOF'
module DuplicateEnter {
  enter when true
  enter when false
}
EOF
cat >"$temp_dir/duplicate-start.vista" <<'EOF'
workflow DuplicateStart {
  use "applicant.vista" as Applicant
  start Applicant
  start Applicant
}
EOF
expect_failure "$temp_dir/duplicate-enter.vista" "SYN002"
expect_failure "$temp_dir/duplicate-start.vista" "SYN002"

baseline_ast="$($binary examples/valid_scholarship.vista --emit ast 2>&1)"
baseline_status=$?
if [[ $baseline_status -ne 0 ]]; then
    echo "FAIL: existing form returned $baseline_status"
    echo "$baseline_ast"
    failures=$((failures + 1))
fi
expect_contains "$baseline_ast" "Form ScholarshipApplication @2:1" "existing form root"
expect_contains "$baseline_ast" "Binary and @25:9" "existing form expression tree"
expect_contains "$baseline_ast" 'Message "Applicants must be at least 18 years old"' "existing form check"

if [[ $failures -ne 0 ]]; then
    echo "$failures VISTA-Link parser test(s) failed."
    exit 1
fi

echo "VISTA-Link parser and AST tests passed."
