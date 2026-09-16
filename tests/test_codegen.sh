#!/usr/bin/env bash
set -u

binary="./build/vista"
output_root="build/test-html"
failures=0

rm -rf -- "$output_root"

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

valid_dir="$output_root/valid"
valid_output="$($binary examples/valid_scholarship.vista --emit html --out-dir "$valid_dir" 2>&1)"
valid_status=$?
if [[ $valid_status -ne 0 || ! -f "$valid_dir/form.html" ]]; then
    echo "FAIL: valid HTML generation returned $valid_status or omitted form.html"
    echo "$valid_output"
    failures=$((failures + 1))
fi
expect_contains "$valid_dir/form.html" '<html lang="en">' "document language"
expect_contains "$valid_dir/form.html" '<label for="name">Full name</label>' "accessible label"
expect_contains "$valid_dir/form.html" 'type="number" data-type="integer" step="1"' "integer control"
expect_contains "$valid_dir/form.html" '<select id="category"' "choice control"
expect_contains "$valid_dir/form.html" '<option value="reserved">reserved</option>' "choice option"
expect_contains "$valid_dir/form.html" 'visible: () => ((readValue("category") === "reserved"))' "visibility rule"
expect_contains "$valid_dir/form.html" 'required: () => ((readValue("category") === "reserved"))' "conditional required rule"
expect_contains "$valid_dir/form.html" 'message: "Enter your full name"' "validation message"
expect_contains "$valid_dir/form.html" '<title>ScholarshipApplication</title>' "legacy form-name title fallback"
expect_contains "$valid_dir/form.html" 'role="status" aria-live="polite"' "accessible validation status"
expect_contains "$valid_dir/form.html" 'class="required-indicator"' "required indicator support"

metadata_dir="$output_root/presentation-metadata"
metadata_output="$($binary examples/presentation_metadata.vista --emit all --out-dir "$metadata_dir" 2>&1)"
metadata_status=$?
if [[ $metadata_status -ne 0 ]]; then
    echo "FAIL: presentation metadata input returned $metadata_status"
    echo "$metadata_output"
    failures=$((failures + 1))
fi
expect_contains "$metadata_dir/form.html" '<title>Undergraduate Scholarship Application</title>' "custom form title"
expect_contains "$metadata_dir/form.html" 'id="form-description"' "form description"
expect_contains "$metadata_dir/form.html" '<section class="form-section" aria-label="Your details">' "section heading group"
expect_contains "$metadata_dir/form.html" '<small class="field-help" id="help-name">Enter your name as it appears on official documents.</small>' "field help text"
expect_contains "$metadata_dir/form.html" 'placeholder="e.g. Maya Patel"' "field placeholder"
expect_contains "$metadata_dir/form.html" 'requiredIndicator.hidden = !control.required;' "conditional required indicator behavior"
expect_contains "$metadata_dir/ast.txt" 'Title "Undergraduate Scholarship Application"' "form-title AST representation"
expect_contains "$metadata_dir/ast.txt" 'Description "Share your details and study information to begin your scholarship application. Fields marked with an asterisk are required."' "form-description AST representation"
expect_contains "$metadata_dir/ast.txt" 'Section "Your details"' "metadata AST representation"
if [[ -f "$metadata_dir/form.html" ]] &&
   ! grep -Fq 'visible: () => ((readValue("category") === "reserved"))' "$metadata_dir/form.html"; then
    echo "FAIL: presentation metadata changed existing conditional visibility behavior"
    failures=$((failures + 1))
fi

types_dir="$output_root/all-types"
$binary examples/all_tokens.vista --emit html --out-dir "$types_dir" >/dev/null 2>&1
types_status=$?
if [[ $types_status -ne 0 ]]; then
    echo "FAIL: all field types input returned $types_status"
    failures=$((failures + 1))
fi
expect_contains "$types_dir/form.html" 'type="number" data-type="decimal" step="any"' "decimal control"
expect_contains "$types_dir/form.html" 'type="checkbox" data-type="boolean"' "Boolean control"
expect_contains "$types_dir/form.html" 'type="date" data-type="date"' "date control"
expect_contains "$types_dir/form.html" 'type="file" data-type="file"' "file control"

escaping_dir="$output_root/escaping"
$binary examples/safe_escaping.vista --emit html --out-dir "$escaping_dir" >/dev/null 2>&1
escaping_status=$?
if [[ $escaping_status -ne 0 ]]; then
    echo "FAIL: safe escaping input returned $escaping_status"
    failures=$((failures + 1))
fi
expect_contains "$escaping_dir/form.html" 'Display &lt;name&gt; &amp; &quot;alias&quot;' "HTML escaping"
expect_contains "$escaping_dir/form.html" 'message: "\u003C/script\u003E\u003Cb\u003ERequired\u003C/b\u003E"' "script-safe message"
if grep -Fq '</script><b>Required</b>' "$escaping_dir/form.html"; then
    echo "FAIL: unsafe closing-script text reached generated HTML"
    failures=$((failures + 1))
fi

safe_metadata_dir="$output_root/safe-presentation-metadata"
$binary examples/safe_presentation_metadata.vista --emit html --out-dir "$safe_metadata_dir" >/dev/null 2>&1
safe_metadata_status=$?
if [[ $safe_metadata_status -ne 0 ]]; then
    echo "FAIL: escaped presentation metadata input returned $safe_metadata_status"
    failures=$((failures + 1))
fi
expect_contains "$safe_metadata_dir/form.html" '<title>Safe &lt;title&gt; &amp; &quot;form&quot;</title>' "title escaping"
expect_contains "$safe_metadata_dir/form.html" 'Description &lt;script&gt;alert(&quot;x&quot;)&lt;/script&gt; &amp; text' "description escaping"
expect_contains "$safe_metadata_dir/form.html" 'aria-label="&lt;Applicant &amp; group&gt;"' "section escaping"
expect_contains "$safe_metadata_dir/form.html" 'Text &amp; details &lt;safe&gt;' "help text escaping"
expect_contains "$safe_metadata_dir/form.html" 'placeholder="e.g. &quot;value&quot;"' "placeholder attribute escaping"

blocked_dir="$output_root/blocked"
mkdir -p "$blocked_dir"
touch "$blocked_dir/form.html"
blocked_output="$($binary examples/hidden_required.vista --emit html --out-dir "$blocked_dir" 2>&1)"
blocked_status=$?
if [[ $blocked_status -ne 1 ]]; then
    echo "FAIL: unsafe form returned $blocked_status instead of 1"
    failures=$((failures + 1))
fi
if [[ -f "$blocked_dir/form.html" ]]; then
    echo "FAIL: unsafe form left a stale or newly generated form.html"
    failures=$((failures + 1))
fi
if ! grep -Fq "ANL003" <<<"$blocked_output"; then
    echo "FAIL: unsafe form did not report ANL003"
    failures=$((failures + 1))
fi

$binary examples/valid_scholarship.vista --emit html >/dev/null 2>&1
missing_out_status=$?
if [[ $missing_out_status -ne 2 ]]; then
    echo "FAIL: HTML mode without --out-dir returned $missing_out_status instead of 2"
    failures=$((failures + 1))
fi

if [[ $failures -ne 0 ]]; then
    echo "$failures Stage 6 test(s) failed."
    exit 1
fi

echo "Stage 6 standalone HTML generation tests passed."
