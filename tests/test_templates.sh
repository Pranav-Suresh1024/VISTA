#!/usr/bin/env bash
set -u

binary="./build/vista"
output_root="build/test-templates"
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

expect_file_contains() {
    local file="$1"
    local expected="$2"
    local label="$3"
    if [[ ! -f "$file" ]] || ! grep -Fq -- "$expected" "$file"; then
        echo "FAIL: $label"
        echo "  Expected to find: $expected"
        failures=$((failures + 1))
    fi
}

catalogue="$($binary --list-templates 2>&1)"
catalogue_status=$?
if [[ $catalogue_status -ne 0 ]]; then
    echo "FAIL: --list-templates returned $catalogue_status"
    echo "$catalogue"
    failures=$((failures + 1))
fi
expect_contains "$catalogue" "templates/scholarship_application.vista" "scholarship source path"
expect_contains "$catalogue" "templates/college_admission.vista" "college admission source path"
expect_contains "$catalogue" "templates/event_registration.vista" "event registration source path"
expect_contains "$catalogue" "--emit all --out-dir" "compile commands"
expect_contains "$catalogue" "do not submit or store forms" "catalogue limitation note"
if grep -Fiq "train" <<<"$catalogue"; then
    echo "FAIL: deferred Train Booking appeared in the starter catalogue"
    failures=$((failures + 1))
fi

$binary --list-templates extra >/dev/null 2>&1
extra_status=$?
if [[ $extra_status -ne 2 ]]; then
    echo "FAIL: --list-templates with an unexpected argument returned $extra_status"
    failures=$((failures + 1))
fi

compile_template() {
    local source="$1"
    local output_name="$2"
    local output_dir="$output_root/$output_name"
    local output
    output="$($binary "$source" --emit all --out-dir "$output_dir" 2>&1)"
    local status=$?
    if [[ $status -ne 0 ]]; then
        echo "FAIL: $source returned $status"
        echo "$output"
        failures=$((failures + 1))
        return
    fi
    for artifact in tokens.txt ast.txt symbols.txt dependencies.txt graph.dot diagnostics.txt form.html; do
        if [[ ! -s "$output_dir/$artifact" ]]; then
            echo "FAIL: $source omitted $artifact"
            failures=$((failures + 1))
        fi
    done
    expect_file_contains "$output_dir/diagnostics.txt" "No diagnostics." "$source clean diagnostics"
    expect_file_contains "$output_dir/form.html" "<html lang=\"en\">" "$source standalone HTML"
    expect_file_contains "$output_dir/form.html" "</html>" "$source complete HTML document"
}

compile_template templates/scholarship_application.vista scholarship
compile_template templates/college_admission.vista college-admission
compile_template templates/event_registration.vista event-registration

expect_file_contains "$output_root/scholarship/form.html" 'type="email" data-type="email"' "scholarship email control"
expect_file_contains "$output_root/scholarship/form.html" 'visible: () => ((readValue("applicant_category") === "reserved"))' "scholarship certificate condition"
expect_file_contains "$output_root/college-admission/form.html" 'id="other_program"' "college custom program field"
expect_file_contains "$output_root/college-admission/form.html" 'visible: () => ((readValue("program") === "other"))' "college program condition"
expect_file_contains "$output_root/college-admission/form.html" 'required: () => ((readValue("program") === "other"))' "college conditional required rule"
expect_file_contains "$output_root/event-registration/form.html" 'id="workshop_track"' "event workshop track field"
expect_file_contains "$output_root/event-registration/form.html" 'visible: () => ((readValue("event") === "workshop"))' "event workshop condition"
expect_file_contains "$output_root/event-registration/form.html" 'visible: () => ((readValue("attendance_mode") === "in_person"))' "event in-person condition"
expect_file_contains "$output_root/event-registration/form.html" 'min="1" max="4"' "event attendee bounds"

if [[ $failures -ne 0 ]]; then
    echo "$failures template catalogue test(s) failed."
    exit 1
fi

echo "Phase 3 template catalogue and compilation tests passed."
