#ifndef VISTA_CERTIFY_HPP
#define VISTA_CERTIFY_HPP
#include <filesystem>
#include "certificate.hpp"
#include "partition.hpp"

namespace vista {
struct CertificateResult {
    Certificate certificate;
    std::vector<Diagnostic> diagnostics;
    bool succeeded() const {
        return diagnostics.empty() && certificate.status == CertificateStatus::Pass;
    }
};
CertificateResult certify_workflow(const LinkedWorkflow&, const RouteSuite&,
                                 std::string_view javascript,
                                 const std::filesystem::path& output_directory);
}  // namespace vista
#endif
