#ifndef VISTA_TOKEN_HPP
#define VISTA_TOKEN_HPP

#include <cstddef>
#include <string>
#include <string_view>

namespace vista {

enum class TokenKind : int {
    EndOfFile = 0,
    Form = 256,
    Field,
    Label,
    Required,
    Show,
    When,
    Check,
    Message,
    TypeText,
    TypeInteger,
    TypeDecimal,
    TypeBoolean,
    TypeDate,
    TypeChoice,
    TypeFile,
    TrueLiteral,
    FalseLiteral,
    And,
    Or,
    Not,
    Identifier,
    StringLiteral,
    IntegerLiteral,
    DecimalLiteral,
    Equal,
    NotEqual,
    Less,
    LessEqual,
    Greater,
    GreaterEqual,
    LeftBrace,
    RightBrace,
    LeftParen,
    RightParen,
    Colon,
    Comma,
    TypeEmail,
    TypePhone,
    TypeTextarea
};

struct Token {
    TokenKind kind = TokenKind::EndOfFile;
    std::string lexeme;
    std::size_t line = 1;
    std::size_t column = 1;
};

std::string_view token_kind_name(TokenKind kind);
std::string display_lexeme(std::string_view lexeme);

}  // namespace vista

#endif
