# VISTA

VISTA stands for **Validation and Interface Specification Translation Analyzer**.

VISTA will read a small declarative form specification, validate its structure and rules, and generate a simple standalone HTML form when the specification is valid.

## Current status

Stages 1 through 4 are complete. VISTA now has a repeatable WSL build, positioned Flex scanner, Yacc-compatible Bison parser, C++ AST, field symbol table, and two-pass semantic analyser.

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
./build/vista examples/type_mismatch.vista --emit diagnostics
```

The token table contains each token's source location, classification, and original lexeme. Whitespace and `//` comments are ignored while their positions are still counted.

## Verify the completed stages

```bash
make test
make demo
```

`make test` runs all cumulative checks. The semantic tests cover symbol insertion, labels, choice options, duplicate declarations, undefined references, type compatibility, contextual choice literals, and Boolean conditions.

## Project layout

```text
include/       C++ headers
src/           C++ source files
examples/      Valid and invalid VISTA inputs
tests/         Cumulative end-to-end tests
build/         Generated executable, not committed
Makefile       WSL build, test, and demo commands
README.md      Setup and current progress
```

## Exit codes

- `0`: command completed successfully
- `1`: the source contains a lexical, syntax, or semantic error
- `2`: invalid command-line usage
