#!/usr/bin/env bash
set -u

binary="./build/vista"
root="build/test-vista-link-module"
failures=0

rm -rf -- "$root" build/vobj-a build/vobj-b

expect_contains() {
    local text="$1"
    local expected="$2"
    local label="$3"
    if ! grep -Fq "$expected" <<<"$text"; then
        echo "FAIL: $label (missing $expected)"
        failures=$((failures + 1))
    fi
}

expect_module_failure() {
    local source="$1"
    local code="$2"
    local directory="$3"
    mkdir -p "$directory"
    printf 'stale' >"$directory/module.vobj"
    local output
    output="$($binary "$source" --emit module --out-dir "$directory" 2>&1)"
    local status=$?
    if [[ $status -ne 1 ]]; then
        echo "FAIL: $source returned $status instead of 1"
        failures=$((failures + 1))
    fi
    expect_contains "$output" "$code" "$source diagnostic"
    if [[ -e "$directory/module.vobj" ]]; then
        echo "FAIL: $source left a stale or newly emitted module.vobj"
        failures=$((failures + 1))
    fi
}

first_output="$($binary examples/vista_link/applicant.vista --emit module --out-dir build/vobj-a 2>&1)"
first_status=$?
second_output="$($binary examples/vista_link/applicant.vista --emit module --out-dir build/vobj-b 2>&1)"
second_status=$?
if [[ $first_status -ne 0 || $second_status -ne 0 ]]; then
    echo "FAIL: deterministic module emission returned $first_status/$second_status"
    echo "$first_output"
    echo "$second_output"
    failures=$((failures + 1))
elif ! cmp build/vobj-a/module.vobj build/vobj-b/module.vobj; then
    echo "FAIL: repeated compilation produced different objects"
    failures=$((failures + 1))
fi

for expected in "VISTA-OBJECT 1" "module Applicant" "export category choice"; do
    if ! grep -Fq "$expected" build/vobj-a/module.vobj 2>/dev/null; then
        echo "FAIL: emitted object omitted $expected"
        failures=$((failures + 1))
    fi
done

expect_module_failure examples/vista_link/errors/missing_export_field.vista MOD002 "$root/missing-export"
expect_module_failure examples/vista_link/errors/duplicate_export.vista MOD003 "$root/duplicate-export"

duplicate_output="$($binary examples/vista_link/errors/duplicate_export.vista --emit module --out-dir "$root/duplicate-count" 2>&1)"
if [[ $(grep -Fc MOD003 <<<"$duplicate_output") -ne 1 ]]; then
    echo "FAIL: duplicate export did not report exactly one MOD003"
    failures=$((failures + 1))
fi

certificate_output="$($binary examples/vista_link/certificate.vista --emit module --out-dir "$root/certificate" 2>&1)"
if [[ $? -ne 0 || ! -s "$root/certificate/module.vobj" ]]; then
    echo "FAIL: imported alias expressions were not deferred"
    echo "$certificate_output"
    failures=$((failures + 1))
fi

mkdir -p "$root/newlines"
cat >"$root/newlines/lf.vista" <<'EOF'
module Newlines {
  field accepted: boolean { label "Accepted" }
  export accepted
}
EOF
sed 's/$/\r/' "$root/newlines/lf.vista" >"$root/newlines/crlf.vista"
tr '\n' '\r' <"$root/newlines/lf.vista" >"$root/newlines/cr.vista"
$binary "$root/newlines/lf.vista" --emit module --out-dir "$root/newlines/lf" >/dev/null 2>&1
lf_status=$?
$binary "$root/newlines/crlf.vista" --emit module --out-dir "$root/newlines/crlf" >/dev/null 2>&1
crlf_status=$?
$binary "$root/newlines/cr.vista" --emit module --out-dir "$root/newlines/cr" >/dev/null 2>&1
cr_status=$?
if [[ $lf_status -ne 0 || $crlf_status -ne 0 || $cr_status -ne 0 ]] ||
   ! cmp "$root/newlines/lf/module.vobj" "$root/newlines/crlf/module.vobj" ||
   ! cmp "$root/newlines/lf/module.vobj" "$root/newlines/cr/module.vobj"; then
    echo "FAIL: LF, CRLF and CR sources did not produce the same object"
    failures=$((failures + 1))
fi

mkdir -p "$root"
cat >"$root/unknown-name.vista" <<'EOF'
module UnknownName {
  field accepted: boolean { label "Accepted" }
  enter when undeclared
  export accepted
}
EOF
expect_module_failure "$root/unknown-name.vista" SEM002 "$root/unknown-name"

cat >"$root/mixed-import.vista" <<'EOF'
module MixedImport {
  import Applicant category as imported_alias
  field accepted: boolean { label "Accepted" }
  enter when imported_alias and undeclared
  export accepted
}
EOF
expect_module_failure "$root/mixed-import.vista" SEM002 "$root/mixed-import"

cat >"$root/mixed-import-local-type.vista" <<'EOF'
module MixedImportLocalType {
  import Applicant category as imported_alias
  field accepted: boolean { label "Accepted" }
  enter when imported_alias and (accepted == 42)
  export accepted
}
EOF
expect_module_failure "$root/mixed-import-local-type.vista" SEM003 "$root/mixed-import-local-type"

cat >"$root/mixed-import-boolean.vista" <<'EOF'
module MixedImportBoolean {
  import Applicant category as imported_alias
  field accepted: boolean { label "Accepted" }
  enter when imported_alias and 42
  export accepted
}
EOF
expect_module_failure "$root/mixed-import-boolean.vista" SEM003 "$root/mixed-import-boolean"

if ! make --no-print-directory test-hash-unit; then
    failures=$((failures + 1))
fi

if [[ $failures -ne 0 ]]; then
    echo "$failures VISTA-Link module test(s) failed."
    exit 1
fi

echo "Deterministic VISTA module object tests passed."
