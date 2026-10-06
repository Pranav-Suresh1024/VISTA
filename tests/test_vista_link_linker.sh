#!/usr/bin/env bash
set -eu
binary="$PWD/build/vista"
root="$PWD/build/test-vista-link-linker"
mkdir -p "$root/modules"
fail() { echo "FAIL: $*"; exit 1; }
link() { "$binary" "$1" --emit linked --out-dir "$2"; }
reject() {
    mkdir -p "$root/rejected"
    printf stale >"$root/rejected/workflow.ir"
    local status=0
    link "$1" "$root/rejected" >"$root/error" 2>&1 || status=$?
    test "$status" -eq 1 || fail "$1 returned $status instead of 1"
    grep -Fq "$2" "$root/error" || { cat "$root/error"; fail "missing $2"; }
    test ! -e "$root/rejected/workflow.ir" || fail "stale IR after $2"
    echo "Rejected ${1##*/}: $2 (no IR)"
}
link examples/vista_link/scholarship.workflow.vista "$root/a" || fail 'linked emission unavailable'
link examples/vista_link/scholarship.workflow.vista "$root/b"
cmp "$root/a/workflow.ir" "$root/b/workflow.ir"
for expected in 'Applicant.category' 'Certificate.applicant_category -> Applicant.category' 'start Applicant' 'route Applicant -> Certificate' 'source-sha256' 'object-sha256' 'rule Applicant.category.required' 'workflow-sha256'; do
    grep -Fq "$expected" "$root/a/workflow.ir" || fail "IR missing $expected"
done
# Verify the digest independently, rather than trusting the linker hash helper.
sed '$d' "$root/a/workflow.ir" >"$root/prefix"
test "$(sha256sum "$root/prefix" | cut -d' ' -f1)" = "$(tail -1 "$root/a/workflow.ir" | cut -d' ' -f2)" || fail 'workflow digest'
(cd /tmp && "$binary" "$OLDPWD/examples/vista_link/scholarship.workflow.vista" --emit linked --out-dir "$root/from-other-cwd")
cmp "$root/a/workflow.ir" "$root/from-other-cwd/workflow.ir"
for item in missing_export:LNK002 type_mismatch:LNK003 duplicate_alias:LNK004 invalid_start:LNK005 import_cycle:LNK006 unreachable:LNK007 ambiguous_unconditional:LNK008; do
    reject "examples/vista_link/errors/${item%:*}.workflow.vista" "${item#*:}"
done
cat >"$root/missing.workflow.vista" <<'EOF'
workflow Missing { use "missing.vista" as Missing start Missing }
EOF
reject "$root/missing.workflow.vista" LNK001
cp examples/vista_link/applicant.vista "$root/modules/applicant.vista"
cp examples/vista_link/certificate.vista "$root/modules/certificate.vista"
cat >"$root/modules/collision.vista" <<'EOF'
module Collision {
  import Applicant category as applicant_category
  field reserved: boolean { label "Reserved flag" }
  field upload: file { label "Upload" show when applicant_category == reserved required when reserved == applicant_category }
  enter when applicant_category == reserved
  check applicant_category != general message "Reserved only"
}
EOF
cat >"$root/collision.workflow.vista" <<'EOF'
workflow Collision {
  use "modules/applicant.vista" as Applicant
  use "modules/collision.vista" as Collision
  start Applicant
  route Applicant -> Collision when Applicant.category == reserved
}
EOF
link "$root/collision.workflow.vista" "$root/collision"
grep -Fq '(choice-literal choice "reserved")' "$root/collision/workflow.ir" || fail 'imported choice collision'
grep -Fq 'name choice "Applicant.category"' "$root/collision/workflow.ir" || fail 'import qualification'
cat >"$root/local.workflow.vista" <<EOF
workflow Local { use "../../examples/vista_link/choice_collision.vista" as Local start Local }
EOF
link "$root/local.workflow.vista" "$root/local"
grep -Fq '(choice-literal choice "reserved")' "$root/local/workflow.ir" || fail 'local choice collision'
# Missing imported typing previously let all of these uses through module compilation.
for condition in 'applicant_category == 42' 'applicant_category == unknown' 'applicant_category' 'present(applicant_category)' 'applicant_category > 2'; do
    cat >"$root/modules/bad.vista" <<EOF
module Bad { import Applicant category as applicant_category enter when $condition }
EOF
    cat >"$root/bad.workflow.vista" <<'EOF'
workflow Bad { use "modules/applicant.vista" as Applicant use "modules/bad.vista" as Bad start Applicant route Applicant -> Bad }
EOF
    reject "$root/bad.workflow.vista" LNK003
done
cat >"$root/modules/bad.vista" <<'EOF'
module Bad { import Applicant category as category field category: boolean { label "Collision" } }
EOF
reject "$root/bad.workflow.vista" LNK002
cat >"$root/modules/bad.vista" <<'EOF'
module Bad { import Applicant category as x import Applicant category as x }
EOF
reject "$root/bad.workflow.vista" LNK002
cat >"$root/route-cycle.workflow.vista" <<'EOF'
workflow Cycle { use "modules/applicant.vista" as Applicant use "modules/certificate.vista" as Certificate start Applicant route Applicant -> Certificate route Certificate -> Applicant }
EOF
reject "$root/route-cycle.workflow.vista" LNK006
cat >"$root/endpoint.workflow.vista" <<'EOF'
workflow Endpoint { use "modules/applicant.vista" as Applicant start Applicant route Applicant -> Missing }
EOF
reject "$root/endpoint.workflow.vista" LNK002
cat >"$root/qualified.workflow.vista" <<'EOF'
workflow Qualified { use "modules/applicant.vista" as Applicant use "modules/certificate.vista" as Certificate start Applicant route Applicant -> Certificate when Certificate.certificate == "x" }
EOF
reject "$root/qualified.workflow.vista" LNK003
cat >"$root/private.workflow.vista" <<'EOF'
workflow Private { use "modules/collision.vista" as Collision use "modules/applicant.vista" as Applicant start Applicant route Applicant -> Collision when Collision.reserved }
EOF
reject "$root/private.workflow.vista" LNK002
# Two conditional routes are retained for the evaluator, never given source priority.
cat >"$root/conditional.workflow.vista" <<'EOF'
workflow Conditional { use "modules/applicant.vista" as Applicant use "modules/certificate.vista" as Certificate start Applicant route Applicant -> Certificate when Applicant.category == reserved route Applicant -> Applicant when Applicant.category == reserved }
EOF
link "$root/conditional.workflow.vista" "$root/conditional"
cat >"$root/reordered.workflow.vista" <<'EOF'
workflow Conditional { use "modules/certificate.vista" as Certificate use "modules/applicant.vista" as Applicant start Applicant route Applicant -> Applicant when Applicant.category == reserved route Applicant -> Certificate when Applicant.category == reserved }
EOF
link "$root/reordered.workflow.vista" "$root/reordered"
cmp "$root/conditional/workflow.ir" "$root/reordered/workflow.ir"
# Objects are validated and parsed from embedded source without needing the original file.
"$binary" "$root/modules/applicant.vista" --emit module --out-dir "$root/objects/applicant"
"$binary" "$root/modules/certificate.vista" --emit module --out-dir "$root/objects/certificate"
cat >"$root/object.workflow.vista" <<'EOF'
workflow ScholarshipApplication { use "objects/applicant/module.vobj" as Applicant use "objects/certificate/module.vobj" as Certificate start Applicant route Applicant -> Certificate when Applicant.category == reserved }
EOF
link "$root/object.workflow.vista" "$root/object"
cmp "$root/a/workflow.ir" "$root/object/workflow.ir"
cp "$root/objects/applicant/module.vobj" "$root/original.vobj"
sed '$d;2s/module Applicant/module Forged/' "$root/original.vobj" >"$root/forged-prefix"
cp "$root/forged-prefix" "$root/objects/applicant/module.vobj"
printf 'object-sha256 %s\n' "$(sha256sum "$root/forged-prefix" | cut -d' ' -f1)" >>"$root/objects/applicant/module.vobj"
reject "$root/object.workflow.vista" MOD004
cp "$root/original.vobj" "$root/objects/applicant/module.vobj"
printf corrupt >>"$root/objects/applicant/module.vobj"
reject "$root/object.workflow.vista" MOD004
# A three-module workflow uses both local and imported contextual choice literals.
cat >"$root/three.workflow.vista" <<'EOF'
workflow Three { use "modules/applicant.vista" as Applicant use "modules/collision.vista" as Collision use "modules/certificate.vista" as Certificate start Applicant route Applicant -> Collision route Collision -> Certificate }
EOF
link "$root/three.workflow.vista" "$root/three"
grep -Fq 'modules 3' "$root/three/workflow.ir" || fail 'three-module workflow'
cat >"$root/duplicates.workflow.vista" <<'EOF'
workflow Duplicates { use "modules/applicant.vista" as Applicant use "modules/certificate.vista" as Certificate start Applicant route Applicant -> Certificate when Applicant.category == reserved route Applicant -> Certificate when Applicant.category == reserved }
EOF
link "$root/duplicates.workflow.vista" "$root/duplicates"
test "$(grep '^route ' "$root/duplicates/workflow.ir" | cut -d' ' -f6 | sort -u | wc -l)" -eq 2 || fail 'duplicate conditional route IDs are not unique'
# Direct API consumer checks owned trees after the input AST arena has been destroyed.
cat >"$root/owned.cpp" <<'EOF'
#include <cassert>
#include <string>
#include "linker.hpp"
#include "parser.hpp"
#include "scanner.hpp"
int main(int argc, char** argv) {
    assert(argc == 2);
    vista::LinkResult linked;
    {
        auto parsed = vista::parse_tokens(vista::scan_file(argv[1]).tokens);
        assert(parsed.succeeded());
        linked = vista::link_workflow(*parsed.workflow, argv[1]);
        assert(linked.succeeded());
    }
    auto copy = linked.workflow;
    linked = {};
    assert(copy.modules.size() == 2);
    const auto& entry = copy.modules[1].entry;
    assert(entry.value == "and" && entry.operands.size() == 2);
    assert(entry.operands[0].operands[0].value == "Applicant.category");
    assert(entry.operands[0].operands[1].choice_literal);
    assert(entry.operands[1].operands[0].value == "Certificate.certificate");
    assert(vista::format_linked_workflow(copy).find("Applicant.category") != std::string::npos);
}
EOF
g++ -std=c++17 -Wall -Wextra -Werror -Iinclude -Ibuild "$root/owned.cpp" src/linker.cpp src/module.cpp src/hash.cpp src/semantic.cpp src/ast.cpp src/parser.cpp src/token.cpp build/vista_parser.cpp build/vista_lexer.cpp -Wno-sign-compare -Wno-unused-function -Wno-unused-parameter -o "$root/owned"
"$root/owned" examples/vista_link/scholarship.workflow.vista
# Provider types/options drive semantics for every scalar and file-presence use.
cat >"$root/modules/all.vista" <<'EOF'
module All {
  field flag: boolean { label "Flag" }
  field count: integer { label "Count" minimum 0 maximum 100 }
  field amount: decimal { label "Amount" minimum -1.5 maximum 1e2 }
  field day: date { label "Day" minimum "2026-01-01" maximum "2026-12-31" }
  field note: text { label "Note" min_length 1 max_length 10 }
  field attachment: file { label "Attachment" }
  field category: choice { general, reserved } { label "Category" }
  export flag export count export amount export day export note export attachment export category
}
EOF
cat >"$root/modules/typed.vista" <<'EOF'
module Typed {
  import All flag as flag
  import All count as count
  import All amount as amount
  import All day as day
  import All note as note
  import All attachment as attachment
  import All category as category
  enter when flag and count >= 18 and amount < 1e2 and day >= "2026-01-01" and note == "ok" and present(attachment) and category != reserved
  check not flag or count != 0 message "Check\nmessage"
}
EOF
cat >"$root/typed.workflow.vista" <<'EOF'
workflow Typed { use "modules/all.vista" as All use "modules/typed.vista" as Typed start All route All -> Typed when All.day <= "2026-12-31" }
EOF
link "$root/typed.workflow.vista" "$root/typed"
for expected in 'name boolean "All.flag"' 'name integer "All.count"' 'name decimal "All.amount"' 'name date "All.day"' 'name text "All.note"' 'name file "All.attachment"' 'choice-literal choice "reserved"' 'rule All.count.minimum minimum "0"' 'rule All.note.maxlength maxlength "10"' 'Typed.check.1' 'Check\x0amessage'; do
    grep -Fq "$expected" "$root/typed/workflow.ir" || fail "typed IR missing $expected"
done
# Complete valid contracts still cannot form an import dependency cycle.
cat >"$root/modules/cycle-a.vista" <<'EOF'
module A { import B flag as other field flag: boolean { label "Flag" } export flag enter when other }
EOF
cat >"$root/modules/cycle-b.vista" <<'EOF'
module B { import A flag as other field flag: boolean { label "Flag" } export flag enter when other }
EOF
cat >"$root/import-cycle.workflow.vista" <<'EOF'
workflow Cycle { use "modules/cycle-a.vista" as A use "modules/cycle-b.vista" as B start A route A -> B }
EOF
reject "$root/import-cycle.workflow.vista" LNK006
cat >"$root/modules/noexport.vista" <<'EOF'
module NoExport { field category: choice { general, reserved } { label "Category" } }
EOF
cat >"$root/noexport.workflow.vista" <<'EOF'
workflow NoExport { use "modules/noexport.vista" as Applicant use "modules/certificate.vista" as Certificate start Applicant route Applicant -> Certificate }
EOF
reject "$root/noexport.workflow.vista" LNK002
cat >"$root/duplicate-missing.workflow.vista" <<'EOF'
workflow Duplicate { use "missing.vista" as A use "also-missing.vista" as A start A }
EOF
reject "$root/duplicate-missing.workflow.vista" LNK004
if grep -Fq LNK001 "$root/error"; then fail 'loaded duplicate aliases'; fi
"$binary" examples/valid_scholarship.vista --emit diagnostics
echo 'Typed workflow linker tests passed.'
