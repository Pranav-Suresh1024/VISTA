# VISTA

VISTA stands for **Validation and Interface Specification Translation Analyzer**.

VISTA will read a small declarative form specification, validate its structure and rules, and generate a simple standalone HTML form when the specification is valid.

## Current status

Stages 1 through 6 are complete. VISTA now has a repeatable WSL build, positioned Flex scanner, Yacc-compatible Bison parser, C++ AST, field symbol table, semantic analyser, dependency graph, finite witness analysis, and a standalone HTML backend.

## Requirements

- Ubuntu on WSL 2
- GNU Make
- G++ with C++17 support
- GNU Flex and GNU Bison

The required compiler tools are already available in the current Ubuntu WSL environment.

## Build

Open Ubuntu WSL, change to this repository, and run:

```bash
make clean
make
```

The executable is created at `build/vista`.

## Run

```bash
./build/vista --version
./build/vista --help
./build/vista examples/valid_scholarship.vista --emit tokens
./build/vista examples/valid_scholarship.vista --emit ast
./build/vista examples/valid_scholarship.vista --emit symbols
./build/vista examples/valid_scholarship.vista --emit dependencies
./build/vista examples/valid_scholarship.vista --emit graph
./build/vista examples/type_mismatch.vista --emit diagnostics
./build/vista examples/hidden_required.vista --emit diagnostics
./build/vista examples/valid_scholarship.vista --emit html --out-dir out/scholarship
./build/vista examples/valid_scholarship.vista --emit all --out-dir out/demo
./build/vista templates/scholarship_application.vista --emit all --out-dir out/scholarship-demo
```

The token table contains each token's source location, classification, and original lexeme. Whitespace and `//` comments are ignored while their positions are still counted.

## Verify the completed stages

```bash
make test
make demo
```

`make test` runs all cumulative checks. Stage 5 tests verify dependency edges, DOT output, self-dependencies, cycles, hidden-required witnesses, and honest analysis-limit diagnostics for unsupported conditions.

Witness analysis exhaustively checks finite Boolean and choice assignments. Numeric, date, and text-dependent reasoning is deliberately reported as `ANL900` rather than approximated.

HTML is generated only after every earlier compiler stage succeeds. The language supports text, email, phone, textarea, integer, decimal, Boolean, date, choice, and file controls. Numeric fields can declare `minimum` and `maximum`; textareas can declare `min_length` and `max_length`. The generated file contains accessible labels, native browser constraints, embedded conditional behavior, escaped author-provided text, and check-message validation without external libraries.

The fuller Phase 2 Scholarship form is an editable source file at `templates/scholarship_application.vista`. Compile it with the command above or run `make demo`; the generated page validates locally in the browser and does not submit or store applications.

`--emit all` is the recommended demonstration command. It writes `tokens.txt`, `ast.txt`, `symbols.txt`, `dependencies.txt`, `graph.dot`, `diagnostics.txt`, and `form.html` into one output directory. Failed compilations write the intermediate results reached and `diagnostics.txt`, but never generate `form.html`.

For the review walkthrough and current implementation evidence, see [DEMO_GUIDE.md](DEMO_GUIDE.md) and [docs/PHASE_2_PROGRESS.md](docs/PHASE_2_PROGRESS.md).

## Project layout

```text
include/       C++ headers
src/           C++ source files
examples/      Valid and invalid VISTA inputs
templates/     The Phase 2 Scholarship application source
tests/         Cumulative end-to-end tests
build/         Generated executable, not committed
Makefile       WSL build, test, and demo commands
README.md      Setup and current progress
```

## Exit codes

- `0`: command completed successfully
- `1`: the source contains a lexical, syntax, semantic, or analysis error
- `2`: invalid command-line usage
