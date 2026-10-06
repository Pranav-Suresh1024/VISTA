#ifndef VISTA_CERTIFICATE_HPP
#define VISTA_CERTIFICATE_HPP
#include <string>
#include <filesystem>
#include <string_view>
#include <utility>
#include <vector>
#include "diagnostic.hpp"

namespace vista {
enum class CertificateStatus { Pass, Fail, Unsupported };
struct CaseEvidence {
    std::string id, input_sha256, cli_sha256, javascript_sha256;
    // ASCII wire: mode, hex active alias, counted hex prefix and assignments.
    std::string input;
};
struct ArtifactEvidence {
    std::string role, path, raw_sha256;
};
struct Certificate {
    unsigned format_version = 1;
    CertificateStatus status = CertificateStatus::Fail;
    std::string compiler_version, workflow, workflow_sha256, javascript_sha256;
    std::vector<std::pair<std::string, std::string>> module_hashes;
    // Generated siblings are certificate-relative; source inputs are absolute.
    std::vector<ArtifactEvidence> artifacts;
    std::vector<std::string> partition_ids;
    bool coverage_complete = false;
    std::size_t expected_case_count = 0, expected_step_count = 0, expected_whole_count = 0;
    std::string coverage_sha256;
    std::vector<CaseEvidence> cases;
    std::string counterexample, counterexample_cli, counterexample_javascript;
    std::string unsupported_rule;
    // Read context only; never serialized. Caller sets the certificate's parent.
    std::filesystem::path certificate_directory;
};
struct CertificateParseResult {
    Certificate certificate;
    std::vector<Diagnostic> diagnostics;
    bool succeeded() const { return diagnostics.empty(); }
};
// Digest of sorted partition IDs, expected counts and case ID/input identities.
// Structural coverage consistency only; neither authentication nor execution proof.
std::string certificate_coverage_sha256(const Certificate&);
std::string serialize_certificate(const Certificate&);
CertificateParseResult parse_certificate(std::string_view);
struct CertificateVerification {
    bool verified = false;
    std::vector<Diagnostic> diagnostics;
    std::string coverage_sha256, workflow_sha256, javascript_sha256;
    std::vector<std::pair<std::string, std::string>> artifact_sha256;
};
CertificateVerification verify_certificate(const Certificate&, const std::filesystem::path* root);
}  // namespace vista
#endif
