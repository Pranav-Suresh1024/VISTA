#include "certificate.hpp"
#include "hash.hpp"
#include <algorithm>
#include <charconv>
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
}  // namespace vista
