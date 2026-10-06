#include "certify.hpp"
#include "evaluator.hpp"
#include "hash.hpp"
#include "linker.hpp"
#include "version.hpp"
#include <algorithm>
#include <charconv>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>

namespace vista {
namespace {
std::string hex(std::string_view s) {
    std::string out;
    for (unsigned char c : s) { out += "0123456789abcdef"[c >> 4]; out += "0123456789abcdef"[c & 15]; }
    return out;
}
std::string unhex(const std::string& s) {
    if (s.size() % 2 || s.find_first_not_of("0123456789abcdef") != std::string::npos)
        throw std::runtime_error("malformed probe hex");
    std::string out;
    auto digit = [](char c) { return c <= '9' ? c-'0' : c-'a'+10; };
    for (std::size_t i = 0; i < s.size(); i += 2) out += static_cast<char>(digit(s[i])*16+digit(s[i+1]));
    return out;
}
std::string input_wire(const RouteCase& c) {
    std::ostringstream out;
    out << "mode " << (c.active_module.empty() ? "whole" : "step") << "\nactive " << hex(c.active_module)
        << "\nprefix-count " << c.expected_route_prefix.size() << '\n';
    for (const auto& route : c.expected_route_prefix) out << "prefix " << hex(route) << '\n';
    out << "assignment-count " << c.values.size() << '\n';
    for (const auto& v : c.values) out << "input " << hex(v.first) << ' ' << hex(v.second) << '\n';
    return out.str();
}
std::string shell_quote(const std::filesystem::path& path) {
    const auto s = path.string();
    if (s.find('\0') != std::string::npos) throw std::runtime_error("NUL in runtime path");
    std::string out = "'";
    for (char c : s) out += c == '\'' ? "'\"'\"'" : std::string(1,c);
    return out + "'";
}
void write(const std::filesystem::path& path, std::string_view content) {
    std::ofstream out(path, std::ios::binary);
    if (!out || !out.write(content.data(), static_cast<std::streamsize>(content.size())) || !out.flush())
        throw std::runtime_error("cannot write probe artifact '" + path.string() + "'");
}
// Validate the counted canonical wire before treating a runtime result as an observation.
void validate_observation(const std::string& s) {
    std::size_t pos = 0;
    auto literal = [&](std::string_view value) {
        if (s.compare(pos,value.size(),value) != 0) throw std::runtime_error("malformed canonical observation");
        pos += value.size();
    };
    auto number = [&](char separator) {
        const auto end = s.find(separator,pos);
        std::size_t n = 0;
        if (end == std::string::npos || end == pos) throw std::runtime_error("malformed observation count");
        const auto result = std::from_chars(s.data()+pos,s.data()+end,n);
        if (result.ec != std::errc{} || result.ptr != s.data()+end || n > s.size())
            throw std::runtime_error("invalid observation count");
        pos = end+1; return n;
    };
    auto text = [&]() { const auto n = number(':'); if (n > s.size()-pos) throw std::runtime_error("truncated observation text"); pos += n; };
    auto flag = [&]() { if (pos >= s.size() || (s[pos] != '0' && s[pos] != '1')) throw std::runtime_error("invalid observation flag"); ++pos; };
    literal("VISTA-OBSERVATION 1\n"); text(); literal("\n");
    auto n = number('\n');
    for (std::size_t i = 0; i < n; ++i) { text(); literal(" "); flag(); literal(" "); flag(); literal(" "); flag(); literal("\n"); }
    text(); literal("\n"); flag(); literal("\n");
    n = number('\n'); for (std::size_t i = 0; i < n; ++i) { text(); literal("\n"); }
    n = number('\n'); for (std::size_t i = 0; i < n; ++i) { text(); text(); text(); literal("\n"); }
    n = number('\n'); for (std::size_t i = 0; i < n; ++i) { text(); literal("\n"); }
    n = number('\n'); for (std::size_t i = 0; i < n; ++i) { text(); literal("\n"); }
    if (pos != s.size()) throw std::runtime_error("trailing observation bytes");
}
std::vector<std::string> probe(const std::vector<RouteCase>& cases, const std::filesystem::path& directory) {
    std::ostringstream script;
    // Buffer paths preserve arbitrary POSIX filename bytes; no Unicode replacement decoding.
    script << "const fs=require('fs'),Module=require('module');\n"
           << "const target=new Module('workflow.js');target.paths=module.paths;\n"
           << "target._compile(fs.readFileSync(Buffer.from('" << hex(std::filesystem::absolute(directory/"workflow.js").string())
           << "','hex'),'utf8'),'workflow.js');\n"
           << "const {vistaEvaluate,vistaCanonicalObservation}=target.exports;\n"
           << "if(typeof vistaEvaluate!=='function'||typeof vistaCanonicalObservation!=='function')throw Error('missing target API');\n"
           << "const cases=[\n";
    for (const auto& c : cases) {
        script << "['" << hex(c.id) << "','" << hex(c.active_module) << "',[";
        bool first = true;
        for (const auto& v : c.values) {
            if (!first) script << ',';
            first = false;
            script << "['" << hex(v.first) << "','" << hex(v.second) << "']";
        }
        script << "]],\n";
    }
    script << "];\nfor(const [id,active,values] of cases){\n"
           << "const input=Object.create(null);for(const [k,v]of values)input[Buffer.from(k,'hex').toString('utf8')]=Buffer.from(v,'hex').toString('latin1');\n"
           << "const observation=vistaCanonicalObservation(vistaEvaluate(input,active?Buffer.from(active,'hex').toString('utf8'):undefined,{inputMode:'bytes'}));\n"
           << "if(typeof observation!=='string')throw Error('invalid canonical observation');\n"
           << "process.stdout.write(id+'\\t'+Buffer.from(observation,'utf8').toString('hex')+'\\n');}\n";
    const auto script_path = directory/"probe.js", output_path = directory/"probe.out", error_path = directory/"probe.err";
    write(script_path, script.str());
    const auto command = "node < " + shell_quote(script_path) + " > " + shell_quote(output_path) + " 2> " + shell_quote(error_path);
    if (std::system(command.c_str()) != 0) throw std::runtime_error("Node unavailable or probe execution failed (see probe.err)");
    std::ifstream output(output_path, std::ios::binary);
    if (!output) throw std::runtime_error("cannot read probe output");
    std::vector<std::string> observations;
    std::string line;
    std::set<std::string> ids;
    while (std::getline(output,line)) {
        const auto tab = line.find('\t');
        if (tab == std::string::npos || observations.size() >= cases.size()) throw std::runtime_error("malformed or extra probe output");
        const auto id = unhex(line.substr(0,tab));
        if (!ids.insert(id).second || id != cases[observations.size()].id) throw std::runtime_error("duplicate or unexpected probe case");
        const auto observation = unhex(line.substr(tab+1)); validate_observation(observation);
        observations.push_back(observation);
    }
    if (output.bad() || observations.size() != cases.size() || (!line.empty() && output.eof()))
        throw std::runtime_error("incomplete probe output");
    return observations;
}
std::string observe(const LinkedWorkflow& w, const RouteCase& c) {
    return canonical_observation((c.active_module.empty() ? evaluate_workflow(w,c.values) :
                                 evaluate_workflow_step(w,c.values,c.active_module)).observation);
}
bool retains_prefix(const LinkedWorkflow& w, const RouteCase& c) {
    if (c.active_module.empty())
        return evaluate_workflow(w,c.values).observation.transition_trace == c.expected_route_prefix;
    auto active = w.start_alias;
    for (const auto& route : c.expected_route_prefix) {
        const auto step = evaluate_workflow_step(w,c.values,active);
        if (step.observation.selected_route != route) return false;
        active = route.substr(route.find("->")+2);
    }
    return c.active_module.empty() || active == c.active_module;
}
}  // namespace

CertificateResult certify_workflow(const LinkedWorkflow& workflow, const RouteSuite& suite,
                                 std::string_view javascript, const std::filesystem::path& directory) {
    CertificateResult result;
    auto& cert = result.certificate;
    cert.compiler_version = kVersion; cert.workflow = workflow.name;
    const auto ir = format_linked_workflow(workflow);
    const auto digest_line = ir.rfind("workflow-sha256 ");
    cert.workflow_sha256 = sha256_hex(ir.substr(0,digest_line));
    cert.javascript_sha256 = sha256_hex(javascript);
    cert.artifacts.push_back({"javascript","workflow.js",cert.javascript_sha256});
    for (const auto& m : workflow.modules) {
        cert.module_hashes.emplace_back(m.alias+".source",m.source_sha256);
        cert.module_hashes.emplace_back(m.alias+".object",m.object_sha256);
    }
    try {
        if (directory.string().find('\0') != std::string::npos) throw std::runtime_error("NUL in output path");
        std::error_code error;
        std::filesystem::remove(directory/"workflow.vcert",error);
        if (error) throw std::runtime_error("cannot clear stale certificate");
        // Verify the caller's suite against the actual bounded derivation, including identities.
        const auto partitions = derive_partitions(workflow);
        const auto expected = enumerate_route_cases(workflow,partitions);
        if (!suite.complete() || !expected.complete()) {
            result.diagnostics = !expected.complete() ? expected.diagnostics : suite.diagnostics;
            for (const auto& d : result.diagnostics) {
                cert.unsupported_rule += (cert.unsupported_rule.empty() ? "" : "; ") + d.code + ": " + d.message;
                if (d.code == "CERT001" || d.code == "CERT003") cert.status = CertificateStatus::Unsupported;
            }
            cert.coverage_sha256 = certificate_coverage_sha256(cert);
            return result;
        }
        if (suite.cases.empty() || suite.cases.size() != expected.cases.size()) throw std::runtime_error("incomplete route suite");
        std::set<std::string> ids;
        for (std::size_t i = 0; i < suite.cases.size(); ++i)
            if (!ids.insert(suite.cases[i].id).second || suite.cases[i].id != expected.cases[i].id || input_wire(suite.cases[i]) != input_wire(expected.cases[i]))
                throw std::runtime_error("duplicate or inconsistent route suite identity");
        for (const auto& f : partitions.fields)
            for (const auto& c : f.classes) cert.partition_ids.push_back("scalar:"+f.field+":"+c.id);
        for (std::size_t i = 0; i < partitions.components.size(); ++i) {
            const auto& component = partitions.components[i];
            for (std::size_t j = 0; j < component.assignments.size(); ++j) {
                RouteCase c; c.values = component.assignments[j];
                cert.partition_ids.push_back("component:"+std::to_string(i)+":"+std::to_string(j)+":"+sha256_hex(input_wire(c)));
            }
        }
        cert.coverage_complete = true; cert.expected_case_count = expected.cases.size();
        for (const auto& c : expected.cases) c.active_module.empty() ? ++cert.expected_whole_count : ++cert.expected_step_count;
        write(directory/"workflow.js",javascript);
        const auto observations = probe(suite.cases,directory);
        std::size_t mismatch = suite.cases.size();
        for (std::size_t i = 0; i < suite.cases.size(); ++i) {
            const auto& c = suite.cases[i]; const auto input = input_wire(c);
            const auto cli = observe(workflow,c);
            cert.cases.push_back({c.id,sha256_hex(input),sha256_hex(cli),sha256_hex(observations[i]),input});
            if (cli != observations[i] && mismatch == suite.cases.size()) mismatch = i;
        }
        cert.coverage_sha256 = certificate_coverage_sha256(cert);
        if (mismatch == suite.cases.size()) { cert.status = CertificateStatus::Pass; return result; }
        RouteCase witness = suite.cases[mismatch];
        std::vector<std::string> fields;
        for (const auto& v : witness.values) fields.push_back(v.first);
        for (const auto& field : fields) {
            auto candidate = witness; candidate.values.erase(field);
            if (!retains_prefix(workflow,candidate)) continue;
            const auto js = probe({candidate},directory)[0];
            if (observe(workflow,candidate) != js) witness = std::move(candidate);
        }
        cert.counterexample = input_wire(witness);
        cert.counterexample_cli = observe(workflow,witness);
        cert.counterexample_javascript = probe({witness},directory)[0];
        if (cert.counterexample_cli == cert.counterexample_javascript) throw std::runtime_error("counterexample failed to reproduce");
        result.diagnostics.push_back({"CERT004","cross-target observation disagreement at "+witness.id,1,1});
    } catch (const std::exception& e) {
        cert.status = CertificateStatus::Fail; cert.coverage_complete = false;
        result.diagnostics.push_back({"CERT006",e.what(),1,1});
    }
    return result;
}
}  // namespace vista
