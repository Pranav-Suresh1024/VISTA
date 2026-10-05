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

struct ImportDecl final : AstNode {
    ImportDecl(std::string import_provider,
               std::string import_field,
               std::string import_alias,
               SourceSpan source_span)
        : AstNode(source_span),
          provider(std::move(import_provider)),
          field(std::move(import_field)),
          alias(std::move(import_alias)) {}

    std::string provider;
    std::string field;
    std::string alias;
};

struct ExportDecl final : AstNode {
    ExportDecl(std::string export_field, SourceSpan source_span)
        : AstNode(source_span), field(std::move(export_field)) {}

    std::string field;
};

struct ModuleAst final : AstNode {
    ModuleAst(std::string module_name,
              std::vector<ImportDecl*> module_imports,
              std::vector<ExportDecl*> module_exports,
              Expression* module_enter_condition,
              std::vector<Declaration*> module_declarations,
              SourceSpan source_span)
        : AstNode(source_span),
          name(std::move(module_name)),
          imports(std::move(module_imports)),
          exports(std::move(module_exports)),
          enter_condition(module_enter_condition),
          declarations(std::move(module_declarations)) {}

    std::string name;
    std::vector<ImportDecl*> imports;
    std::vector<ExportDecl*> exports;
    Expression* enter_condition = nullptr;
    std::vector<Declaration*> declarations;
};

struct WorkflowUse final : AstNode {
    WorkflowUse(std::string workflow_source_path,
                std::string workflow_alias,
                SourceSpan source_span)
        : AstNode(source_span),
          source_path(std::move(workflow_source_path)),
          alias(std::move(workflow_alias)) {}

    std::string source_path;
    std::string alias;
};

struct RouteDecl final : AstNode {
    RouteDecl(std::string route_source,
              std::string route_destination,
              Expression* route_condition,
              SourceSpan source_span)
        : AstNode(source_span),
          source(std::move(route_source)),
          destination(std::move(route_destination)),
          condition(route_condition) {}

    std::string source;
    std::string destination;
    Expression* condition = nullptr;
};

struct WorkflowAst final : AstNode {
    WorkflowAst(std::string workflow_name,
                std::vector<WorkflowUse*> workflow_uses,
                std::string workflow_start_module,
                std::vector<RouteDecl*> workflow_routes,
                SourceSpan source_span)
        : AstNode(source_span),
          name(std::move(workflow_name)),
          uses(std::move(workflow_uses)),
          start_module(std::move(workflow_start_module)),
          routes(std::move(workflow_routes)) {}

    std::string name;
    std::vector<WorkflowUse*> uses;
    std::string start_module;
    std::vector<RouteDecl*> routes;
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
std::string format_ast(const ModuleAst& module);
std::string format_ast(const WorkflowAst& workflow);

}  // namespace vista

#endif
