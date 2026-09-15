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

version_output="$($binary --version)"
expect_contains "$version_output" "VISTA 0.6.0" "version output"

help_output="$($binary --help)"
expect_contains "$help_output" "Usage: vista <source.vista> --emit <tokens|ast|symbols|dependencies|graph|diagnostics>" "help usage"

valid_output="$($binary examples/valid_scholarship.vista --emit tokens)"
valid_status=$?
if [[ $valid_status -ne 0 ]]; then
    echo "FAIL: valid scanner input returned $valid_status"
    failures=$((failures + 1))
fi
expect_contains "$valid_output" "2:1" "form token position"
expect_contains "$valid_output" "FORM" "form token kind"
expect_contains "$valid_output" '"ScholarshipApplication"' "form identifier"
expect_contains "$valid_output" "TYPE_CHOICE" "choice type token"
expect_contains "$valid_output" "GREATER_EQUAL" "comparison token"
expect_contains "$valid_output" "END_OF_FILE" "end-of-file token"
if grep -Fq "scanner demonstration" <<<"$valid_output"; then
    echo "FAIL: text inside a comment was tokenized"
    failures=$((failures + 1))
fi

catalogue_output="$($binary examples/all_tokens.vista --emit tokens)"
for expected_token in \
    TYPE_TEXT TYPE_INTEGER TYPE_DECIMAL TYPE_BOOLEAN TYPE_DATE TYPE_CHOICE TYPE_FILE \
    INTEGER_LITERAL DECIMAL_LITERAL TRUE_LITERAL FALSE_LITERAL \
    EQUAL NOT_EQUAL LESS LESS_EQUAL GREATER GREATER_EQUAL AND OR NOT \
    LEFT_BRACE RIGHT_BRACE LEFT_PAREN RIGHT_PAREN COLON COMMA; do
    expect_contains "$catalogue_output" "$expected_token" "token catalogue: $expected_token"
done

lexical_output="$($binary examples/lexical_error.vista --emit tokens 2>&1)"
lexical_status=$?
if [[ $lexical_status -ne 1 ]]; then
    echo "FAIL: lexical error returned $lexical_status instead of 1"
    failures=$((failures + 1))
fi
expect_contains "$lexical_output" "4:14: LEX001" "unexpected-character location"
expect_contains "$lexical_output" 'unexpected character "@"' "unexpected-character message"

string_output="$($binary examples/unterminated_string.vista --emit tokens 2>&1)"
string_status=$?
if [[ $string_status -ne 1 ]]; then
    echo "FAIL: unterminated string returned $string_status instead of 1"
    failures=$((failures + 1))
fi
expect_contains "$string_output" "3:11: LEX002" "unterminated-string location"
expect_contains "$string_output" "unterminated string literal" "unterminated-string message"

$binary --unknown >/dev/null 2>&1
unknown_status=$?
if [[ $unknown_status -ne 2 ]]; then
    echo "FAIL: unknown option returned $unknown_status instead of 2"
    failures=$((failures + 1))
fi

$binary examples/missing.vista --emit tokens >/dev/null 2>&1
missing_status=$?
if [[ $missing_status -ne 2 ]]; then
    echo "FAIL: missing input returned $missing_status instead of 2"
    failures=$((failures + 1))
fi

if [[ $failures -ne 0 ]]; then
    echo "$failures Stage 2 test(s) failed."
    exit 1
fi

echo "Stage 1 and Stage 2 tests passed."
