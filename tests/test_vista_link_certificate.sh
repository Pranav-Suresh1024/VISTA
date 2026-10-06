#!/usr/bin/env bash
set -euo pipefail
tmp=$(mktemp -d)
trap 'rm -rf -- "$tmp"' EXIT
binary=./build/vista
source=examples/vista_link/scholarship.workflow.vista
# Catches absent emission, fabricated hashes, missed step differences, unsafe
# cleanup/quoting, and runtime/protocol failures incorrectly issuing PASS.
if ! "$binary" "$source" --emit certificate --out-dir "$tmp/first"; then
    echo 'FAIL: certificate emission unavailable'; exit 1
fi
for file in workflow.ir workflow.html workflow.js workflow.vcert; do test -s "$tmp/first/$file"; done
checker=./build/vista-cert-check
test -x "$checker" || { echo 'FAIL: independent checker unavailable'; exit 1; }
"$checker" "$tmp/first/workflow.vcert" --root . | grep -q VERIFIED
"$checker" "$tmp/first/workflow.vcert" | grep -q VERIFIED
reject_certificate() {
    local code=0
    "$checker" "$@" > "$tmp/check.log" 2>&1 || code=$?
    test "$code" = 1 || { cat "$tmp/check.log"; echo "FAIL: checker expected exit 1, got $code"; exit 1; }
}
reject_certificate examples/vista_link/errors/corrupt.vcert
reject_certificate "$tmp/missing.vcert"
reject_certificate "$tmp/first/workflow.vcert" --root "$tmp/missing-root"
reject_certificate "$tmp/first/workflow.vcert" --unknown .
env PATH="$tmp/no-runtime" "$checker" "$tmp/first/workflow.vcert" --root . | grep -q VERIFIED
"$binary" "$source" --emit certificate --out-dir "$tmp/second"
cmp "$tmp/first/workflow.vcert" "$tmp/second/workflow.vcert"
node - "$tmp/first/workflow.vcert" <<'JS'
const fs=require('fs'), assert=require('assert'), crypto=require('crypto'),path=require('path');
const lines=fs.readFileSync(process.argv[2],'utf8').trimEnd().split('\n');
assert.equal(lines[0],'VISTA-CERT 1');
for(const line of ['status PASS','workflow ScholarshipApplication','compiler-version 0.8.0','coverage-complete 1']) assert(lines.includes(line));
for(const key of ['workflow-sha256','javascript-sha256','coverage-sha256']) assert(lines.some(l=>new RegExp('^'+key+' [a-f0-9]{64}$').test(l)));
const cases=lines.filter(l=>l.startsWith('case ')).map(l=>l.split(' '));
assert(cases.length>0); assert.equal(Number(lines.find(l=>l.startsWith('case-count ')).split(' ')[1]),cases.length);
assert.equal(new Set(cases.map(c=>c[1])).size,cases.length);
for(const c of cases) { assert.equal(c.length,5); assert.equal(c[3],c[4]); for(const h of c.slice(2))assert(/^[a-f0-9]{64}$/.test(h)); }
const inputs=new Map(lines.filter(l=>l.startsWith('case-input ')).map(l=>{const p=l.split(' ');return [p[1],decodeURIComponent(p[2])];}));
for(const c of cases) assert.equal(crypto.createHash('sha256').update(inputs.get(c[1])).digest('hex'),c[2]);
const target=require(path.join(path.dirname(process.argv[2]),'workflow.js'));
for(const c of cases) {
  const wire=inputs.get(c[1]).split('\n'),active=Buffer.from(wire[1].slice(7),'hex').toString('utf8');
  const input=Object.create(null);
  for(const line of wire.filter(l=>l.startsWith('input '))) {const [,k,v]=line.split(' ');input[Buffer.from(k,'hex').toString('utf8')]=Buffer.from(v,'hex').toString('latin1');}
  const observation=target.vistaCanonicalObservation(target.vistaEvaluate(input,active||undefined,{inputMode:'bytes'}));
  assert.equal(crypto.createHash('sha256').update(observation).digest('hex'),c[4]);
}
assert([...inputs.values()].some(v=>v.startsWith('mode whole\n')));
assert([...inputs.values()].some(v=>v.startsWith('mode step\n')));
assert(lines.some(l=>l.startsWith('artifact ')&&decodeURIComponent(l.split(' ')[1]).startsWith('module-source.')));
JS
# Exact supplied target aliases the output that stale cleanup removes.
cp "$tmp/first/workflow.js" "$tmp/original.js"
"$binary" "$source" --emit certificate --out-dir "$tmp/first" --javascript "$tmp/first/workflow.js"
cmp "$tmp/original.js" "$tmp/first/workflow.js"
cmp "$tmp/first/workflow.vcert" "$tmp/second/workflow.vcert"
odd="$tmp/path with apostrophe's and "$(printf '\377')
mkdir -p "$odd"
cp examples/vista_link/{scholarship.workflow,applicant,certificate}.vista "$odd/"
"$binary" "$odd/scholarship.workflow.vista" --emit certificate --out-dir "$odd/output's"
grep -q '^status PASS$' "$odd/output's/workflow.vcert"
"$checker" "$odd/output's/workflow.vcert" --root .
# Lexical link/.. removal changes the file: the filesystem follows link first.
# Reopen both recorded workflow and module paths, preserving their raw bytes.
links="$odd/artifact-links"
mkdir -p "$links/actual/child" "$links/spelled"
ln -s "$links/actual/child" "$links/spelled/link"
cp examples/vista_link/{scholarship.workflow,applicant,certificate}.vista "$links/actual/"
printf 'different workflow bytes\n' > "$links/spelled/scholarship.workflow.vista"
"$binary" "$links/spelled/link/../scholarship.workflow.vista" --emit certificate --out-dir "$tmp/workflow-link"
sed 's|applicant.vista|spelled/link/../applicant.vista|' "$source" > "$links/module-path.workflow.vista"
cp examples/vista_link/certificate.vista "$links/"
"$binary" "$links/module-path.workflow.vista" --emit certificate --out-dir "$tmp/module-link"
node - "$tmp/workflow-link/workflow.vcert" "$tmp/module-link/workflow.vcert" <<'JS'
const fs=require('fs'),assert=require('assert'),crypto=require('crypto');
const rawToken=token=>Buffer.from(token.replace(/%([0-9A-F]{2})/g,(_,h)=>String.fromCharCode(parseInt(h,16))),'latin1');
const failures=[];
for(const certificate of process.argv.slice(2)) {
  const lines=fs.readFileSync(certificate,'utf8').split('\n');
  assert(lines.includes('status PASS'));
  const checked=new Set();
  for(const line of lines.filter(l=>l.startsWith('artifact '))) {
    const [,encodedRole,encodedPath,expected]=line.split(' '),role=rawToken(encodedRole).toString('ascii');
    if(role!=='workflow-source'&&!role.startsWith('module-source.'))continue;
    checked.add(role);
    try {
      const actual=crypto.createHash('sha256').update(fs.readFileSync(rawToken(encodedPath))).digest('hex');
      if(actual!==expected) failures.push(role+': recorded path hashes different bytes');
    } catch(error) { failures.push(role+': recorded path cannot be read ('+error.code+')'); }
  }
  for(const role of ['workflow-source','module-source.Applicant','module-source.Certificate'])assert(checked.has(role));
}
assert.deepEqual(failures,[],'recorded source paths must reopen the hashed input bytes');
console.log('Symlink/.. workflow-source and module-source raw artifact hashes passed.');
JS
"$checker" "$tmp/workflow-link/workflow.vcert" --root .
"$checker" "$tmp/module-link/workflow.vcert" --root .
# Raw CRLF hash differs from the canonical source hash but is independently named.
mkdir "$tmp/crlf-source"
cp examples/vista_link/{scholarship.workflow,certificate}.vista "$tmp/crlf-source/"
sed 's/$/\r/' examples/vista_link/applicant.vista > "$tmp/crlf-source/applicant.vista"
"$binary" "$tmp/crlf-source/scholarship.workflow.vista" --emit certificate --out-dir "$tmp/crlf"
node - "$tmp/crlf/workflow.vcert" "$tmp/crlf-source/applicant.vista" <<'JS'
const fs=require('fs'),assert=require('assert'),crypto=require('crypto');
const lines=fs.readFileSync(process.argv[2],'utf8').split('\n');
const raw=crypto.createHash('sha256').update(fs.readFileSync(process.argv[3])).digest('hex');
assert(lines.some(l=>l.startsWith('artifact ')&&decodeURIComponent(l.split(' ')[1])==='module-source.Applicant'&&l.endsWith(' '+raw)));
assert(!lines.includes('module-hash Applicant.source '+raw));
JS
"$checker" "$tmp/crlf/workflow.vcert" --root .
cp "$tmp/crlf-source/applicant.vista" "$tmp/saved-source"
printf '\nchanged\n' >> "$tmp/crlf-source/applicant.vista"
reject_certificate "$tmp/crlf/workflow.vcert" --root .
"$checker" "$tmp/crlf/workflow.vcert"
mv "$tmp/saved-source" "$tmp/crlf-source/applicant.vista"
mv "$tmp/crlf-source/applicant.vista" "$tmp/saved-source"
reject_certificate "$tmp/crlf/workflow.vcert" --root .
mv "$tmp/saved-source" "$tmp/crlf-source/applicant.vista"
for artifact in workflow.ir workflow.js workflow.html; do
    cp "$tmp/crlf/$artifact" "$tmp/saved-artifact"
    printf 'changed\n' >> "$tmp/crlf/$artifact"
    reject_certificate "$tmp/crlf/workflow.vcert" --root .
    mv "$tmp/saved-artifact" "$tmp/crlf/$artifact"
    mv "$tmp/crlf/$artifact" "$tmp/saved-artifact"
    reject_certificate "$tmp/crlf/workflow.vcert" --root .
    mv "$tmp/saved-artifact" "$tmp/crlf/$artifact"
done
for input in scholarship.workflow.vista certificate.vista; do
    cp "$tmp/crlf-source/$input" "$tmp/saved-input"
    printf 'changed\n' >> "$tmp/crlf-source/$input"
    reject_certificate "$tmp/crlf/workflow.vcert" --root .
    mv "$tmp/saved-input" "$tmp/crlf-source/$input"
    mv "$tmp/crlf-source/$input" "$tmp/saved-input"
    reject_certificate "$tmp/crlf/workflow.vcert" --root .
    mv "$tmp/saved-input" "$tmp/crlf-source/$input"
done
expect_operational_failure() {
    local code=0
    "$@" > "$tmp/failure.log" 2>&1 || code=$?
    test "$code" = 2 || { cat "$tmp/failure.log"; echo "FAIL: expected operational exit 2, got $code"; exit 1; }
    test ! -e "$tmp/first/workflow.vcert"
}
mkdir "$tmp/no-node"
expect_operational_failure env PATH="$tmp/no-node" "$binary" "$source" --emit certificate --out-dir "$tmp/first"
real_node=$(command -v node)
mkdir "$tmp/bad-runtime"
cat > "$tmp/bad-runtime/node" <<'SH'
#!/bin/sh
case "$PROBE_FAILURE" in
  count) printf 'run\n' >> "$PROBE_COUNT"; exec "$REAL_NODE" "$@";;
  exit) exit 9;;
  malformed) printf 'malformed\n';;
  malformed_observation) printf '636173652e30\t61626364\n';;
  duplicate) "$REAL_NODE" "$@" | awk '{print;print}';;
  incomplete) "$REAL_NODE" "$@" | head -n 1;;
  unterminated) "$REAL_NODE" "$@" | awk '{if(NR>1)printf "\n";printf "%s",$0}';;
esac
SH
chmod +x "$tmp/bad-runtime/node"
env PATH="$tmp/bad-runtime:$PATH" REAL_NODE="$real_node" PROBE_FAILURE=count PROBE_COUNT="$tmp/invocations" "$binary" "$source" --emit certificate --out-dir "$tmp/batch"
test "$(wc -l < "$tmp/invocations")" = 1
for failure in exit malformed malformed_observation duplicate incomplete unterminated; do
    cp "$tmp/second/workflow.vcert" "$tmp/first/workflow.vcert"
    expect_operational_failure env PATH="$tmp/bad-runtime:$PATH" REAL_NODE="$real_node" PROBE_FAILURE="$failure" "$binary" "$source" --emit certificate --out-dir "$tmp/first"
done
printf 'module.exports={};\n' > "$tmp/no-api.js"
expect_operational_failure "$binary" "$source" --emit certificate --out-dir "$tmp/first" --javascript "$tmp/no-api.js"
cp "$tmp/second/workflow.vcert" "$tmp/first/workflow.vcert"
expect_operational_failure "$binary" "$source" --emit certificate --out-dir "$tmp/first" --javascript "$tmp/not-found.js"
cp "$tmp/second/workflow.vcert" "$tmp/first/workflow.vcert"
expect_operational_failure "$binary" "$tmp/missing.vista" --emit certificate --out-dir "$tmp/first"
printf 'workflow Nul { use "applicant.vista\0suffix" as Applicant start Applicant }\n' > "$tmp/nul.vista"
expect_operational_failure "$binary" "$tmp/nul.vista" --emit certificate --out-dir "$tmp/first"
grep -q 'NUL in input path' "$tmp/failure.log"
cp "$tmp/original.js" "$tmp/mutant.js"
cat >> "$tmp/mutant.js" <<'JS'
const originalEvaluate=module.exports.vistaEvaluate;
module.exports.vistaEvaluate=(input,active,options)=>{
  const result=originalEvaluate(input,active,options);
  if(active==='Applicant' && result.fields.length) result.fields[0].visible=!result.fields[0].visible;
  return result;
};
JS
code=0
"$binary" "$source" --emit certificate --out-dir "$tmp/mismatch" --javascript "$tmp/mutant.js" || code=$?
test "$code" = 1
grep -q '^status FAIL$' "$tmp/mismatch/workflow.vcert"
grep -q '^counterexample ' "$tmp/mismatch/workflow.vcert"
grep -q '^counterexample-cli ' "$tmp/mismatch/workflow.vcert"
grep -q '^counterexample-js ' "$tmp/mismatch/workflow.vcert"
code=0
"$binary" examples/vista_link/conditional_cycle.workflow.vista --emit certificate --out-dir "$tmp/cycle" || code=$?
test "$code" = 1
test ! -e "$tmp/cycle/workflow.vcert" || ! grep -q '^status PASS$' "$tmp/cycle/workflow.vcert"
mkdir "$tmp/unsupported-source"
cat > "$tmp/unsupported-source/module.vista" <<'VISTA'
module Huge { field value: text { label "Value" min_length 1000000000 } export value }
VISTA
cat > "$tmp/unsupported-source/main.vista" <<'VISTA'
workflow Huge { use "module.vista" as Huge start Huge }
VISTA
code=0
"$binary" "$tmp/unsupported-source/main.vista" --emit certificate --out-dir "$tmp/unsupported" || code=$?
test "$code" = 1
grep -q '^status UNSUPPORTED$' "$tmp/unsupported/workflow.vcert"
grep -q '^coverage-complete 0$' "$tmp/unsupported/workflow.vcert"
for directory in mismatch cycle unsupported; do reject_certificate "$tmp/$directory/workflow.vcert"; done
# Independent codec link: no scanner, parser, linker, evaluator or partitioner.
cat > "$tmp/codec.cpp" <<'CPP'
#include "certificate.hpp"
#include "hash.hpp"
#include <cassert>
#include <fstream>
#include <iterator>
using namespace vista;
std::string read(const char* p) { std::ifstream f(p,std::ios::binary); return {std::istreambuf_iterator<char>(f),{}}; }
int main(int argc,char** argv) {
    assert(argc>=4);
    auto pass=read(argv[1]); auto result=parse_certificate(pass); assert(result.succeeded());
    assert(serialize_certificate(result.certificate)==pass);
    assert(verify_certificate(result.certificate,nullptr).verified);
    const std::filesystem::path root=".";
    result.certificate.certificate_directory=std::filesystem::path(argv[1]).parent_path();
    assert(verify_certificate(result.certificate,&root).verified);
    auto root_bad=result.certificate;root_bad.workflow_sha256=std::string(64,'0');assert(!verify_certificate(root_bad,&root).verified);
    root_bad=result.certificate;root_bad.module_hashes[0].second=std::string(64,'0');assert(!verify_certificate(root_bad,&root).verified);
    root_bad=result.certificate;root_bad.module_hashes[1].second=std::string(64,'0');assert(!verify_certificate(root_bad,&root).verified);
    auto rejected=[&](Certificate c){assert(!verify_certificate(c,nullptr).verified);};
    auto write_invalid=[&](const char* name,Certificate c){std::ofstream f(std::string(argv[1])+name);f<<serialize_certificate(c);rejected(c);};
    auto bad=result.certificate;bad.cases[0].input="mode step\nactive \nprefix-count 0\nassignment-count 0\n";bad.cases[0].input_sha256=sha256_hex(bad.cases[0].input);bad.coverage_sha256=certificate_coverage_sha256(bad);write_invalid(".state",bad);
    bad=result.certificate;bad.artifacts.erase(bad.artifacts.begin());write_invalid(".artifact",bad);
    bad=result.certificate;bad.cases.erase(bad.cases.begin());write_invalid(".coverage",bad);
    bad=result.certificate;bad.javascript_sha256=std::string(64,'0');write_invalid(".aggregate",bad);
    bad=result.certificate;bad.cases[0].input.insert(bad.cases[0].input.find("prefix-count ")+13,"0");bad.cases[0].input_sha256=sha256_hex(bad.cases[0].input);bad.coverage_sha256=certificate_coverage_sha256(bad);write_invalid(".input-count",bad);
    for(int i=2;i<argc;++i) { auto s=read(argv[i]);auto p=parse_certificate(s);assert(p.succeeded());assert(serialize_certificate(p.certificate)==s);assert(p.certificate.status!=CertificateStatus::Pass); }
    auto invalid=[&](std::string s){assert(!parse_certificate(s).succeeded());};
    invalid(pass.substr(0,pass.size()-1)); invalid(pass+"garbage\n");
    auto c=result.certificate;
    c.cases.push_back(c.cases.front());invalid(serialize_certificate(c));
    c=result.certificate;c.cases[0].cli_sha256=std::string(64,'0');invalid(serialize_certificate(c));
    c=result.certificate;c.expected_case_count++;invalid(serialize_certificate(c));
    c=result.certificate;c.partition_ids.push_back(c.partition_ids.front());invalid(serialize_certificate(c));
    c=result.certificate;c.coverage_sha256=std::string(64,'0');invalid(serialize_certificate(c));
    c=result.certificate;c.cases[0].input+="extra";invalid(serialize_certificate(c));
    c=result.certificate;c.artifacts[0].path=std::string("bad\0path",8);invalid(serialize_certificate(c));
    c=result.certificate;c.artifacts[0].path="space's/%"+std::string(1,'\xff');
    auto p=parse_certificate(serialize_certificate(c));assert(p.succeeded());assert(p.certificate.artifacts[0].path==c.artifacts[0].path);
    const auto at=pass.find("workflow ");auto malformed=pass;malformed.insert(at+9,"%GG");invalid(malformed);
}
CPP
g++ -std=c++17 -Wall -Wextra -Werror -Iinclude "$tmp/codec.cpp" src/certificate.cpp src/hash.cpp src/diagnostic.cpp -o "$tmp/codec"
"$tmp/codec" "$tmp/second/workflow.vcert" "$tmp/mismatch/workflow.vcert" "$tmp/cycle/workflow.vcert" "$tmp/unsupported/workflow.vcert"
for suffix in state artifact coverage aggregate input-count; do reject_certificate "$tmp/second/workflow.vcert.$suffix"; done
node - "$tmp/second/workflow.vcert" "$tmp" <<'JS'
const fs=require('fs'),assert=require('assert');
const original=fs.readFileSync(process.argv[2],'utf8');
const change=(name,from,to)=>{assert(original.includes(from));const changed=original.replace(from,to);assert.notEqual(changed,original);fs.writeFileSync(process.argv[3]+'/'+name+'.vcert',changed);};
change('version','VISTA-CERT 1','VISTA-CERT 2');
change('count','case-count ','case-count 999');
change('complete','coverage-complete 1','coverage-complete 0');
change('digest',/coverage-sha256 [a-f0-9]{64}/.exec(original)[0],'coverage-sha256 '+'0'.repeat(64));
change('duplicate',/^case (\S+) /m.exec(original)[0],/^case (\S+) /m.exec(original)[0].replace('case.0','case.1'));
change('percent','workflow ScholarshipApplication','workflow %GG');
const row=/^case \S+ [a-f0-9]{64} ([a-f0-9]{64})/m.exec(original);change('case-hash',row[0],row[0].slice(0,-64)+'0'.repeat(64));
fs.writeFileSync(process.argv[3]+'/truncated.vcert',original.slice(0,-1));
fs.writeFileSync(process.argv[3]+'/trailing.vcert',original+'junk\n');
JS
for variant in version count complete digest duplicate percent case-hash truncated trailing; do reject_certificate "$tmp/$variant.vcert"; done
# A later-step mismatch may delete payloads, but must retain the reaching prefix.
cp "$tmp/original.js" "$tmp/later.js"
sed "s/active==='Applicant'/active==='Certificate'/" < <(tail -n 6 "$tmp/mutant.js") >> "$tmp/later.js"
code=0
"$binary" "$source" --emit certificate --out-dir "$tmp/later" --javascript "$tmp/later.js" || code=$?
test "$code" = 1
node - "$tmp/later/workflow.vcert" <<'JS'
const fs=require('fs'),assert=require('assert');
const line=fs.readFileSync(process.argv[2],'utf8').split('\n').find(l=>l.startsWith('counterexample '));
const wire=decodeURIComponent(line.slice(15));
assert(wire.startsWith('mode step\nactive '+Buffer.from('Certificate').toString('hex')+'\n'));
assert(wire.includes('prefix-count 1\nprefix '+Buffer.from('Applicant->Certificate').toString('hex')+'\n'));
assert(wire.includes('assignment-count 1\ninput '+Buffer.from('Applicant.category').toString('hex')+' '+Buffer.from('reserved').toString('hex')+'\n'));
JS
# Deleting an assignment must not turn a terminal whole-case into a different route.
mkdir "$tmp/prefix-source"
cat > "$tmp/prefix-source/a.vista" <<'VISTA'
module A { field flag: boolean { label "Flag" } export flag }
VISTA
cat > "$tmp/prefix-source/b.vista" <<'VISTA'
module B { field note: text { label "Text" } export note }
VISTA
cat > "$tmp/prefix-source/main.vista" <<'VISTA'
workflow Prefix { use "a.vista" as A use "b.vista" as B start A route A -> B when A.flag == false }
VISTA
"$binary" "$tmp/prefix-source/main.vista" --emit workflow --out-dir "$tmp/prefix-base"
cp "$tmp/prefix-base/workflow.js" "$tmp/prefix-mutant.js"
cat >> "$tmp/prefix-mutant.js" <<'JS'
const original=module.exports.vistaEvaluate;
module.exports.vistaEvaluate=(input,active,options)=>{
  const r=original(input,active,options);
  if(!active && r.fields.length)r.fields[0].visible=!r.fields[0].visible;
  return r;
};
JS
code=0
"$binary" "$tmp/prefix-source/main.vista" --emit certificate --out-dir "$tmp/prefix" --javascript "$tmp/prefix-mutant.js" || code=$?
test "$code" = 1
node - "$tmp/prefix/workflow.vcert" <<'JS'
const fs=require('fs'),assert=require('assert');
const wire=decodeURIComponent(fs.readFileSync(process.argv[2],'utf8').split('\n').find(l=>l.startsWith('counterexample ')).slice(15));
assert(wire.startsWith('mode whole\nactive \nprefix-count 0\n'));
assert(wire.includes('input '+Buffer.from('A.flag').toString('hex')+' '+Buffer.from('true').toString('hex')+'\n'));
JS
"$binary" examples/vista_link/all_type_routes.workflow.vista --emit certificate --out-dir "$tmp/all-types"
"$checker" "$tmp/all-types/workflow.vcert" --root .
grep -q '^status PASS$' "$tmp/all-types/workflow.vcert"
"$tmp/codec" "$tmp/all-types/workflow.vcert" "$tmp/mismatch/workflow.vcert" "$tmp/cycle/workflow.vcert"
mkdir "$tmp/object-source"
"$binary" "$tmp/crlf-source/applicant.vista" --emit module --out-dir "$tmp/object-source"
cp examples/vista_link/certificate.vista "$tmp/object-source/"
sed 's/applicant.vista/module.vobj/' "$source" > "$tmp/object-source/main.vista"
"$binary" "$tmp/object-source/main.vista" --emit certificate --out-dir "$tmp/object-cert"
"$checker" "$tmp/object-cert/workflow.vcert" --root .
"$tmp/codec" "$tmp/object-cert/workflow.vcert" "$tmp/mismatch/workflow.vcert" "$tmp/cycle/workflow.vcert"
node - "$tmp/object-cert/workflow.vcert" "$tmp/object-source/module.vobj" <<'JS'
const fs=require('fs'),assert=require('assert'),crypto=require('crypto');
const raw=crypto.createHash('sha256').update(fs.readFileSync(process.argv[3])).digest('hex');
assert(fs.readFileSync(process.argv[2],'utf8').split('\n').some(l=>l.startsWith('artifact ')&&decodeURIComponent(l.split(' ')[1])==='module-object.Applicant'&&l.endsWith(' '+raw)));
JS
cp "$tmp/object-source/module.vobj" "$tmp/saved-object"
sed -i 's/label "Applicant category"/label "Changed!! category"/' "$tmp/object-source/module.vobj"
test "$(wc -c < "$tmp/saved-object")" = "$(wc -c < "$tmp/object-source/module.vobj")"
cmp -s "$tmp/saved-object" "$tmp/object-source/module.vobj" && { echo 'FAIL: stale object mutation did not apply'; exit 1; }
reject_certificate "$tmp/object-cert/workflow.vcert" --root .
code=0
"$binary" "$tmp/object-source/main.vista" --emit certificate --out-dir "$tmp/stale-object" > "$tmp/stale.log" 2>&1 || code=$?
test "$code" != 0
grep -q MOD004 "$tmp/stale.log"
test ! -e "$tmp/stale-object/workflow.vcert"
mv "$tmp/object-source/module.vobj" "$tmp/missing-object"
reject_certificate "$tmp/object-cert/workflow.vcert" --root .
echo 'VISTA-Link certificate tests passed.'
