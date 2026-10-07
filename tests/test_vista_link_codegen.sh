#!/usr/bin/env bash
set -euo pipefail
tmp=$(mktemp -d)
trap 'rm -rf -- "$tmp"' EXIT
# Missing workflow output, divergent scalar semantics, lost intermediate state,
# unsafe author strings, and stale success artifacts must all fail this check.
if ! ./build/vista examples/vista_link/scholarship.workflow.vista --emit workflow --out-dir "$tmp/demo"; then
    echo 'FAIL: workflow emission unavailable'; exit 1
fi
for file in workflow.ir workflow.js workflow.html; do test -s "$tmp/demo/$file"; done
for group in numeric textual selection; do
    case "$group" in
        numeric) types='integer|decimal|date|boolean';;
        textual) types='text|textarea|email|phone|boolean';;
        selection) types='choice|file|boolean';;
    esac
    mkdir "$tmp/$group-source"
    cp examples/vista_link/{all_type_routes.workflow,applicant}.vista "$tmp/$group-source/"
    awk -v types="$types" '$1 != "field" || $3 ~ ("^(" types ")$")' examples/vista_link/all_type_routes.vista > "$tmp/$group-source/all_type_routes.vista"
done
grep -h '^  field ' "$tmp/"*-source/all_type_routes.vista | sort -u > "$tmp/group-fields"
grep '^  field ' examples/vista_link/all_type_routes.vista | sort > "$tmp/original-fields"
cmp "$tmp/original-fields" "$tmp/group-fields"
cat > "$tmp/probe.cpp" <<'CPP'
#include "codegen.hpp"
#include "evaluator.hpp"
#include "partition.hpp"
#include "linker.hpp"
#include "scanner.hpp"
#include "parser.hpp"
#include <cassert>
#include <fstream>
#include <iostream>
using namespace vista;
std::string hex(const std::string& s) {
    std::string out; for (unsigned char c : s) { out += "0123456789abcdef"[c >> 4]; out += "0123456789abcdef"[c & 15]; } return out;
}
IrExpression name(std::string s, ValueType t = ValueType::Text) { return {ExpressionKind::Name,t,s,{},false}; }
IrExpression lit(std::string s, ValueType t = ValueType::Text) { return {ExpressionKind::StringLiteral,t,s,{},false}; }
IrExpression op(std::string s, IrExpression a, IrExpression b) { return {ExpressionKind::Binary,ValueType::Boolean,s,{a,b},false}; }
LinkedField field(std::string s, FieldType t) {
    LinkedField f; f.qualified_name=s; f.type=t; f.label="Label </script> & "+s; f.help="Help <img>";
    f.visibility_rule_id=s+".visible"; f.requiredness_rule_id=s+".required"; return f;
}
void artifacts(const LinkedWorkflow& w, const std::string& path) {
    const auto js=generate_workflow_javascript(w);
    std::ofstream(path+".js") << js;
    std::ofstream(path+".html") << generate_workflow_html(w,js);
}
void probe(const LinkedWorkflow& w, const std::string& path, const InputValues& input, const std::string& active="") {
    const auto result=active.empty()?evaluate_workflow(w,input):evaluate_workflow_step(w,input,active);
    std::cout << "{\"js\":\"" << path << ".js\",\"active\":\"" << active << "\",\"input\":{";
    bool first=true; for (const auto& x:input) { if(!first)std::cout<<','; first=false; std::cout<<'"'<<x.first<<"\":\""<<hex(x.second)<<'"'; }
    std::cout << "},\"expected\":\"" << hex(canonical_observation(result.observation)) << "\"}\n";
}
int main(int argc,char** argv) {
    assert(argc==2); const std::string root=argv[1];
    auto parsed=parse_tokens(scan_file("examples/vista_link/scholarship.workflow.vista").tokens); assert(parsed.succeeded());
    auto linked=link_workflow(*parsed.workflow,"examples/vista_link/scholarship.workflow.vista"); assert(linked.succeeded());
    auto p=derive_partitions(linked.workflow); auto suite=enumerate_route_cases(linked.workflow,p); assert(suite.complete());
    artifacts(linked.workflow,root+"/suite");
    for(const auto& c:suite.cases) probe(linked.workflow,root+"/suite",c.values,c.active_module);
    std::cerr<<"Compared demo partition cases: "<<suite.cases.size()<<'\n';
    parsed=parse_tokens(scan_file("examples/vista_link/all_type_routes.workflow.vista").tokens); assert(parsed.succeeded());
    linked=link_workflow(*parsed.workflow,"examples/vista_link/all_type_routes.workflow.vista"); assert(linked.succeeded());
    p=derive_partitions(linked.workflow); suite=enumerate_route_cases(linked.workflow,p); assert(!suite.complete());
    bool limited=false; for(const auto& d:suite.diagnostics)limited=limited||d.code=="CERT003";assert(limited);
    // Still execute a concrete full-source case; exhaustive groups follow below.
    artifacts(linked.workflow,root+"/combined");
    probe(linked.workflow,root+"/combined",{{"AllTypes.boolean_value","true"},{"Finish.category","general"},
        {"AllTypes.integer_value","18"},{"AllTypes.decimal_value","1.5"},{"AllTypes.date_value","2024-03-01"},
        {"AllTypes.choice_value","first"},{"AllTypes.text_value","name"},{"AllTypes.textarea_value","note"},
        {"AllTypes.email_value","a@b.c"},{"AllTypes.phone_value","1234567"},{"AllTypes.file_value","file.pdf"}});
    std::size_t group_cases=0;
    for(const std::string group:{"numeric","textual","selection"}) {
        const auto source=root+"/"+group+"-source/all_type_routes.workflow.vista";
        parsed=parse_tokens(scan_file(source).tokens);assert(parsed.succeeded());
        linked=link_workflow(*parsed.workflow,source);assert(linked.succeeded());
        p=derive_partitions(linked.workflow);suite=enumerate_route_cases(linked.workflow,p);assert(suite.complete());
        artifacts(linked.workflow,root+"/"+group);
        bool finish=false,whole=false;
        for(const auto& c:suite.cases){probe(linked.workflow,root+"/"+group,c.values,c.active_module);finish=finish||c.active_module=="Finish";whole=whole||c.active_module.empty();}
        assert(finish&&whole);group_cases+=suite.cases.size();
        std::cerr<<"Compared "<<group<<" all-type partition cases: "<<suite.cases.size()<<'\n';
    }
    std::cerr<<"Compared all-type grouped partition cases: "<<group_cases<<'\n';
    LinkedWorkflow w; w.name="Safe </script>"; w.start_alias="A";
    LinkedModule a; a.alias="A"; a.name="A"; a.entry_rule_id="A.entry";
    const std::vector<FieldType> types={FieldType::Text,FieldType::Integer,FieldType::Decimal,FieldType::Boolean,FieldType::Date,FieldType::Choice,FieldType::File,FieldType::Email,FieldType::Phone,FieldType::Textarea};
    for(std::size_t i=0;i<types.size();++i) { auto f=field("A.f"+std::to_string(i),types[i]); f.options={"yes","é"}; a.exports.push_back(f.qualified_name); a.fields.push_back(f); }
    a.fields[0].constraints={{PropertyKind::MinLength,"2","A.min"},{PropertyKind::MaxLength,"2","A.max"}};
    a.fields[1].constraints={{PropertyKind::Minimum,"-1","A.imin"},{PropertyKind::Maximum,"9007199254740993","A.imax"}};
    a.fields[2].constraints={{PropertyKind::Minimum,"-0.1","A.dmin"},{PropertyKind::Maximum,"1e2","A.dmax"}};
    a.fields[3].requiredness={ExpressionKind::BooleanLiteral,ValueType::Boolean,"true",{},false};
    a.fields[4].constraints={{PropertyKind::Minimum,"2024-02-28","A.date.min"},{PropertyKind::Maximum,"2024-03-01","A.date.max"}};
    a.fields[7].visibility={ExpressionKind::BooleanLiteral,ValueType::Boolean,"false",{},false};
    a.checks={{op("==",name("A.f0"),lit("é")),"Unicode </script>","A.unicode"},
              {op("==",name("A.f7"),lit("bad")),"Hidden check","A.hidden"},
              {op("and",{ExpressionKind::BooleanLiteral,ValueType::Boolean,"false",{},false},op("<",name("A.f1",ValueType::Integer),{ExpressionKind::IntegerLiteral,ValueType::Integer,"1",{},false})),"Eager and","A.and"},
              {op("or",{},op("==",name("A.f2",ValueType::Decimal),{ExpressionKind::DecimalLiteral,ValueType::Decimal,"0",{},false})),"Eager or","A.or"}};
    w.modules.push_back(a); artifacts(w,root+"/scalar"); probe(w,root+"/scalar",{});
    const std::vector<std::string> values={"","true","false","0","-0","+1","1.00","1e2","-1e-1","9007199254740993","9007199254740994","1e100000","1e100001","10e100000","0e-100000","0.0e-100000","1e-100000","NaN","0x1","2024-02-29","2023-02-29","0000-01-01","2000-02-29","1900-02-29","a@b.c","a@..b","a@b.c\n","a@b.\xc2\xa0","1234567","1234567\n","+(123) 456-7","1234567890123456","é","yes"," </script>",std::string(4097,'1'),std::string(1,'\xff'),std::string("\0@b.c",5),std::string("\xc3",1)};
    for(const auto& f:a.fields) for(const auto& v:values) probe(w,root+"/scalar",{{f.qualified_name,v}},"A");
    for(int b=0;b<256;++b) probe(w,root+"/scalar",{{"A.f0",std::string(1,static_cast<char>(b))},{"A.f7",std::string("a@b.")+static_cast<char>(b)}});
    // Constraints and both eager operands become individually observable here.
    w.modules[0].checks.clear(); artifacts(w,root+"/plain");
    for(const auto& f:a.fields) for(const auto& v:values) probe(w,root+"/plain",{{f.qualified_name,v},{"A.f3","true"}});
    probe(w,root+"/plain",{{"unknown","x"}},"A"); probe(w,root+"/plain",{},"Unknown");
    // UTF-16 places U+10000 before U+E000; C++ UTF-8 byte ordering reverses them.
    probe(w,root+"/plain",{{u8"\U00010000","x"},{u8"\uE000","x"}},"A");
    for(const std::string relation:{"==","!=","<","<=",">",">="})
        w.modules[0].checks.push_back({op(relation,name("A.f1",ValueType::Integer),name("A.f2",ValueType::Decimal)),"Numeric relation", "A.numeric."+relation});
    w.modules[0].checks.push_back({{ExpressionKind::Unary,ValueType::Boolean,"present",{name("A.f6",ValueType::File)},false},"File present","A.present"});
    w.modules[0].checks.push_back({{ExpressionKind::Unary,ValueType::Boolean,"not",{name("A.f3",ValueType::Boolean)},false},"Not boolean","A.not"});
    w.modules[0].checks.push_back({op("==",name("A.f5",ValueType::Choice),{ExpressionKind::Name,ValueType::Choice,"é",{},true}),"Choice literal","A.choice"});
    w.modules[0].checks.push_back({op("<=",name("A.f4",ValueType::Date),lit("2024-02-29",ValueType::Date)),"Date relation","A.date"});
    artifacts(w,root+"/relations");
    for(const std::string x:{"-1","0","1","9007199254740993","bad"})
        for(const std::string y:{"-1e0","-0e100000","1.0","9007199254740992","9007199254740993","9007199254740994","1e100001"})
            for(const std::string file:{"","false","0"," "})
                probe(w,root+"/relations",{{"A.f1",x},{"A.f2",y},{"A.f3","true"},{"A.f6",file},{"A.f5","é"},{"A.f4","2024-02-29"}});
    w.modules[0].checks.clear();
    w.modules[0].entry=op("or",{},op("==",name("A.f1",ValueType::Integer),{ExpressionKind::IntegerLiteral,ValueType::Integer,"0",{},false}));
    artifacts(w,root+"/entry"); probe(w,root+"/entry",{{"A.f1","invalid"}});
    w.modules[0].entry={ExpressionKind::BooleanLiteral,ValueType::Boolean,"false",{},false}; artifacts(w,root+"/falseentry"); probe(w,root+"/falseentry",{});
    w.modules[0].entry={}; w.modules[0].fields.clear(); w.modules[0].exports.clear();
    LinkedModule b; b.alias="B"; b.entry_rule_id="B.entry"; w.modules.push_back(b);
    w.routes={{"A","B",{},"A.route"},{"B","A",{},"B.route"}}; artifacts(w,root+"/cycle"); probe(w,root+"/cycle",{});
    w.routes={{"A","B",{},"A.route1"},{"A","B",{},"A.route2"}}; artifacts(w,root+"/ambiguous"); probe(w,root+"/ambiguous",{});
    w.routes={{"A","B",op("or",{},op("==",name("A.missing"),lit("x"))),"A.route"}}; artifacts(w,root+"/invalidroute"); probe(w,root+"/invalidroute",{});
    w.routes.clear(); auto f=field("A.t",FieldType::Text); f.constraints={{PropertyKind::MinLength,"bad","A.bad"}};
    f.visibility=op("==",name("A.missing"),lit("x")); f.requiredness=f.visibility;
    w.modules[0].fields={f}; artifacts(w,root+"/invalidconditions"); probe(w,root+"/invalidconditions",{{"A.t","x"}});
    w.modules[0].fields[0].visibility={}; w.modules[0].fields[0].requiredness={};
    artifacts(w,root+"/invalidconstraint"); probe(w,root+"/invalidconstraint",{{"A.t","x"}});
}
CPP
g++ -std=c++17 -O2 -Iinclude -Ibuild "$tmp/probe.cpp" src/{codegen,evaluator,partition,linker,module,hash,semantic,ast,token,parser,diagnostic}.cpp build/vista_{lexer,parser}.cpp -o "$tmp/probe"
"$tmp/probe" "$tmp" > "$tmp/cases.jsonl"
node - "$tmp" <<'JS'
const fs=require('node:fs'), vm=require('node:vm'), assert=require('node:assert/strict');
const root=process.argv[2]; let count=0;
for(const line of fs.readFileSync(root+'/cases.jsonl','utf8').trim().split('\n')) {
  const c=JSON.parse(line), api=require(c.js);
  const input=Object.fromEntries(Object.entries(c.input).map(([k,v])=>[k,Buffer.from(v,'hex').toString('latin1')]));
  const result=api.vistaEvaluate(input,c.active||undefined,{inputMode:'bytes'});
  assert.equal(Buffer.from(api.vistaCanonicalObservation(result),'utf8').toString('hex'),c.expected,JSON.stringify(c));
  // Every well-formed Unicode input must reach the identical byte semantics.
  if(Object.values(input).every(s=>Buffer.from(s,'latin1').equals(Buffer.from(Buffer.from(s,'latin1').toString('utf8'))))) {
    const unicode=Object.fromEntries(Object.entries(input).map(([k,v])=>[k,Buffer.from(v,'latin1').toString('utf8')]));
    assert.equal(api.vistaCanonicalObservation(api.vistaEvaluate(unicode,c.active||undefined)),api.vistaCanonicalObservation(result));
  }
  count++;
}
const js=fs.readFileSync(root+'/demo/workflow.js','utf8'), html=fs.readFileSync(root+'/demo/workflow.html','utf8');
assert.ok(html.includes('<script>\n'+js+'</script>'));
assert.ok(html.includes('novalidate')); assert.ok(html.includes('role="alert"')); assert.ok(html.includes('role="status"'));
assert.ok(html.includes('aria-labelledby="module-Applicant-title"')); assert.ok(html.includes('for="Applicant.category"'));
assert.equal(require(root+'/demo/workflow.js').vistaEvaluate({'Applicant.category':'reserved'}).selected_route,'Applicant->Certificate');
const api=require(root+'/demo/workflow.js');
assert.throws(()=>api.vistaEvaluate({'Applicant.category':true}),TypeError);
assert.throws(()=>api.vistaEvaluate({},undefined,{inputMode:'unknown'}),TypeError);
assert.throws(()=>api.vistaEvaluate({'Applicant.category':'€'},undefined,{inputMode:'bytes'}),TypeError);
const safe=fs.readFileSync(root+'/scalar.html','utf8'); assert.equal((safe.match(/<\/script>/gi)||[]).length,2);
// Execute the actual embedded scripts and handlers with a small DOM boundary.
const controls=[{id:'Applicant.category',dataset:{type:'choice'},value:'reserved',setAttribute(){},closest(){return {hidden:false};}},
 {id:'Certificate.certificate',dataset:{type:'file'},files:[],value:'',setAttribute(){},closest(){return {hidden:false};}}];
for(const c of controls) { c.attributes={}; c.setAttribute=(k,v)=>{c.attributes[k]=v;}; c.getAttribute=k=>c.attributes[k]; c.focus=()=>{}; }
const events={}, sections=['Applicant','Certificate'].map(alias=>({dataset:{module:alias},hidden:false,querySelector(){return {focus(){}};}}));
const nodes={'vista-workflow':{addEventListener(n,f){events[n]=f;}},'status':{textContent:''},'validation-errors':{items:[],replaceChildren(){this.items=[];},appendChild(x){this.items.push(x);}}, ...Object.fromEntries(controls.map(c=>[c.id,c]))};
const document={getElementById(id){return nodes[id];},querySelectorAll(selector){return selector==='[data-module]'?sections:controls;},createElement(){return {textContent:''};}};
const context=vm.createContext({document,TextEncoder});
for(const m of html.matchAll(/<script>\n([\s\S]*?)<\/script>/g)) vm.runInContext(m[1],context);
assert.equal(sections[0].hidden,false); assert.equal(sections[1].hidden,true);
const real=context.vistaEvaluate; let calls=0; context.vistaEvaluate=(...args)=>{calls++; return real(...args);};
events.submit({preventDefault(){}}); assert.ok(calls>0); assert.equal(sections[1].hidden,false);
assert.equal(controls[1].required,true); events.submit({preventDefault(){}}); assert.ok(nodes['validation-errors'].items.length);
controls[1].files=[{name:'certificate.pdf'}]; events.submit({preventDefault(){}}); assert.match(nodes.status.textContent,/complete/i);
console.log(`Workflow C++/JavaScript canonical observations passed: ${count} cases; embedded browser progression passed.`);
JS
# Re-emission is deterministic, and every failure removes all three old artifacts.
./build/vista examples/vista_link/scholarship.workflow.vista --emit workflow --out-dir "$tmp/repeat" >/dev/null
for file in workflow.ir workflow.js workflow.html; do cmp "$tmp/demo/$file" "$tmp/repeat/$file"; done
printf '%s\n' 'workflow Bad { @ }' > "$tmp/scan.vista"
printf '%s\n' 'workflow Bad {' > "$tmp/parse.vista"
for source in "$tmp/missing.vista" "$tmp/scan.vista" "$tmp/parse.vista" examples/vista_link/errors/missing_export.workflow.vista examples/valid_scholarship.vista; do
    cp "$tmp/demo/"* "$tmp/repeat/"
    if ./build/vista "$source" --emit workflow --out-dir "$tmp/repeat" >"$tmp/error" 2>&1; then echo 'FAIL: invalid workflow accepted'; exit 1; fi
    for file in workflow.ir workflow.js workflow.html; do test ! -e "$tmp/repeat/$file"; done
done
echo 'VISTA-Link workflow generation tests passed.'
