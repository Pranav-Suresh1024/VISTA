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
    std::vector<Diagnostic> diagnostics;

    bool succeeded() const {
        return form != nullptr && diagnostics.empty();
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
};

ParseResult parse_tokens(const std::vector<Token>& tokens);

}  // namespace vista

#endif
