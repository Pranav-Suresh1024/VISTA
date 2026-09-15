#include <iostream>
#include <string>

#include "version.hpp"

namespace {

void print_help(std::ostream& output) {
    output << "VISTA - Validation and Interface Specification Translation Analyzer\n\n"
           << "Usage: vista [option]\n\n"
           << "Options:\n"
           << "  --help       Show this help message\n"
           << "  --version    Show the current VISTA version\n\n"
           << "Source-file compilation will be added in the next implementation stage.\n";
}

}  // namespace

int main(int argc, char* argv[]) {
    if (argc == 1) {
        print_help(std::cout);
        return 0;
    }

    const std::string argument = argv[1];

    if (argument == "--help") {
        print_help(std::cout);
        return 0;
    }

    if (argument == "--version") {
        std::cout << vista::kProgramName << ' ' << vista::kVersion << '\n';
        return 0;
    }

    std::cerr << "VISTA: unknown option '" << argument << "'\n"
              << "Run 'vista --help' to see the available options.\n";
    return 2;
}
