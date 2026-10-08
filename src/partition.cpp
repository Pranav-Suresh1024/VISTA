#include "partition.hpp"
#include <algorithm>
#include <cstdlib>
#include <functional>
#include <iomanip>
#include <numeric>
#include <queue>
#include <set>
#include <sstream>

namespace vista {
namespace {
// ponytail: 100000-state safety ceiling; replace with symbolic composition if real workflows exceed it.
constexpr std::size_t ceiling = 100000;
struct Limit {};
struct Unsupported { std::string rule; };
struct Budget {
    std::size_t used = 0, bytes = 0;
    void state(std::size_t n = 1) { if (n > ceiling - used) throw Limit{}; used += n; }
    void storage(std::size_t n) { if (n > ceiling * 64 - bytes) throw Limit{}; bytes += n; }
    void retain(const InputValues& input) {
        state(input.size());
        for (const auto& value : input) storage(value.first.size() + value.second.size());
    }
    void retain(const std::vector<std::string>& prefix) {
        state(prefix.size());
        for (const auto& route : prefix) storage(route.size());
    }
};
bool numeric(FieldType t) { return t == FieldType::Integer || t == FieldType::Decimal; }
bool textual(FieldType t) { return t == FieldType::Text || t == FieldType::Textarea || t == FieldType::Email || t == FieldType::Phone; }
bool comparison(const IrExpression& e) {
    return e.kind == ExpressionKind::Binary && e.value != "and" && e.value != "or";
}
void references(const IrExpression& e, std::set<std::string>& names) {
    if (e.kind == ExpressionKind::Name && !e.choice_literal) names.insert(e.value);
    for (const auto& o : e.operands) references(o, names);
}
void inspect(const IrExpression& e, const std::string& rule, Budget& budget,
             std::vector<const IrExpression*>& atoms, std::size_t depth = 0) {
    budget.state();
    if (depth > 256) throw Limit{};
    if (e.kind == ExpressionKind::IntegerLiteral || e.kind == ExpressionKind::DecimalLiteral) {
        CanonicalDecimal value;
        if (!parse_decimal(e.value, value)) throw Unsupported{rule};
    }
    if (e.kind == ExpressionKind::Unary) {
        if ((e.value != "not" && e.value != "present") || e.operands.size() != 1) throw Unsupported{rule};
        if (e.value == "present") atoms.push_back(&e);
    } else if (e.kind == ExpressionKind::Binary) {
        static const std::set<std::string> ops{"and", "or", "==", "!=", "<", "<=", ">", ">="};
        if (!ops.count(e.value) || e.operands.size() != 2) throw Unsupported{rule};
        if (comparison(e)) atoms.push_back(&e);
    } else if (!e.operands.empty()) throw Unsupported{rule};
    if (e.kind == ExpressionKind::Name && !e.choice_literal && e.type == ValueType::Boolean) atoms.push_back(&e);
    budget.storage(e.value.size());
    for (const auto& o : e.operands) inspect(o, rule, budget, atoms, depth + 1);
}

// Signed decimal integer arithmetic on bounded strings, no floating point.
std::string strip(std::string s) {
    const auto first = s.find_first_not_of('0');
    return first == std::string::npos ? "0" : s.substr(first);
}
int magnitude_order(const std::string& a, const std::string& b) {
    return a.size() != b.size() ? (a.size() < b.size() ? -1 : 1) : (a < b ? -1 : a > b ? 1 : 0);
}
std::string add_magnitude(std::string a, std::string b) {
    std::string out; int carry = 0;
    while (!a.empty() || !b.empty() || carry) {
        int v = carry;
        if (!a.empty()) { v += a.back() - '0'; a.pop_back(); }
        if (!b.empty()) { v += b.back() - '0'; b.pop_back(); }
        out += static_cast<char>('0' + v % 10); carry = v / 10;
    }
    std::reverse(out.begin(), out.end()); return out;
}
std::string subtract_magnitude(std::string a, std::string b) {
    int borrow = 0;
    for (std::size_t i = 0; i < a.size(); ++i) {
        int v = a[a.size() - i - 1] - '0' - borrow;
        if (i < b.size()) v -= b[b.size() - i - 1] - '0';
        borrow = v < 0; if (borrow) v += 10;
        a[a.size() - i - 1] = static_cast<char>('0' + v);
    }
    return strip(a);
}
std::string add_signed(std::string a, std::string b) {
    bool an = a[0] == '-', bn = b[0] == '-';
    if (an) a.erase(0, 1);
    if (bn) b.erase(0, 1);
    std::string out;
    bool negative = an;
    if (an == bn) out = add_magnitude(a, b);
    else if (magnitude_order(a, b) >= 0) out = subtract_magnitude(a, b);
    else { out = subtract_magnitude(b, a); negative = bn; }
    return negative && out != "0" ? "-" + out : out;
}
CanonicalDecimal decimal(const std::string& s) {
    CanonicalDecimal d; if (!parse_decimal(s, d)) throw Limit{}; return d;
}
std::string plain(const CanonicalDecimal& d) {
    if (!d.sign) return "0";
    if (d.digits.size() + static_cast<std::size_t>(std::abs(d.scale)) + 3 > 4096) throw Limit{};
    std::string s = d.digits;
    if (d.scale <= 0) s.append(static_cast<std::size_t>(-d.scale), '0');
    else if (static_cast<std::size_t>(d.scale) >= s.size())
        s = "0." + std::string(static_cast<std::size_t>(d.scale) - s.size(), '0') + s;
    else s.insert(s.size() - static_cast<std::size_t>(d.scale), ".");
    return d.sign < 0 ? "-" + s : s;
}
std::string floor_number(const CanonicalDecimal& d) {
    auto p = plain(d);
    auto dot = p.find('.');
    if (dot == std::string::npos) return p;
    const bool fraction = p.find_first_not_of('0', dot + 1) != std::string::npos;
    p.resize(dot);
    if (p == "-0") p = "0";
    return d.sign < 0 && fraction ? add_signed(p, "-1") : p;
}
std::string midpoint(const std::string& a, const std::string& b) {
    auto x = decimal(a), y = decimal(b);
    int scale = std::max({0, x.scale, y.scale});
    auto aligned = [&](const CanonicalDecimal& d) {
        if (!d.sign) return std::string("0");
        const auto n = static_cast<std::size_t>(scale - d.scale);
        if (d.digits.size() + n > 4096) throw Limit{};
        return (d.sign < 0 ? "-" : "") + d.digits + std::string(n, '0');
    };
    auto sum = add_signed(aligned(x), aligned(y));
    bool negative = sum[0] == '-'; if (negative) sum.erase(0, 1);
    std::string half; int remainder = 0;
    for (char c : sum) { int v = remainder * 10 + c - '0'; half += static_cast<char>('0' + v / 2); remainder = v % 2; }
    if (remainder) { half += '5'; ++scale; }
    CanonicalDecimal d{negative ? -1 : 1, strip(half), scale};
    return plain(decimal(plain(d)));
}
struct NumberLess {
    bool operator()(const std::string& a, const std::string& b) const { return compare_decimal(decimal(a), decimal(b)) < 0; }
};
std::vector<std::string> number_pool(const std::set<std::string>& constants, std::size_t n, bool decimals, Budget& budget) {
    std::set<std::string, NumberLess> boundaries{"0"}, pool;
    for (const auto& c : constants) boundaries.insert(plain(decimal(c)));
    // First N integers in each open boundary interval; all when fewer exist.
    std::vector<std::string> b(boundaries.begin(), boundaries.end());
    for (std::size_t i = 0; i <= b.size(); ++i) {
        std::string v = i ? add_signed(floor_number(decimal(b[i - 1])), "1") :
            add_signed(floor_number(decimal(b.front())), "-" + std::to_string(n));
        for (std::size_t rank = 0; rank < n; ++rank) {
            budget.state();
            if (i < b.size() && !NumberLess{}(v, b[i])) break;
            pool.insert(v); v = add_signed(v, "1");
        }
    }
    for (const auto& c : b) {
        CanonicalDecimal integer;
        if (decimals || parse_integer(c, integer)) pool.insert(c);
    }
    if (decimals) {
        // Include endpoints outside both tails so every ordered decimal block fits.
        pool.insert(add_signed(floor_number(decimal(b.front())), "-" + std::to_string(n + 1)));
        pool.insert(add_signed(floor_number(decimal(b.back())), std::to_string(n + 1)));
        std::vector<std::string> cuts(pool.begin(), pool.end());
        for (std::size_t i = 1; i < cuts.size(); ++i) {
            std::string lower = cuts[i - 1];
            for (std::size_t rank = 0; rank < n; ++rank) {
                budget.state();
                lower = midpoint(lower, cuts[i]);
                if (!NumberLess{}(cuts[i - 1], lower) || !NumberLess{}(lower, cuts[i])) throw Limit{};
                pool.insert(lower);
            }
        }
    }
    return {pool.begin(), pool.end()};
}

int ordinal(const std::string& date) {
    if (!valid_iso_date(date)) throw Unsupported{"invalid date boundary"};
    const int y = std::stoi(date.substr(0, 4)), m = std::stoi(date.substr(5, 2)), d = std::stoi(date.substr(8));
    int days = 365 * (y - 1) + (y - 1) / 4 - (y - 1) / 100 + (y - 1) / 400;
    for (int month = 1; month < m; ++month) {
        static const int lengths[]{31,28,31,30,31,30,31,31,30,31,30,31};
        days += lengths[month - 1] + (month == 2 && (y % 400 == 0 || (y % 4 == 0 && y % 100 != 0)));
    }
    return days + d;
}
std::string day_text(int day) {
    int low = 1, high = 9999;
    while (low < high) {
        const int middle = (low + high + 1) / 2;
        std::ostringstream s; s << std::setw(4) << std::setfill('0') << middle << "-01-01";
        if (ordinal(s.str()) <= day) low = middle; else high = middle - 1;
    }
    for (int month = 1; month <= 12; ++month) {
        std::ostringstream start; start << std::setw(4) << std::setfill('0') << low << '-' << std::setw(2) << month << "-01";
        int delta = day - ordinal(start.str()) + 1;
        if (delta >= 1 && delta <= 31) {
            std::ostringstream s; s << start.str().substr(0, 8) << std::setw(2) << std::setfill('0') << delta;
            if (valid_iso_date(s.str())) return s.str();
        }
    }
    throw Limit{};
}
std::vector<std::string> date_pool(const std::set<std::string>& constants, std::size_t n, Budget& budget) {
    std::set<int> cuts{0, ordinal("9999-12-31") + 1}, pool;
    for (const auto& c : constants) { int d = ordinal(c); cuts.insert(d); pool.insert(d); }
    std::vector<int> b(cuts.begin(), cuts.end());
    for (std::size_t i = 1; i < b.size(); ++i)
        for (int d = b[i - 1] + 1; d < b[i] && d <= b[i - 1] + static_cast<int>(n); ++d) { budget.state(); pool.insert(d); }
    std::vector<std::string> values;
    for (int d : pool) values.push_back(day_text(d));
    return values;
}

// Product DFA for the *existing* email/phone byte languages. State counts are
// saturated at N + excluded literals, then deterministic unranking yields N
// distinct nonliteral witnesses per length/format cell (or the entire cell).
constexpr int email_states = 7, phone_states = 18, dfa_states = email_states * phone_states;
int transition(int state, unsigned char c) {
    int e = state / phone_states, p = state % phone_states;
    bool whitespace = c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\f' || c == '\v';
    if (e != 6) {
        if (whitespace) e = 6;
        else if (c == '@') e = e == 1 ? 2 : 6;
        else if (e == 0) e = 1;
        else if (e == 2) e = 3;
        else if (e == 3 && c == '.') e = 4;
        else if (e == 4) e = 5;
    }
    if (p != 17) {
        if (c >= '0' && c <= '9') p = p == 15 ? 17 : p + 1;
        else if (c != '+' && c != '(' && c != ')' && c != ' ' && c != '-') p = 17;
    }
    return e * phone_states + p;
}
int format_cell(int state) {
    return (state / phone_states == 5 ? 1 : 0) | (state % phone_states >= 7 && state % phone_states <= 15 ? 2 : 0);
}
std::vector<unsigned char> alphabet() {
    std::vector<unsigned char> a;
    const std::string preferred = "a0123456789bcdefghijklmnopqrstuvwxyz@.+()- ";
    for (unsigned char c : preferred) if (std::find(a.begin(), a.end(), c) == a.end()) a.push_back(c);
    for (int c = 0; c < 256; ++c) if (std::find(a.begin(), a.end(), c) == a.end()) a.push_back(static_cast<unsigned char>(c));
    return a;
}
std::vector<std::string> string_pool(const std::set<std::string>& literals,
                                   const std::set<std::size_t>& lengths, std::size_t n,
                                   bool formats, Budget& budget) {
    std::set<std::size_t> cuts{0, 1};
    if (formats) cuts.insert({5, 7, 16});
    for (auto l : lengths) {
        if (l > ceiling) throw Limit{};
        cuts.insert(l); cuts.insert(l + 1);
    }
    // Every literal is a singleton, excluded from the generic DFA cells.
    for (const auto& literal : literals) budget.storage(literal.size());
    std::vector<std::string> result(literals.begin(), literals.end());
    std::vector<std::size_t> starts(cuts.begin(), cuts.end());
    const auto chars = alphabet();
    const std::size_t cap = n + literals.size();
    for (std::size_t region = 0; region < starts.size(); ++region) {
        const auto end = region + 1 < starts.size() ? starts[region + 1] : ceiling;
        for (int cell = 0; cell < (formats ? 3 : 1); ++cell) {
            std::size_t remaining = n;
            for (std::size_t len = starts[region]; len < end && remaining; ++len) {
                // Email needs >=5 bytes, phone >=7; intersection is empty (@).
                if ((cell == 1 && len < 5) || (cell == 2 && len < 7)) continue;
                const int states = formats ? dfa_states : 1;
                budget.state((len + 1) * states);
                std::vector<std::size_t> dp((len + 1) * states);
                auto count_at = [&](std::size_t k, int state) -> std::size_t& { return dp[k * states + state]; };
                for (int state = 0; state < states; ++state)
                    count_at(0, state) = !formats || format_cell(state) == cell;
                for (std::size_t k = 1; k <= len; ++k) for (int state = 0; state < states; ++state) {
                    std::size_t count = 0;
                    for (auto c : chars) {
                        auto value = count_at(k - 1, formats ? transition(state, c) : 0);
                        count = std::min(cap, count + value);
                    }
                    count_at(k, state) = count;
                }
                for (std::size_t rank = 0; rank < count_at(len, 0) && remaining; ++rank) {
                    std::size_t index = rank; int state = 0; std::string value;
                    for (std::size_t k = len; k; --k) for (auto c : chars) {
                        const int next = formats ? transition(state, c) : 0;
                        const auto count = count_at(k - 1, next);
                        if (index >= count) { index -= count; continue; }
                        value += static_cast<char>(c); state = next; break;
                    }
                    if (!literals.count(value)) { budget.storage(value.size()); result.push_back(value); --remaining; }
                }
                // Nonformat unconstrained cells saturate by length ceil(log256 N).
                // Format cells at/above their minimum likewise grow without gaps.
            }
        }
    }
    return result;
}
std::string signature(const std::vector<const LinkedField*>& fields,
                      const std::vector<const IrExpression*>& atoms, const InputValues& input) {
    RuntimeValues values; std::ostringstream out;
    for (const auto* f : fields) {
        auto it = input.find(f->qualified_name); std::vector<Diagnostic> diagnostics;
        auto value = convert_value(*f, it == input.end() ? nullptr : &it->second, diagnostics);
        values[f->qualified_name] = value;
        // Keep missing and supplied-empty probes distinct even when VISTA agrees.
        out << (it != input.end()) << ':' << value.valid << ':';
        if (f->type == FieldType::Choice) out << value.present;
        const bool can_require = f->requiredness.kind != ExpressionKind::BooleanLiteral || f->requiredness.value != "false";
        if (can_require) out << (!value.present || (f->type == FieldType::Boolean && !value.boolean));
        for (const auto& constraint : f->constraints) {
            auto scalar = *f; scalar.constraints = {constraint}; diagnostics.clear();
            validate_constraints(scalar, value, diagnostics);
            // A present, valid value activates rule IDs even when bounds pass.
            out << ':' << (value.present && value.valid);
            for (const auto& d : diagnostics) out << d.code << ',';
        }
        out << ';';
    }
    for (const auto* atom : atoms) {
        bool value = false; const bool valid = evaluate_expression(*atom, values, value);
        out << valid << (valid && value) << ';';
    }
    return out.str();
}
void constants_in(const IrExpression& e, std::set<std::string>& numbers,
                  std::set<std::string>& dates, std::set<std::string>& strings) {
    if (e.kind == ExpressionKind::IntegerLiteral || e.kind == ExpressionKind::DecimalLiteral) numbers.insert(e.value);
    else if (e.kind == ExpressionKind::StringLiteral) {
        if (e.type == ValueType::Date) dates.insert(e.value); else strings.insert(e.value);
    } else if (e.choice_literal) strings.insert(e.value);
    for (const auto& o : e.operands) constants_in(o, numbers, dates, strings);
}
void diagnostic(std::vector<Diagnostic>& d, std::string code, std::string message) {
    d.push_back({std::move(code), std::move(message), 0, 0});
}
}  // namespace

PartitionResult derive_partitions(const LinkedWorkflow& workflow) {
    PartitionResult result; Budget budget;
    try {
        std::vector<const LinkedField*> fields;
        std::vector<const IrExpression*> atoms;
        for (const auto& m : workflow.modules) {
            budget.storage(m.alias.size());
            inspect(m.entry, m.entry_rule_id, budget, atoms);
            for (const auto& f : m.fields) {
                fields.push_back(&f);
                inspect(f.visibility, f.visibility_rule_id, budget, atoms);
                inspect(f.requiredness, f.requiredness_rule_id, budget, atoms);
            }
            for (const auto& check : m.checks) inspect(check.condition, check.rule_id, budget, atoms);
        }
        for (const auto& r : workflow.routes) inspect(r.condition, r.rule_id, budget, atoms);
        std::sort(fields.begin(), fields.end(), [](auto a, auto b) { return a->qualified_name < b->qualified_name; });
        std::map<std::string, std::size_t> index;
        for (std::size_t i = 0; i < fields.size(); ++i) {
            budget.state();
            budget.storage(fields[i]->qualified_name.size());
            if (!index.emplace(fields[i]->qualified_name, i).second) throw Unsupported{"duplicate field"};
        }
        std::vector<std::size_t> parent(fields.size()); std::iota(parent.begin(), parent.end(), 0);
        std::function<std::size_t(std::size_t)> root = [&](std::size_t i) { return parent[i] == i ? i : parent[i] = root(parent[i]); };
        for (const auto* atom : atoms) {
            std::set<std::string> names; references(*atom, names);
            std::size_t first = fields.size();
            for (const auto& name : names) {
                if (!index.count(name)) throw Unsupported{"unresolved field '" + name + "'"};
                auto i = index.at(name);
                if (first == fields.size()) first = i; else parent[root(i)] = root(first);
            }
        }
        std::map<std::size_t, std::vector<const LinkedField*>> groups;
        for (std::size_t i = 0; i < fields.size(); ++i) groups[root(i)].push_back(fields[i]);
        for (const auto& group : groups) {
            const auto& members = group.second;
            const std::size_t n = members.size();
            if (n > 256) throw Limit{};
            PartitionComponent component;
            std::vector<const IrExpression*> local_atoms;
            std::set<std::string> numbers, dates, strings;
            std::set<std::size_t> lengths;
            bool has_decimal = false, has_number = false, has_date = false, has_text = false, formats = false;
            for (auto* f : members) {
                component.fields.push_back(f->qualified_name);
                has_number = has_number || numeric(f->type);
                has_decimal = has_decimal || f->type == FieldType::Decimal;
                has_date = has_date || f->type == FieldType::Date;
                has_text = has_text || textual(f->type);
                formats = formats || f->type == FieldType::Email || f->type == FieldType::Phone;
                if (f->type == FieldType::Choice) for (const auto& option : f->options) {
                    budget.state(); budget.storage(option.size()); strings.insert(option);
                }
                for (const auto& c : f->constraints) {
                    budget.state();
                    if (c.kind == PropertyKind::Minimum || c.kind == PropertyKind::Maximum) {
                        if (numeric(f->type)) numbers.insert(c.value);
                        else if (f->type == FieldType::Date) dates.insert(c.value);
                        else throw Unsupported{c.rule_id};
                    } else if (c.kind == PropertyKind::MinLength || c.kind == PropertyKind::MaxLength) {
                        CanonicalDecimal d;
                        if (!parse_integer(c.value, d) || d.sign < 0) throw Unsupported{c.rule_id};
                        if (compare_decimal(d, decimal(std::to_string(ceiling))) > 0) throw Limit{};
                        lengths.insert(static_cast<std::size_t>(std::stoul(plain(d))));
                    } else throw Unsupported{c.rule_id};
                }
            }
            for (auto* atom : atoms) {
                std::set<std::string> names; references(*atom, names);
                if (!names.empty() && std::find(component.fields.begin(), component.fields.end(), *names.begin()) != component.fields.end()) {
                    local_atoms.push_back(atom); constants_in(*atom, numbers, dates, strings);
                }
            }
            if (has_number) {
                std::set<std::string> canonical;
                for (const auto& value : numbers) canonical.insert(plain(decimal(value)));
                numbers = std::move(canonical);
            }
            auto numeric_values = has_number ? number_pool(numbers, n, has_decimal, budget) : std::vector<std::string>{};
            auto date_values = has_date ? date_pool(dates, n, budget) : std::vector<std::string>{};
            auto string_values = has_text ? string_pool(strings, lengths, n, formats, budget) : std::vector<std::string>{};
            std::vector<FieldPartition> partitions;
            for (auto* f : members) {
                FieldPartition partition{f->qualified_name, {}};
                std::set<std::string> supplied;
                auto add = [&](const std::string& value, BehaviorClassKind kind, const std::string& description) {
                    if (kind != BehaviorClassKind::Missing && !supplied.insert(value).second) return;
                    budget.state(); budget.storage(value.size());
                    const auto id = f->qualified_name + ".class." + std::to_string(partition.classes.size());
                    partition.classes.push_back({f->qualified_name, id, value, description, kind});
                };
                add("", BehaviorClassKind::Missing, "missing");
                add("", BehaviorClassKind::Exact, "supplied empty");
                if (numeric(f->type)) {
                    for (const auto& v : numeric_values) {
                        CanonicalDecimal d;
                        if (f->type == FieldType::Integer && !parse_integer(v, d)) continue;
                        auto kind = BehaviorClassKind::Between;
                        if (numbers.count(v)) kind = BehaviorClassKind::Exact;
                        else if (!numbers.empty()) {
                            std::set<std::string, NumberLess> ordered(numbers.begin(), numbers.end());
                            if (NumberLess{}(v, *ordered.begin())) kind = BehaviorClassKind::Below;
                            else if (NumberLess{}(*ordered.rbegin(), v)) kind = BehaviorClassKind::Above;
                        }
                        add(v, kind, "coordinated numeric rank");
                    }
                    add("invalid", BehaviorClassKind::Invalid, "conversion invalid");
                } else if (f->type == FieldType::Date) {
                    for (const auto& v : date_values) {
                        auto kind = dates.count(v) ? BehaviorClassKind::Exact : BehaviorClassKind::Between;
                        if (!dates.empty() && v < *dates.begin()) kind = BehaviorClassKind::Below;
                        if (!dates.empty() && v > *dates.rbegin()) kind = BehaviorClassKind::Above;
                        add(v, kind, "coordinated date rank");
                    }
                    add("invalid", BehaviorClassKind::Invalid, "conversion invalid");
                } else if (f->type == FieldType::Boolean) {
                    add("false", BehaviorClassKind::Exact, "false"); add("true", BehaviorClassKind::Exact, "true");
                    add("invalid", BehaviorClassKind::Invalid, "conversion invalid");
                } else if (f->type == FieldType::Choice) {
                    for (const auto& option : f->options) add(option, BehaviorClassKind::Exact, "choice option");
                    // Invalid choice equality is never evaluated as a valid scalar.
                    std::string invalid = "INVALID";
                    while (supplied.count(invalid)) { budget.state(); invalid += '_'; }
                    add(invalid, BehaviorClassKind::Invalid, "invalid choice");
                } else if (f->type == FieldType::File) add("file", BehaviorClassKind::Present, "nonempty file");
                else if (textual(f->type)) {
                    for (const auto& v : string_values) {
                        std::vector<Diagnostic> ignored;
                        const auto converted = convert_value(*f, &v, ignored);
                        add(v, !converted.valid ? BehaviorClassKind::Invalid : strings.count(v) ? BehaviorClassKind::Exact : BehaviorClassKind::Other,
                            "byte length / literal / format cell");
                    }
                } else throw Unsupported{f->qualified_name};
                partitions.push_back(std::move(partition));
            }
            std::set<std::string> signatures;
            InputValues input;
            std::function<void(std::size_t)> enumerate = [&](std::size_t i) {
                budget.state();
                if (i == partitions.size()) {
                    auto key = signature(members, local_atoms, input);
                    if (!signatures.count(key)) {
                        budget.storage(key.size()); signatures.insert(std::move(key));
                        budget.retain(input); component.assignments.push_back(input);
                    }
                    return;
                }
                for (const auto& c : partitions[i].classes) {
                    if (c.kind == BehaviorClassKind::Missing) input.erase(c.field); else input[c.field] = c.representative;
                    enumerate(i + 1);
                }
                input.erase(partitions[i].field);
            };
            enumerate(0);
            result.fields.insert(result.fields.end(), partitions.begin(), partitions.end());
            result.components.push_back(std::move(component));
        }
        std::sort(result.fields.begin(), result.fields.end(), [](const auto& a, const auto& b) { return a.field < b.field; });
    } catch (const Limit&) {
        diagnostic(result.diagnostics, "CERT003", "UNSUPPORTED: partition generation exhausted the 100000-state/storage budget");
    } catch (const Unsupported& e) {
        diagnostic(result.diagnostics, "CERT001", "UNSUPPORTED: no exact partition rule for '" + e.rule + "'");
    }
    result.generation_states = budget.used;
    result.generation_bytes = budget.bytes;
    return result;
}

RouteSuite enumerate_route_cases(const LinkedWorkflow& workflow, const PartitionResult& partitions) {
    RouteSuite suite;
    suite.diagnostics = partitions.diagnostics;
    if (!partitions.complete()) return suite;
    Budget budget; budget.used = partitions.generation_states; budget.bytes = partitions.generation_bytes;
    struct State { InputValues values; std::string active, input_class; std::vector<std::string> prefix; };
    std::queue<State> queue;
    try {
        if (partitions.components.size() > 256) throw Limit{};
        InputValues input;
        std::function<void(std::size_t, std::string)> combine = [&](std::size_t i, std::string key) {
            budget.state();
            if (i == partitions.components.size()) {
                budget.retain(input);
                queue.push({input, workflow.start_alias, std::move(key), {}});
                return;
            }
            const auto& c = partitions.components[i];
            for (std::size_t j = 0; j < c.assignments.size(); ++j) {
                for (const auto& name : c.fields) input.erase(name);
                input.insert(c.assignments[j].begin(), c.assignments[j].end());
                combine(i + 1, key + std::to_string(j) + ',');
            }
            for (const auto& name : c.fields) input.erase(name);
        };
        combine(0, "");
        std::set<std::pair<std::string, std::string>> visited;
        std::set<std::string> whole;
        bool cycle = false;
        while (!queue.empty()) {
            State state = std::move(queue.front()); queue.pop();
            if (!visited.emplace(state.active, state.input_class).second) { cycle = true; continue; }
            budget.state();
            budget.storage(state.active.size() + state.input_class.size());
            auto step = evaluate_workflow_step(workflow, state.values, state.active);
            auto id = "case." + std::to_string(suite.cases.size());
            budget.retain(state.values);
            budget.retain(state.prefix);
            suite.cases.push_back({id, state.values, state.prefix, state.active});
            if (whole.insert(state.input_class).second) {
                budget.state();
                auto evaluation = evaluate_workflow(workflow, state.values);
                budget.retain(state.values);
                budget.retain(evaluation.observation.transition_trace);
                suite.cases.push_back({"case." + std::to_string(suite.cases.size()), state.values,
                                       evaluation.observation.transition_trace, ""});
            }
            if (!step.observation.selected_route.empty()) {
                const auto& route = step.observation.selected_route;
                state.prefix.push_back(route);
                state.active = route.substr(route.find("->") + 2);
                budget.retain(state.values);
                queue.push(std::move(state));
            }
        }
        if (cycle) diagnostic(suite.diagnostics, "CERT002", "UNSUPPORTED: reachable conditional route cycle has no completing path");
    } catch (const Limit&) {
        diagnostic(suite.diagnostics, "CERT003", "UNSUPPORTED: reachable-case generation exhausted the 100000-state budget");
    }
    // Sort both modes by prefix length so the first divergent case is shortest.
    std::stable_sort(suite.cases.begin(), suite.cases.end(), [](const auto& a, const auto& b) {
        return a.expected_route_prefix.size() < b.expected_route_prefix.size();
    });
    for (std::size_t i = 0; i < suite.cases.size(); ++i) suite.cases[i].id = "case." + std::to_string(i);
    return suite;
}

std::string format_partitions(const PartitionResult& partitions, const RouteSuite& suite) {
    static const char* kinds[]{"MISSING", "PRESENT", "EXACT", "BELOW", "BETWEEN", "ABOVE", "OTHER", "INVALID"};
    std::ostringstream out;
    auto quote = [&](const std::string& s) {
        out << '"';
        for (unsigned char c : s) {
            if (c == '\\' || c == '"') out << '\\' << c;
            else if (c < 32 || c >= 127) out << "\\x" << std::hex << std::setw(2) << std::setfill('0') << static_cast<unsigned>(c) << std::dec;
            else out << c;
        }
        out << '"';
    };
    out << "VISTA-PARTITIONS 1\nStatus: " << (suite.complete() ? "COMPLETE" : "UNSUPPORTED") << '\n';
    for (const auto& p : partitions.fields) {
        out << "field " << p.field << '\n';
        for (const auto& c : p.classes) {
            out << "  " << c.id << ' ' << kinds[static_cast<unsigned>(c.kind)] << ' ';
            quote(c.representative); out << ' ' << c.description << '\n';
        }
    }
    out << "Generation states: " << partitions.generation_states
        << "\nGeneration bytes: " << partitions.generation_bytes << '\n';
    for (std::size_t i = 0; i < partitions.components.size(); ++i) {
        const auto& c = partitions.components[i];
        out << "component " << i << " assignments=" << c.assignments.size() << " fields=";
        for (const auto& f : c.fields) out << f << ' ';
        out << '\n';
    }
    out << "Cases: " << suite.cases.size() << '\n';
    for (const auto& c : suite.cases) {
        out << c.id << " mode=" << (c.active_module.empty() ? "whole" : c.active_module) << " prefix=";
        for (const auto& r : c.expected_route_prefix) out << r << ' ';
        out << '\n';
        for (const auto& v : c.values) { out << "  " << v.first << '='; quote(v.second); out << '\n'; }
    }
    for (const auto& d : suite.diagnostics) out << d.code << ' ' << d.message << '\n';
    return out.str();
}
}  // namespace vista
