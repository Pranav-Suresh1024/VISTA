#ifndef VISTA_DIAGNOSTIC_HPP
#define VISTA_DIAGNOSTIC_HPP

#include <cstddef>
#include <string>

namespace vista {

struct Diagnostic {
    std::string code;
    std::string message;
    std::size_t line = 1;
    std::size_t column = 1;
};

std::string format_diagnostic(const std::string& source_path,
                              const Diagnostic& diagnostic);

}  // namespace vista

#endif
