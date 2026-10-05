%{
#include "parser.hpp"
%}

%code requires {
#include <string>
#include <vector>

#include "ast.hpp"

namespace vista {
struct ParserContext;
}
}

%code {
static vista::SourceSpan make_span(const YYLTYPE& first, const YYLTYPE& last) {
    return {
        static_cast<std::size_t>(first.first_line),
        static_cast<std::size_t>(first.first_column),
        static_cast<std::size_t>(last.last_line),
        static_cast<std::size_t>(last.last_column)
    };
}

static bool constraint_kind_for_name(const std::string& name,
                                     vista::PropertyKind& kind) {
    if (name == "minimum") {
        kind = vista::PropertyKind::Minimum;
    } else if (name == "maximum") {
        kind = vista::PropertyKind::Maximum;
    } else if (name == "min_length") {
        kind = vista::PropertyKind::MinLength;
    } else if (name == "max_length") {
        kind = vista::PropertyKind::MaxLength;
    } else {
        return false;
    }
    return true;
}
}

%code provides {
int yylex(YYSTYPE* semantic_value,
          YYLTYPE* location,
          vista::ParserContext& context);
void yyerror(YYLTYPE* location,
             vista::ParserContext& context,
             const char* message);
}

%define parse.error detailed
%define api.pure full
%expect 0
%locations
%parse-param { vista::ParserContext& context }
%lex-param { vista::ParserContext& context }

%union {
    std::string* text;
    vista::Expression* expression;
    vista::FieldProperty* property;
    vista::FieldDecl* field;
    vista::CheckDecl* check;
    vista::Declaration* declaration;
    vista::ImportDecl* import_decl;
    vista::ExportDecl* export_decl;
    vista::WorkflowUse* workflow_use;
    vista::RouteDecl* route;
    vista::TypeSpec* type_spec;
    std::vector<std::string>* text_list;
    std::vector<vista::FieldProperty*>* property_list;
    std::vector<vista::Declaration*>* declaration_list;
}

%token FORM 256 "form"
%token FIELD 257 "field"
%token LABEL 258 "label"
%token REQUIRED 259 "required"
%token SHOW 260 "show"
%token WHEN 261 "when"
%token CHECK 262 "check"
%token MESSAGE 263 "message"
%token TYPE_TEXT 264 "text"
%token TYPE_INTEGER 265 "integer"
%token TYPE_DECIMAL 266 "decimal"
%token TYPE_BOOLEAN 267 "boolean"
%token TYPE_DATE 268 "date"
%token TYPE_CHOICE 269 "choice"
%token TYPE_FILE 270 "file"
%token TRUE_LITERAL 271 "true"
%token FALSE_LITERAL 272 "false"
%token AND 273 "and"
%token OR 274 "or"
%token NOT 275 "not"
%token <text> IDENTIFIER 276 "identifier"
%token <text> STRING_LITERAL 277 "string"
%token <text> INTEGER_LITERAL 278 "integer literal"
%token <text> DECIMAL_LITERAL 279 "decimal literal"
%token EQUAL 280 "=="
%token NOT_EQUAL 281 "!="
%token LESS 282 "<"
%token LESS_EQUAL 283 "<="
%token GREATER 284 ">"
%token GREATER_EQUAL 285 ">="
%token LEFT_BRACE 286 "{"
%token RIGHT_BRACE 287 "}"
%token LEFT_PAREN 288 "("
%token RIGHT_PAREN 289 ")"
%token COLON 290 ":"
%token COMMA 291 ","
%token <text> TYPE_EMAIL 292 "email"
%token <text> TYPE_PHONE 293 "phone"
%token <text> TYPE_TEXTAREA 294 "textarea"
%token MODULE 295 "module"
%token IMPORT 296 "import"
%token EXPORT 297 "export"
%token ENTER 298 "enter"
%token WORKFLOW 299 "workflow"
%token USE 300 "use"
%token AS 301 "as"
%token START 302 "start"
%token ROUTE 303 "route"
%token PRESENT 304 "present"
%token ARROW 305 "->"
%token DOT 306 "."

%type <text> identifier
%type <declaration_list> declaration_list
%type <declaration> declaration
%type <field> field_declaration
%type <check> check_declaration
%type <type_spec> field_type
%type <text_list> option_list
%type <property_list> property_list
%type <property> field_property
%type <expression> expression
%type <expression> optional_when
%type <import_decl> import_declaration
%type <export_decl> export_declaration
%type <workflow_use> workflow_use
%type <route> route_declaration

%destructor { delete $$; } <text> <text_list> <property_list> <declaration_list> <type_spec>

%left OR
%left AND
%precedence NOT
%nonassoc EQUAL NOT_EQUAL LESS LESS_EQUAL GREATER GREATER_EQUAL

%start program

%%

program:
    FORM identifier LEFT_BRACE declaration_list RIGHT_BRACE
    {
        context.result.form = context.result.arena.make<vista::FormAst>(
            *$2,
            std::move(context.form_title),
            std::move(context.form_description),
            std::move(*$4),
            make_span(@1, @5));
        delete $2;
        delete $4;
    }
  | MODULE identifier LEFT_BRACE module_item_list RIGHT_BRACE
    {
        context.result.module = context.result.arena.make<vista::ModuleAst>(
            *$2,
            std::move(context.module_imports),
            std::move(context.module_exports),
            context.module_enter_condition,
            std::move(context.module_declarations),
            make_span(@1, @5));
        delete $2;
    }
  | WORKFLOW identifier LEFT_BRACE workflow_item_list RIGHT_BRACE
    {
        context.result.workflow = context.result.arena.make<vista::WorkflowAst>(
            *$2,
            std::move(context.workflow_uses),
            std::move(context.workflow_start_module),
            std::move(context.workflow_routes),
            make_span(@1, @5));
        delete $2;
    }
;

module_item_list:
    %empty
  | module_item_list module_item
;

module_item:
    import_declaration
    {
        context.module_imports.push_back($1);
    }
  | export_declaration
    {
        context.module_exports.push_back($1);
    }
  | ENTER WHEN expression
    {
        if (context.has_module_enter) {
            context.result.diagnostics.push_back(
                {"SYN002", "module enter condition is declared more than once",
                 static_cast<std::size_t>(@1.first_line),
                 static_cast<std::size_t>(@1.first_column)});
        } else {
            context.module_enter_condition = $3;
            context.has_module_enter = true;
        }
    }
  | declaration
    {
        if ($1 != nullptr) {
            context.module_declarations.push_back($1);
        }
    }
;

import_declaration:
    IMPORT identifier identifier AS identifier
    {
        $$ = context.result.arena.make<vista::ImportDecl>(
            *$2, *$3, *$5, make_span(@1, @5));
        delete $2;
        delete $3;
        delete $5;
    }
;

export_declaration:
    EXPORT identifier
    {
        $$ = context.result.arena.make<vista::ExportDecl>(
            *$2, make_span(@1, @2));
        delete $2;
    }
;

workflow_item_list:
    %empty
  | workflow_item_list workflow_item
;

workflow_item:
    workflow_use
    {
        context.workflow_uses.push_back($1);
    }
  | START identifier
    {
        if (context.has_workflow_start) {
            context.result.diagnostics.push_back(
                {"SYN002", "workflow start module is declared more than once",
                 static_cast<std::size_t>(@1.first_line),
                 static_cast<std::size_t>(@1.first_column)});
        } else {
            context.workflow_start_module = *$2;
            context.has_workflow_start = true;
        }
        delete $2;
    }
  | route_declaration
    {
        context.workflow_routes.push_back($1);
    }
;

workflow_use:
    USE STRING_LITERAL AS identifier
    {
        $$ = context.result.arena.make<vista::WorkflowUse>(
            *$2, *$4, make_span(@1, @4));
        delete $2;
        delete $4;
    }
;

route_declaration:
    ROUTE identifier ARROW identifier optional_when
    {
        $$ = context.result.arena.make<vista::RouteDecl>(
            *$2, *$4, $5, make_span(@1, @5));
        delete $2;
        delete $4;
    }
;

optional_when:
    %empty
    {
        $$ = nullptr;
    }
  | WHEN expression
    {
        $$ = $2;
    }
;

declaration_list:
    %empty
    {
        $$ = new std::vector<vista::Declaration*>();
    }
  | declaration_list declaration
    {
        if ($2 != nullptr) {
            $1->push_back($2);
        }
        $$ = $1;
    }
  | declaration_list IDENTIFIER STRING_LITERAL
    {
        if (*$2 == "title") {
            if (context.has_form_title) {
                context.result.diagnostics.push_back(
                    {"SYN002", "form title is declared more than once",
                     static_cast<std::size_t>(@2.first_line),
                     static_cast<std::size_t>(@2.first_column)});
            } else {
                context.form_title = *$3;
                context.has_form_title = true;
            }
        } else if (*$2 == "description") {
            if (context.has_form_description) {
                context.result.diagnostics.push_back(
                    {"SYN002", "form description is declared more than once",
                     static_cast<std::size_t>(@2.first_line),
                     static_cast<std::size_t>(@2.first_column)});
            } else {
                context.form_description = *$3;
                context.has_form_description = true;
            }
        } else {
            delete $2;
            delete $3;
            yyerror(&@2, context, "unknown form presentation property");
            YYERROR;
        }
        delete $2;
        delete $3;
        $$ = $1;
    }
;

declaration:
    field_declaration
    {
        $$ = context.result.arena.make<vista::Declaration>($1, $1->span);
    }
  | check_declaration
    {
        $$ = context.result.arena.make<vista::Declaration>($1, $1->span);
    }
  | error RIGHT_BRACE
    {
        yyerrok;
        $$ = nullptr;
    }
;

field_declaration:
    FIELD identifier COLON field_type LEFT_BRACE property_list RIGHT_BRACE
    {
        $$ = context.result.arena.make<vista::FieldDecl>(
            *$2, std::move(*$4), std::move(*$6), make_span(@1, @7));
        delete $2;
        delete $4;
        delete $6;
    }
;

field_type:
    TYPE_TEXT
    {
        $$ = new vista::TypeSpec{vista::FieldType::Text, {}};
    }
  | TYPE_INTEGER
    {
        $$ = new vista::TypeSpec{vista::FieldType::Integer, {}};
    }
  | TYPE_DECIMAL
    {
        $$ = new vista::TypeSpec{vista::FieldType::Decimal, {}};
    }
  | TYPE_BOOLEAN
    {
        $$ = new vista::TypeSpec{vista::FieldType::Boolean, {}};
    }
  | TYPE_DATE
    {
        $$ = new vista::TypeSpec{vista::FieldType::Date, {}};
    }
  | TYPE_FILE
    {
        $$ = new vista::TypeSpec{vista::FieldType::File, {}};
    }
  | TYPE_EMAIL
    {
        $$ = new vista::TypeSpec{vista::FieldType::Email, {}};
        delete $1;
    }
  | TYPE_PHONE
    {
        $$ = new vista::TypeSpec{vista::FieldType::Phone, {}};
        delete $1;
    }
  | TYPE_TEXTAREA
    {
        $$ = new vista::TypeSpec{vista::FieldType::Textarea, {}};
        delete $1;
    }
  | TYPE_CHOICE LEFT_BRACE option_list RIGHT_BRACE
    {
        $$ = new vista::TypeSpec{vista::FieldType::Choice, std::move(*$3)};
        delete $3;
    }
;

option_list:
    identifier
    {
        $$ = new std::vector<std::string>();
        $$->push_back(*$1);
        delete $1;
    }
  | option_list COMMA identifier
    {
        $1->push_back(*$3);
        delete $3;
        $$ = $1;
    }
;

property_list:
    %empty
    {
        $$ = new std::vector<vista::FieldProperty*>();
    }
  | property_list field_property
    {
        $1->push_back($2);
        $$ = $1;
    }
;

field_property:
    LABEL STRING_LITERAL
    {
        $$ = context.result.arena.make<vista::FieldProperty>(
            vista::PropertyKind::Label, *$2, nullptr, make_span(@1, @2));
        delete $2;
    }
  | IDENTIFIER STRING_LITERAL
    {
        vista::PropertyKind kind;
        if (*$1 == "section") {
            kind = vista::PropertyKind::Section;
        } else if (*$1 == "help") {
            kind = vista::PropertyKind::Help;
        } else if (*$1 == "placeholder") {
            kind = vista::PropertyKind::Placeholder;
        } else if (*$1 == "minimum") {
            kind = vista::PropertyKind::Minimum;
        } else if (*$1 == "maximum") {
            kind = vista::PropertyKind::Maximum;
        } else {
            delete $1;
            delete $2;
            yyerror(&@1, context, "unknown field presentation property");
            YYERROR;
        }
        $$ = context.result.arena.make<vista::FieldProperty>(
            kind, *$2, nullptr, make_span(@1, @2));
        delete $1;
        delete $2;
    }
  | IDENTIFIER INTEGER_LITERAL
    {
        vista::PropertyKind kind;
        if (!constraint_kind_for_name(*$1, kind)) {
            delete $1;
            delete $2;
            yyerror(&@1, context, "unknown field constraint");
            YYERROR;
        }
        $$ = context.result.arena.make<vista::FieldProperty>(
            kind, *$2, nullptr, make_span(@1, @2));
        delete $1;
        delete $2;
    }
  | IDENTIFIER DECIMAL_LITERAL
    {
        vista::PropertyKind kind;
        if (!constraint_kind_for_name(*$1, kind)) {
            delete $1;
            delete $2;
            yyerror(&@1, context, "unknown field constraint");
            YYERROR;
        }
        $$ = context.result.arena.make<vista::FieldProperty>(
            kind, *$2, nullptr, make_span(@1, @2));
        delete $1;
        delete $2;
    }
  | REQUIRED
    {
        $$ = context.result.arena.make<vista::FieldProperty>(
            vista::PropertyKind::Required, "", nullptr, make_span(@1, @1));
    }
  | REQUIRED WHEN expression
    {
        $$ = context.result.arena.make<vista::FieldProperty>(
            vista::PropertyKind::RequiredWhen, "", $3, make_span(@1, @3));
    }
  | SHOW WHEN expression
    {
        $$ = context.result.arena.make<vista::FieldProperty>(
            vista::PropertyKind::ShowWhen, "", $3, make_span(@1, @3));
    }
;

check_declaration:
    CHECK expression MESSAGE STRING_LITERAL
    {
        $$ = context.result.arena.make<vista::CheckDecl>(
            $2, *$4, make_span(@1, @4));
        delete $4;
    }
;

expression:
    identifier
    {
        $$ = context.result.arena.make<vista::Expression>(
            vista::ExpressionKind::Name, *$1, make_span(@1, @1));
        delete $1;
    }
  | identifier DOT identifier
    {
        $$ = context.result.arena.make<vista::Expression>(
            vista::ExpressionKind::Name, *$1 + "." + *$3, make_span(@1, @3));
        delete $1;
        delete $3;
    }
  | STRING_LITERAL
    {
        $$ = context.result.arena.make<vista::Expression>(
            vista::ExpressionKind::StringLiteral, *$1, make_span(@1, @1));
        delete $1;
    }
  | INTEGER_LITERAL
    {
        $$ = context.result.arena.make<vista::Expression>(
            vista::ExpressionKind::IntegerLiteral, *$1, make_span(@1, @1));
        delete $1;
    }
  | DECIMAL_LITERAL
    {
        $$ = context.result.arena.make<vista::Expression>(
            vista::ExpressionKind::DecimalLiteral, *$1, make_span(@1, @1));
        delete $1;
    }
  | TRUE_LITERAL
    {
        $$ = context.result.arena.make<vista::Expression>(
            vista::ExpressionKind::BooleanLiteral, "true", make_span(@1, @1));
    }
  | FALSE_LITERAL
    {
        $$ = context.result.arena.make<vista::Expression>(
            vista::ExpressionKind::BooleanLiteral, "false", make_span(@1, @1));
    }
  | LEFT_PAREN expression RIGHT_PAREN
    {
        $$ = $2;
        $$->span = make_span(@1, @3);
    }
  | NOT expression
    {
        $$ = context.result.arena.make<vista::Expression>(
            vista::ExpressionKind::Unary, "not", make_span(@1, @2), $2);
    }
  | PRESENT LEFT_PAREN identifier RIGHT_PAREN
    {
        vista::Expression* operand = context.result.arena.make<vista::Expression>(
            vista::ExpressionKind::Name, *$3, make_span(@3, @3));
        $$ = context.result.arena.make<vista::Expression>(
            vista::ExpressionKind::Unary, "present", make_span(@1, @4), operand);
        delete $3;
    }
  | expression OR expression
    {
        $$ = context.result.arena.make<vista::Expression>(
            vista::ExpressionKind::Binary, "or", make_span(@1, @3), $1, $3);
    }
  | expression AND expression
    {
        $$ = context.result.arena.make<vista::Expression>(
            vista::ExpressionKind::Binary, "and", make_span(@1, @3), $1, $3);
    }
  | expression EQUAL expression
    {
        $$ = context.result.arena.make<vista::Expression>(
            vista::ExpressionKind::Binary, "==", make_span(@1, @3), $1, $3);
    }
  | expression NOT_EQUAL expression
    {
        $$ = context.result.arena.make<vista::Expression>(
            vista::ExpressionKind::Binary, "!=", make_span(@1, @3), $1, $3);
    }
  | expression LESS expression
    {
        $$ = context.result.arena.make<vista::Expression>(
            vista::ExpressionKind::Binary, "<", make_span(@1, @3), $1, $3);
    }
  | expression LESS_EQUAL expression
    {
        $$ = context.result.arena.make<vista::Expression>(
            vista::ExpressionKind::Binary, "<=", make_span(@1, @3), $1, $3);
    }
  | expression GREATER expression
    {
        $$ = context.result.arena.make<vista::Expression>(
            vista::ExpressionKind::Binary, ">", make_span(@1, @3), $1, $3);
    }
  | expression GREATER_EQUAL expression
    {
        $$ = context.result.arena.make<vista::Expression>(
            vista::ExpressionKind::Binary, ">=", make_span(@1, @3), $1, $3);
    }
;

identifier:
    IDENTIFIER
    {
        $$ = $1;
    }
  | TYPE_EMAIL
    {
        $$ = $1;
    }
  | TYPE_PHONE
    {
        $$ = $1;
    }
  | TYPE_TEXTAREA
    {
        $$ = $1;
    }
;

%%
