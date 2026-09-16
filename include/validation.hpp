#ifndef VISTA_VALIDATION_HPP
#define VISTA_VALIDATION_HPP

#include <string>
#include <vector>

#include "ast.hpp"
#include "diagnostic.hpp"
#include "semantic.hpp"

namespace vista {

struct ValidationResult {
    bool data_opened = false;
    std::size_t supplied_fields = 0;
    std::size_t checked_fields = 0;
    std::size_t passed_checks = 0;
    std::size_t skipped_checks = 0;
    std::vector<Diagnostic> diagnostics;

    bool succeeded() const {
        return data_opened && diagnostics.empty();
    }
};

ValidationResult validate_data_file(const FormAst& form,
                                    const SemanticResult& semantics,
                                    const std::string& data_path);
std::string format_validation_report(const ValidationResult& result);

}  // namespace vista

#endif
