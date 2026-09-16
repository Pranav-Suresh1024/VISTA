#!/usr/bin/env bash
set -u

binary="./build/vista"
output_root="build/test-pipeline"
failures=0
artifacts=(tokens.txt ast.txt symbols.txt dependencies.txt graph.dot diagnostics.txt form.html)

rm -rf -- "$output_root"

valid_dir="$output_root/valid"
valid_output="$($binary examples/valid_scholarship.vista --emit all --out-dir "$valid_dir" 2>&1)"
valid_status=$?
if [[ $valid_status -ne 0 ]]; then
    echo "FAIL: complete valid pipeline returned $valid_status"
    echo "$valid_output"
    failures=$((failures + 1))
fi
for artifact in "${artifacts[@]}"; do
    if [[ ! -s "$valid_dir/$artifact" ]]; then
        echo "FAIL: valid pipeline omitted $artifact"
        failures=$((failures + 1))
    fi
done
if [[ -f "$valid_dir/diagnostics.txt" ]] &&
   ! grep -Fq "No diagnostics." "$valid_dir/diagnostics.txt"; then
    echo "FAIL: valid diagnostics file is not clean"
    failures=$((failures + 1))
fi

check_failed_pipeline() {
    local source="$1"
    local output_name="$2"
    local code="$3"
    local expected_artifact="$4"
    local directory="$output_root/$output_name"

    mkdir -p "$directory"
    touch "$directory/form.html"
    local output
    output="$($binary "$source" --emit all --out-dir "$directory" 2>&1)"
    local status=$?
    if [[ $status -ne 1 ]]; then
        echo "FAIL: $source returned $status instead of 1"
        failures=$((failures + 1))
    fi
    if [[ ! -s "$directory/$expected_artifact" ]]; then
        echo "FAIL: $source omitted progressive artifact $expected_artifact"
        failures=$((failures + 1))
    fi
    if [[ ! -s "$directory/diagnostics.txt" ]] ||
       ! grep -Fq "$code" "$directory/diagnostics.txt"; then
        echo "FAIL: $source diagnostics file omitted $code"
        failures=$((failures + 1))
    fi
    if [[ -f "$directory/form.html" ]]; then
        echo "FAIL: $source left a stale or newly generated form.html"
        failures=$((failures + 1))
    fi
    if ! grep -Fq "$code" <<<"$output"; then
        echo "FAIL: $source terminal output omitted $code"
        failures=$((failures + 1))
    fi
}

check_failed_pipeline examples/lexical_error.vista lexical LEX001 tokens.txt
check_failed_pipeline examples/syntax_error.vista syntax SYN001 tokens.txt
check_failed_pipeline examples/type_mismatch.vista semantic SEM003 symbols.txt
check_failed_pipeline examples/hidden_required.vista analysis ANL003 graph.dot

$binary examples/valid_scholarship.vista --emit all >/dev/null 2>&1
missing_out_status=$?
if [[ $missing_out_status -ne 2 ]]; then
    echo "FAIL: all mode without --out-dir returned $missing_out_status instead of 2"
    failures=$((failures + 1))
fi

$binary examples/valid_scholarship.vista --emit all \
    --out-dir examples/valid_scholarship.vista >/dev/null 2>&1
bad_directory_status=$?
if [[ $bad_directory_status -ne 2 ]]; then
    echo "FAIL: invalid output directory returned $bad_directory_status instead of 2"
    failures=$((failures + 1))
fi

if [[ $failures -ne 0 ]]; then
    echo "$failures final pipeline test(s) failed."
    exit 1
fi

echo "Complete VISTA compiler and validation pipeline tests passed."
