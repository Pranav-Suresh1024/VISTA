#!/usr/bin/env bash
set -u

if [[ ${1:-} == --vista-link ]]; then
    set -eo pipefail
    tmp=$(mktemp -d)
    trap 'rm -rf -- "$tmp"' EXIT
    source=examples/vista_link/scholarship-demo.workflow.vista
    # Missing typed third module, changed routes, nondeterministic evidence, or
    # stale success evidence after compiler/runtime failures must fail this path.
    for run in first second; do
        for module in applicant eligibility certificate; do
            ./build/vista "examples/vista_link/$module.vista" --emit module --out-dir "$tmp/$run/$module"
        done
        ./build/vista "$source" --emit linked --out-dir "$tmp/$run/linked"
        ./build/vista "$source" --emit partitions > "$tmp/$run/partitions.txt"
        grep -q 'COMPLETE' "$tmp/$run/partitions.txt"
        ./build/vista "$source" --emit certificate --out-dir "$tmp/$run/cert"
        ./build/vista-cert-check "$tmp/$run/cert/workflow.vcert" --root . | grep -q VERIFIED
    done
    for module in applicant eligibility certificate; do cmp "$tmp/first/$module/module.vobj" "$tmp/second/$module/module.vobj"; done
    cmp "$tmp/first/linked/workflow.ir" "$tmp/second/linked/workflow.ir"
    cmp "$tmp/first/partitions.txt" "$tmp/second/partitions.txt"
    for artifact in workflow.ir workflow.js workflow.vcert; do cmp "$tmp/first/cert/$artifact" "$tmp/second/cert/$artifact"; done
    printf 'Applicant.category = general\nEligibility.age = 18\n' > "$tmp/general.data"
    printf 'Applicant.category = reserved\nEligibility.age = 18\nCertificate.certificate = certificate.pdf\n' > "$tmp/reserved.data"
    for category in general reserved; do
        ./build/vista "$source" --validate-data "$tmp/$category.data" > "$tmp/$category.log"
        grep -q 'Complete        : true' "$tmp/$category.log"
        grep -q 'Applicant->Eligibility' "$tmp/$category.log"
    done
    grep -q 'Eligibility->Certificate' "$tmp/reserved.log"
    if grep -q 'Eligibility->Certificate' "$tmp/general.log"; then echo 'FAIL: general applicant routed to Certificate'; exit 1; fi
    node - "$tmp/first/cert/workflow.js" <<'JS'
const api=require(process.argv[2]),assert=require('node:assert/strict');
for(const category of ['general','reserved']) {
  const input={'Applicant.category':category,'Eligibility.age':'18'};
  if(category==='reserved')input['Certificate.certificate']='certificate.pdf';
  const r=api.vistaEvaluate(input);
  assert.equal(r.complete,true);assert.equal(r.active_module,category==='general'?'Eligibility':'Certificate');
  assert.deepEqual(r.transition_trace,category==='general'?['Applicant->Eligibility']:['Applicant->Eligibility','Eligibility->Certificate']);
}
JS
    # Supplied target bytes must be saved before the known-artifact cleanup.
    cp "$tmp/first/cert/workflow.js" "$tmp/saved.js"
    ./build/vista "$source" --emit certificate --out-dir "$tmp/first/cert" --javascript "$tmp/first/cert/workflow.js"
    cmp "$tmp/saved.js" "$tmp/first/cert/workflow.js"
    printf 'workflow Bad { @ }\n' > "$tmp/scan.vista"
    printf 'workflow Bad {\n' > "$tmp/parse.vista"
    mkdir "$tmp/runtime"
    printf '#!/bin/sh\nprintf "malformed\\n"\n' > "$tmp/runtime/node"
    chmod +x "$tmp/runtime/node"
    for failure in scanner parser linker runtime malformed; do
        mkdir -p "$tmp/stale"
        printf 'keep this unrelated user file\n' > "$tmp/stale/keep.txt"
        cp "$tmp/second/cert/"workflow.* "$tmp/stale/"
        code=0
        case "$failure" in
            scanner) ./build/vista "$tmp/scan.vista" --emit certificate --out-dir "$tmp/stale" > "$tmp/error" 2>&1 || code=$?;;
            parser) ./build/vista "$tmp/parse.vista" --emit certificate --out-dir "$tmp/stale" > "$tmp/error" 2>&1 || code=$?;;
            linker) ./build/vista examples/vista_link/errors/missing_export.workflow.vista --emit certificate --out-dir "$tmp/stale" > "$tmp/error" 2>&1 || code=$?;;
            runtime) env PATH="$tmp/absent" ./build/vista "$source" --emit certificate --out-dir "$tmp/stale" > "$tmp/error" 2>&1 || code=$?;;
            malformed) env PATH="$tmp/runtime:$PATH" ./build/vista "$source" --emit certificate --out-dir "$tmp/stale" > "$tmp/error" 2>&1 || code=$?;;
        esac
        case "$failure" in runtime|malformed) test "$code" = 2;; *) test "$code" = 1;; esac
        test ! -e "$tmp/stale/workflow.vcert"
        if [[ $failure == scanner || $failure == parser || $failure == linker ]]; then
            for artifact in workflow.ir workflow.js workflow.html; do test ! -e "$tmp/stale/$artifact"; done
        fi
        grep -q 'keep this unrelated user file' "$tmp/stale/keep.txt"
    done
    echo 'VISTA-Link full pipeline, three-module CLI/JS routes, cleanup and deterministic rebuild passed.'
    exit 0
fi
if [[ $# -ne 0 ]]; then echo 'Usage: test_pipeline.sh [--vista-link]' >&2; exit 2; fi

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
