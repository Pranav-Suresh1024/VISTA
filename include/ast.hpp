#ifndef VISTA_AST_HPP
#define VISTA_AST_HPP

#include <cstddef>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace vista {

struct SourceSpan {
    std::size_t first_line = 1;
    std::size_t first_column = 1;
    std::size_t last_line = 1;
    std::size_t last_column = 1;
};

struct AstNode {
    explicit AstNode(SourceSpan source_span) : span(source_span) {}
    virtual ~AstNode() = default;

    SourceSpan span;
};

enum class FieldType {
    Text,
    Integer,
    Decimal,
    Boolean,
    Date,
    Choice,
    File,
    Email,
    Phone,
    Textarea
};

struct TypeSpec {
    FieldType type = FieldType::Text;
    std::vector<std::string> options;
};

enum class ExpressionKind {
    Name,
    StringLiteral,
    IntegerLiteral,
    DecimalLiteral,
    BooleanLiteral,
    Unary,
    Binary
};

struct Expression final : AstNode {
    Expression(ExpressionKind expression_kind,
               std::string expression_value,
               SourceSpan source_span,
               Expression* left_expression = nullptr,
               Expression* right_expression = nullptr)
        : AstNode(source_span),
          kind(expression_kind),
          value(std::move(expression_value)),
          left(left_expression),
          right(right_expression) {}

    ExpressionKind kind;
    std::string value;
    Expression* left = nullptr;
    Expression* right = nullptr;
};

enum class PropertyKind {
    Label,
    Section,
    Help,
    Placeholder,
    Required,
    RequiredWhen,
    ShowWhen,
    Minimum,
    Maximum,
    MinLength,
    MaxLength
};

struct FieldProperty final : AstNode {
    FieldProperty(PropertyKind property_kind,
                  std::string property_text,
                  Expression* property_condition,
                  SourceSpan source_span)
        : AstNode(source_span),
          kind(property_kind),
          text(std::move(property_text)),
          condition(property_condition) {}

    PropertyKind kind;
    std::string text;
    Expression* condition = nullptr;
};

struct FieldDecl final : AstNode {
    FieldDecl(std::string field_name,
              TypeSpec type_spec,
              std::vector<FieldProperty*> field_properties,
              SourceSpan source_span)
        : AstNode(source_span),
          name(std::move(field_name)),
          type(type_spec.type),
          options(std::move(type_spec.options)),
          properties(std::move(field_properties)) {}

    std::string name;
    FieldType type;
    std::vector<std::string> options;
    std::vector<FieldProperty*> properties;
};

struct CheckDecl final : AstNode {
    CheckDecl(Expression* check_condition,
              std::string check_message,
              SourceSpan source_span)
        : AstNode(source_span),
          condition(check_condition),
          message(std::move(check_message)) {}

    Expression* condition = nullptr;
    std::string message;
};

enum class DeclarationKind {
    Field,
    Check
};

struct Declaration final : AstNode {
    Declaration(FieldDecl* field_declaration, SourceSpan source_span)
        : AstNode(source_span), kind(DeclarationKind::Field), field(field_declaration) {}

    Declaration(CheckDecl* check_declaration, SourceSpan source_span)
        : AstNode(source_span), kind(DeclarationKind::Check), check(check_declaration) {}

    DeclarationKind kind;
    FieldDecl* field = nullptr;
    CheckDecl* check = nullptr;
};

struct FormAst final : AstNode {
    FormAst(std::string form_name,
            std::string form_title,
            std::string form_description,
            std::vector<Declaration*> form_declarations,
            SourceSpan source_span)
        : AstNode(source_span),
          name(std::move(form_name)),
          title(form_title.empty() ? name : std::move(form_title)),
          description(std::move(form_description)),
          declarations(std::move(form_declarations)) {}

    std::string name;
    std::string title;
    std::string description;
    std::vector<Declaration*> declarations;
};

class AstArena {
public:
    template <typename Node, typename... Arguments>
    Node* make(Arguments&&... arguments) {
        auto node = std::make_unique<Node>(std::forward<Arguments>(arguments)...);
        Node* pointer = node.get();
        nodes_.push_back(std::move(node));
        return pointer;
    }

private:
    std::vector<std::unique_ptr<AstNode>> nodes_;
};

std::string field_type_name(FieldType type);
std::string format_ast(const FormAst& form);

}  // namespace vista

#endif
