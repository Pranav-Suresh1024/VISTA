# VISTA

VISTA stands for **Validation and Interface Specification Translation Analyzer**.

VISTA will read a small declarative form specification, validate its structure and rules, and generate a simple standalone HTML form when the specification is valid.

## Current status

Stages 1 through 6 are complete. VISTA has a repeatable WSL build, positioned Flex scanner, Yacc-compatible Bison parser, C++ AST, field symbol table, semantic analyser, dependency graph, finite witness analysis, standalone HTML generation, and CLI runtime data validation. It includes editable Scholarship, College Admission, and Event Registration starter forms with valid and invalid sample datasets.

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
./build/vista --list-templates
./build/vista templates/scholarship_application.vista --validate-data samples/scholarship-valid.data
./build/vista templates/scholarship_application.vista --validate-data samples/scholarship-invalid.data
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
./build/vista templates/college_admission.vista --emit all --out-dir out/college-admission-demo
./build/vista templates/event_registration.vista --emit all --out-dir out/event-registration-demo
```

The token table contains each token's source location, classification, and original lexeme. Whitespace and `//` comments are ignored while their positions are still counted.

## Verify the completed stages

```bash
make test
make demo
```

`make test` runs all cumulative checks. Stage 5 tests verify dependency edges, DOT output, self-dependencies, cycles, hidden-required witnesses, and honest analysis-limit diagnostics for unsupported conditions.

Witness analysis exhaustively checks finite Boolean and choice assignments. Numeric, date, and text-dependent reasoning is deliberately reported as `ANL900` rather than approximated.

`--validate-data` runs a compiled form against an editable `field = value` data file. It checks required and conditionally required fields, visibility, types, choices, email and phone formats, signed numeric and date bounds, text lengths, declarations, and form-level checks. It exits with `0` for a passing dataset, `1` for validation failure, and `2` for command or file errors.

Example data syntax:

```text
full_name = "Maya Patel"
email_address = maya.patel@example.com
gpa = 8.4
applicant_category = general
declaration = true
```

Blank lines and lines beginning with `#` or `//` are ignored. Quote values that contain spaces. Choice values and Booleans use their declared names directly.

HTML is generated only after every earlier compiler stage succeeds. The language supports text, email, phone, textarea, integer, decimal, Boolean, date, choice, and file controls. Numeric and date fields can declare `minimum` and `maximum`; textual fields can declare `min_length` and `max_length`. The generated file contains accessible labels, native browser constraints, embedded conditional behavior, escaped author-provided text, and visibility-aware check-message validation without external libraries.

The Scholarship form is an editable source file at `templates/scholarship_application.vista`. Run `make demo` for a backend-only walkthrough containing a passing dataset, a failing dataset, and a static safety diagnostic.

`./build/vista --list-templates` lists the three source paths and compile commands. The catalogue at [templates/README.md](templates/README.md) summarizes each form's purpose and fields. The College Admission form demonstrates a conditionally required “other program” field; Event Registration demonstrates workshop-track and in-person-only fields.

`--emit all` is the recommended demonstration command. It writes `tokens.txt`, `ast.txt`, `symbols.txt`, `dependencies.txt`, `graph.dot`, `diagnostics.txt`, and `form.html` into one output directory. Failed compilations write the intermediate results reached and `diagnostics.txt`, but never generate `form.html`.

For the review walkthrough and current implementation evidence, see [DEMO_GUIDE.md](DEMO_GUIDE.md) and [docs/PHASE_2_PROGRESS.md](docs/PHASE_2_PROGRESS.md).

## Project layout

```text
include/       C++ headers
src/           C++ source files
examples/      Valid and invalid VISTA inputs
templates/     Editable Scholarship, College Admission, and Event Registration sources
samples/       Editable valid and invalid CLI value datasets
tests/         Cumulative end-to-end tests
build/         Generated executable, not committed
Makefile       WSL build, test, and demo commands
README.md      Setup and current progress
```

## Exit codes

- `0`: command completed successfully
- `1`: the source contains a compiler error, or supplied field values fail validation
- `2`: invalid command-line usage
