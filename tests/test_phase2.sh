#!/usr/bin/env bash
set -u

binary="./build/vista"
output_dir="build/test-phase2-scholarship"
failures=0

if ! "$binary" templates/scholarship_application.vista --emit all --out-dir "$output_dir"; then
    echo "FAIL: Scholarship template did not compile"
    exit 1
fi

for artifact in tokens.txt ast.txt symbols.txt dependencies.txt graph.dot diagnostics.txt form.html; do
    if [[ ! -f "$output_dir/$artifact" ]]; then
        echo "FAIL: missing generated artifact $artifact"
        failures=$((failures + 1))
    fi
done

expect_contains() {
    local file="$1"
    local expected="$2"
    local label="$3"
    if [[ ! -f "$file" ]] || ! grep -Fq "$expected" "$file"; then
        echo "FAIL: $label"
        echo "  Expected to find: $expected"
        failures=$((failures + 1))
    fi
}

expect_contains "$output_dir/diagnostics.txt" "No diagnostics." "clean end-to-end diagnostics"
expect_contains "$output_dir/form.html" 'type="email" data-type="email"' "scholarship email field"
expect_contains "$output_dir/form.html" 'id="full_name" name="full_name" type="text" data-type="text"' "scholarship name field"
expect_contains "$output_dir/form.html" 'minlength="3" maxlength="100"' "normal text length checks"
expect_contains "$output_dir/form.html" 'type="tel" data-type="phone"' "scholarship phone field"
expect_contains "$output_dir/form.html" '<textarea id="personal_statement"' "scholarship statement textarea"
expect_contains "$output_dir/form.html" 'minlength="50" maxlength="1000"' "personal statement length checks"
expect_contains "$output_dir/form.html" 'id="gpa" name="gpa" type="number" data-type="decimal"' "GPA numeric control"
expect_contains "$output_dir/form.html" 'min="0" max="10"' "GPA lower and upper bounds"
expect_contains "$output_dir/form.html" 'id="household_income" name="household_income" type="number"' "household income control"
expect_contains "$output_dir/form.html" 'visible: () => ((readValue("applicant_category") === "reserved"))' "category-dependent certificate visibility"
expect_contains "$output_dir/form.html" 'required: () => ((readValue("applicant_category") === "reserved"))' "category-dependent certificate requirement"
expect_contains "$output_dir/form.html" 'id="declaration" name="declaration" type="checkbox"' "required declaration checkbox"
expect_contains "$output_dir/form.html" 'message: "GPA must be between 0 and 10"' "helpful GPA validation message"
expect_contains "$output_dir/form.html" 'message: "Confirm the declaration before continuing"' "helpful declaration validation message"

if [[ $failures -ne 0 ]]; then
    echo "$failures Scholarship template test(s) failed."
    exit 1
fi

echo "Scholarship template end-to-end tests passed."
