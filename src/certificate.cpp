#include "certificate.hpp"
#include "hash.hpp"
#include <algorithm>
#include <charconv>
#include <fstream>
#include <iterator>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>

namespace vista {
namespace {
std::string encode(std::string_view value) {
    if (value.empty()) return "-";
    std::string result;
    for (unsigned char c : value) {
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '.' || c == '_') result += c;
        else { result += '%'; result += "0123456789ABCDEF"[c >> 4]; result += "0123456789ABCDEF"[c & 15]; }
    }
    return result;
}
int digit(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}
std::string decode(const std::string& value) {
    if (value == "-") return "";
    std::string result;
    for (std::size_t i = 0; i < value.size(); ++i) {
        if (value[i] != '%') result += value[i];
        else {
            if (i + 2 >= value.size() || digit(value[i+1]) < 0 || digit(value[i+2]) < 0)
                throw std::runtime_error("invalid percent encoding");
            result += static_cast<char>(digit(value[i+1]) * 16 + digit(value[i+2])); i += 2;
        }
    }
    if (encode(result) != value) throw std::runtime_error("noncanonical free text");
    return result;
}
bool hash(const std::string& s) {
    return s.size() == 64 && s.find_first_not_of("0123456789abcdef") == std::string::npos;
}
std::size_t count(const std::string& s) {
    std::size_t n = 0;
    const auto p = std::from_chars(s.data(), s.data() + s.size(), n);
    if (s.empty() || p.ec != std::errc{} || p.ptr != s.data()+s.size())
        throw std::runtime_error("invalid count");
    return n;
}
const char* status(CertificateStatus s) {
    return s == CertificateStatus::Pass ? "PASS" : s == CertificateStatus::Fail ? "FAIL" : "UNSUPPORTED";
}
}  // namespace

std::string certificate_coverage_sha256(const Certificate& c) {
    auto partitions = c.partition_ids;
    auto cases = c.cases;
    std::sort(partitions.begin(), partitions.end());
    std::sort(cases.begin(), cases.end(), [](const auto& a, const auto& b) { return a.id < b.id; });
    std::ostringstream out;
    out << "VISTA-COVERAGE 1\n" << c.coverage_complete << ' ' << c.expected_case_count
        << ' ' << c.expected_step_count << ' ' << c.expected_whole_count << '\n';
    for (const auto& id : partitions) out << "partition " << encode(id) << '\n';
    for (const auto& e : cases) out << "case " << encode(e.id) << ' ' << e.input_sha256 << '\n';
    return sha256_hex(out.str());
}

std::string serialize_certificate(const Certificate& source) {
    auto c = source;
    std::sort(c.module_hashes.begin(), c.module_hashes.end());
    std::sort(c.artifacts.begin(), c.artifacts.end(), [](const auto& a, const auto& b) { return a.role < b.role; });
    std::sort(c.partition_ids.begin(), c.partition_ids.end());
    std::sort(c.cases.begin(), c.cases.end(), [](const auto& a, const auto& b) { return a.id < b.id; });
    std::ostringstream out;
    out << "VISTA-CERT " << c.format_version << "\nstatus " << status(c.status)
        << "\nworkflow " << encode(c.workflow) << "\ncompiler-version " << encode(c.compiler_version)
        << "\nworkflow-sha256 " << c.workflow_sha256 << "\njavascript-sha256 " << c.javascript_sha256
        << "\nmodule-count " << c.module_hashes.size() << '\n';
    for (const auto& m : c.module_hashes) out << "module-hash " << encode(m.first) << ' ' << m.second << '\n';
    out << "artifact-count " << c.artifacts.size() << '\n';
    for (const auto& a : c.artifacts) out << "artifact " << encode(a.role) << ' ' << encode(a.path) << ' ' << a.raw_sha256 << '\n';
    out << "partition-count " << c.partition_ids.size() << '\n';
    for (const auto& id : c.partition_ids) out << "partition " << encode(id) << '\n';
    out << "coverage-schema vista-route-suite-v1\ncoverage-complete " << c.coverage_complete
        << "\nexpected-case-count " << c.expected_case_count << "\nexpected-step-count " << c.expected_step_count
        << "\nexpected-whole-count " << c.expected_whole_count << "\ncoverage-sha256 " << c.coverage_sha256
        << "\ncase-count " << c.cases.size() << '\n';
    for (const auto& e : c.cases) {
        out << "case " << encode(e.id) << ' ' << e.input_sha256 << ' ' << e.cli_sha256 << ' ' << e.javascript_sha256 << '\n';
        out << "case-input " << encode(e.id) << ' ' << encode(e.input) << '\n';
    }
    out << "counterexample " << encode(c.counterexample) << "\ncounterexample-cli " << encode(c.counterexample_cli)
        << "\ncounterexample-js " << encode(c.counterexample_javascript)
        << "\nunsupported-rule " << encode(c.unsupported_rule) << '\n';
    return out.str();
}

CertificateParseResult parse_certificate(std::string_view bytes) {
    CertificateParseResult result;
    auto& c = result.certificate;
    try {
        std::istringstream in{std::string(bytes)};
        auto line = [&](const std::string& key, std::size_t n) {
            std::string s; if (!std::getline(in, s)) throw std::runtime_error("truncated certificate");
            std::istringstream row(s); std::string k, v; row >> k;
            std::vector<std::string> values;
            while (row >> v) values.push_back(v);
            if (k != key || values.size() != n) throw std::runtime_error("invalid " + key + " record");
            return values;
        };
        auto scalar = [&](const std::string& key) { return line(key, 1)[0]; };
        if (scalar("VISTA-CERT") != "1") throw std::runtime_error("unsupported certificate version");
        const auto s = scalar("status");
        if (s == "PASS") c.status = CertificateStatus::Pass;
        else if (s == "FAIL") c.status = CertificateStatus::Fail;
        else if (s == "UNSUPPORTED") c.status = CertificateStatus::Unsupported;
        else throw std::runtime_error("invalid status");
        c.workflow = decode(scalar("workflow")); c.compiler_version = decode(scalar("compiler-version"));
        c.workflow_sha256 = scalar("workflow-sha256"); c.javascript_sha256 = scalar("javascript-sha256");
        auto unique = [](std::set<std::string>& seen, const std::string& id) {
            if (id.empty() || !seen.insert(id).second) throw std::runtime_error("empty or duplicate identity");
        };
        std::set<std::string> seen;
        auto n = count(scalar("module-count"));
        for (std::size_t i = 0; i < n; ++i) {
            const auto v = line("module-hash", 2); const auto id = decode(v[0]); unique(seen, id);
            if (!hash(v[1])) throw std::runtime_error("invalid module hash");
            c.module_hashes.emplace_back(id, v[1]);
        }
        seen.clear(); n = count(scalar("artifact-count"));
        for (std::size_t i = 0; i < n; ++i) {
            const auto v = line("artifact", 3); ArtifactEvidence a{decode(v[0]), decode(v[1]), v[2]}; unique(seen, a.role);
            if (a.path.empty() || a.path.find('\0') != std::string::npos || !hash(a.raw_sha256)) throw std::runtime_error("invalid artifact");
            c.artifacts.push_back(a);
        }
        seen.clear(); n = count(scalar("partition-count"));
        for (std::size_t i = 0; i < n; ++i) { auto id = decode(scalar("partition")); unique(seen,id); c.partition_ids.push_back(id); }
        if (scalar("coverage-schema") != "vista-route-suite-v1") throw std::runtime_error("unknown coverage schema");
        const auto complete = scalar("coverage-complete");
        if (complete != "0" && complete != "1") throw std::runtime_error("invalid coverage flag");
        c.coverage_complete = complete == "1";
        c.expected_case_count = count(scalar("expected-case-count"));
        c.expected_step_count = count(scalar("expected-step-count"));
        c.expected_whole_count = count(scalar("expected-whole-count"));
        c.coverage_sha256 = scalar("coverage-sha256");
        seen.clear(); n = count(scalar("case-count"));
        std::size_t steps = 0, whole = 0;
        for (std::size_t i = 0; i < n; ++i) {
            const auto v = line("case", 4); CaseEvidence e{decode(v[0]), v[1], v[2], v[3], ""}; unique(seen, e.id);
            const auto input = line("case-input", 2);
            if (decode(input[0]) != e.id) throw std::runtime_error("case input identity mismatch");
            e.input = decode(input[1]);
            if (!hash(e.input_sha256) || !hash(e.cli_sha256) || !hash(e.javascript_sha256) || sha256_hex(e.input) != e.input_sha256)
                throw std::runtime_error("invalid case hash");
            if (e.input.rfind("mode step\n",0) == 0) ++steps;
            else if (e.input.rfind("mode whole\n",0) == 0) ++whole;
            else throw std::runtime_error("invalid case mode");
            c.cases.push_back(e);
        }
        c.counterexample = decode(scalar("counterexample")); c.counterexample_cli = decode(scalar("counterexample-cli"));
        c.counterexample_javascript = decode(scalar("counterexample-js")); c.unsupported_rule = decode(scalar("unsupported-rule"));
        if (!hash(c.workflow_sha256) || !hash(c.javascript_sha256) || !hash(c.coverage_sha256) ||
            c.workflow.empty() || c.compiler_version.empty() || c.coverage_sha256 != certificate_coverage_sha256(c))
            throw std::runtime_error("invalid aggregate evidence");
        if (c.coverage_complete && (n == 0 || n != c.expected_case_count || steps != c.expected_step_count || whole != c.expected_whole_count || !steps || !whole))
            throw std::runtime_error("inconsistent complete coverage");
        if (c.status == CertificateStatus::Pass) {
            if (!c.coverage_complete || !c.counterexample.empty() || !c.unsupported_rule.empty()) throw std::runtime_error("invalid PASS");
            for (const auto& e : c.cases) if (e.cli_sha256 != e.javascript_sha256) throw std::runtime_error("PASS disagreement");
        }
        // Canonical serialization also rejects trailing, reordered, extra and malformed tokens.
        if (serialize_certificate(c) != bytes) throw std::runtime_error("noncanonical or trailing certificate data");
    } catch (const std::exception& e) {
        result.diagnostics.push_back({"CERT005", e.what(), 1, 1});
    }
    return result;
}
namespace {
bool identifier(std::string_view s) {
    auto letter = [](char c) { return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_'; };
    return !s.empty() && letter(s.front()) && std::all_of(s.begin(),s.end(),[&](char c) { return letter(c) || (c >= '0' && c <= '9'); });
}
bool qualified(const std::string& name, const std::set<std::string>& aliases) {
    const auto dot = name.find('.');
    return dot != std::string::npos && identifier(std::string_view(name).substr(0,dot)) &&
        identifier(std::string_view(name).substr(dot+1)) && aliases.count(name.substr(0,dot));
}
std::size_t identity_count(const std::string& text) {
    const auto n = count(text);
    if (text != std::to_string(n)) throw std::runtime_error("noncanonical partition index");
    return n;
}
std::set<std::string> verify_partitions(const Certificate& c, const std::set<std::string>& aliases) {
    std::map<std::string,std::set<std::size_t>> scalars;
    std::map<std::size_t,std::set<std::size_t>> components;
    for (const auto& id : c.partition_ids) {
        const auto first = id.find(':'), second = id.find(':',first == std::string::npos ? 0 : first+1);
        if (first == std::string::npos || second == std::string::npos) throw std::runtime_error("invalid partition identity");
        const auto kind = id.substr(0,first), middle = id.substr(first+1,second-first-1), tail = id.substr(second+1);
        if (kind == "scalar") {
            const auto prefix = middle+".class.";
            if (!qualified(middle,aliases) || tail.rfind(prefix,0) != 0) throw std::runtime_error("invalid scalar field/class identity");
            scalars[middle].insert(identity_count(tail.substr(prefix.size())));
        } else if (kind == "component") {
            const auto at = tail.find(':');
            if (at == std::string::npos || !hash(tail.substr(at+1)) ||
                !components[identity_count(middle)].insert(identity_count(tail.substr(0,at))).second)
                throw std::runtime_error("invalid component assignment identity");
        } else throw std::runtime_error("unknown partition identity kind");
    }
    auto contiguous = [](const std::set<std::size_t>& ids) {
        std::size_t expected = 0;
        for (const auto id : ids) if (id != expected++) throw std::runtime_error("incomplete partition indices");
    };
    std::set<std::string> fields; std::size_t expected = 0;
    for (const auto& scalar : scalars) { contiguous(scalar.second); fields.insert(scalar.first); }
    for (const auto& component : components) {
        if (component.first != expected++) throw std::runtime_error("incomplete component indices");
        contiguous(component.second);
    }
    if (components.size() > fields.size() || (components.empty() != fields.empty()))
        throw std::runtime_error("inconsistent scalar/component manifest");
    return fields;
}
std::string unhex(const std::string& s) {
    if (s.size() % 2 || s.find_first_not_of("0123456789abcdef") != std::string::npos)
        throw std::runtime_error("invalid case input hex");
    auto value = [](char c) { return c <= '9' ? c-'0' : c-'a'+10; };
    std::string out;
    for (std::size_t i = 0; i < s.size(); i += 2) out += static_cast<char>(value(s[i])*16+value(s[i+1]));
    return out;
}
// Validate the recorded state/count wire, without evaluating source semantics.
void verify_input(const CaseEvidence& e, const std::set<std::string>& aliases, const std::set<std::string>& fields) {
    std::istringstream in(e.input);
    auto line = [&]() { std::string s; if (!std::getline(in,s)) throw std::runtime_error("truncated case input"); return s; };
    auto field = [&](const std::string& key) {
        const auto s = line();
        if (s.rfind(key+' ',0) != 0) throw std::runtime_error("invalid case input record");
        return s.substr(key.size()+1);
    };
    auto input_count = [&](const std::string& key) {
        const auto text = field(key); const auto n = count(text);
        if (text != std::to_string(n) || n > e.input.size()) throw std::runtime_error("noncanonical case input count");
        return n;
    };
    const auto mode = field("mode"), active = unhex(field("active"));
    if ((mode == "whole" && !active.empty()) || (mode == "step" && !aliases.count(active)) ||
        (mode != "whole" && mode != "step")) throw std::runtime_error("invalid case state mode");
    auto n = input_count("prefix-count");
    std::string destination;
    for (std::size_t i = 0; i < n; ++i) {
        const auto route = unhex(field("prefix")); const auto at = route.find("->");
        if (at == std::string::npos || !aliases.count(route.substr(0,at)) || !aliases.count(route.substr(at+2)) ||
            (i && route.substr(0,at) != destination)) throw std::runtime_error("invalid recorded route prefix");
        destination = route.substr(at+2);
    }
    if (mode == "step" && n && destination != active) throw std::runtime_error("prefix/state mismatch");
    n = input_count("assignment-count");
    std::string previous;
    for (std::size_t i = 0; i < n; ++i) {
        const auto s = field("input"); const auto at = s.find(' ');
        if (at == std::string::npos) throw std::runtime_error("invalid assignment");
        const auto name = unhex(s.substr(0,at)); unhex(s.substr(at+1));
        if (!qualified(name,aliases) || (i && name <= previous) || !fields.count(name))
            throw std::runtime_error("invalid assignment identity");
        previous = name;
    }
    if (in.peek() != std::char_traits<char>::eof() || e.input.empty() || e.input.back() != '\n')
        throw std::runtime_error("trailing or unterminated case input");
}
std::string read_artifact(const std::filesystem::path& path) {
    std::ifstream in(path,std::ios::binary);
    if (!in) throw std::runtime_error("cannot read artifact '"+path.string()+"'");
    std::string bytes{std::istreambuf_iterator<char>(in),{}};
    if (in.bad()) throw std::runtime_error("failed reading artifact");
    return bytes;
}
std::string normalized_source(const std::string& bytes) {
    std::string out;
    for (std::size_t i = 0; i < bytes.size(); ++i) {
        out += bytes[i] == '\r' ? '\n' : bytes[i];
        if (bytes[i] == '\r' && i+1 < bytes.size() && bytes[i+1] == '\n') ++i;
    }
    return out;
}
}  // namespace

CertificateVerification verify_certificate(const Certificate& c, const std::filesystem::path* root) {
    CertificateVerification result;
    try {
        // Reuse the strict codec also for direct API callers with constructed values.
        const auto parsed = parse_certificate(serialize_certificate(c));
        if (!parsed.succeeded()) throw std::runtime_error(parsed.diagnostics.front().message);
        if (c.status != CertificateStatus::Pass) throw std::runtime_error("certificate is not PASS");
        if (!c.counterexample_cli.empty() || !c.counterexample_javascript.empty()) throw std::runtime_error("PASS contains counterexample observations");
        result.coverage_sha256 = certificate_coverage_sha256(c);
        std::map<std::string,std::string> modules(c.module_hashes.begin(),c.module_hashes.end());
        std::set<std::string> aliases;
        for (const auto& m : modules) {
            if (m.first.size() > 7 && m.first.compare(m.first.size()-7,7,".source") == 0) aliases.insert(m.first.substr(0,m.first.size()-7));
        }
        if (aliases.empty() || modules.size() != aliases.size()*2) throw std::runtime_error("incomplete module identities");
        std::set<std::string> roles{"workflow-source","workflow-ir","javascript","html"};
        std::map<std::string,ArtifactEvidence> artifacts;
        for (const auto& a : c.artifacts) artifacts.emplace(a.role,a);
        for (const auto& alias : aliases) {
            if (!identifier(alias)) throw std::runtime_error("invalid module alias");
            if (!modules.count(alias+".object")) throw std::runtime_error("missing module object identity");
            const auto source = "module-source."+alias, object = "module-object."+alias;
            if (artifacts.count(source)+artifacts.count(object) != 1) throw std::runtime_error("missing or ambiguous module artifact");
            roles.insert(artifacts.count(source) ? source : object);
        }
        if (roles.size() != artifacts.size()) throw std::runtime_error("unexpected artifact role");
        for (const auto& role : roles) if (!artifacts.count(role)) throw std::runtime_error("missing artifact "+role);
        if (artifacts.at("javascript").raw_sha256 != c.javascript_sha256) throw std::runtime_error("JavaScript aggregate mismatch");
        const auto fields = verify_partitions(c,aliases);
        for (const auto& e : c.cases) verify_input(e,aliases,fields);
        if (root) {
            if (!std::filesystem::is_directory(*root)) throw std::runtime_error("root is not a project directory");
            std::map<std::string,std::string> bytes;
            for (const auto& a : c.artifacts) {
                const std::filesystem::path recorded(a.path);
                const bool generated = a.role == "workflow-ir" || a.role == "javascript" || a.role == "html";
                if (generated == recorded.is_absolute()) throw std::runtime_error("invalid artifact path policy");
                // Preserve exact filesystem spelling, particularly symlink/.. and raw bytes.
                const auto path = generated ? c.certificate_directory/recorded : recorded;
                bytes[a.role] = read_artifact(path);
                const auto digest = sha256_hex(bytes.at(a.role));
                result.artifact_sha256.emplace_back(a.role,digest);
                if (digest != a.raw_sha256) throw std::runtime_error("changed artifact "+a.role);
            }
            result.javascript_sha256 = sha256_hex(bytes.at("javascript"));
            const auto& ir = bytes.at("workflow-ir");
            const auto at = ir.rfind("\nworkflow-sha256 ");
            if (at == std::string::npos || ir.substr(at+1) != "workflow-sha256 "+c.workflow_sha256+"\n")
                throw std::runtime_error("invalid workflow IR digest record");
            result.workflow_sha256 = sha256_hex(std::string_view(ir).substr(0,at+1));
            if (result.workflow_sha256 != c.workflow_sha256) throw std::runtime_error("workflow aggregate mismatch");
            std::istringstream rows(ir.substr(0,at+1)); std::string row, alias, name, extra;
            if (!std::getline(rows,row) || row != "VISTA-WORKFLOW 1" || !std::getline(rows,row) || row != "workflow "+c.workflow)
                throw std::runtime_error("workflow IR identity mismatch");
            std::set<std::string> seen;
            while (std::getline(rows,row)) if (row.rfind("module ",0) == 0) {
                std::istringstream header(row.substr(7));
                if (!(header >> alias >> name) || (header >> extra) || !aliases.count(alias) || !seen.insert(alias).second)
                    throw std::runtime_error("invalid IR module identity");
                for (const auto& kind : {"source", "object"}) {
                    if (!std::getline(rows,row) || row != std::string(kind)+"-sha256 "+modules.at(alias+"."+kind))
                        throw std::runtime_error("IR/module semantic identity mismatch");
                }
            }
            if (seen != aliases) throw std::runtime_error("incomplete IR module identities");
            for (const auto& a : aliases) {
                const auto source = bytes.find("module-source."+a);
                if (source != bytes.end()) {
                    if (sha256_hex(normalized_source(source->second)) != modules.at(a+".source"))
                        throw std::runtime_error("module normalized source identity mismatch");
                } else {
                    const auto& object = bytes.at("module-object."+a);
                    const auto end = object.rfind("\nobject-sha256 ");
                    if (end == std::string::npos || object.substr(end+1) != "object-sha256 "+modules.at(a+".object")+"\n" ||
                        sha256_hex(std::string_view(object).substr(0,end+1)) != modules.at(a+".object"))
                        throw std::runtime_error("module object aggregate mismatch");
                    // Inspect the counted embedded-source envelope only; no module compiler/loader.
                    std::istringstream envelope(object.substr(0,end+1));
                    if (!std::getline(envelope,row) || row != "VISTA-OBJECT 1" || !std::getline(envelope,row) || row.rfind("module ",0) != 0 ||
                        !std::getline(envelope,row) || row != "source-sha256 "+modules.at(a+".source"))
                        throw std::runtime_error("module object source identity mismatch");
                    bool found = false;
                    while (std::getline(envelope,row)) if (row.rfind("source-bytes ",0) == 0) {
                        const auto n = count(row.substr(13)); const auto pos = static_cast<std::size_t>(envelope.tellg());
                        if (pos > end || n != end-pos || object.substr(pos,n).find('\r') != std::string::npos ||
                            sha256_hex(std::string_view(object).substr(pos,n)) != modules.at(a+".source"))
                            throw std::runtime_error("stale embedded source identity");
                        found = true; break;
                    }
                    if (!found) throw std::runtime_error("missing embedded source");
                }
            }
        }
        result.verified = true;
    } catch (const std::exception& e) { result.diagnostics.push_back({"CERT005",e.what(),1,1}); }
    return result;
}
}  // namespace vista
