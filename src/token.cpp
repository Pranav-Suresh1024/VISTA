#include "token.hpp"

#include <sstream>

namespace vista {

std::string_view token_kind_name(TokenKind kind) {
    switch (kind) {
        case TokenKind::EndOfFile: return "END_OF_FILE";
        case TokenKind::Form: return "FORM";
        case TokenKind::Field: return "FIELD";
        case TokenKind::Label: return "LABEL";
        case TokenKind::Required: return "REQUIRED";
        case TokenKind::Show: return "SHOW";
        case TokenKind::When: return "WHEN";
        case TokenKind::Check: return "CHECK";
        case TokenKind::Message: return "MESSAGE";
        case TokenKind::TypeText: return "TYPE_TEXT";
        case TokenKind::TypeInteger: return "TYPE_INTEGER";
        case TokenKind::TypeDecimal: return "TYPE_DECIMAL";
        case TokenKind::TypeBoolean: return "TYPE_BOOLEAN";
        case TokenKind::TypeDate: return "TYPE_DATE";
        case TokenKind::TypeChoice: return "TYPE_CHOICE";
        case TokenKind::TypeFile: return "TYPE_FILE";
        case TokenKind::TrueLiteral: return "TRUE_LITERAL";
        case TokenKind::FalseLiteral: return "FALSE_LITERAL";
        case TokenKind::And: return "AND";
        case TokenKind::Or: return "OR";
        case TokenKind::Not: return "NOT";
        case TokenKind::Identifier: return "IDENTIFIER";
        case TokenKind::StringLiteral: return "STRING_LITERAL";
        case TokenKind::IntegerLiteral: return "INTEGER_LITERAL";
        case TokenKind::DecimalLiteral: return "DECIMAL_LITERAL";
        case TokenKind::Equal: return "EQUAL";
        case TokenKind::NotEqual: return "NOT_EQUAL";
        case TokenKind::Less: return "LESS";
        case TokenKind::LessEqual: return "LESS_EQUAL";
        case TokenKind::Greater: return "GREATER";
        case TokenKind::GreaterEqual: return "GREATER_EQUAL";
        case TokenKind::LeftBrace: return "LEFT_BRACE";
        case TokenKind::RightBrace: return "RIGHT_BRACE";
        case TokenKind::LeftParen: return "LEFT_PAREN";
        case TokenKind::RightParen: return "RIGHT_PAREN";
        case TokenKind::Colon: return "COLON";
        case TokenKind::Comma: return "COMMA";
    }
    return "UNKNOWN";
}

std::string display_lexeme(std::string_view lexeme) {
    std::ostringstream output;
    output << '"';
    for (const char character : lexeme) {
        switch (character) {
            case '\\': output << "\\\\"; break;
            case '"': output << "\\\""; break;
            case '\n': output << "\\n"; break;
            case '\r': output << "\\r"; break;
            case '\t': output << "\\t"; break;
            default: output << character; break;
        }
    }
    output << '"';
    return output.str();
}

}  // namespace vista
