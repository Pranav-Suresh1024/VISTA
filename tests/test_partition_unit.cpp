#include "partition.hpp"
#include <algorithm>
#include <cassert>
#include <iostream>
#include <set>

using namespace vista;
IrExpression name(std::string n, ValueType t = ValueType::Integer) {
    return {ExpressionKind::Name, t, std::move(n), {}, false};
}
IrExpression number(std::string n) {
    return {ExpressionKind::IntegerLiteral, ValueType::Integer, std::move(n), {}, false};
}
IrExpression literal(std::string n) {
    return {ExpressionKind::StringLiteral, ValueType::Text, std::move(n), {}, false};
}
IrExpression op(std::string n, IrExpression a, IrExpression b) {
    return {ExpressionKind::Binary, ValueType::Boolean, std::move(n), {std::move(a), std::move(b)}, false};
}
LinkedField field(std::string n, FieldType t) {
    LinkedField f;
    f.qualified_name = std::move(n); f.type = t;
    f.visibility_rule_id = f.qualified_name + ".visible";
    f.requiredness_rule_id = f.qualified_name + ".required";
    return f;
}
LinkedWorkflow workflow(std::vector<LinkedField> fields, IrExpression condition = {}) {
    LinkedWorkflow w;
    w.name = "Test"; w.start_alias = "A";
    LinkedModule m; m.alias = "A"; m.fields = std::move(fields); m.entry_rule_id = "A.entry";
    for (const auto& f : m.fields) m.exports.push_back(f.qualified_name);
    m.checks.push_back({std::move(condition), "test", "A.check"});
    w.modules.push_back(std::move(m)); return w;
}
bool has_code(const std::vector<Diagnostic>& d, const std::string& code) {
    return std::any_of(d.begin(), d.end(), [&](const auto& x) { return x.code == code; });
}
RouteSuite suite(const LinkedWorkflow& w) {
    auto p = derive_partitions(w);
    if (!p.complete()) for (const auto& d : p.diagnostics) std::cerr << d.code << ' ' << d.message << '\n';
    assert(p.complete());
    auto s = enumerate_route_cases(w, p); assert(s.complete()); return s;
}
void covers(const LinkedWorkflow& w, const RouteSuite& s, const std::vector<InputValues>& concrete) {
    std::set<std::string> signatures;
    for (const auto& c : s.cases) if (c.active_module == "A")
        signatures.insert(canonical_observation(evaluate_workflow_step(w, c.values, "A").observation));
    for (const auto& input : concrete) {
        const auto wanted = canonical_observation(evaluate_workflow_step(w, input, "A").observation);
        assert(signatures.count(wanted) && "concrete observation absent from derived suite");
    }
}
int main() {
    // Independent representatives x=y=0 miss the strict (1,2) tight-gap witness.
    auto tight = op("and", op("<", number("0"), name("A.x")),
        op("and", op("<", name("A.x"), name("A.y")), op("<", name("A.y"), number("3"))));
    auto w = workflow({field("A.x", FieldType::Integer), field("A.y", FieldType::Integer)}, tight);
    auto s = suite(w);
    std::vector<InputValues> concrete;
    for (int x = -1; x <= 4; ++x) for (int y = -1; y <= 4; ++y)
        concrete.push_back({{"A.x", std::to_string(x)}, {"A.y", std::to_string(y)}});
    concrete.push_back({}); concrete.push_back({{"A.x", ""}, {"A.y", "invalid"}});
    covers(w, s, concrete);
    bool found = false;
    for (const auto& c : s.cases) if (c.values.count("A.x") && c.values.count("A.y"))
        found = found || (c.values.at("A.x") == "1" && c.values.at("A.y") == "2");
    assert(found);
    // Integer equality blocks and multiple decimal blocks inside an integer gap.
    w = workflow({field("A.x", FieldType::Integer), field("A.y", FieldType::Decimal),
                  field("A.z", FieldType::Decimal)},
        op("and", op("<", name("A.x"), name("A.y", ValueType::Decimal)),
            op("and", op("<", name("A.y", ValueType::Decimal), name("A.z", ValueType::Decimal)),
                op("<", name("A.z", ValueType::Decimal), number("1")))));
    covers(w, suite(w), {{{"A.x", "0"}, {"A.y", "0.25"}, {"A.z", "5e-1"}},
                        {{"A.x", "1"}, {"A.y", "1.0"}, {"A.z", "1e0"}},
                        {{"A.x", "oops"}, {"A.y", "1e100001"}}});
    // A shared valid email at length 5 must survive exclusion of a@b.c.
    auto text = field("A.t", FieldType::Text), email = field("A.e", FieldType::Email);
    for (auto* f : {&text, &email}) {
        f->constraints = {{PropertyKind::MinLength, "5", f->qualified_name + ".minlength"},
                          {PropertyKind::MaxLength, "5", f->qualified_name + ".maxlength"}};
    }
    w = workflow({text, email}, op("and", op("==", name("A.t", ValueType::Text), name("A.e", ValueType::Text)),
                                         op("!=", name("A.e", ValueType::Text), literal("a@b.c"))));
    covers(w, suite(w), {{{"A.t", "b@b.c"}, {"A.e", "b@b.c"}},
                        {{"A.t", "a@b.c"}, {"A.e", "a@b.c"}},
                        {{"A.t", "abcde"}, {"A.e", "abcde"}},
                        {{"A.t", "\xc3\xa9\xc3\xa9x"}, {"A.e", "b@b.c"}}});
    auto phone = field("A.p", FieldType::Phone);
    text.constraints = {{PropertyKind::MinLength, "7", "A.t.minlength"}, {PropertyKind::MaxLength, "7", "A.t.maxlength"}};
    phone.constraints = {{PropertyKind::MinLength, "7", "A.p.minlength"}, {PropertyKind::MaxLength, "7", "A.p.maxlength"}};
    w = workflow({text, phone}, op("==", name("A.t", ValueType::Text), name("A.p", ValueType::Text)));
    covers(w, suite(w), {{{"A.t", "1234567"}, {"A.p", "1234567"}},
                        {{"A.t", "abcdefg"}, {"A.p", "abcdefg"}},
                        {{"A.t", "1234567"}, {"A.p", "+1234567"}}});
    // Context-resolved choice literals cannot become field references.
    auto choice = field("A.c", FieldType::Choice); choice.options = {"other", "yes"};
    auto collision = name("other", ValueType::Choice); collision.choice_literal = true;
    w = workflow({choice, field("A.other", FieldType::Integer)}, op("==", name("A.c", ValueType::Choice), collision));
    covers(w, suite(w), {{{"A.c", "other"}}, {{"A.c", "yes"}}, {{"A.c", "INVALID"}}, {{"A.other", "x"}}});
    // Unicode length is UTF-8 bytes, ordinary nonpredicate fields still validate.
    text.constraints = {{PropertyKind::MaxLength, "1", "A.t.maxlength"}};
    w = workflow({text, field("A.e", FieldType::Email), field("A.i", FieldType::Integer)});
    covers(w, suite(w), {{{"A.t", "\xc3\xa9"}}, {{"A.t", "x"}}, {{"A.e", "bad"}}, {{"A.i", "NaN"}}});
    auto unsupported = workflow({}, op("future-op", number("0"), number("1")));
    auto p = derive_partitions(unsupported); assert(!p.complete() && has_code(p.diagnostics, "CERT001"));
    auto huge = field("A.t", FieldType::Text); huge.constraints = {{PropertyKind::MinLength, "1000000000", "A.t.minlength"}};
    p = derive_partitions(workflow({huge})); assert(!p.complete() && has_code(p.diagnostics, "CERT003"));
    auto long_text = field("A.t", FieldType::Text);
    long_text.constraints = {{PropertyKind::MaxLength, "1000", "A.t.maxlength"}};
    w = workflow({long_text});
    covers(w, suite(w), {{{"A.t", std::string(1000, 'x')}}, {{"A.t", std::string(1001, 'x')}}});
    // Each individual relation is observable, so conjunction-only coverage cannot pass.
    w = workflow({field("A.x", FieldType::Integer), field("A.y", FieldType::Integer)},
                 op("<", name("A.x"), name("A.y")));
    w.modules[0].checks.push_back({op("==", name("A.x"), name("A.y")), "equal", "A.equal"});
    w.modules[0].checks.push_back({op(">=", name("A.x"), number("0")), "nonnegative", "A.nonnegative"});
    w.modules[0].checks.push_back({op("<=", name("A.y"), number("3")), "bounded", "A.bounded"});
    covers(w, suite(w), concrete);
    // Exact negative/exponent decimal bounds and all strict/non-strict regions.
    auto d = field("A.d", FieldType::Decimal);
    d.constraints = {{PropertyKind::Minimum, "-1e-1", "A.d.minimum"},
                     {PropertyKind::Maximum, "0.1", "A.d.maximum"}};
    w = workflow({d}, op(">", name("A.d", ValueType::Decimal), number("0")));
    p = derive_partitions(w);
    assert(p.complete());
    bool exact_negative = false;
    for (const auto& f : p.fields) for (const auto& c : f.classes)
        exact_negative = exact_negative || (c.kind == BehaviorClassKind::Exact && c.representative == "-0.1");
    assert(exact_negative && "exponent-normalized boundary must be an exact class");
    covers(w, suite(w), {{{"A.d", "-1"}}, {{"A.d", "-0.10"}}, {{"A.d", "-0.05"}},
                        {{"A.d", "-0e100000"}}, {{"A.d", "0.05"}}, {{"A.d", "1e-1"}}, {{"A.d", "1"}},
                        {{"A.d", "1e100001"}}});
    auto date = field("A.d", FieldType::Date);
    date.constraints = {{PropertyKind::Minimum, "2024-02-28", "A.d.minimum"},
                        {PropertyKind::Maximum, "2024-03-01", "A.d.maximum"}};
    auto bound = literal("2024-02-29"); bound.type = ValueType::Date;
    w = workflow({date}, op("==", name("A.d", ValueType::Date), bound));
    covers(w, suite(w), {{{"A.d", "2024-02-27"}}, {{"A.d", "2024-02-28"}}, {{"A.d", "2024-02-29"}},
                        {{"A.d", "2024-03-01"}}, {{"A.d", "2024-03-02"}}, {{"A.d", "2023-02-29"}}, {}});
    auto dx = field("A.x", FieldType::Date), dy = field("A.y", FieldType::Date);
    dx.constraints = {{PropertyKind::Minimum, "2024-02-28", "A.x.minimum"}, {PropertyKind::Maximum, "2024-03-02", "A.x.maximum"}};
    dy.constraints = {{PropertyKind::Minimum, "2024-02-28", "A.y.minimum"}, {PropertyKind::Maximum, "2024-03-02", "A.y.maximum"}};
    w = workflow({dx, dy}, op("<", name("A.x", ValueType::Date), name("A.y", ValueType::Date)));
    w.modules[0].checks.push_back({op("==", name("A.x", ValueType::Date), name("A.y", ValueType::Date)), "equal dates", "A.equal"});
    std::vector<InputValues> days;
    for (const std::string x : {"2024-02-27", "2024-02-28", "2024-02-29", "2024-03-01", "2024-03-02", "2024-03-03", "invalid", ""})
        for (const std::string y : {"2024-02-27", "2024-02-28", "2024-02-29", "2024-03-01", "2024-03-02", "2024-03-03", "invalid", ""})
            days.push_back({{"A.x", x}, {"A.y", y}});
    covers(w, suite(w), days);
    w = workflow({field("A.p", FieldType::Phone), field("A.e", FieldType::Email)});
    covers(w, suite(w), {{{"A.p", "123456"}}, {{"A.p", "1234567"}}, {{"A.p", "+(123) 456-7"}},
                        {{"A.p", "123456789012345"}}, {{"A.p", "1234567890123456"}}, {{"A.p", "123456a"}},
                        {{"A.e", "a@b.c"}}, {{"A.e", "a@..b"}}, {{"A.e", "a@b.c\t"}}, {{"A.e", "a@b.\xc2\xa0"}}});
    w = workflow({field("A.f", FieldType::File)},
        {ExpressionKind::Unary, ValueType::Boolean, "present", {name("A.f", ValueType::File)}, false});
    covers(w, suite(w), {{}, {{"A.f", ""}}, {{"A.f", "false"}}, {{"A.f", "0"}}, {{"A.f", "any"}}});
    // The ceiling applies to generation too; no truncated Cartesian suite passes.
    std::vector<LinkedField> many;
    for (int i = 0; i < 16; ++i) many.push_back(field("A.b" + std::to_string(i), FieldType::Boolean));
    w = workflow(many);
    for (const auto& f : many) w.modules[0].checks.push_back({name(f.qualified_name, ValueType::Boolean), "flag", f.qualified_name + ".check"});
    p = derive_partitions(w); assert(p.complete());
    s = enumerate_route_cases(w, p); assert(!s.complete() && has_code(s.diagnostics, "CERT003"));
    const auto again = enumerate_route_cases(w, p);
    assert(format_partitions(p, s) == format_partitions(p, again));
    // Intermediate optional visibility survives traversal to a final empty module.
    auto b = field("A.b", FieldType::Boolean), optional = field("A.optional", FieldType::Text);
    optional.visibility = name("A.b", ValueType::Boolean);
    w = workflow({b, optional}); LinkedModule final; final.alias = "B"; final.entry_rule_id = "B.entry";
    w.modules.push_back(final); w.routes.push_back({"A", "B", {}, "A.route"});
    s = suite(w); std::set<bool> seen; bool whole = false, destination = false;
    for (const auto& c : s.cases) {
        whole = whole || c.active_module.empty();
        if (c.active_module == "B") { destination = true; assert(c.expected_route_prefix == std::vector<std::string>{"A->B"}); }
        if (c.active_module == "A") for (const auto& f : evaluate_workflow_step(w, c.values, "A").observation.fields)
            if (f.field == "A.optional") seen.insert(f.visible);
    }
    assert(seen.size() == 2 && whole && destination);
    w.routes.push_back({"B", "A", name("A.b", ValueType::Boolean), "B.route"});
    final.imports.push_back({"B.b", "A.b", FieldType::Boolean, {}}); w.modules[1] = final;
    p = derive_partitions(w); assert(p.complete());
    s = enumerate_route_cases(w, p); assert(!s.complete() && has_code(s.diagnostics, "CERT002"));
    std::cout << "Partition completeness adversaries passed.\n";
}
