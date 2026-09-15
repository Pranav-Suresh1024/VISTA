#ifndef VISTA_CODEGEN_HPP
#define VISTA_CODEGEN_HPP

#include <string>

#include "analysis.hpp"
#include "ast.hpp"
#include "semantic.hpp"

namespace vista {

std::string generate_html(const FormAst& form,
                          const SemanticResult& semantics,
                          const AnalysisResult& analysis);

}  // namespace vista

#endif
