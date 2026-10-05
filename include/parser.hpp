#ifndef VISTA_PARSER_HPP
#define VISTA_PARSER_HPP

#include <cstddef>
#include <vector>

#include "ast.hpp"
#include "diagnostic.hpp"
#include "token.hpp"

namespace vista {

struct ParseResult {
    AstArena arena;
    FormAst* form = nullptr;
    ModuleAst* module = nullptr;
    WorkflowAst* workflow = nullptr;
    std::vector<Diagnostic> diagnostics;

    bool succeeded() const {
        const int root_count = (form != nullptr ? 1 : 0) +
                               (module != nullptr ? 1 : 0) +
                               (workflow != nullptr ? 1 : 0);
        return root_count == 1 && diagnostics.empty();
    }
};

struct ParserContext {
    ParserContext(const std::vector<Token>& source_tokens, ParseResult& parse_result)
        : tokens(source_tokens), result(parse_result) {}

    const std::vector<Token>& tokens;
    ParseResult& result;
    std::size_t next_token = 0;
    std::string form_title;
    std::string form_description;
    bool has_form_title = false;
    bool has_form_description = false;
    std::vector<ImportDecl*> module_imports;
    std::vector<ExportDecl*> module_exports;
    Expression* module_enter_condition = nullptr;
    std::vector<Declaration*> module_declarations;
    std::vector<WorkflowUse*> workflow_uses;
    std::string workflow_start_module;
    std::vector<RouteDecl*> workflow_routes;
    bool has_module_enter = false;
    bool has_workflow_start = false;
};

ParseResult parse_tokens(const std::vector<Token>& tokens);

}  // namespace vista

#endif
