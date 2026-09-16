#!/usr/bin/env bash
set -u

binary="./build/vista"
output_root="build/test-templates"
failures=0

rm -rf -- "$output_root"

template_list="$("$binary" --list-templates 2>&1)"
template_list_status=$?
if [[ $template_list_status -ne 0 ]]; then
    echo "FAIL: --list-templates returned $template_list_status"
    echo "$template_list"
    failures=$((failures + 1))
fi
for template_id in scholarship train-booking event-registration; do
    if ! grep -Fq "$template_id" <<<"$template_list"; then
        echo "FAIL: template list omitted '$template_id'"
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

check_template() {
    local template_id="$1"
    local name="$2"
    local title="$3"
    local marker="$4"
    local directory="$output_root/$name"
    local output
    local status

    output="$("$binary" --template "$template_id" --emit all --out-dir "$directory" 2>&1)"
    status=$?
    if [[ $status -ne 0 ]]; then
        echo "FAIL: template '$template_id' returned $status"
        echo "$output"
        failures=$((failures + 1))
    fi

    for artifact in tokens.txt ast.txt symbols.txt dependencies.txt graph.dot diagnostics.txt form.html; do
        if [[ ! -s "$directory/$artifact" ]]; then
            echo "FAIL: template '$template_id' omitted $artifact"
            failures=$((failures + 1))
        fi
    done

    expect_contains "$directory/diagnostics.txt" "No diagnostics." "$name clean diagnostics"
    expect_contains "$directory/form.html" "<title>$title</title>" "$name rendered title"
    expect_contains "$directory/form.html" "$marker" "$name domain-specific form behavior"
}

check_template scholarship scholarship "Scholarship Application" 'categoryCertificate'
check_template train-booking train-booking "Train Ticket Booking" 'Departure and destination must be different'
check_template event-registration event-registration "Event Registration" 'accessibilitySupport'

unknown_output="$("$binary" --template unknown --emit all --out-dir "$output_root/unknown" 2>&1)"
unknown_status=$?
if [[ $unknown_status -ne 2 ]] || ! grep -Fq "unknown template ID 'unknown'" <<<"$unknown_output"; then
    echo "FAIL: unknown template did not return CLI usage status 2"
    echo "$unknown_output"
    failures=$((failures + 1))
fi

if [[ $failures -ne 0 ]]; then
    echo "$failures template test(s) failed."
    exit 1
fi

echo "Phase 2 starter-template tests passed."
