# VISTA Backend-Only Demonstration Guide

This walkthrough demonstrates the compiler and runtime validator entirely in Ubuntu/WSL. No browser is required.

## 1. Open the correct terminal and build

Run in Ubuntu/WSL:

```bash
cd "/mnt/c/Users/prana/.codex/worktrees/vista-link/Compiler Design"
make all
make test
```

The final line should be:

```text
Complete VISTA compiler and validation pipeline tests passed.
```

## 2. Introduce the compiler

Say:

> VISTA compiles a declarative form specification through lexical, syntax, semantic, dependency, and safety analysis. It can then validate teacher-editable input datasets directly in the CLI.

Show the available templates:

```bash
./build/vista --version
./build/vista --list-templates
```

## 3. Show the source input

```bash
sed -n '1,220p' templates/scholarship_application.vista
```

Point out field types, numeric and date bounds, length limits, conditional fields, and `check` rules.

## 4. Show compiler stages

```bash
./build/vista templates/scholarship_application.vista --emit tokens | head -25
./build/vista templates/scholarship_application.vista --emit ast | head -60
./build/vista templates/scholarship_application.vista --emit symbols
./build/vista templates/scholarship_application.vista --emit dependencies
./build/vista templates/scholarship_application.vista --emit diagnostics
```

The final command should print `No diagnostics.`

## 5. Validate a passing dataset

First show the editable values:

```bash
cat samples/scholarship-valid.data
```

Then validate them:

```bash
./build/vista templates/scholarship_application.vista \
  --validate-data samples/scholarship-valid.data
```

Expected result:

```text
Result          : PASS

No validation errors.
```

## 6. Validate deliberately incorrect values

```bash
cat samples/scholarship-invalid.data
./build/vista templates/scholarship_application.vista \
  --validate-data samples/scholarship-invalid.data
```

Exit status `1` is expected. The report demonstrates invalid email and phone values, name and statement lengths, date and GPA bounds, missing conditionally required documents, and a false declaration.

For values that have valid types but fail eligibility rules:

```bash
./build/vista templates/scholarship_application.vista \
  --validate-data samples/scholarship-ineligible.data
```

This reports the GPA and household-income eligibility messages.

## 7. Let the teacher change input

Preserve the supplied example by making a demo copy:

```bash
mkdir -p out/cli-demo
cp samples/scholarship-valid.data out/cli-demo/teacher-values.data
nano out/cli-demo/teacher-values.data
```

Suggested changes:

- Change `gpa = 8.4` to `gpa = 5.5`.
- Change `applicant_category = general` to `applicant_category = reserved` without adding `category_certificate`.
- Change the email to `invalid-email`.

Save with Ctrl+O, Enter, then exit with Ctrl+X. Validate the changed file:

```bash
./build/vista templates/scholarship_application.vista \
  --validate-data out/cli-demo/teacher-values.data
```

Restore individual values in the same file and rerun the command until it passes.

## 8. Show other working domains

```bash
./build/vista templates/college_admission.vista \
  --validate-data samples/college-valid.data
./build/vista templates/event_registration.vista \
  --validate-data samples/event-valid.data
```

Both should report `PASS`. Their `*-invalid.data` counterparts demonstrate program, qualification, workshop, attendance-mode, guest, and capacity rules.

## 9. Demonstrate compile-time safety analysis

```bash
./build/vista examples/hidden_required.vista --emit diagnostics
```

Expected diagnostic:

```text
ANL003: field 'certificate' can be required while hidden
```

Explain that this is different from invalid applicant data: VISTA has detected an unsafe form definition before runtime.

## One-command fallback

If review time is short:

```bash
make demo
```

It lists the templates, compiles the Scholarship definition, validates one passing and one failing dataset, and shows the hidden-required safety diagnostic.

## VISTA-Link: three typed modules and conformance evidence

Use this implementation worktree for the new workflow demonstration. From its WSL root, with Node.js 18+ on PATH:

```bash
make
make test
make test-vista-link-targets
make vista-link-demo
```

The explicit gate runs all types in three complete groups taken from the actual module source, retaining Boolean routing and Finish. The full Cartesian fixture is also tested for deterministic CERT003 rejection under unchanged budgets. Ordinary `make test` remains Node-free. On this machine, the ignored verified Node v22.23.3 wrapper can supply PATH without installation:

```bash
bash .superpowers/sdd/2026-10-05-vista-link/runtime/with-node.sh make vista-link-demo
```

Show `examples/vista_link/scholarship-demo.workflow.vista`: Applicant selects a category, Eligibility imports that typed choice and requires age 18–120, and only reserved applicants proceed to Certificate. Existing Applicant/Certificate sources and the two-module regression fixture are unchanged. The target prints a valid certificate `VERIFIED` result, passing general/reserved CLI paths, then an expected `FAIL` from changing the generated age minimum from 18 to 19. It prints the reduced witness and both observations and exits successfully only if that mutant was detected.

Edit `out/vista-link-demo/general.data` or `reserved.data`, then run:

```bash
./build/vista examples/vista_link/scholarship-demo.workflow.vista \
  --validate-data out/vista-link-demo/reserved.data
```

Age 17 or a reserved applicant without a filename fails. Files mean nonempty filename strings; no file contents are read or uploaded. Rerunning `make vista-link-demo` restores its sample data.

Explain PASS as complete recorded finite source-derived case agreement for declared fields, not arbitrary JavaScript equivalence or authenticity. The checker independently checks recorded consistency and named hashes, not execution or source semantics. `--root .` does not relocate raw absolute input paths. Supplied JS is trusted executable code with no sandbox/deadline. See [the full workflow guide](docs/VISTA_LINK_DEMO.md) for exact artifacts, exits, unsupported/budget behavior, wrapper use and the protected rollback branch.
