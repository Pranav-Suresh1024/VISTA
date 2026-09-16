# VISTA Phase 2 Demonstration Guide

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

The demonstration bundle will be in `out/demo/`.

## 1. Introduce the project

Say:

> VISTA is a small source-to-source compiler. It reads a declarative form specification, performs lexical, syntax, semantic, and conditional-rule analysis, and generates a standalone HTML form only when the specification is safe.

Show the input:

```bash
sed -n '1,120p' examples/valid_scholarship.vista
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
./build/vista examples/valid_scholarship.vista \
  --emit all \
  --out-dir out/demo
```

The command should print:

```text
Generated complete VISTA output in out/demo
```

Show the generated files:

```bash
ls -1 out/demo
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
sed -n '1,18p' out/demo/tokens.txt
sed -n '1,40p' out/demo/ast.txt
cat out/demo/symbols.txt
cat out/demo/dependencies.txt
cat out/demo/diagnostics.txt
```

Use these explanations:

- `tokens.txt`: Flex identifies tokens and records line and column positions.
- `ast.txt`: Bison validates the grammar and constructs the abstract syntax tree.
- `symbols.txt`: The semantic pass stores each field's name, type, choices, label, and location.
- `dependencies.txt`: Conditional references become directed dependency edges.
- `diagnostics.txt`: A valid form reports `No diagnostics.`
- `graph.dot`: The same dependency information is available in Graphviz DOT format.

## 5. Demonstrate the generated form

Open `out/demo/form.html` in a browser.

1. Select `general`. The certificate field remains hidden.
2. Select `reserved`. The certificate field appears and becomes required.
3. Enter a name, choose `general`, and enter an age below 18.
4. Select **Validate form**. The form explains that applicants must be at least 18 years old.
5. Enter age `18` or above, choose `reserved`, attach any local test file, and select **Validate form** again. The page reports that validation passed.

Explain that the HTML, CSS, and restricted validation JavaScript are embedded in one file and use no external framework.

## 6. Show the starter-template catalog

List and compile a template:

```bash
./build/vista --list-templates
./build/vista --template train-booking --emit all --out-dir out/train-booking
```

Open `out/train-booking/form.html`. Enter the same city for **From** and **To** to show the validation message, then choose two different cities and validate again. Point out that this is a sample form only—it does not check live routes, fares, or reservations. The scholarship and event-registration templates demonstrate other domain-specific fields and conditional behavior. A custom `.vista` file can still be compiled by passing its path directly.

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

No. It is a substantial Phase 2 working prototype. The completed compiler pipeline is demonstrable, while broader analysis and final-project documentation remain future work.
