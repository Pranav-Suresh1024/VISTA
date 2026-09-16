%option noyywrap nounput noinput nodefault

%{
#include <cstdio>
#include <string>
#include <utility>

#include "scanner.hpp"

namespace {

vista::ScanResult* current_result = nullptr;
std::size_t current_line = 1;
std::size_t current_column = 1;
std::size_t token_line = 1;
std::size_t token_column = 1;

void advance_position(const char* text, int length) {
    for (int index = 0; index < length; ++index) {
        if (text[index] == '\n') {
            ++current_line;
            current_column = 1;
        } else {
            ++current_column;
        }
    }
}

void emit_token(vista::TokenKind kind, const char* text, int length) {
    current_result->tokens.push_back(
        {kind, std::string(text, static_cast<std::size_t>(length)), token_line, token_column});
}

void emit_error(std::string code, std::string message) {
    current_result->diagnostics.push_back(
        {std::move(code), std::move(message), token_line, token_column});
}

}  // namespace

#define YY_USER_ACTION                                      \
    do {                                                    \
        token_line = current_line;                          \
        token_column = current_column;                      \
        advance_position(yytext, static_cast<int>(yyleng)); \
    } while (false);

#define TOKEN(kind)                                                    \
    do {                                                               \
        emit_token(vista::TokenKind::kind, yytext, yyleng);            \
        return static_cast<int>(vista::TokenKind::kind);               \
    } while (false)
%}

IDENTIFIER      [A-Za-z_][A-Za-z0-9_]*
INTEGER         [0-9]+
DECIMAL         [0-9]+\.[0-9]+
STRING          \"([^\"\\\n]|\\[\"\\nrt])*\"
BAD_STRING      \"([^\"\\\n]|\\.)*

%%

[ \t\r]+       { /* Ignore horizontal whitespace. */ }
\n             { /* Position tracking is handled by YY_USER_ACTION. */ }
"//"[^\n]*     { /* Ignore comments while preserving positions. */ }

"form"         { TOKEN(Form); }
"field"        { TOKEN(Field); }
"label"        { TOKEN(Label); }
"required"     { TOKEN(Required); }
"show"         { TOKEN(Show); }
"when"         { TOKEN(When); }
"check"        { TOKEN(Check); }
"message"      { TOKEN(Message); }

"text"         { TOKEN(TypeText); }
"integer"      { TOKEN(TypeInteger); }
"decimal"      { TOKEN(TypeDecimal); }
"boolean"      { TOKEN(TypeBoolean); }
"date"         { TOKEN(TypeDate); }
"choice"       { TOKEN(TypeChoice); }
"file"         { TOKEN(TypeFile); }
"email"        { TOKEN(TypeEmail); }
"phone"        { TOKEN(TypePhone); }
"textarea"     { TOKEN(TypeTextarea); }

"true"         { TOKEN(TrueLiteral); }
"false"        { TOKEN(FalseLiteral); }
"and"          { TOKEN(And); }
"or"           { TOKEN(Or); }
"not"          { TOKEN(Not); }

"=="           { TOKEN(Equal); }
"!="           { TOKEN(NotEqual); }
"<="           { TOKEN(LessEqual); }
">="           { TOKEN(GreaterEqual); }
"<"            { TOKEN(Less); }
">"            { TOKEN(Greater); }

"{"            { TOKEN(LeftBrace); }
"}"            { TOKEN(RightBrace); }
"("            { TOKEN(LeftParen); }
")"            { TOKEN(RightParen); }
":"            { TOKEN(Colon); }
","            { TOKEN(Comma); }

{DECIMAL}       { TOKEN(DecimalLiteral); }
{INTEGER}       { TOKEN(IntegerLiteral); }
{STRING}        { TOKEN(StringLiteral); }
{BAD_STRING}    {
                    emit_error("LEX002", "unterminated string literal");
                    return -1;
                }
{IDENTIFIER}    { TOKEN(Identifier); }

.               {
                    emit_error("LEX001",
                               "unexpected character " + vista::display_lexeme(yytext));
                    return -1;
                }

%%

namespace vista {

ScanResult scan_file(const std::string& source_path) {
    ScanResult result;
    FILE* input = std::fopen(source_path.c_str(), "rb");
    if (input == nullptr) {
        return result;
    }

    result.input_opened = true;
    current_result = &result;
    current_line = 1;
    current_column = 1;
    token_line = 1;
    token_column = 1;

    yyrestart(input);
    while (yylex() != 0) {
        // Tokens and diagnostics are captured by the Flex actions.
    }
    std::fclose(input);

    result.tokens.push_back({TokenKind::EndOfFile, "", current_line, current_column});
    current_result = nullptr;
    return result;
}

}  // namespace vista
