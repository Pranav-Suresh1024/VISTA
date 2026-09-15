# VISTA

VISTA stands for **Validation and Interface Specification Translation Analyzer**.

VISTA will read a small declarative form specification, validate its structure and rules, and generate a simple standalone HTML form when the specification is valid.

## Current status

Stages 1 through 3 are complete. VISTA now has a repeatable WSL build, a positioned Flex scanner, and a Yacc-compatible Bison parser that constructs and prints a C++ abstract syntax tree.

## Requirements

- Ubuntu on WSL 2
- GNU Make
- G++ with C++17 support
- GNU Flex and GNU Bison for the following stages

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
```

The token table contains each token's source location, classification, and original lexeme. Whitespace and `//` comments are ignored while their positions are still counted.

## Verify the completed stages

```bash
make test
make demo
```

`make test` runs all cumulative checks. Scanner tests cover token classes, positions, lexical failures, missing files, and command-line errors. Parser tests cover every field type, declarations, operator precedence, AST output, syntax diagnostics, and the rule that parsing stops after a lexical failure.

## Project layout

```text
include/       C++ headers
src/           C++ source files
examples/      Valid and invalid VISTA inputs
tests/         End-to-end scanner tests
build/         Generated executable, not committed
Makefile       WSL build, test, and demo commands
README.md      Setup and current progress
```

## Exit codes

- `0`: command completed successfully
- `1`: the source contains a lexical error
- `2`: invalid command-line usage
