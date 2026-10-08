#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>

#include "analysis.hpp"
#include "ast.hpp"
#include "codegen.hpp"
#include "certify.hpp"
#include "hash.hpp"
#include "file_io.hpp"
#include "diagnostic.hpp"
#include "module.hpp"
#include "linker.hpp"
#include "partition.hpp"
#include "parser.hpp"
#include "scanner.hpp"
#include "semantic.hpp"
#include "token.hpp"
#include "validation.hpp"
#include "version.hpp"

namespace {

void print_help(std::ostream& output) {
    output << "VISTA - Validation and Interface Specification Translation Analyzer\n\n"
           << "Usage: vista <source.vista> --emit <tokens|ast|symbols|dependencies|graph|diagnostics>\n"
           << "       vista <source.vista> --emit <html|all|module|linked|workflow|certificate> --out-dir <directory>\n"
           << "       vista <workflow.vista> --emit certificate --out-dir DIR [--javascript FILE]\n"
           << "       vista <source.vista> --validate-data <values.data>\n"
           << "       vista --list-templates\n"
           << "       vista [option]\n\n"
           << "Options:\n"
           << "  --help       Show this help message\n"
           << "  --version    Show the current VISTA version\n"
           << "  --list-templates  List editable starter form sources and compile commands\n"
           << "  --emit tokens  Print the positioned Flex token stream\n"
           << "  --emit ast     Parse the source and print its abstract syntax tree\n"
           << "  --emit symbols Print the field symbol table\n"
           << "  --emit dependencies  Print conditional field dependencies\n"
           << "  --emit graph   Print the dependency graph in DOT format\n"
           << "  --emit diagnostics  Run semantic and dependency analysis\n"
           << "  --emit html --out-dir DIR  Generate a standalone HTML form\n"
           << "  --emit all --out-dir DIR   Write every demonstration artifact\n"
           << "  --emit module --out-dir DIR  Compile a deterministic module object\n"
           << "  --emit linked --out-dir DIR  Link a typed workflow to workflow.ir\n"
           << "  --emit workflow --out-dir DIR  Generate workflow IR, HTML and JavaScript\n"
           << "  --emit certificate --out-dir DIR  Execute both targets and record conformance evidence\n"
           << "  --emit partitions  Derive exact classes and reachable workflow cases\n"
           << "  --validate-data FILE  Validate supplied field values in the CLI\n";
}

struct StarterTemplate {
    const char* id;
    const char* source_path;
    const char* output_directory;
};

const std::vector<StarterTemplate>& starter_templates() {
    static const std::vector<StarterTemplate> templates{
        {"scholarship", "templates/scholarship_application.vista", "out/phase3-templates/scholarship"},
        {"college-admission", "templates/college_admission.vista", "out/phase3-templates/college-admission"},
        {"event-registration", "templates/event_registration.vista", "out/phase3-templates/event-registration"}
    };
    return templates;
}

void print_templates(std::ostream& output) {
    output << "Available starter templates (editable VISTA source files):\n";
    for (const StarterTemplate& form_template : starter_templates()) {
        output << "  " << form_template.id << "\n"
               << "    Source: " << form_template.source_path << "\n"
               << "    Compile: ./build/vista " << form_template.source_path
               << " --emit all --out-dir " << form_template.output_directory << "\n";
    }
    output << "These examples support local CLI and browser validation; they do not submit or store forms.\n";
}

std::string format_tokens(const std::vector<vista::Token>& tokens) {
    std::ostringstream output;
    output << std::left << std::setw(10) << "LOCATION"
           << std::setw(22) << "TOKEN" << "LEXEME\n";
    output << std::string(56, '-') << '\n';

    for (const vista::Token& token : tokens) {
        const std::string location =
            std::to_string(token.line) + ':' + std::to_string(token.column);
        output << std::left << std::setw(10) << location
               << std::setw(22) << vista::token_kind_name(token.kind)
               << vista::display_lexeme(token.lexeme) << '\n';
    }
    return output.str();
}

void print_diagnostics(const std::string& source_path,
                       const std::vector<vista::Diagnostic>& diagnostics) {
    for (const vista::Diagnostic& diagnostic : diagnostics) {
        std::cerr << vista::format_diagnostic(source_path, diagnostic) << '\n';
    }
}

std::string format_diagnostics(const std::string& source_path,
                               const std::vector<vista::Diagnostic>& diagnostics) {
    if (diagnostics.empty()) {
        return "No diagnostics.\n";
    }
    std::ostringstream output;
    for (const vista::Diagnostic& diagnostic : diagnostics) {
        output << vista::format_diagnostic(source_path, diagnostic) << '\n';
    }
    return output.str();
}

bool prepare_output_directory(const std::filesystem::path& directory) {
    std::error_code error;
    std::filesystem::create_directories(directory, error);
    if (error) {
        std::cerr << "VISTA: cannot create output directory '" << directory.string()
                  << "': " << error.message() << '\n';
        return false;
    }
    return true;
}

bool write_artifact(const std::filesystem::path& directory,
                    const std::string& filename,
                    const std::string& content) {
    const std::filesystem::path output_path = directory / filename;
    std::ofstream output_file(output_path, std::ios::binary);
    if (!output_file) {
        std::cerr << "VISTA: cannot write output file '" << output_path.string() << "'\n";
        return false;
    }
    output_file << content;
    if (!output_file) {
        std::cerr << "VISTA: failed while writing output file '"
                  << output_path.string() << "'\n";
        return false;
    }
    return true;
}

bool remove_artifact(const std::filesystem::path& directory,
                     const std::string& filename) {
    const std::filesystem::path artifact_path = directory / filename;
    std::error_code error;
    std::filesystem::remove(artifact_path, error);
    if (error) {
        std::cerr << "VISTA: cannot clear old output file '" << artifact_path.string()
                  << "': " << error.message() << '\n';
        return false;
    }
    return true;
}

bool clear_workflow_artifacts(const std::filesystem::path& directory) {
    bool cleared = true;
    for (const auto* file : {"workflow.ir", "workflow.html", "workflow.js"})
        cleared = remove_artifact(directory, file) && cleared;
    return cleared;
}

bool read_bytes(const std::filesystem::path& path, std::string& content) {
    if (path.string().find('\0') != std::string::npos) {
        std::cerr << "VISTA: NUL in input path\n"; return false;
    }
    if (vista::read_binary_file(path, content)) return true;
    std::cerr << "VISTA: cannot read input '" << path.string() << "'\n";
    return false;
}

bool clear_generated_artifacts(const std::filesystem::path& directory,
                               bool all_mode,
                               bool module_mode) {
    static const std::vector<std::string> all_artifacts{
        "tokens.txt",
        "ast.txt",
        "symbols.txt",
        "dependencies.txt",
        "graph.dot",
        "diagnostics.txt",
        "form.html"
    };
    if (module_mode) {
        return remove_artifact(directory, "module.vobj");
    }
    if (!all_mode) {
        return remove_artifact(directory, "form.html");
    }
    for (const std::string& filename : all_artifacts) {
        if (!remove_artifact(directory, filename)) {
            return false;
        }
    }
    return true;
}

bool write_failure_diagnostics(const std::filesystem::path& directory,
                               const std::string& source_path,
                               const std::vector<vista::Diagnostic>& diagnostics) {
    return write_artifact(
        directory, "diagnostics.txt", format_diagnostics(source_path, diagnostics));
}

}  // namespace

int main(int argc, char* argv[]) {
    if (argc == 1) {
        print_help(std::cout);
        return 0;
    }

    const std::string source_path = argv[1];
    if (source_path == "--help") {
        if (argc != 2) {
            std::cerr << "VISTA: --help does not accept additional arguments\n";
            return 2;
        }
        print_help(std::cout);
        return 0;
    }
    if (source_path == "--version") {
        if (argc != 2) {
            std::cerr << "VISTA: --version does not accept additional arguments\n";
            return 2;
        }
        std::cout << vista::kProgramName << ' ' << vista::kVersion << '\n';
        return 0;
    }
    if (source_path == "--list-templates") {
        if (argc != 2) {
            std::cerr << "VISTA: --list-templates does not accept additional arguments\n";
            return 2;
        }
        print_templates(std::cout);
        return 0;
    }

    const bool validation_mode =
        argc == 4 && std::string(argv[2]) == "--validate-data";
    if (!validation_mode &&
        ((argc != 4 && argc != 6 && argc != 8) || std::string(argv[2]) != "--emit")) {
        std::cerr << "VISTA: invalid command-line arguments\n"
                  << "Run 'vista --help' to see the available options.\n";
        return 2;
    }

    const std::string emit_mode = validation_mode ? "" : argv[3];
    const bool module_mode = emit_mode == "module";
    const bool linked_mode = emit_mode == "linked";
    const bool workflow_mode = emit_mode == "workflow";
    const bool certificate_mode = emit_mode == "certificate";
    const bool supplied_javascript = certificate_mode && argc == 8 && std::string(argv[6]) == "--javascript" && !std::string(argv[7]).empty();
    const bool directory_mode = emit_mode == "html" || emit_mode == "all" ||
                                module_mode || linked_mode || workflow_mode || certificate_mode;
    const bool known_mode =
        emit_mode == "tokens" || emit_mode == "ast" || emit_mode == "symbols" ||
        emit_mode == "dependencies" || emit_mode == "graph" ||
        emit_mode == "diagnostics" || emit_mode == "partitions" || directory_mode;
    if (!validation_mode && !known_mode) {
        std::cerr << "VISTA: unknown emit mode '" << emit_mode << "'\n";
        return 2;
    }
    if (!validation_mode && ((directory_mode &&
          ((argc != 6 && !supplied_javascript) || std::string(argv[4]) != "--out-dir" ||
           std::string(argv[5]).empty())) ||
        (!directory_mode && argc != 4))) {
        std::cerr << "VISTA: invalid command-line arguments\n"
                  << "Run 'vista --help' to see the available options.\n";
        return 2;
    }

    const bool all_mode = emit_mode == "all";
    const std::filesystem::path output_directory = directory_mode ? argv[5] : "";
    std::vector<std::string> touched_outputs;
    if (certificate_mode) touched_outputs = {"workflow.ir", "workflow.html", "workflow.js", "workflow.vcert", "probe.js", "probe.out", "probe.err"};
    else if (workflow_mode) touched_outputs = {"workflow.ir", "workflow.html", "workflow.js"};
    else if (linked_mode) touched_outputs = {"workflow.ir"};
    else if (module_mode) touched_outputs = {"module.vobj"};
    else if (all_mode) touched_outputs = {"tokens.txt", "ast.txt", "symbols.txt", "dependencies.txt", "graph.dot", "diagnostics.txt", "form.html"};
    else if (directory_mode) touched_outputs = {"form.html"};
    const auto overlaps_output = [&](const std::filesystem::path& input) {
        for (const auto& file : touched_outputs) {
            const auto output = output_directory / file;
            std::error_code error;
            if (std::filesystem::equivalent(input, output, error)) return true;
            // Resolve existing prefixes before .., including nonexisting output leaves.
            const auto resolved_input = std::filesystem::weakly_canonical(input, error);
            if (error) continue;
            const auto resolved_output = std::filesystem::weakly_canonical(output, error);
            if (!error && resolved_input == resolved_output) return true;
        }
        return false;
    };
    if (overlaps_output(source_path)) {
        std::cerr << "VISTA: source input overlaps output\n"; return 2;
    }
    const auto clear_outputs = [&]() {
        return certificate_mode ? (remove_artifact(output_directory,"workflow.vcert") && clear_workflow_artifacts(output_directory)) :
               workflow_mode ? clear_workflow_artifacts(output_directory) :
               linked_mode ? remove_artifact(output_directory, "workflow.ir") :
               clear_generated_artifacts(output_directory, all_mode, module_mode);
    };
    std::string exact_javascript, workflow_source_bytes;
    if (!read_bytes(source_path, workflow_source_bytes)) {
        if (directory_mode) clear_outputs();
        return 2;
    }
    const vista::ScanResult scan_result = vista::scan_file(source_path);
    vista::ParseResult parse_result;
    if (emit_mode != "tokens" && scan_result.input_opened && scan_result.diagnostics.empty())
        parse_result = vista::parse_tokens(scan_result.tokens);
    if (parse_result.workflow != nullptr) {
        for (const auto* use : parse_result.workflow->uses) {
            if (overlaps_output(std::filesystem::path(source_path).parent_path() / use->source_path)) {
                std::cerr << "VISTA: module input overlaps output\n"; return 2;
            }
        }
    }
    if (certificate_mode) {
        // Protect the supplied target from cleanup, including output/workflow.js aliases.
        if (supplied_javascript && !read_bytes(argv[7],exact_javascript)) {
            remove_artifact(output_directory,"workflow.vcert"); return 2;
        }
    }
    if (directory_mode) {
        if (!prepare_output_directory(output_directory) || !clear_outputs()) {
            return 2;
        }
    }

    if (!scan_result.input_opened) {
        std::cerr << "VISTA: cannot open input file '" << source_path << "'\n";
        return 2;
    }

    const std::string tokens_text = format_tokens(scan_result.tokens);
    if (emit_mode == "tokens") {
        std::cout << tokens_text;
    } else if (all_mode && !write_artifact(output_directory, "tokens.txt", tokens_text)) {
        return 2;
    }
    print_diagnostics(source_path, scan_result.diagnostics);
    if (!scan_result.diagnostics.empty()) {
        if (all_mode && !write_failure_diagnostics(
                            output_directory, source_path, scan_result.diagnostics)) {
            return 2;
        }
        return 1;
    }
    if (emit_mode == "tokens") {
        return 0;
    }

    if (!parse_result.succeeded()) {
        print_diagnostics(source_path, parse_result.diagnostics);
        if (all_mode && !write_failure_diagnostics(
                            output_directory, source_path, parse_result.diagnostics)) {
            return 2;
        }
        return 1;
    }

    std::string ast_text;
    if (parse_result.form != nullptr) {
        ast_text = vista::format_ast(*parse_result.form);
    } else if (parse_result.module != nullptr) {
        ast_text = vista::format_ast(*parse_result.module);
    } else {
        ast_text = vista::format_ast(*parse_result.workflow);
    }
    if (emit_mode == "ast") {
        std::cout << ast_text;
        return 0;
    }
    if (linked_mode || workflow_mode || certificate_mode || emit_mode == "partitions") {
        if (parse_result.workflow == nullptr) {
            std::cerr << "VISTA: --emit " << emit_mode << " requires a workflow source\n";
            return 2;
        }
        std::vector<vista::ArtifactEvidence> source_artifacts;
        if (certificate_mode) {
            // Preserve link/.. spelling: lexical normalization can name a different file.
            source_artifacts.push_back({"workflow-source",std::filesystem::absolute(source_path).string(),vista::sha256_hex(workflow_source_bytes)});
            for (const auto* use : parse_result.workflow->uses) {
                const auto path = std::filesystem::path(source_path).parent_path()/use->source_path;
                std::string bytes;
                if (!read_bytes(path,bytes)) return 2;
                source_artifacts.push_back({(path.extension() == ".vobj" ? "module-object." : "module-source.")+use->alias,
                    std::filesystem::absolute(path).string(),vista::sha256_hex(bytes)});
            }
        }
        const auto linked = vista::link_workflow(*parse_result.workflow, source_path);
        print_diagnostics(source_path, linked.diagnostics);
        if (!linked.succeeded()) return linked.operational_failure ? 2 : 1;
        if (emit_mode == "partitions") {
            const auto partitions = vista::derive_partitions(linked.workflow);
            const auto suite = vista::enumerate_route_cases(linked.workflow, partitions);
            std::cout << vista::format_partitions(partitions, suite);
            return suite.complete() ? 0 : 1;
        }
        if (workflow_mode || certificate_mode) {
            const auto javascript = supplied_javascript ? exact_javascript : vista::generate_workflow_javascript(linked.workflow);
            const auto html = vista::generate_workflow_html(linked.workflow, javascript);
            const auto ir = vista::format_linked_workflow(linked.workflow);
            if (!write_artifact(output_directory, "workflow.ir", ir) ||
                !write_artifact(output_directory, "workflow.js", javascript) ||
                !write_artifact(output_directory, "workflow.html", html)) {
                clear_workflow_artifacts(output_directory);
                return 2;
            }
            if (certificate_mode) {
                const auto partitions = vista::derive_partitions(linked.workflow);
                const auto suite = vista::enumerate_route_cases(linked.workflow,partitions);
                auto result = vista::certify_workflow(linked.workflow,suite,javascript,output_directory);
                print_diagnostics(source_path,result.diagnostics);
                for (const auto& d : result.diagnostics) if (d.code == "CERT006") return 2;
                auto& certificate = result.certificate;
                certificate.artifacts.insert(certificate.artifacts.end(),source_artifacts.begin(),source_artifacts.end());
                certificate.artifacts.push_back({"workflow-ir","workflow.ir",vista::sha256_hex(ir)});
                certificate.artifacts.push_back({"html","workflow.html",vista::sha256_hex(html)});
                if (!write_artifact(output_directory,"workflow.vcert",vista::serialize_certificate(certificate))) {
                    remove_artifact(output_directory,"workflow.vcert"); return 2;
                }
                std::cout << "Certificate " << (result.succeeded() ? "PASS" : certificate.status == vista::CertificateStatus::Unsupported ? "UNSUPPORTED" : "FAIL")
                          << " in " << (output_directory/"workflow.vcert").string() << '\n';
                return result.succeeded() ? 0 : 1;
            }
            std::cout << "Generated workflow output in " << output_directory.string() << '\n';
            return 0;
        }
        if (!write_artifact(output_directory, "workflow.ir", vista::format_linked_workflow(linked.workflow))) return 2;
        std::cout << "Generated " << (output_directory / "workflow.ir").string() << '\n';
        return 0;
    }
    if (module_mode) {
        if (parse_result.module == nullptr) {
            std::cerr << "VISTA: --emit module requires a module source\n";
            return 2;
        }
        const vista::ModuleCompileResult module_result =
            vista::compile_module(*parse_result.module, workflow_source_bytes);
        print_diagnostics(source_path, module_result.diagnostics);
        if (!module_result.succeeded()) {
            return 1;
        }
        if (!write_artifact(output_directory,
                            "module.vobj",
                            vista::serialize_module_object(module_result.object))) {
            return 2;
        }
        std::cout << "Generated " << (output_directory / "module.vobj").string()
                  << '\n';
        return 0;
    }
    if (validation_mode && parse_result.workflow != nullptr) {
        const auto linked = vista::link_workflow(*parse_result.workflow, source_path);
        print_diagnostics(source_path, linked.diagnostics);
        if (!linked.succeeded()) return linked.operational_failure ? 2 : 1;
        const auto input = vista::load_input_values(argv[3]);
        auto evaluation = vista::evaluate_workflow(linked.workflow, input.values);
        for (const auto& diagnostic : input.diagnostics) {
            evaluation.diagnostics.push_back(diagnostic);
            evaluation.observation.diagnostic_codes.push_back(diagnostic.code);
            evaluation.observation.diagnostic_details.push_back({diagnostic.code, "", ""});
        }
        if (!input.succeeded()) evaluation.observation.complete = false;
        std::cout << vista::format_observation(evaluation.observation);
        for (const auto& diagnostic : evaluation.diagnostics)
            std::cout << "  " << diagnostic.code << ": " << diagnostic.message << '\n';
        if (input.operational_failure) return 2;
        return evaluation.succeeded() ? 0 : 1;
    }
    if (parse_result.form == nullptr) {
        std::cerr << "VISTA: only --emit ast is supported for modules and workflows\n";
        return 2;
    }
    if (all_mode && !write_artifact(output_directory, "ast.txt", ast_text)) {
        return 2;
    }

    const vista::SemanticResult semantic_result =
        vista::analyze_semantics(*parse_result.form);
    const std::string symbols_text = vista::format_symbol_table(semantic_result.symbols);
    if (emit_mode == "symbols") {
        std::cout << symbols_text;
    } else if (all_mode &&
               !write_artifact(output_directory, "symbols.txt", symbols_text)) {
        return 2;
    }
    print_diagnostics(source_path, semantic_result.diagnostics);
    if (!semantic_result.succeeded()) {
        if (all_mode && !write_failure_diagnostics(
                            output_directory, source_path, semantic_result.diagnostics)) {
            return 2;
        }
        return 1;
    }
    if (emit_mode == "symbols") {
        return 0;
    }

    const vista::AnalysisResult analysis_result =
        vista::analyze_dependencies(*parse_result.form, semantic_result);
    const std::string dependencies_text = vista::format_dependencies(analysis_result);
    const std::string graph_text =
        vista::format_dependency_graph_dot(*parse_result.form, analysis_result);
    if (emit_mode == "dependencies") {
        std::cout << dependencies_text;
    } else if (emit_mode == "graph") {
        std::cout << graph_text;
    } else if (all_mode) {
        if (!write_artifact(output_directory, "dependencies.txt", dependencies_text) ||
            !write_artifact(output_directory, "graph.dot", graph_text)) {
            return 2;
        }
    }
    print_diagnostics(source_path, analysis_result.diagnostics);
    if (!analysis_result.succeeded()) {
        if (all_mode && !write_failure_diagnostics(
                            output_directory, source_path, analysis_result.diagnostics)) {
            return 2;
        }
        return 1;
    }
    if (emit_mode == "dependencies" || emit_mode == "graph") {
        return 0;
    }

    if (validation_mode) {
        const vista::ValidationResult validation =
            vista::validate_data_file(*parse_result.form, semantic_result, argv[3]);
        std::cout << vista::format_validation_report(validation);
        if (validation.operational_failure) {
            return 2;
        }
        return validation.succeeded() ? 0 : 1;
    }

    if (emit_mode == "diagnostics") {
        std::cout << "No diagnostics.\n";
        return 0;
    }

    const std::string html_text =
        vista::generate_html(*parse_result.form, semantic_result, analysis_result);
    if (all_mode) {
        if (!write_artifact(output_directory, "diagnostics.txt", "No diagnostics.\n") ||
            !write_artifact(output_directory, "form.html", html_text)) {
            return 2;
        }
        std::cout << "Generated complete VISTA output in "
                  << output_directory.string() << '\n';
        return 0;
    }

    if (!write_artifact(output_directory, "form.html", html_text)) {
        return 2;
    }
    std::cout << "Generated " << (output_directory / "form.html").string() << '\n';
    return 0;
}
