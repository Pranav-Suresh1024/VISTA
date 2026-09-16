# VISTA Backend-Only Demonstration Guide

This walkthrough demonstrates the compiler and runtime validator entirely in Ubuntu/WSL. No browser is required.

## 1. Open the correct terminal and build

Run in Ubuntu/WSL:

```bash
cd "/mnt/c/Users/prana/OneDrive/Documents/ChatGPT/Compiler Design"
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
