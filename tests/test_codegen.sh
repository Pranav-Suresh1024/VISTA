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
