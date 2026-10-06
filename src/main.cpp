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
           << "       vista <source.vista> --emit <html|all|module|linked|workflow> --out-dir <directory>\n"
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
        ((argc != 4 && argc != 6) || std::string(argv[2]) != "--emit")) {
        std::cerr << "VISTA: invalid command-line arguments\n"
                  << "Run 'vista --help' to see the available options.\n";
        return 2;
    }

    const std::string emit_mode = validation_mode ? "" : argv[3];
    const bool module_mode = emit_mode == "module";
    const bool linked_mode = emit_mode == "linked";
    const bool workflow_mode = emit_mode == "workflow";
    const bool directory_mode = emit_mode == "html" || emit_mode == "all" ||
                                module_mode || linked_mode || workflow_mode;
    const bool known_mode =
        emit_mode == "tokens" || emit_mode == "ast" || emit_mode == "symbols" ||
        emit_mode == "dependencies" || emit_mode == "graph" ||
        emit_mode == "diagnostics" || emit_mode == "partitions" || directory_mode;
    if (!validation_mode && !known_mode) {
        std::cerr << "VISTA: unknown emit mode '" << emit_mode << "'\n";
        return 2;
    }
    if (!validation_mode && ((directory_mode &&
          (argc != 6 || std::string(argv[4]) != "--out-dir" ||
           std::string(argv[5]).empty())) ||
        (!directory_mode && argc != 4))) {
        std::cerr << "VISTA: invalid command-line arguments\n"
                  << "Run 'vista --help' to see the available options.\n";
        return 2;
    }

    const bool all_mode = emit_mode == "all";
    const std::filesystem::path output_directory = directory_mode ? argv[5] : "";
    if (directory_mode) {
        if (!prepare_output_directory(output_directory) ||
            !(workflow_mode ? clear_workflow_artifacts(output_directory) :
              linked_mode ? remove_artifact(output_directory, "workflow.ir") :
              clear_generated_artifacts(output_directory, all_mode, module_mode))) {
            return 2;
        }
    }

    const vista::ScanResult scan_result = vista::scan_file(source_path);
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

    const vista::ParseResult parse_result = vista::parse_tokens(scan_result.tokens);
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
    if (linked_mode || workflow_mode || emit_mode == "partitions") {
        if (parse_result.workflow == nullptr) {
            std::cerr << "VISTA: --emit " << emit_mode << " requires a workflow source\n";
            return 2;
        }
        const auto linked = vista::link_workflow(*parse_result.workflow, source_path);
        print_diagnostics(source_path, linked.diagnostics);
        if (!linked.succeeded()) return 1;
        if (emit_mode == "partitions") {
            const auto partitions = vista::derive_partitions(linked.workflow);
            const auto suite = vista::enumerate_route_cases(linked.workflow, partitions);
            std::cout << vista::format_partitions(partitions, suite);
            return suite.complete() ? 0 : 1;
        }
        if (workflow_mode) {
            const auto javascript = vista::generate_workflow_javascript(linked.workflow);
            const auto html = vista::generate_workflow_html(linked.workflow, javascript);
            const auto ir = vista::format_linked_workflow(linked.workflow);
            if (!write_artifact(output_directory, "workflow.ir", ir) ||
                !write_artifact(output_directory, "workflow.js", javascript) ||
                !write_artifact(output_directory, "workflow.html", html)) {
                clear_workflow_artifacts(output_directory);
                return 2;
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
        std::ifstream source_file(source_path, std::ios::binary);
        if (!source_file) {
            std::cerr << "VISTA: cannot reopen input file '" << source_path << "'\n";
            return 2;
        }
        const std::string source_text((std::istreambuf_iterator<char>(source_file)),
                                      std::istreambuf_iterator<char>());
        if (source_file.bad()) {
            std::cerr << "VISTA: failed while reading input file '" << source_path << "'\n";
            return 2;
        }
        const vista::ModuleCompileResult module_result =
            vista::compile_module(*parse_result.module, source_text);
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
        if (!linked.succeeded()) return 1;
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
        if (!input.data_opened) return 2;
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
        if (!validation.data_opened) {
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
