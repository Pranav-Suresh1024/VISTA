#!/usr/bin/env bash
set -eu
tmp=$(mktemp -d)
trap 'rm -rf -- "$tmp"' EXIT
binary=./build/vista
check() {
    local source=$1 data=$2 want=$3 output status=0
    shift 3
    output=$($binary "$source" --validate-data "$data" 2>&1) || status=$?
    if [[ $status != "$want" ]]; then
        printf 'FAIL: %s expected status %s, got %s\n%s\n' "$data" "$want" "$status" "$output"
        exit 1
    fi
    for expected in "$@"; do
        if ! grep -Fq -- "$expected" <<< "$output"; then
            printf 'FAIL: missing %s\n%s\n' "$expected" "$output"
            exit 1
        fi
    done
}
workflow=examples/vista_link/scholarship.workflow.vista
check "$workflow" samples/vista_link/general.data 0 \
    'Active module   : Applicant' 'Visible fields  : Applicant.category' \
    'Required fields : Applicant.category' 'Selected route  : none' \
    'Complete        : true' 'Result          : PASS' 'Applicant.entry'
check "$workflow" samples/vista_link/reserved-valid.data 0 \
    'Active module   : Certificate' 'Selected route  : Applicant->Certificate' \
    'Complete        : true' 'Certificate.certificate.required'
check "$workflow" samples/vista_link/reserved-missing-file.data 1 \
    'Active module   : Certificate' 'Selected route  : Applicant->Certificate' \
    'Complete        : false' 'VAL004' 'Certificate.certificate'
printf 'Applicant.category = general\nApplicant.unknown = x\n' > "$tmp/unknown.data"
check "$workflow" "$tmp/unknown.data" 1 'VAL003' 'Applicant.unknown'
check "$workflow" "$tmp/absent.data" 2 'VAL000'
printf 'Applicant.category = general\nApplicant.category = reserved\nbad..name = x\n' > "$tmp/bad.data"
check "$workflow" "$tmp/bad.data" 1 'VAL001' 'VAL002'

cat > "$tmp/scalar.vista" <<'EOF'
module Scalar {
  field amount: decimal { label "Amount" required minimum -125 maximum -125 }
  field note: text { label "Note" min_length 2 max_length 2 }
  field upload: file { label "Upload" }
  check amount == -125 message "exact negative decimal"
  check present(upload) message "file presence"
  export amount
}
EOF
printf 'workflow Numbers { use "scalar.vista" as Scalar start Scalar }\n' > "$tmp/numbers.vista"
cat samples/vista_link/decimal-exponent.data > "$tmp/number.data"
printf 'Scalar.note = é\nScalar.upload = false\n' >> "$tmp/number.data"
check "$tmp/numbers.vista" "$tmp/number.data" 0 'Complete        : true' 'Scalar.check.2'
first=$($binary "$tmp/numbers.vista" --validate-data "$tmp/number.data")
printf 'Scalar.upload = false\nScalar.note = é\nScalar.amount = -125\n' > "$tmp/equivalent.data"
second=$($binary "$tmp/numbers.vista" --validate-data "$tmp/equivalent.data")
[[ "$first" == "$second" ]] || { echo 'FAIL: equivalent decimals/input order differ'; exit 1; }
printf 'Scalar.amount = -125\nScalar.note = a\n' > "$tmp/invalid.data"
check "$tmp/numbers.vista" "$tmp/invalid.data" 1 'VAL006' 'VAL009'
printf 'Scalar.amount = 1e999999999999999999999999\n' > "$tmp/hostile.data"
check "$tmp/numbers.vista" "$tmp/hostile.data" 1 'VAL005'

cat > "$tmp/exact.vista" <<'EOF'
form Exact {
  field x: decimal { label "X" required }
  check x > 100000000000000000000 message "must be strictly greater"
}
EOF
printf 'x = 100000000000000000001\n' > "$tmp/exact.data"
check "$tmp/exact.vista" "$tmp/exact.data" 0 'Result          : PASS'

cat > "$tmp/a.vista" <<'EOF'
module A { field flag: boolean { label "Flag" } export flag }
EOF
printf 'module B { enter when false }\n' > "$tmp/b.vista"
printf 'module C {}\n' > "$tmp/c.vista"
cat > "$tmp/ambiguous.vista" <<'EOF'
workflow Ambiguous {
  use "a.vista" as A use "b.vista" as B use "c.vista" as C start A
  route A -> B when A.flag
  route A -> C when A.flag == true
}
EOF
printf 'A.flag = true\n' > "$tmp/true.data"
check "$tmp/ambiguous.vista" "$tmp/true.data" 1 'LNK008' 'Complete        : false'
cat > "$tmp/entry.vista" <<'EOF'
workflow Entry { use "a.vista" as A use "b.vista" as B start A route A -> B when A.flag }
EOF
check "$tmp/entry.vista" "$tmp/true.data" 1 'VAL010' 'Selected route  : A->B' 'Complete        : false'
cat > "$tmp/cycle.vista" <<'EOF'
workflow Cycle {
  use "a.vista" as A use "c.vista" as C start A
  route A -> C when A.flag
  route C -> A when A.flag
}
EOF
check "$tmp/cycle.vista" "$tmp/true.data" 1 'LNK006' 'Complete        : false'
printf 'A.flag = invalid\n' > "$tmp/invalid-flag.data"
check "$tmp/entry.vista" "$tmp/invalid-flag.data" 1 'VAL005' 'Complete        : false'
echo 'VISTA-Link evaluator tests passed.'
