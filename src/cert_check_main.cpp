#include "certificate.hpp"
#include <fstream>
#include <iostream>
#include <iterator>

int main(int argc, char** argv) {
    if (argc != 2 && (argc != 4 || std::string(argv[2]) != "--root")) {
        std::cerr << "Usage: vista-cert-check <workflow.vcert> [--root <directory>]\n";
        return 1;
    }
    std::ifstream input(argv[1], std::ios::binary);
    const std::string bytes{std::istreambuf_iterator<char>(input), {}};
    if (!input || input.bad()) {
        std::cerr << "CERT005: cannot read certificate\n";
        return 1;
    }
    auto parsed = vista::parse_certificate(bytes);
    if (!parsed.succeeded()) {
        for (const auto& d : parsed.diagnostics) std::cerr << vista::format_diagnostic(argv[1], d) << '\n';
        return 1;
    }
    parsed.certificate.certificate_directory = std::filesystem::path(argv[1]).parent_path();
    const std::filesystem::path root = argc == 4 ? argv[3] : "";
    const auto result = vista::verify_certificate(parsed.certificate, argc == 4 ? &root : nullptr);
    for (const auto& d : result.diagnostics) std::cerr << vista::format_diagnostic(argv[1], d) << '\n';
    if (!result.verified) return 1;
    std::cout << "VERIFIED: " << (argc == 4 ? "recorded evidence and named files" : "recorded evidence only")
              << "\ncoverage-sha256 " << result.coverage_sha256 << '\n';
    if (argc == 4) {
        std::cout << "workflow-sha256 " << result.workflow_sha256
                  << "\njavascript-sha256 " << result.javascript_sha256 << '\n';
        for (const auto& a : result.artifact_sha256) std::cout << "artifact-sha256 " << a.first << ' ' << a.second << '\n';
    }
}
