#ifndef VISTA_WORKFLOW_HPP
#define VISTA_WORKFLOW_HPP

#include <string>
#include <vector>

#include "semantic.hpp"

namespace vista {

// No AST pointers: a linked workflow remains usable after all parser arenas die.
struct IrExpression {
    ExpressionKind kind = ExpressionKind::BooleanLiteral;
    ValueType type = ValueType::Boolean;
    std::string value = "true";
    std::vector<IrExpression> operands;
    bool choice_literal = false;
};

struct LinkedConstraint {
    PropertyKind kind;
    std::string value;
    std::string rule_id;
};

struct LinkedField {
    std::string qualified_name;
    FieldType type = FieldType::Text;
    std::vector<std::string> options;
    std::string label, section, help, placeholder;
    std::vector<LinkedConstraint> constraints;
    IrExpression visibility;
    IrExpression requiredness{ExpressionKind::BooleanLiteral, ValueType::Boolean, "false", {}, false};
    std::string visibility_rule_id, requiredness_rule_id;
};

struct LinkedImport {
    std::string alias;
    std::string qualified_name;
    FieldType type;
    std::vector<std::string> options;
};

struct LinkedCheck {
    IrExpression condition;
    std::string message;
    std::string rule_id;
};

struct LinkedModule {
    std::string alias, name, source_sha256, object_sha256;
    std::vector<LinkedImport> imports;
    std::vector<std::string> exports;
    std::vector<LinkedField> fields;
    std::vector<LinkedCheck> checks;
    IrExpression entry;
    std::string entry_rule_id;
};

struct LinkedRoute {
    std::string source, destination;
    IrExpression condition;
    std::string rule_id;
};

struct LinkedWorkflow {
    std::string name, start_alias;
    std::vector<LinkedModule> modules;
    std::vector<LinkedRoute> routes;
    std::string canonical_sha256;
};

}  // namespace vista
#endif
