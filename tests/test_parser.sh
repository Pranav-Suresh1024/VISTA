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

valid_output="$($binary examples/valid_scholarship.vista --emit ast 2>&1)"
valid_status=$?
if [[ $valid_status -ne 0 ]]; then
    echo "FAIL: valid parser input returned $valid_status"
    echo "$valid_output"
    failures=$((failures + 1))
fi
expect_contains "$valid_output" "Form ScholarshipApplication @2:1" "form AST node"
expect_contains "$valid_output" "Field name : text @3:3" "text field AST node"
expect_contains "$valid_output" "Field category : choice [general, reserved] @8:3" "choice AST node"
expect_contains "$valid_output" "ShowWhen @15:5" "conditional visibility node"
expect_contains "$valid_output" "Binary and @25:9" "operator precedence tree"
expect_contains "$valid_output" 'Message "Applicants must be at least 18 years old"' "check message"

catalogue_output="$($binary examples/all_tokens.vista --emit ast 2>&1)"
catalogue_status=$?
if [[ $catalogue_status -ne 0 ]]; then
    echo "FAIL: token catalogue did not parse"
    echo "$catalogue_output"
    failures=$((failures + 1))
fi
for expected_field in \
    "Field title : text" \
    "Field count : integer" \
    "Field price : decimal" \
    "Field active : boolean" \
    "Field opened : date" \
    "Field group : choice [first, second]" \
    "Field attachment : file"; do
    expect_contains "$catalogue_output" "$expected_field" "field type: $expected_field"
done
expect_contains "$catalogue_output" "Unary not" "unary expression"
expect_contains "$catalogue_output" "Binary or" "or expression"

typed_output="$($binary examples/typed_fields.vista --emit ast 2>&1)"
typed_status=$?
if [[ $typed_status -ne 0 ]]; then
    echo "FAIL: new field types or constraint syntax did not parse"
    echo "$typed_output"
    failures=$((failures + 1))
fi
expect_contains "$typed_output" "Field email : email" "email field type"
expect_contains "$typed_output" "Field phone : phone" "phone field type"
expect_contains "$typed_output" "Field textarea : textarea" "textarea field type"
expect_contains "$typed_output" "Minimum 0" "numeric minimum AST property"
expect_contains "$typed_output" "Maximum 4.0" "numeric maximum AST property"
expect_contains "$typed_output" "MinLength 5" "text minimum length AST property"
expect_contains "$typed_output" "MaxLength 200" "text maximum length AST property"

syntax_output="$($binary examples/syntax_error.vista --emit ast 2>&1)"
syntax_status=$?
if [[ $syntax_status -ne 1 ]]; then
    echo "FAIL: syntax error returned $syntax_status instead of 1"
    failures=$((failures + 1))
fi
expect_contains "$syntax_output" "2:14: SYN001" "syntax-error location"
expect_contains "$syntax_output" "unexpected text, expecting :" "syntax-error explanation"
if grep -Fq "Form BrokenSyntax" <<<"$syntax_output"; then
    echo "FAIL: malformed input was reported as a successful AST"
    failures=$((failures + 1))
fi

recovery_output="$($binary examples/syntax_recovery.vista --emit ast 2>&1)"
recovery_status=$?
if [[ $recovery_status -ne 1 ]]; then
    echo "FAIL: syntax recovery input returned $recovery_status instead of 1"
    failures=$((failures + 1))
fi
recovery_count="$(grep -Fc "SYN001" <<<"$recovery_output")"
if [[ $recovery_count -lt 2 ]]; then
    echo "FAIL: declaration recovery produced $recovery_count diagnostic(s), expected at least 2"
    echo "$recovery_output"
    failures=$((failures + 1))
fi

lexical_output="$($binary examples/lexical_error.vista --emit ast 2>&1)"
lexical_status=$?
if [[ $lexical_status -ne 1 ]]; then
    echo "FAIL: lexical failure in AST mode returned $lexical_status instead of 1"
    failures=$((failures + 1))
fi
expect_contains "$lexical_output" "LEX001" "lexical error before parsing"
if grep -Fq "Form BrokenForm" <<<"$lexical_output"; then
    echo "FAIL: parsing continued after a lexical failure"
    failures=$((failures + 1))
fi

if [[ $failures -ne 0 ]]; then
    echo "$failures Stage 3 test(s) failed."
    exit 1
fi

echo "Stage 3 parser and AST tests passed."
