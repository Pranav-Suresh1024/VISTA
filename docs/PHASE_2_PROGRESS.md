# VISTA Compiler Implementation Progress

Date: 16 September 2026

## Prototype status

VISTA 0.7.0 is a working compiler prototype. It compiles a `.vista` form specification through six implemented stages, validates editable field-value datasets in the CLI, and generates standalone HTML only when every compiler stage succeeds.

```text
.vista source
    -> Flex scanner
    -> Bison parser
    -> C++ AST
    -> symbol table and semantic analysis
    -> dependency and finite witness analysis
    -> standalone HTML, CSS, and restricted JavaScript
```

## Completed modules

| Module | Completed behavior | Demonstration evidence |
|---|---|---|
| Build and CLI | Repeatable C++17 WSL build, help, version, exit codes, output directories | `make clean all`, `./build/vista --help` |
| Lexical analysis | Keywords, identifiers, literals, operators, punctuation, comments, positions, illegal characters, unterminated strings | `tokens.txt`, `LEX001`, `LEX002` |
| Syntax analysis | Yacc-compatible Bison grammar, precedence, AST construction, detailed syntax errors, declaration recovery | `ast.txt`, `SYN001` |
| Symbol table | Declaration-order insertion and lookup with field type, options, label, and source location | `symbols.txt` |
| Semantic analysis | Duplicate fields/properties/options, undefined names, incompatible types, invalid choices, Boolean conditions, empty-form rejection, and type-safe numeric/date/text constraints | `SEM001`-`SEM017` |
| Dependency analysis | Visibility and requirement edges, text table, DOT graph, self-dependencies, DFS cycle detection | `dependencies.txt`, `graph.dot`, `ANL001`, `ANL002` |
| Witness analysis | Exhaustive Boolean/choice assignments and concrete hidden-required witnesses | `ANL003` |
| Analysis boundary | Explicit refusal to overclaim unsupported numeric/date/text witness reasoning | `ANL900` |
| HTML backend | Ten field types, labels, email/phone/textarea controls, numeric and text-length constraints, conditional behavior, check messages, embedded CSS/JavaScript, output escaping | `form.html` |
| CLI data validator | Editable value files, field/type/format/constraint checks, conditional requirements, visibility-aware rules, readable PASS/FAIL reports | `--validate-data`, `samples/` |
| Integrated pipeline | One command produces all seven evidence files; failed compilation preserves progressive results and removes stale HTML | `--emit all`, `tests/test_pipeline.sh` |

## Output bundle

For a valid source, this command:

```bash
./build/vista examples/valid_scholarship.vista --emit all --out-dir out/demo
./build/vista templates/scholarship_application.vista --emit all --out-dir out/scholarship
```

produces:

| File | Purpose |
|---|---|
| `tokens.txt` | Positioned lexical token stream |
| `ast.txt` | Parsed abstract syntax tree |
| `symbols.txt` | Field symbol table |
| `dependencies.txt` | Human-readable conditional dependency edges |
| `graph.dot` | Machine-readable dependency graph |
| `diagnostics.txt` | Clean result or compiler diagnostics |
| `form.html` | Standalone generated form |

## Testing evidence

`make test` performs cumulative end-to-end checks for:

- valid compilation and all expected artifacts;
- every supported token, expression operator, field type, and constraint;
- token positions and comment handling;
- lexical failures and unterminated strings;
- syntax errors and multi-error recovery;
- AST structure and operator precedence;
- symbol insertion and semantic type rules;
- undefined fields, invalid choices, and missing labels;
- dependency edges and DOT output;
- self-dependencies and multi-field cycles;
- Boolean and choice hidden-required witnesses;
- honest unsupported-analysis diagnostics;
- HTML controls, conditional rules, validation messages, and output escaping;
- the complete Scholarship template, including native email/phone/textarea controls, GPA and income bounds, statement length, declaration, and category certificate conditions;
- stale-output cleanup and the guarantee that failed compilations have no `form.html`;
- command-line and missing-file exit codes.
- valid and invalid CLI datasets for all three templates, including conditional requirements, email/phone formats, signed bounds, dates, choices, and eligibility checks.

The C++ source is also tested with AddressSanitizer and UndefinedBehaviorSanitizer. Flex and Bison generation uses strict warnings, and the project compiles with `-Wall -Wextra -Wpedantic -Werror`.

## Review 2 evidence mapping

| Review criterion | Current evidence |
|---|---|
| Implementation progress | Six connected compiler stages and seven generated artifacts |
| Functional correctness | Cumulative compiler and CLI runtime validation tests |
| Compiler-concept application | Flex, Bison, AST, symbol table, semantic checks, graph analysis, and code generation |
| Code quality | Separate headers/sources, strict compiler warnings, deterministic outputs, stable diagnostics |
| Testing | Valid, invalid, boundary, security-escaping, runtime-data, sanitizer, and fail-closed cases |
| Problem solving | Contextual choice literals, cycle reconstruction, finite witness generation, stale-output prevention |
| Innovation | Concrete witness assignment for a required-but-hidden form state |
| Individual understanding | `DEMO_GUIDE.md` contains concise module and algorithm explanations |

## Honest limitations and deferred work

The prototype deliberately does not claim:

- numeric, date, or free-text counterexample solving;
- comprehensive contradiction or unreachable-field proofs;
- Graphviz image rendering as a required dependency;
- full WCAG certification;
- a form-submission server, database, authentication, or persistence;
- arbitrary user JavaScript;
- a drag-and-drop editor or framework-based frontend;
- final performance evaluation and complete final report.

These items do not prevent the current compiler and CLI validation pipeline from being built, tested, and demonstrated end to end.
