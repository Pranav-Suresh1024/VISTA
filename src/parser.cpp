#include "parser.hpp"

#include <set>
#include <string>

#include "vista_parser.hpp"

#define VISTA_ASSERT_TOKEN(vista_name, bison_name) \
    static_assert(static_cast<int>(vista::TokenKind::vista_name) == bison_name)

VISTA_ASSERT_TOKEN(Form, FORM);
VISTA_ASSERT_TOKEN(Field, FIELD);
VISTA_ASSERT_TOKEN(Label, LABEL);
VISTA_ASSERT_TOKEN(Required, REQUIRED);
VISTA_ASSERT_TOKEN(Show, SHOW);
VISTA_ASSERT_TOKEN(When, WHEN);
VISTA_ASSERT_TOKEN(Check, CHECK);
VISTA_ASSERT_TOKEN(Message, MESSAGE);
VISTA_ASSERT_TOKEN(TypeText, TYPE_TEXT);
VISTA_ASSERT_TOKEN(TypeInteger, TYPE_INTEGER);
VISTA_ASSERT_TOKEN(TypeDecimal, TYPE_DECIMAL);
VISTA_ASSERT_TOKEN(TypeBoolean, TYPE_BOOLEAN);
VISTA_ASSERT_TOKEN(TypeDate, TYPE_DATE);
VISTA_ASSERT_TOKEN(TypeChoice, TYPE_CHOICE);
VISTA_ASSERT_TOKEN(TypeFile, TYPE_FILE);
VISTA_ASSERT_TOKEN(TrueLiteral, TRUE_LITERAL);
VISTA_ASSERT_TOKEN(FalseLiteral, FALSE_LITERAL);
VISTA_ASSERT_TOKEN(And, AND);
VISTA_ASSERT_TOKEN(Or, OR);
VISTA_ASSERT_TOKEN(Not, NOT);
VISTA_ASSERT_TOKEN(Identifier, IDENTIFIER);
VISTA_ASSERT_TOKEN(StringLiteral, STRING_LITERAL);
VISTA_ASSERT_TOKEN(IntegerLiteral, INTEGER_LITERAL);
VISTA_ASSERT_TOKEN(DecimalLiteral, DECIMAL_LITERAL);
VISTA_ASSERT_TOKEN(Equal, EQUAL);
VISTA_ASSERT_TOKEN(NotEqual, NOT_EQUAL);
VISTA_ASSERT_TOKEN(Less, LESS);
VISTA_ASSERT_TOKEN(LessEqual, LESS_EQUAL);
VISTA_ASSERT_TOKEN(Greater, GREATER);
VISTA_ASSERT_TOKEN(GreaterEqual, GREATER_EQUAL);
VISTA_ASSERT_TOKEN(LeftBrace, LEFT_BRACE);
VISTA_ASSERT_TOKEN(RightBrace, RIGHT_BRACE);
VISTA_ASSERT_TOKEN(LeftParen, LEFT_PAREN);
VISTA_ASSERT_TOKEN(RightParen, RIGHT_PAREN);
VISTA_ASSERT_TOKEN(Colon, COLON);
VISTA_ASSERT_TOKEN(Comma, COMMA);
VISTA_ASSERT_TOKEN(TypeEmail, TYPE_EMAIL);
VISTA_ASSERT_TOKEN(TypePhone, TYPE_PHONE);
VISTA_ASSERT_TOKEN(TypeTextarea, TYPE_TEXTAREA);
VISTA_ASSERT_TOKEN(Module, MODULE);
VISTA_ASSERT_TOKEN(Import, IMPORT);
VISTA_ASSERT_TOKEN(Export, EXPORT);
VISTA_ASSERT_TOKEN(Enter, ENTER);
VISTA_ASSERT_TOKEN(Workflow, WORKFLOW);
VISTA_ASSERT_TOKEN(Use, USE);
VISTA_ASSERT_TOKEN(As, AS);
VISTA_ASSERT_TOKEN(Start, START);
VISTA_ASSERT_TOKEN(Route, ROUTE);
VISTA_ASSERT_TOKEN(Present, PRESENT);
VISTA_ASSERT_TOKEN(Arrow, ARROW);
VISTA_ASSERT_TOKEN(Dot, DOT);

#undef VISTA_ASSERT_TOKEN

namespace {

std::string decode_string(const std::string& lexeme) {
    std::string value;
    if (lexeme.size() < 2) {
        return value;
    }

    for (std::size_t index = 1; index + 1 < lexeme.size(); ++index) {
        if (lexeme[index] != '\\' || index + 2 >= lexeme.size()) {
            value.push_back(lexeme[index]);
            continue;
        }

        ++index;
        switch (lexeme[index]) {
            case 'n': value.push_back('\n'); break;
            case 'r': value.push_back('\r'); break;
            case 't': value.push_back('\t'); break;
            case '\\': value.push_back('\\'); break;
            case '"': value.push_back('"'); break;
            default: value.push_back(lexeme[index]); break;
        }
    }
    return value;
}

}  // namespace

int yylex(YYSTYPE* semantic_value,
          YYLTYPE* location,
          vista::ParserContext& context) {
    if (context.next_token >= context.tokens.size()) {
        return 0;
    }

    const vista::Token& token = context.tokens[context.next_token++];
    location->first_line = static_cast<int>(token.line);
    location->first_column = static_cast<int>(token.column);
    location->last_line = static_cast<int>(token.line);
    location->last_column = static_cast<int>(
        token.column + (token.lexeme.empty() ? 0 : token.lexeme.size() - 1));

    switch (token.kind) {
        case vista::TokenKind::Identifier:
        case vista::TokenKind::IntegerLiteral:
        case vista::TokenKind::DecimalLiteral:
        case vista::TokenKind::TypeEmail:
        case vista::TokenKind::TypePhone:
        case vista::TokenKind::TypeTextarea:
            semantic_value->text = new std::string(token.lexeme);
            break;
        case vista::TokenKind::StringLiteral:
            semantic_value->text = new std::string(decode_string(token.lexeme));
            break;
        default:
            break;
    }

    return static_cast<int>(token.kind);
}

void yyerror(YYLTYPE* location,
             vista::ParserContext& context,
             const char* message) {
    context.result.diagnostics.push_back(
        {"SYN001",
         message == nullptr ? "syntax error" : std::string(message),
         static_cast<std::size_t>(location->first_line),
         static_cast<std::size_t>(location->first_column)});
}

namespace vista {

ParseResult parse_tokens(const std::vector<Token>& tokens) {
    ParseResult result;
    for (std::size_t i = 0; i < tokens.size(); ++i) {
        if (tokens[i].kind != TokenKind::Use) continue;
        if (i + 1 < tokens.size() && tokens[i + 1].kind == TokenKind::StringLiteral)
            result.module_input_paths.push_back(decode_string(tokens[i + 1].lexeme));
        else result.module_inputs_known = false;
    }
    ParserContext context(tokens, result);
    const int status = yyparse(context);
    if (status != 0 && result.diagnostics.empty()) {
        result.diagnostics.push_back({"SYN001", "syntax analysis failed", 1, 1});
    }
    if (status == 0 && result.module != nullptr) {
        std::set<std::string> exported_fields;
        for (const ExportDecl* export_decl : result.module->exports) {
            if (!exported_fields.insert(export_decl->field).second) {
                result.diagnostics.push_back(
                    {"MOD003", "field '" + export_decl->field +
                                   "' is exported more than once",
                     export_decl->span.first_line, export_decl->span.first_column});
            }
        }
    }
    return result;
}

}  // namespace vista
