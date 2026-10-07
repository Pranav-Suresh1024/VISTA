# VISTA

VISTA stands for **Validation and Interface Specification Translation Analyzer**.

VISTA compiles declarative form specifications, validates editable datasets, and generates standalone HTML. VISTA-Link additionally compiles typed modules, links workflows, generates JavaScript, and records executed C++/JavaScript conformance evidence.

## Current status

Version **0.8.0** includes the existing form pipeline and VISTA-Link module objects, typed linking, exact bounded partitions, workflow evaluation/code generation, certificate emission and an independent checker. The separate three-module teacher example is `examples/vista_link/scholarship-demo.workflow.vista`; the original two-module scholarship regression fixture is retained. All-type coverage runs in three complete groups derived from the original source; the combined fixture deliberately verifies deterministic resource rejection under unchanged budgets. See [decisions.md](decisions.md) for the decision and review boundaries.

## Requirements

- Ubuntu on WSL 2
- GNU Make
- G++ with C++17 support
- GNU Flex and GNU Bison
- Node.js 18+ on PATH only for certificate execution, generated-target tests and the VISTA-Link demo

Builds, ordinary tests, CLI data validation, workflow generation and the independent checker do not require Node. Local verification uses Node v22.23.3; the stated minimum is not a separate Node 18 test result. The retained WSL runtime wrapper is described in [the workflow guide](docs/VISTA_LINK_DEMO.md#local-wsl-runtime).

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
make test-vista-link-targets
make vista-link-demo
```

`make test` runs the ordinary cumulative checks without Node. `make test-vista-link-targets` explicitly runs generated-JavaScript comparisons, certificate/checker tests, six real mutations with C++/JS witness replay, and `tests/test_pipeline.sh --vista-link`. Both targets are required for full verification. The all-type numeric/date, textual/format and choice/file groups each retain Boolean routing and Finish; they enumerate 896, 1216 and 280 cases respectively. The combined fixture's 124416 input assignments exceed the unchanged shared ceiling and must produce CERT003/UNSUPPORTED.

The generated-target script executes 3613 canonical C++/JavaScript comparison cases, plus default Unicode comparisons where the bytes are well-formed and embedded-handler progression checks. This includes grouped all-type coverage and one concrete combined-source case; it is not complete coverage of the over-budget combined Cartesian product.

Witness analysis uses exact source-derived classes for supported Boolean, choice, numeric, date, text and format predicates. Unsupported rules or exhausted budgets produce incomplete analysis (`ANL900` for the form analysis adapter), never an approximate success.

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

## VISTA-Link commands and evidence

```bash
./build/vista examples/vista_link/eligibility.vista --emit module --out-dir out/eligibility
./build/vista examples/vista_link/scholarship-demo.workflow.vista --emit linked --out-dir out/link
./build/vista examples/vista_link/scholarship-demo.workflow.vista --emit partitions
./build/vista examples/vista_link/scholarship-demo.workflow.vista --emit workflow --out-dir out/workflow
./build/vista examples/vista_link/scholarship-demo.workflow.vista --emit certificate --out-dir out/cert
./build/vista-cert-check out/cert/workflow.vcert --root .
./build/vista examples/vista_link/scholarship-demo.workflow.vista --emit certificate --out-dir out/supplied --javascript out/workflow/workflow.js
```

Module mode writes `module.vobj`; linked mode writes `workflow.ir`; workflow mode writes `workflow.ir`, `workflow.js`, `workflow.html`. Certificate mode adds `workflow.vcert` and local `probe.js`, `probe.out`, `probe.err` execution evidence. PASS means agreement on the complete recorded finite source-derived suite for declared fields, including reachable intermediate states. It does not prove arbitrary JavaScript equivalence. FAIL can contain reduced inputs, state/prefix and both differing observations; incomplete cycles also fail. Unsupported rules or the 100000-state/shared resource ceiling produce UNSUPPORTED, never PASS.

The independent checker checks unsigned recorded coverage consistency and, with `--root .`, named-file hashes. It does not recompile, regenerate partitions, execute targets or authenticate execution. Version-1 source/object paths are raw absolute paths, preserving symlink/`..` meaning; `--root` enables file checking and does not relocate or confine them. Generated siblings are relative to the certificate. Supplied JavaScript runs as trusted local Node code with no sandbox or subprocess timeout. Full commands, budgets, safe cleanup and rollback guidance are in [docs/VISTA_LINK_DEMO.md](docs/VISTA_LINK_DEMO.md).

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
- `1`: compiler/validation failure, certificate FAIL/UNSUPPORTED, or rejected/malformed/changed certificate evidence
- `2`: invalid usage, operational file/runtime/probe failure (module-load diagnostics can instead return 1)
