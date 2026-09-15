#ifndef VISTA_SCANNER_HPP
#define VISTA_SCANNER_HPP

#include <string>
#include <vector>

#include "diagnostic.hpp"
#include "token.hpp"

namespace vista {

struct ScanResult {
    bool input_opened = false;
    std::vector<Token> tokens;
    std::vector<Diagnostic> diagnostics;
};

ScanResult scan_file(const std::string& source_path);

}  // namespace vista

#endif
