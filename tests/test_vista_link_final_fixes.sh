#!/usr/bin/env bash
set -euo pipefail
ulimit -c 0
binary="$PWD/build/vista"
tmp=$(mktemp -d)
trap 'rm -rf -- "$tmp"' EXIT
fail() { echo "FAIL: $*"; exit 1; }
expect_exit() {
    local want=$1 code=0; shift
    "$@" > "$tmp/log" 2>&1 || code=$?
    test "$code" = "$want" || { cat "$tmp/log"; fail "expected $want, got $code: $*"; }
}
printf 'module A { field attachment: file { label "Attachment" } export attachment }\n' > "$tmp/a.vista"
printf 'module B {}\n' > "$tmp/b.vista"
printf 'workflow W { use "a.vista" as A use "b.vista" as B start A route A -> B }\n' > "$tmp/main.vista"
group=${1:-all}
if [[ $group = F1 || $group = F1-ambiguous || $group = all ]]; then
    for failure in scanner parser; do
        for mode in workflow certificate linked all; do
            directory="$tmp/ambiguous-$failure-$mode"
            mkdir -p "$directory/out"
            for file in workflow.ir workflow.js workflow.html workflow.vcert tokens.txt diagnostics.txt; do
                printf 'preserve unknown input\n' > "$directory/out/$file"
            done
            if [[ $failure = scanner ]]; then printf 'workflow W { use "unfinished\n' > "$directory/main.vista";
            else printf 'workflow W { use Missing }\n' > "$directory/main.vista"; fi
            expect_exit 1 "$binary" "$directory/main.vista" --emit "$mode" --out-dir "$directory/out"
            for file in workflow.ir workflow.js workflow.html workflow.vcert tokens.txt diagnostics.txt; do
                cmp "$directory/out/$file" <(printf 'preserve unknown input\n') || fail 'ambiguous inventory changed output'
            done
            grep -q 'inventory is incomplete; old evidence is unusable' "$tmp/log"
        done
    done
    echo 'F1 ambiguous inventories preserve outputs with unusable-evidence warning.'
fi
if [[ $group = F1 || $group = F1-scanner || $group = F1-parser || $group = all ]]; then
    for failure in scanner parser parser-before-use; do
        [[ $group != F1-scanner || $failure = scanner ]] || continue
        [[ $group != F1-parser || $failure != scanner ]] || continue
        for mode in workflow certificate linked; do
            artifacts=workflow.js
            [[ $mode != linked ]] || artifacts=workflow.ir
            [[ $mode != certificate ]] || artifacts='workflow.js workflow.vcert'
            for artifact in $artifacts; do
                for alias in symlink hardlink; do
                    directory="$tmp/error-$failure-$mode-$artifact-$alias"
                    mkdir -p "$directory/out"
                    for file in workflow.ir workflow.js workflow.html workflow.vcert; do
                        printf 'stale success\n' > "$directory/out/$file"
                    done
                    printf 'module A {}\n' > "$directory/out/$artifact"
                    if [[ $alias = symlink ]]; then ln -s "out/$artifact" "$directory/a.vista";
                    else ln "$directory/out/$artifact" "$directory/a.vista"; fi
                    case $failure in
                        scanner) printf 'workflow W { use "a.vista" as A start A } @\n';;
                        parser) printf 'workflow W { use "a.vista" as A start A\n';;
                        parser-before-use) printf 'workflow W { route -> use "a.vista" as A }\n';;
                    esac > "$directory/main.vista"
                    expect_exit 1 "$binary" "$directory/main.vista" --emit "$mode" --out-dir "$directory/out"
                    cmp "$directory/a.vista" <(printf 'module A {}\n') || fail "$failure/$mode/$alias lost module input"
                    cmp "$directory/out/$artifact" <(printf 'module A {}\n') || fail "$failure/$mode/$alias removed input alias"
                    if [[ $mode = certificate && $artifact != workflow.vcert ]]; then
                        test ! -e "$directory/out/workflow.vcert" || fail 'safe stale certificate survived'
                    fi
                    if [[ $mode = certificate && $artifact = workflow.vcert ]]; then
                        grep -q 'preserv' "$tmp/log" || fail 'protected certificate needs explicit warning'
                    fi
                done
            done
        done
    done
    echo 'F1 malformed-source module aliases preserved; safe stale certificates cleared.'
fi
if [[ $group = F5 || $group = all ]]; then
    printf 'workflow W { use "a.vista" as A use "b.vista" as B start A route A -> B when present(A.attachment) }\n' > "$tmp/presence.vista"
    expect_exit 0 "$binary" "$tmp/presence.vista" --emit ast
    grep -q 'Name A.attachment' "$tmp/log"
    expect_exit 0 "$binary" "$tmp/presence.vista" --emit certificate --out-dir "$tmp/presence-out"
    expect_exit 0 ./build/vista-cert-check "$tmp/presence-out/workflow.vcert" --root .
    for value in missing empty present; do
        case $value in
            missing) printf '' > "$tmp/presence.data";;
            empty) printf 'A.attachment = ""\n' > "$tmp/presence.data";;
            present) printf 'A.attachment = "file.pdf"\n' > "$tmp/presence.data";;
        esac
        expect_exit 0 "$binary" "$tmp/presence.vista" --validate-data "$tmp/presence.data"
        if [[ $value = present ]]; then grep -q 'A->B' "$tmp/log"; else ! grep -q 'A->B' "$tmp/log"; fi
    done
    node - "$tmp/presence-out/workflow.js" "$tmp/presence-out/workflow.vcert" <<'JS'
const fs=require('fs'),assert=require('node:assert/strict');
const {vistaEvaluate}=require(process.argv[2]);
for(const [input,route,active] of [[{},'','A'],[{'A.attachment':''},'','A'],[{'A.attachment':'file.pdf'},'A->B','B']]) {
    const step=vistaEvaluate(input,'A'),whole=vistaEvaluate(input);
    assert.equal(step.selected_route,route);assert.equal(whole.active_module,active);assert.equal(whole.complete,true);
}
const cert=fs.readFileSync(process.argv[3],'utf8');assert(cert.includes('\nstatus PASS\n'));
const inputs=cert.split('\n').filter(l=>l.startsWith('case-input ')).map(l=>decodeURIComponent(l.split(' ')[2]));
const key=Buffer.from('A.attachment').toString('hex');
assert(inputs.some(s=>s.includes('assignment-count 0\n')),'missing case');
assert(inputs.some(s=>s.includes('input '+key+' \n')),'supplied-empty case');
assert(inputs.some(s=>s.includes('input '+key+' '+Buffer.from('file').toString('hex')+'\n')),'present case');
JS
    # Both tokens in the qualified operand can be contextual identifiers.
    printf 'module A { field present: file { label "File" } export present enter when not present(present) or present(present) }\n' > "$tmp/a.vista"
    printf 'workflow W { use "a.vista" as route use "b.vista" as B start route route route -> B when present(route.present) }\n' > "$tmp/context.vista"
    expect_exit 0 "$binary" "$tmp/context.vista" --emit linked --out-dir "$tmp/context-out"
    printf 'module A { field attachment: file { label "Attachment" } export attachment }\n' > "$tmp/a.vista"
    echo 'F5 qualified/local/contextual presence and actual Node/certificate regressions passed.'
fi
if [[ $group = F1 || $group = all ]]; then
    for failure in scanner parser; do
        for mode in workflow certificate linked; do
            directory="$tmp/safe-$failure-$mode"
            mkdir -p "$directory/out"
            cp "$tmp/a.vista" "$directory/a.vista"
            for file in workflow.ir workflow.js workflow.html workflow.vcert; do printf stale > "$directory/out/$file"; done
            if [[ $failure = scanner ]]; then printf 'workflow W { use "a.vista" as A start A } @\n' > "$directory/main.vista";
            else printf 'workflow W { use "a.vista" as A start A\n' > "$directory/main.vista"; fi
            expect_exit 1 "$binary" "$directory/main.vista" --emit "$mode" --out-dir "$directory/out"
            cmp "$tmp/a.vista" "$directory/a.vista"
            test ! -e "$directory/out/workflow.ir" || fail 'safe stale IR survived'
            if [[ $mode != linked ]]; then
                test ! -e "$directory/out/workflow.js" || fail 'safe stale JS survived'
                test ! -e "$directory/out/workflow.html" || fail 'safe stale HTML survived'
            fi
            if [[ $mode = certificate ]]; then test ! -e "$directory/out/workflow.vcert" || fail 'safe stale certificate survived'; fi
        done
    done
    for mode in linked workflow certificate html all module; do
        mkdir -p "$tmp/$mode"
        case $mode in
            linked|workflow|certificate) artifact=workflow.ir; source=main.vista;;
            html|all) artifact=form.html; source=form.vista;;
            module) artifact=module.vobj; source=a.vista;;
        esac
        printf 'form F { field x: text { label "X" } }\n' > "$tmp/form.vista"
        cp "$tmp/$source" "$tmp/$mode/$artifact"
        cp "$tmp/$mode/$artifact" "$tmp/saved"
        expect_exit 2 "$binary" "$tmp/$mode/$artifact" --emit "$mode" --out-dir "$tmp/$mode"
        cmp "$tmp/saved" "$tmp/$mode/$artifact" || fail "$mode changed source"
    done
    # A source dependency can alias an output even with a valid .vista suffix.
    for alias in hardlink symlink parent; do
        mkdir -p "$tmp/$alias/actual/child" "$tmp/$alias/spelled"
        cp "$tmp/a.vista" "$tmp/$alias/actual/workflow.js"
        case $alias in
            hardlink) ln "$tmp/$alias/actual/workflow.js" "$tmp/$alias/a.vista";;
            symlink) ln -s "$tmp/$alias/actual/workflow.js" "$tmp/$alias/a.vista";;
            parent) ln -s "$tmp/$alias/actual/child" "$tmp/$alias/spelled/link"
                    ln -s "$tmp/$alias/spelled/link/../workflow.js" "$tmp/$alias/a.vista";;
        esac
        printf 'workflow W { use "a.vista" as A start A }\n' > "$tmp/$alias/main.vista"
        cp "$tmp/a.vista" "$tmp/saved"
        expect_exit 2 "$binary" "$tmp/$alias/main.vista" --emit workflow --out-dir "$tmp/$alias/actual"
        cmp "$tmp/saved" "$tmp/$alias/a.vista" || fail "$alias changed module input"
    done
    # Top-level aliases must also be refused before cleanup.
    cp "$tmp/main.vista" "$tmp/workflow.html"
    ln -s "$tmp/workflow.html" "$tmp/top.vista"
    expect_exit 2 "$binary" "$tmp/top.vista" --emit workflow --out-dir "$tmp"
    cmp "$tmp/main.vista" "$tmp/top.vista"
    echo 'F1 input/output identity regressions passed.'
fi
if [[ $group = F3 || $group = all ]]; then
    mkdir -p "$tmp/read-out" "$tmp/directory.vista" "$tmp/directory.js"
    for failure in javascript source module missing-source missing-javascript; do
        printf 'stale PASS\n' > "$tmp/read-out/workflow.vcert"
        source="$tmp/main.vista"; args=()
        case $failure in
            javascript) args=(--javascript "$tmp/directory.js");;
            source) source="$tmp/directory.vista";;
            module) printf 'workflow W { use "directory.vista" as A start A }\n' > "$tmp/read.vista"; source="$tmp/read.vista";;
            missing-source) source="$tmp/absent.vista";;
            missing-javascript) args=(--javascript "$tmp/absent.js");;
        esac
        expect_exit 2 "$binary" "$source" --emit certificate --out-dir "$tmp/read-out" "${args[@]}"
        test ! -e "$tmp/read-out/workflow.vcert" || fail "$failure retained stale success certificate"
    done
    echo 'F3 exception-safe read and stale-evidence regressions passed.'
fi
if [[ $group = F4 || $group = all ]]; then
    mkdir -p "$tmp/dir.vista" "$tmp/data-dir"
    printf 'not a module\n' > "$tmp/malformed.vista"
    printf 'not an object\n' > "$tmp/malformed.vobj"
    printf 'not data\n' > "$tmp/malformed.data"
    printf '' > "$tmp/valid.data"
    for item in missing.vista:2 dir.vista:2 malformed.vista:1 malformed.vobj:1; do
        printf 'workflow W { use "%s" as A start A }\n' "${item%:*}" > "$tmp/status.vista"
        for mode in linked workflow certificate partitions data; do
            if [[ $mode = data ]]; then
                expect_exit "${item#*:}" "$binary" "$tmp/status.vista" --validate-data "$tmp/valid.data"
            elif [[ $mode = partitions ]]; then
                expect_exit "${item#*:}" "$binary" "$tmp/status.vista" --emit partitions
            else
                expect_exit "${item#*:}" "$binary" "$tmp/status.vista" --emit "$mode" --out-dir "$tmp/status-out"
            fi
        done
    done
    printf 'form F { field x: text { label "X" } }\n' > "$tmp/form.vista"
    for source in main.vista form.vista; do
        for item in absent.data:2 data-dir:2 malformed.data:1; do
            expect_exit "${item#*:}" "$binary" "$tmp/$source" --validate-data "$tmp/${item%:*}"
        done
    done
    echo 'F4 operational versus malformed module/data status matrix passed.'
fi
if [[ $group = F2 || $group = all ]]; then
    for site in route nested entry check visibility requiredness field unreachable; do
        cp "$tmp/a.vista" "$tmp/original-a"
        cp "$tmp/b.vista" "$tmp/original-b"
        condition='1e100001 > 0'
        route='true'
        case $site in
            route) route=$condition;;
            nested) route="false and (true or ($condition))";;
            entry) printf 'module A { enter when %s }\n' "$condition" > "$tmp/a.vista";;
            check) printf 'module A { check %s message "Number" }\n' "$condition" > "$tmp/a.vista";;
            visibility) printf 'module A { field x: text { label "X" show when %s } }\n' "$condition" > "$tmp/a.vista";;
            requiredness) printf 'module A { field x: text { label "X" required when %s } }\n' "$condition" > "$tmp/a.vista";;
            field) printf 'module A { field x: decimal { label "X" } export x }\n' > "$tmp/a.vista"; route='A.x > 1e100001';;
            unreachable) route=false; printf 'module B { check %s message "Never reached" }\n' "$condition" > "$tmp/b.vista";;
        esac
        printf 'workflow W { use "a.vista" as A use "b.vista" as B start A route A -> B when %s }\n' "$route" > "$tmp/number.vista"
        expect_exit 1 "$binary" "$tmp/number.vista" --emit partitions
        grep -q 'UNSUPPORTED' "$tmp/log" || fail "$site partitions did not reject literal"
        grep -q CERT001 "$tmp/log" || fail "$site did not identify unsupported semantics"
        expect_exit 1 "$binary" "$tmp/number.vista" --emit certificate --out-dir "$tmp/number-out"
        grep -q '^status UNSUPPORTED$' "$tmp/number-out/workflow.vcert" || fail "$site certified unsupported literal"
        mv "$tmp/original-a" "$tmp/a.vista"; mv "$tmp/original-b" "$tmp/b.vista"
    done
    # Constant-only boundary needs no numeric representative expansion.
    for literal in 1e100000 1e-100000; do
        printf 'workflow W { use "a.vista" as A use "b.vista" as B start A route A -> B when %s > 0 }\n' "$literal" > "$tmp/boundary.vista"
        expect_exit 0 "$binary" "$tmp/boundary.vista" --emit certificate --out-dir "$tmp/boundary-out"
        grep -q '^status PASS$' "$tmp/boundary-out/workflow.vcert"
    done
    printf 'module A { field x: integer { label "X" required } check x > 0 message "Positive" }\n' > "$tmp/a.vista"
    expect_exit 0 "$binary" "$tmp/main.vista" --emit certificate --out-dir "$tmp/respondent"
    grep -q '^status PASS$' "$tmp/respondent/workflow.vcert" || fail 'respondent errors must remain certifiable'
    echo 'F2 unsupported source literal and supported respondent-error regressions passed.'
fi
