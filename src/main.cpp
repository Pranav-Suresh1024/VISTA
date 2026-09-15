#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>

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

void print_help(std::ostream& output) {
    output << "VISTA - Validation and Interface Specification Translation Analyzer\n\n"
           << "Usage: vista <source.vista> --emit <tokens|ast|symbols|dependencies|graph|diagnostics>\n"
           << "       vista <source.vista> --emit html --out-dir <directory>\n"
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
           << "  --emit html --out-dir DIR  Generate a standalone HTML form\n";
}

void print_tokens(const std::vector<vista::Token>& tokens) {
    std::cout << std::left << std::setw(10) << "LOCATION"
              << std::setw(22) << "TOKEN" << "LEXEME\n";
    std::cout << std::string(56, '-') << '\n';

    for (const vista::Token& token : tokens) {
        const std::string location =
            std::to_string(token.line) + ':' + std::to_string(token.column);
        std::cout << std::left << std::setw(10) << location
                  << std::setw(22) << vista::token_kind_name(token.kind)
                  << vista::display_lexeme(token.lexeme) << '\n';
    }
}

}  // namespace

int main(int argc, char* argv[]) {
    if (argc == 1) {
        print_help(std::cout);
        return 0;
    }

    const std::string first_argument = argv[1];

    if (first_argument == "--help") {
        print_help(std::cout);
        return 0;
    }

    if (first_argument == "--version") {
        std::cout << vista::kProgramName << ' ' << vista::kVersion << '\n';
        return 0;
    }

    if ((argc != 4 && argc != 6) || std::string(argv[2]) != "--emit") {
        std::cerr << "VISTA: invalid command-line arguments\n"
                  << "Run 'vista --help' to see the available options.\n";
        return 2;
    }

    const std::string emit_mode = argv[3];
    if (emit_mode != "tokens" && emit_mode != "ast" && emit_mode != "symbols" &&
        emit_mode != "dependencies" && emit_mode != "graph" &&
        emit_mode != "diagnostics" && emit_mode != "html") {
        std::cerr << "VISTA: unknown emit mode '" << emit_mode << "'\n";
        return 2;
    }
    const bool html_mode = emit_mode == "html";
    if ((html_mode && (argc != 6 || std::string(argv[4]) != "--out-dir" ||
                       std::string(argv[5]).empty())) ||
        (!html_mode && argc != 4)) {
        std::cerr << "VISTA: invalid command-line arguments\n"
                  << "Run 'vista --help' to see the available options.\n";
        return 2;
    }

    const vista::ScanResult result = vista::scan_file(first_argument);
    if (!result.input_opened) {
        std::cerr << "VISTA: cannot open input file '" << first_argument << "'\n";
        return 2;
    }

    if (emit_mode == "tokens") {
        print_tokens(result.tokens);
    }
    for (const vista::Diagnostic& diagnostic : result.diagnostics) {
        std::cerr << vista::format_diagnostic(first_argument, diagnostic) << '\n';
    }
    if (!result.diagnostics.empty()) {
        return 1;
    }

    if (emit_mode != "tokens") {
        const vista::ParseResult parse_result = vista::parse_tokens(result.tokens);
        if (!parse_result.succeeded()) {
            for (const vista::Diagnostic& diagnostic : parse_result.diagnostics) {
                std::cerr << vista::format_diagnostic(first_argument, diagnostic) << '\n';
            }
            return 1;
        }

        if (emit_mode == "ast") {
            std::cout << vista::format_ast(*parse_result.form);
            return 0;
        }

        const vista::SemanticResult semantic_result =
            vista::analyze_semantics(*parse_result.form);
        if (emit_mode == "symbols") {
            std::cout << vista::format_symbol_table(semantic_result.symbols);
        }
        for (const vista::Diagnostic& diagnostic : semantic_result.diagnostics) {
            std::cerr << vista::format_diagnostic(first_argument, diagnostic) << '\n';
        }
        if (!semantic_result.succeeded() || emit_mode == "symbols") {
            return semantic_result.succeeded() ? 0 : 1;
        }

        const vista::AnalysisResult analysis_result =
            vista::analyze_dependencies(*parse_result.form, semantic_result);
        if (emit_mode == "dependencies") {
            std::cout << vista::format_dependencies(analysis_result);
        } else if (emit_mode == "graph") {
            std::cout << vista::format_dependency_graph_dot(
                *parse_result.form, analysis_result);
        }
        for (const vista::Diagnostic& diagnostic : analysis_result.diagnostics) {
            std::cerr << vista::format_diagnostic(first_argument, diagnostic) << '\n';
        }
        if (!analysis_result.succeeded()) {
            return 1;
        }
        if (emit_mode == "html") {
            const std::filesystem::path output_directory = argv[5];
            std::error_code error;
            std::filesystem::create_directories(output_directory, error);
            if (error) {
                std::cerr << "VISTA: cannot create output directory '"
                          << output_directory.string() << "': " << error.message() << '\n';
                return 2;
            }
            const std::filesystem::path output_path = output_directory / "form.html";
            std::ofstream output_file(output_path, std::ios::binary);
            if (!output_file) {
                std::cerr << "VISTA: cannot write output file '"
                          << output_path.string() << "'\n";
                return 2;
            }
            output_file << vista::generate_html(
                *parse_result.form, semantic_result, analysis_result);
            if (!output_file) {
                std::cerr << "VISTA: failed while writing output file '"
                          << output_path.string() << "'\n";
                return 2;
            }
            std::cout << "Generated " << output_path.string() << '\n';
            return 0;
        }
        if (emit_mode == "diagnostics" && analysis_result.diagnostics.empty()) {
            std::cout << "No diagnostics.\n";
        }
        return 0;
    }

    return 0;
}
