# VISTA

VISTA stands for **Validation and Interface Specification Translation Analyzer**.

VISTA will read a small declarative form specification, validate its structure and rules, and generate a simple standalone HTML form when the specification is valid.

## Current status

Stages 1 and 2 are complete. The project now has a repeatable WSL build and a Flex scanner that reads a `.vista` source file, recognizes its tokens, preserves line and column positions, and reports lexical errors.

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
```

The token table contains each token's source location, classification, and original lexeme. Whitespace and `//` comments are ignored while their positions are still counted.

## Verify the completed stages

```bash
make test
make demo
```

`make test` runs the cumulative Stage 1 and Stage 2 checks. It covers valid tokenization, token positions, illegal characters, unterminated strings, missing input files, and command-line errors. `make demo` shows one valid scan and one lexical failure.

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
