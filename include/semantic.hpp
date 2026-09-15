#ifndef VISTA_SEMANTIC_HPP
#define VISTA_SEMANTIC_HPP

#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>

#include "ast.hpp"
#include "diagnostic.hpp"

namespace vista {

enum class ValueType {
    Text,
    Integer,
    Decimal,
    Boolean,
    Date,
    Choice,
    File,
    Error
};

struct Symbol {
    std::string name;
    FieldType type = FieldType::Text;
    std::vector<std::string> options;
    std::string label;
    SourceSpan span;
};

class SymbolTable {
public:
    bool insert(Symbol symbol);
    const Symbol* find(const std::string& name) const;
    const std::vector<Symbol>& entries() const;

private:
    std::vector<Symbol> entries_;
    std::unordered_map<std::string, std::size_t> indexes_;
};

struct ExpressionInfo {
    ValueType type = ValueType::Error;
    std::string referenced_field;
    bool choice_literal = false;
};

struct SemanticResult {
    SymbolTable symbols;
    std::unordered_map<const Expression*, ExpressionInfo> expressions;
    std::vector<Diagnostic> diagnostics;

    bool succeeded() const {
        return diagnostics.empty();
    }
};

SemanticResult analyze_semantics(const FormAst& form);
std::string format_symbol_table(const SymbolTable& symbols);

}  // namespace vista

#endif
