# VISTA Public-Service Template Catalogue Demonstration Guide

This guide presents the current working prototype in about five minutes.

## Before the review

Open Ubuntu WSL and run:

```bash
cd "/mnt/c/Users/prana/OneDrive/Documents/ChatGPT/Compiler Design"
make all
make test
make demo
```

The final test line must be:

```text
Complete Phase 2 pipeline and failure-path tests passed.
```

The three complete evidence bundles will be in `out/phase3-templates/`.

## 1. Introduce the project

Say:

> VISTA is a small source-to-source compiler. It reads a declarative form specification, performs lexical, syntax, semantic, and conditional-rule analysis, and generates a standalone HTML form only when the specification is safe.

List the starter sources:

```bash
./build/vista --list-templates
```

The list points to ordinary editable `.vista` source files. The concise catalogue is in `templates/README.md`.

Show one source:

```bash
sed -n '1,180p' templates/college_admission.vista
```

Explain that the input is a `.vista` source file, not an interactive prompt.

## 2. Show incremental development and tests

```bash
git log --oneline --reverse
make test
```

Point out that each compiler module was added as a focused commit and every test suite passes cumulatively.

## 3. Compile all three starter forms

```bash
./build/vista templates/scholarship_application.vista \
  --emit all \
  --out-dir out/phase3-templates/scholarship
./build/vista templates/college_admission.vista \
  --emit all \
  --out-dir out/phase3-templates/college-admission
./build/vista templates/event_registration.vista \
  --emit all \
  --out-dir out/phase3-templates/event-registration
```

Each command should print its generated output directory. Each folder contains:

```text
ast.txt
dependencies.txt
diagnostics.txt
form.html
graph.dot
symbols.txt
tokens.txt
```

## 4. Explain the template-specific rules

- Scholarship: choosing `reserved` displays a required category certificate.
- College Admission: choosing `other` reveals a required program-name field.
- Event Registration: choosing `workshop` reveals a required track; dietary requirements appear only for in-person attendance.

These are demonstrations of VISTA conditions and validation, not real application or reservation services.

## 5. Explain intermediate compiler results

```bash
sed -n '1,18p' out/phase3-templates/college-admission/tokens.txt
sed -n '1,40p' out/phase3-templates/college-admission/ast.txt
cat out/phase3-templates/college-admission/symbols.txt
cat out/phase3-templates/college-admission/dependencies.txt
cat out/phase3-templates/college-admission/diagnostics.txt
```

Use these explanations:

- `tokens.txt`: Flex identifies tokens and records line and column positions.
- `ast.txt`: Bison validates the grammar and constructs the abstract syntax tree.
- `symbols.txt`: The semantic pass stores each field's name, type, choices, label, and location.
- `dependencies.txt`: Conditional references become directed dependency edges.
- `diagnostics.txt`: A valid form reports `No diagnostics.`
- `graph.dot`: The same dependency information is available in Graphviz DOT format.

## 6. Demonstrate the generated forms

Open each `form.html` from `out/phase3-templates/` in a browser.

1. Scholarship: toggle `general` and `reserved` to see the conditional certificate.
2. College Admission: choose `other` to show and require the program-name field.
3. Event Registration: choose `workshop`, then toggle `in_person` and `online` to inspect the conditional fields.
4. Try a malformed email, out-of-range score/count, or missing required field to see browser validation.
5. Each form is self-contained HTML and performs browser-side validation only; nothing is transmitted or persisted.

Explain that the HTML, CSS, and restricted validation JavaScript are embedded in one file and use no external framework. This is a browser-side demonstration; it does not upload files, submit applications, or store personal information.

## 7. Demonstrate the novelty feature

```bash
./build/vista examples/hidden_required.vista \
  --emit all \
  --out-dir out/hidden-required
```

Expected diagnostic:

```text
ANL003: field 'certificate' can be required while hidden; witness: category=general, certificate.visible=false, certificate.required=true
```

Then run:

```bash
ls -1 out/hidden-required
cat out/hidden-required/diagnostics.txt
```

Point out that `form.html` is absent. VISTA fails closed and gives a concrete assignment that reproduces the problem.

## 8. Briefly show other error levels

```bash
./build/vista examples/lexical_error.vista --emit diagnostics
./build/vista examples/syntax_error.vista --emit diagnostics
./build/vista examples/type_mismatch.vista --emit diagnostics
./build/vista examples/dependency_cycle.vista --emit diagnostics
```

The stable prefixes identify the compiler stage:

- `LEX`: lexical analysis
- `SYN`: syntax analysis
- `SEM`: semantic analysis
- `ANL`: dependency and witness analysis

## Likely viva questions

### Why use Flex and Bison?

Flex converts source characters into positioned tokens. Bison checks those tokens against the VISTA grammar and constructs the AST.

### Why is a symbol table required?

It resolves field references and stores type, choice-option, label, and source-location information for semantic checking.

### How are dependency cycles found?

Each conditional field reference becomes a directed edge. A depth-first search reports a cycle when it reaches a node already on the active DFS path.

### How is a hidden-required witness produced?

For Boolean and choice fields, VISTA enumerates their finite values. It evaluates visibility and requirement conditions and reports an assignment where `required` is true while `visible` is false.

### Why does `ANL900` exist?

The current prototype does not pretend to prove numeric, date, or text-dependent conditions. It reports the limitation explicitly so unsafe HTML is not generated.

### Is this the final complete project?

No. It is a working compiler prototype with three public-service form examples. It does not provide a backend for submitting or storing forms, nor a graphical template gallery or visual builder.
