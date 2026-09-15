#include <iomanip>
#include <iostream>
#include <string>

#include "ast.hpp"
#include "diagnostic.hpp"
#include "parser.hpp"
#include "scanner.hpp"
#include "token.hpp"
#include "version.hpp"

namespace {

void print_help(std::ostream& output) {
    output << "VISTA - Validation and Interface Specification Translation Analyzer\n\n"
           << "Usage: vista <source.vista> --emit <tokens|ast>\n"
           << "       vista [option]\n\n"
           << "Options:\n"
           << "  --help       Show this help message\n"
           << "  --version    Show the current VISTA version\n"
           << "  --emit tokens  Print the positioned Flex token stream\n"
           << "  --emit ast     Parse the source and print its abstract syntax tree\n";
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

    if (argc != 4 || std::string(argv[2]) != "--emit") {
        std::cerr << "VISTA: invalid command-line arguments\n"
                  << "Run 'vista --help' to see the available options.\n";
        return 2;
    }

    const vista::ScanResult result = vista::scan_file(first_argument);
    if (!result.input_opened) {
        std::cerr << "VISTA: cannot open input file '" << first_argument << "'\n";
        return 2;
    }

    const std::string emit_mode = argv[3];
    if (emit_mode != "tokens" && emit_mode != "ast") {
        std::cerr << "VISTA: unknown emit mode '" << emit_mode << "'\n";
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

    if (emit_mode == "ast") {
        const vista::ParseResult parse_result = vista::parse_tokens(result.tokens);
        for (const vista::Diagnostic& diagnostic : parse_result.diagnostics) {
            std::cerr << vista::format_diagnostic(first_argument, diagnostic) << '\n';
        }
        if (!parse_result.succeeded()) {
            return 1;
        }
        std::cout << vista::format_ast(*parse_result.form);
    }

    return 0;
}
