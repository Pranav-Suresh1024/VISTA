#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "analysis.hpp"
#include "ast.hpp"
#include "codegen.hpp"
#include "diagnostic.hpp"
#include "parser.hpp"
#include "scanner.hpp"
#include "semantic.hpp"
#include "token.hpp"
#include "version.hpp"

namespace {

struct StarterTemplate {
    const char* id;
    const char* title;
    const char* source_path;
};

constexpr StarterTemplate kStarterTemplates[]{
    {"scholarship", "Scholarship Application", "templates/scholarship_application.vista"},
    {"train-booking", "Train Ticket Booking", "templates/train_booking.vista"},
    {"event-registration", "Event Registration", "templates/event_registration.vista"}
};

const StarterTemplate* find_template(const std::string& id) {
    for (const StarterTemplate& starter_template : kStarterTemplates) {
        if (id == starter_template.id) {
            return &starter_template;
        }
    }
    return nullptr;
}

void print_templates(std::ostream& output) {
    output << "Available VISTA starter templates:\n";
    for (const StarterTemplate& starter_template : kStarterTemplates) {
        output << "  " << starter_template.id << " - " << starter_template.title
               << " (" << starter_template.source_path << ")\n";
    }
}

void print_help(std::ostream& output) {
    output << "VISTA - Validation and Interface Specification Translation Analyzer\n\n"
           << "Usage: vista <source.vista> --emit <tokens|ast|symbols|dependencies|graph|diagnostics>\n"
           << "       vista <source.vista> --emit <html|all> --out-dir <directory>\n"
           << "       vista --template <id> --emit <mode> [--out-dir <directory>]\n"
           << "       vista --list-templates\n"
           << "       vista [option]\n\n"
           << "Options:\n"
           << "  --help       Show this help message\n"
           << "  --version    Show the current VISTA version\n"
           << "  --emit tokens  Print the positioned Flex token stream\n"
           << "  --emit ast     Parse the source and print its abstract syntax tree\n"
           << "  --emit symbols Print the field symbol table\n"
           << "  --emit dependencies  Print conditional field dependencies\n"
           << "  --emit graph   Print the dependency graph in DOT format\n"
           << "  --emit diagnostics  Run semantic and dependency analysis\n"
           << "  --emit html --out-dir DIR  Generate a standalone HTML form\n"
           << "  --emit all --out-dir DIR   Write every demonstration artifact\n"
           << "  --list-templates           List the built-in starter templates\n"
           << "  --template ID              Compile a starter template by ID\n";
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

bool clear_generated_artifacts(const std::filesystem::path& directory,
                               bool all_mode) {
    static const std::vector<std::string> all_artifacts{
        "tokens.txt",
        "ast.txt",
        "symbols.txt",
        "dependencies.txt",
        "graph.dot",
        "diagnostics.txt",
        "form.html"
    };
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

    std::string source_path = argv[1];
    if (source_path == "--help") {
        print_help(std::cout);
        return 0;
    }
    if (source_path == "--version") {
        std::cout << vista::kProgramName << ' ' << vista::kVersion << '\n';
        return 0;
    }

    if (source_path == "--list-templates") {
        if (argc != 2) {
            std::cerr << "VISTA: invalid command-line arguments\n"
                      << "Run 'vista --help' to see the available options.\n";
            return 2;
        }
        print_templates(std::cout);
        return 0;
    }

    int emit_flag_index = 2;
    if (source_path == "--template") {
        if (argc < 3) {
            std::cerr << "VISTA: missing template ID\n"
                      << "Run 'vista --list-templates' to see available templates.\n";
            return 2;
        }
        const StarterTemplate* starter_template = find_template(argv[2]);
        if (starter_template == nullptr) {
            std::cerr << "VISTA: unknown template ID '" << argv[2] << "'\n"
                      << "Run 'vista --list-templates' to see available templates.\n";
            return 2;
        }
        source_path = starter_template->source_path;
        emit_flag_index = 3;
    }

    if ((argc != emit_flag_index + 2 && argc != emit_flag_index + 4) ||
        std::string(argv[emit_flag_index]) != "--emit") {
        std::cerr << "VISTA: invalid command-line arguments\n"
                  << "Run 'vista --help' to see the available options.\n";
        return 2;
    }

    const std::string emit_mode = argv[emit_flag_index + 1];
    const bool directory_mode = emit_mode == "html" || emit_mode == "all";
    const bool known_mode =
        emit_mode == "tokens" || emit_mode == "ast" || emit_mode == "symbols" ||
        emit_mode == "dependencies" || emit_mode == "graph" ||
        emit_mode == "diagnostics" || directory_mode;
    if (!known_mode) {
        std::cerr << "VISTA: unknown emit mode '" << emit_mode << "'\n";
        return 2;
    }
    if ((directory_mode &&
         (argc != emit_flag_index + 4 ||
          std::string(argv[emit_flag_index + 2]) != "--out-dir" ||
          std::string(argv[emit_flag_index + 3]).empty())) ||
        (!directory_mode && argc != emit_flag_index + 2)) {
        std::cerr << "VISTA: invalid command-line arguments\n"
                  << "Run 'vista --help' to see the available options.\n";
        return 2;
    }

    const vista::ScanResult scan_result = vista::scan_file(source_path);
    if (!scan_result.input_opened) {
        std::cerr << "VISTA: cannot open input file '" << source_path << "'\n";
        return 2;
    }

    const bool all_mode = emit_mode == "all";
    const std::filesystem::path output_directory =
        directory_mode ? argv[emit_flag_index + 3] : "";
    if (directory_mode) {
        if (!prepare_output_directory(output_directory) ||
            !clear_generated_artifacts(output_directory, all_mode)) {
            return 2;
        }
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

    const std::string ast_text = vista::format_ast(*parse_result.form);
    if (emit_mode == "ast") {
        std::cout << ast_text;
        return 0;
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
