# VISTA Phase 2 Scholarship Demonstration Guide

This guide presents the current working prototype in about five minutes.

## Before the review

Open Ubuntu WSL and run:

```bash
cd "/mnt/c/Users/prana/OneDrive/Documents/ChatGPT/Compiler Design"
make clean all
make test
make demo
```

The final test line must be:

```text
Complete Phase 2 pipeline and failure-path tests passed.
```

The demonstration bundle will be in `out/phase2-scholarship/`.

## 1. Introduce the project

Say:

> VISTA is a small source-to-source compiler. It reads a declarative form specification, performs lexical, syntax, semantic, and conditional-rule analysis, and generates a standalone HTML form only when the specification is safe.

Show the input:

```bash
sed -n '1,180p' templates/scholarship_application.vista
```

Explain that the input is a `.vista` source file, not an interactive prompt.

## 2. Show incremental development and tests

```bash
git log --oneline --reverse
make test
```

Point out that each compiler module was added as a focused commit and every test suite passes cumulatively.

## 3. Compile the valid form

```bash
./build/vista templates/scholarship_application.vista \
  --emit all \
  --out-dir out/phase2-scholarship
```

The command should print:

```text
Generated complete VISTA output in out/phase2-scholarship
```

Show the generated files:

```bash
ls -1 out/phase2-scholarship
```

Expected files:

```text
ast.txt
dependencies.txt
diagnostics.txt
form.html
graph.dot
symbols.txt
tokens.txt
```

## 4. Explain intermediate compiler results

```bash
sed -n '1,18p' out/phase2-scholarship/tokens.txt
sed -n '1,40p' out/phase2-scholarship/ast.txt
cat out/phase2-scholarship/symbols.txt
cat out/phase2-scholarship/dependencies.txt
cat out/phase2-scholarship/diagnostics.txt
```

Use these explanations:

- `tokens.txt`: Flex identifies tokens and records line and column positions.
- `ast.txt`: Bison validates the grammar and constructs the abstract syntax tree.
- `symbols.txt`: The semantic pass stores each field's name, type, choices, label, and location.
- `dependencies.txt`: Conditional references become directed dependency edges.
- `diagnostics.txt`: A valid form reports `No diagnostics.`
- `graph.dot`: The same dependency information is available in Graphviz DOT format.

## 5. Demonstrate the generated form

Open `out/phase2-scholarship/form.html` in a browser.

1. Select `general`. The category certificate field stays hidden.
2. Select `reserved`. The certificate field appears and becomes required.
3. Try to submit with required fields empty; the browser focuses a missing field and shows its native validation message.
4. Enter an invalid email or a GPA outside `0`–`10`; the browser rejects it using the generated input constraints.
5. Enter a statement shorter than 50 characters; the browser reports the minimum-length requirement.
6. Fill the fields with valid sample values, choose `general`, attach sample files, check the declaration, and validate again. The page reports that validation passed.

Explain that the HTML, CSS, and restricted validation JavaScript are embedded in one file and use no external framework. This is a browser-side demonstration; it does not upload files, submit applications, or store personal information.

## 6. Demonstrate the novelty feature

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

## 7. Briefly show other error levels

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

No. It is a substantial Phase 2 working prototype. The completed compiler pipeline is demonstrable, while broader analysis and final-project documentation remain future work.
