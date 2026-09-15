#include "diagnostic.hpp"

#include <sstream>

namespace vista {

std::string format_diagnostic(const std::string& source_path,
                              const Diagnostic& diagnostic) {
    std::ostringstream output;
    output << source_path << ':' << diagnostic.line << ':' << diagnostic.column << ": "
           << diagnostic.code << ": " << diagnostic.message;
    return output.str();
}

}  // namespace vista
