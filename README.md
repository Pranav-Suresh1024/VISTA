# VISTA

VISTA stands for **Validation and Interface Specification Translation Analyzer**.

VISTA will read a small declarative form specification, validate its structure and rules, and generate a simple standalone HTML form when the specification is valid.

## Current status

Stage 1 provides the repository foundation, repeatable WSL build, version command, help command, and automated CLI checks. Source-file compilation begins in Stage 2 with the Flex scanner.

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
```

## Verify Stage 1

```bash
make test
make demo
```

`make test` verifies the version, help output, and command-line error exit code. `make demo` prints the working Stage 1 interface.

## Project layout

```text
include/       C++ headers
src/           C++ source files
build/         Generated executable, not committed
Makefile       WSL build, test, and demo commands
README.md      Setup and current progress
```

## Exit codes

- `0`: command completed successfully
- `2`: invalid command-line usage
