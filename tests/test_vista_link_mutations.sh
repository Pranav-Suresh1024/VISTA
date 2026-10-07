#!/usr/bin/env bash
set -euo pipefail
# Real generated-target regressions: wrong predicates, bounds and presence.
tmp=$(mktemp -d)
trap 'rm -rf -- "$tmp"' EXIT
node - "$tmp" <<'JS'
const fs=require('node:fs'),path=require('node:path');
const root=process.argv[2];
const fixtures={
  boolean:'field value: boolean { label "Consent" } check value == true message "Consent needed"',
  choice:'field value: choice { first, second } { label "Category" } check value == first message "First category needed"',
  number:'field value: integer { label "Age" minimum 18 }',
  date:'field value: date { label "Date" minimum "2024-03-01" }',
  text:'field value: text { label "Name" required }',
  file:'field value: file { label "Document" required }'
};
for(const [name,body] of Object.entries(fixtures)) {
  const dir=path.join(root,name);fs.mkdirSync(dir);
  fs.writeFileSync(dir+'/module.vista','module Probe { '+body+' export value }\n');
  fs.writeFileSync(dir+'/main.vista','workflow Mutation { use "module.vista" as Probe start Probe }\n');
}
JS
# Replay uses the real parser/linker/evaluator and certificate codec, not a model.
cat > "$tmp/replay.cpp" <<'CPP'
#include "certificate.hpp"
#include "evaluator.hpp"
#include "linker.hpp"
#include "parser.hpp"
#include "scanner.hpp"
#include <cassert>
#include <fstream>
#include <iterator>
#include <sstream>
using namespace vista;
std::string unhex(const std::string& s) {
    assert(s.size()%2==0);std::string out;
    for(std::size_t i=0;i<s.size();i+=2)out+=static_cast<char>(std::stoul(s.substr(i,2),nullptr,16));
    return out;
}
int main(int argc,char** argv) {
    assert(argc==4);
    const auto parsed=parse_tokens(scan_file(argv[1]).tokens);assert(parsed.succeeded());
    const auto linked=link_workflow(*parsed.workflow,argv[1]);assert(linked.succeeded());
    std::ifstream in(argv[2],std::ios::binary);assert(in);
    const std::string bytes{std::istreambuf_iterator<char>(in),{}};
    const auto cert=parse_certificate(bytes);assert(cert.succeeded());
    const auto& c=cert.certificate;assert(c.status==CertificateStatus::Fail&&!c.counterexample.empty());
    std::istringstream wire(c.counterexample);std::string line;
    std::getline(wire,line);const bool whole=line=="mode whole";assert(whole||line=="mode step");
    std::getline(wire,line);assert(line.substr(0,7)=="active ");const auto active=unhex(line.substr(7));
    std::getline(wire,line);assert(line.substr(0,13)=="prefix-count ");auto count=std::stoul(line.substr(13));
    std::vector<std::string> prefix;
    while(count--){std::getline(wire,line);assert(line.substr(0,7)=="prefix ");prefix.push_back(unhex(line.substr(7)));}
    std::getline(wire,line);assert(line.substr(0,17)=="assignment-count ");count=std::stoul(line.substr(17));
    InputValues values;
    while(count--){std::getline(wire,line);const auto space=line.find(' ',6);assert(line.substr(0,6)=="input ");values.emplace(unhex(line.substr(6,space-6)),unhex(line.substr(space+1)));}
    assert(!std::getline(wire,line));
    auto state=linked.workflow.start_alias;
    for(const auto& route:prefix){const auto step=evaluate_workflow_step(linked.workflow,values,state);assert(step.succeeded()&&step.observation.selected_route==route);state=route.substr(route.find("->")+2);}
    const auto result=whole?evaluate_workflow(linked.workflow,values):evaluate_workflow_step(linked.workflow,values,active);
    if(whole)assert(result.observation.transition_trace==prefix);else assert(state==active);
    const auto observation=canonical_observation(result.observation);
    assert(observation==c.counterexample_cli&&observation!=c.counterexample_javascript);
    std::ofstream out(argv[3],std::ios::binary);out<<observation;out.close();assert(out);
}
CPP
g++ -std=c++17 -O2 -Iinclude -Ibuild "$tmp/replay.cpp" src/{certificate,evaluator,linker,module,hash,semantic,ast,token,parser,diagnostic}.cpp build/vista_{lexer,parser}.cpp -o "$tmp/replay"
for mutation in boolean choice number date text file; do
    directory="$tmp/$mutation"
    ./build/vista "$directory/main.vista" --emit certificate --out-dir "$directory/base"
    ./build/vista-cert-check "$directory/base/workflow.vcert" --root . >/dev/null
    node - "$directory" "$mutation" <<'JS'
const fs=require('node:fs'),assert=require('node:assert/strict');
const dir=process.argv[2],name=process.argv[3],original=fs.readFileSync(dir+'/base/workflow.js','utf8');
const quote=s=>'"'+[...Buffer.from(s)].map(b=>'\\u00'+b.toString(16).padStart(2,'0')).join('')+'"';
const presence="present:text !== undefined && text !== ''";
const changes={
  boolean:['[6,3,'+quote('==')+',','[6,3,'+quote('!=')+','],
  choice:[quote('first')+',true,[]]',quote('second')+',true,[]]'],
  number:[quote('18'),quote('19')],
  date:[quote('2024-03-01'),quote('2024-03-02')],
  text:[presence,"present:text !== undefined && (field.type === 0 || text !== '')"],
  file:[presence,"present:field.type === 6 || (text !== undefined && text !== '')"]
};
const [from,to]=changes[name];
assert.equal(original.split(from).length-1,1,name+': replacement must apply exactly once');
const mutant=original.replace(from,to);assert.notEqual(mutant,original);
fs.writeFileSync(dir+'/mutant.js',mutant);
JS
    code=0
    ./build/vista "$directory/main.vista" --emit certificate --out-dir "$directory/mutated" --javascript "$directory/mutant.js" || code=$?
    test "$code" = 1
    "$tmp/replay" "$directory/main.vista" "$directory/mutated/workflow.vcert" "$directory/replay-cli"
    node - "$directory" "$mutation" <<'JS'
const fs=require('node:fs'),assert=require('node:assert/strict');
const dir=process.argv[2],name=process.argv[3];
const lines=fs.readFileSync(dir+'/mutated/workflow.vcert','utf8').trimEnd().split('\n');
assert(lines.includes('status FAIL'));
const token=key=>{const row=lines.find(l=>l.startsWith(key+' '));assert(row);const value=row.slice(key.length+1);assert.notEqual(value,'-');return Buffer.from(value.replace(/%([0-9A-F]{2})/g,(_,h)=>String.fromCharCode(parseInt(h,16))),'latin1');};
const wire=token('counterexample').toString('ascii').trimEnd().split('\n');
const active=Buffer.from(wire[1].slice(7),'hex').toString('utf8'),input=Object.create(null);
for(const row of wire.filter(l=>l.startsWith('input '))){const [,k,v='']=row.split(' ');input[Buffer.from(k,'hex').toString('utf8')]=Buffer.from(v,'hex').toString('latin1');}
if(name==='text')assert.equal(input['Probe.value'],'','empty text must be replayed explicitly');
const api=require(dir+'/mutated/workflow.js');
const replay=Buffer.from(api.vistaCanonicalObservation(api.vistaEvaluate(input,active||undefined,{inputMode:'bytes'})));
const cli=token('counterexample-cli'),js=token('counterexample-js');
assert(cli.equals(fs.readFileSync(dir+'/replay-cli')));assert(js.equals(replay));assert(!cli.equals(js));
assert(lines.filter(l=>l.startsWith('case ')).some(l=>{const p=l.split(' ');return p[3]!==p[4];}));
const rule={boolean:'Probe.check.1',choice:'Probe.check.1',number:'Probe.value.minimum',date:'Probe.value.minimum',text:'Probe.value.required',file:'Probe.value.required'}[name];
assert(cli.includes(Buffer.from('Probe.value'))||js.includes(Buffer.from('Probe.value')));
assert(cli.includes(Buffer.from(rule))||js.includes(Buffer.from(rule)),name+': differing rule absent');
// Each retained assignment is needed to reproduce this mismatch at this prefix.
const original=require(dir+'/base/workflow.js');
for(const key of Object.keys(input)) {
  const smaller={...input};delete smaller[key];
  const a=original.vistaEvaluate(smaller,active||undefined,{inputMode:'bytes'});
  const b=api.vistaEvaluate(smaller,active||undefined,{inputMode:'bytes'});
  assert.equal(original.vistaCanonicalObservation(a),api.vistaCanonicalObservation(b),name+': removable assignment');
}
console.log(name+': PASS baseline, FAIL mutant, differing '+rule+', minimized C++/JS replay passed');
JS
done
echo 'Six real generated-JavaScript mutations and witness replays passed.'
