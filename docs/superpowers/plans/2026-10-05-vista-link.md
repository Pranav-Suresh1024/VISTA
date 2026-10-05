# VISTA-Link Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Extend VISTA with typed modular workflows and an independently checkable certificate proving that its CLI and generated JavaScript agree for every behaviorally distinct input class supported by the current language.

**Architecture:** Keep the existing scanner, parser, AST, semantic analyser, validator, and HTML generator. Add module/workflow AST nodes, lower linked modules into one canonical value-owned workflow IR, derive finite predicate-induced cases, run those cases through the C++ evaluator and generated JavaScript, and serialize the evidence into a deterministic `.vcert` checked by a separate executable.

**Tech Stack:** C++17, Flex, GNU Bison, GNU Make, Bash end-to-end tests, Node.js 18+ for executing generated JavaScript; no third-party C++ libraries.

**Spec:** `docs/superpowers/specs/2026-10-05-vista-link-design.md`

## Global Constraints

- Preserve compatibility with existing single-form `.vista` files and commands.
- Reuse the existing expression, field, semantic, diagnostic, validation, and HTML-generation behavior.
- Keep normal form compilation usable without Node.js; require Node.js only for certificate generation and certificate end-to-end tests.
- Use deterministic ordering everywhere; never depend on pointer values or unordered-container iteration for emitted artifacts.
- Emit `PASS` only after complete CLI-versus-JavaScript comparison; unsupported semantics emit `UNSUPPORTED`, and disagreement emits `FAIL`.
- Cover Boolean, choice, integer, decimal, date, text, email, phone, textarea, and file-presence semantics.
- Use canonical checked decimal comparison for certified predicates rather than target-dependent floating-point formatting.
- Do not add AI, ML, an SMT solver, bytecode VM, runtime module loading, or file-content inspection.
- Use exit code `0` for success, `1` for source/conformance failure, and `2` for command, file, or missing-runtime errors.
- Work from a `codex/` feature branch created from protected baseline commit `510059d9c7878e322e6cb5c4cd0822d42861f199`; do not add `.codex-build/`, `.codex-finalizer/`, `deliverables/`, or `vista_hard_novelty/`.

## Review Focus

- A workflow or output path containing spaces or apostrophes must reach Node without command injection or path truncation; Task 7 adds this test.
- A choice literal that has the same spelling as a local or imported field must retain choice-literal semantics; Tasks 3 and 5 add this test.
- Negative decimals, exponent notation, and adjacent decimal boundaries must receive canonical classes and identical target results; Tasks 4 and 5 add this test.
- A conditional route cycle must terminate exploration by canonical-state deduplication and must never be certified as an infinite successful route; Task 5 adds this test.
- A `.vobj` whose embedded source or recorded hash has changed must be rejected before linking; Tasks 2 and 8 add this test.

---

## File Structure

### Existing files to modify

- `include/token.hpp`, `src/token.cpp`, `src/vista.l`, `src/parser.cpp`, `src/vista.y`: module/workflow tokens and grammar.
- `include/ast.hpp`, `src/ast.cpp`, `include/parser.hpp`: module/workflow AST ownership and formatting.
- `include/semantic.hpp`, `src/semantic.cpp`: semantic analysis with linker-supplied imported symbols.
- `include/validation.hpp`, `src/validation.cpp`: delegate shared value parsing and expression evaluation to the canonical evaluator.
- `include/codegen.hpp`, `src/codegen.cpp`: preserve current form generation and add workflow HTML/JavaScript generation.
- `src/main.cpp`: add module, linked-workflow, partition, workflow, and certificate modes without changing old modes.
- `include/version.hpp`, `tests/test_scanner.sh`: raise and pin the version when certificate metadata becomes public.
- `Makefile`: compile the new sources, build the checker, and run the new test scripts.
- `README.md`, `DEMO_GUIDE.md`: document commands, requirements, claims, and demonstration flow.

### New production files

- `include/hash.hpp`, `src/hash.cpp`: deterministic SHA-256 used by objects, IR, generated JavaScript, and certificates.
- `include/module.hpp`, `src/module.cpp`: module contracts and `.vobj` serialization/loading.
- `include/workflow.hpp`: value-owned linked workflow IR shared by the linker, evaluator, generator, and certifier.
- `include/linker.hpp`, `src/linker.cpp`: source/object loading, import/export resolution, lowering, and structural workflow checks.
- `include/evaluator.hpp`, `src/evaluator.cpp`: canonical scalar parsing, expression evaluation, form/module state, routing, and observations.
- `include/partition.hpp`, `src/partition.cpp`: behavioral classes and reachable route-case generation.
- `include/certificate.hpp`, `src/certificate.cpp`: certificate value types, deterministic codec, and verification helpers.
- `include/certify.hpp`, `src/certify.cpp`: JavaScript probe execution, target comparison, witness minimization, and certificate production.
- `src/cert_check_main.cpp`: independent `build/vista-cert-check` command.

### New tests and fixtures

- `tests/test_vista_link_parser.sh`
- `tests/test_vista_link_module.sh`
- `tests/test_vista_link_linker.sh`
- `tests/test_vista_link_evaluator.sh`
- `tests/test_vista_link_partition.sh`
- `tests/test_vista_link_codegen.sh`
- `tests/test_vista_link_certificate.sh`
- `tests/test_vista_link_mutations.sh`
- `examples/vista_link/`: valid workflow modules and focused invalid fixtures.
- `samples/vista_link/`: workflow data files used by CLI evaluation tests.

## Phase 0: Protected Starting Point

This phase changes no product files.

- [ ] **Step 1: Confirm the protected baseline exists**

Run: `git show --quiet --oneline 510059d9c7878e322e6cb5c4cd0822d42861f199`

Expected: commit subject `docs: add backend-only demo workflow`.

- [ ] **Step 2: Create an isolated implementation worktree and feature branch**

Use the required worktree skill at execution time and create a branch named `codex/vista-link` from the approved design commit `464b385`.

Expected: implementation occurs outside the user's current checkout and `git status --short` is empty in the worktree.

- [ ] **Step 3: Record the current regression result**

Run: `make clean && make test`

Expected: all existing scanner, parser, semantic, analysis, codegen, phase-2, template, validation, and pipeline tests pass.

---

### Task 1: Parse and Print Module and Workflow Source Units

**Files:**
- Modify: `include/token.hpp:10-56`
- Modify: `src/token.cpp:7-58`
- Modify: `src/vista.l:67-107`
- Modify: `include/ast.hpp:27-166`
- Modify: `src/ast.cpp:67-154`
- Modify: `include/parser.hpp:12-35`
- Modify: `src/parser.cpp:7-48`
- Modify: `src/vista.y:59-451`
- Create: `tests/test_vista_link_parser.sh`
- Create: `examples/vista_link/applicant.vista`
- Create: `examples/vista_link/certificate.vista`
- Create: `examples/vista_link/scholarship.workflow.vista`
- Create: `examples/vista_link/errors/duplicate_export.vista`
- Create: `examples/vista_link/errors/malformed_route.vista`

**Interfaces:**
- Consumes: existing `Expression`, `FieldDecl`, `Declaration`, `FormAst`, `AstArena`, `Token`, and `Diagnostic` types.
- Produces: `ModuleAst`, `ImportDecl`, `ExportDecl`, `WorkflowAst`, `WorkflowUse`, `RouteDecl`, and a `ParseResult` containing exactly one of `form`, `module`, or `workflow`.
- Produces token kinds: `Module`, `Import`, `Export`, `Enter`, `Workflow`, `Use`, `As`, `Start`, `Route`, `Present`, `Arrow`, and `Dot`.

- [ ] **Step 1: Write the failing parser end-to-end test**

In `tests/test_vista_link_parser.sh`, assert:

```bash
./build/vista examples/vista_link/applicant.vista --emit ast
# contains: Module Applicant
# contains: Export category

./build/vista examples/vista_link/certificate.vista --emit ast
# contains: Import Applicant.category as applicant_category
# contains: EnterWhen

./build/vista examples/vista_link/scholarship.workflow.vista --emit ast
# contains: Workflow ScholarshipApplication
# contains: Use Applicant "applicant.vista"
# contains: Start Applicant
# contains: Route Applicant -> Certificate
# contains: Name Applicant.category
```

Also assert that duplicate exports and malformed routes return `1`, and that `examples/valid_scholarship.vista --emit ast` remains unchanged.

- [ ] **Step 2: Register the new test and verify failure**

Add `tests/test_vista_link_parser.sh` immediately after `tests/test_parser.sh` in the `Makefile` test target.

Run: `make clean && make && bash tests/test_vista_link_parser.sh`

Expected: FAIL because `module`, `workflow`, `->`, and qualified names are not recognized.

- [ ] **Step 3: Add tokens and scanner rules**

Append the new token kinds after `TypeTextarea` in `TokenKind`; assign matching explicit Bison token values beginning at `295`. Add scanner rules before `{IDENTIFIER}`, with `"->"` before any single-character rule and `"."` mapped to `Dot`. Add token names and parser static assertions.

- [ ] **Step 4: Add value-owned AST declarations**

Add these exact public shapes to `include/ast.hpp` as `AstNode` subclasses owned by `AstArena`:

```cpp
struct ImportDecl;
struct ExportDecl;
struct ModuleAst;
struct WorkflowUse;
struct RouteDecl;
struct WorkflowAst;
```

`ImportDecl` stores `provider`, `field`, and `alias`; `ExportDecl` stores `field`; `ModuleAst` stores `name`, imports, exports, optional `enter_condition`, and ordinary declarations. `WorkflowUse` stores `source_path` and `alias`; `RouteDecl` stores `source`, `destination`, and optional condition; `WorkflowAst` stores `name`, uses, `start_module`, and routes.

Change `ParseResult::succeeded()` to require no diagnostics and exactly one non-null root among `form`, `module`, and `workflow`.

- [ ] **Step 5: Extend the Bison grammar**

Keep the existing `form` production. Add top-level `module` and `workflow` productions, `import Provider field as alias`, `export field`, `enter when expression`, `use "path" as Alias`, `start Alias`, and `route Source -> Destination [when expression]`. Parse `identifier DOT identifier` in expressions as one `ExpressionKind::Name` value containing `Alias.field`. Parse the built-in `present(identifier)` as `ExpressionKind::Unary` with value `present`; semantic analysis accepts it for file fields and returns Boolean.

Reject duplicate `enter` or `start` declarations during parsing with `SYN002`. Leave duplicate exports for semantic/module-contract diagnostics.

- [ ] **Step 6: Extend AST formatting and main's AST dispatch**

Add:

```cpp
std::string format_ast(const ModuleAst& module);
std::string format_ast(const WorkflowAst& workflow);
```

Update only the AST-mode dispatch in `src/main.cpp` so the correct overload is called. Do not change the remaining form pipeline yet; non-AST module/workflow modes may return a clear unsupported-command error until later tasks.

- [ ] **Step 7: Run parser and regression tests**

Run: `make clean && make && bash tests/test_vista_link_parser.sh && make test`

Expected: all tests pass; existing AST snapshots remain unchanged.

- [ ] **Step 8: Commit**

```bash
git add include/token.hpp include/ast.hpp include/parser.hpp src/token.cpp src/vista.l src/parser.cpp src/vista.y src/ast.cpp src/main.cpp Makefile tests/test_vista_link_parser.sh examples/vista_link
git commit -m "feat: parse VISTA modules and workflows"
```

---

### Task 2: Compile Deterministic Module Objects

**Files:**
- Create: `include/hash.hpp`
- Create: `src/hash.cpp`
- Create: `include/module.hpp`
- Create: `src/module.cpp`
- Modify: `include/semantic.hpp:50-62`
- Modify: `src/semantic.cpp:153-580`
- Modify: `src/main.cpp:22-376`
- Modify: `Makefile:12-29`
- Create: `tests/test_vista_link_module.sh`
- Create: `examples/vista_link/errors/missing_export_field.vista`
- Create: `examples/vista_link/errors/tampered.vobj`

**Interfaces:**
- Consumes: `ModuleAst`, existing field semantics, canonical LF-normalized source text.
- Produces: `std::string sha256_hex(std::string_view input)`.
- Produces: `ModuleCompileResult compile_module(const ModuleAst&, std::string_view canonical_source)`.
- Produces: `std::string serialize_module_object(const ModuleObject&)` and `ModuleLoadResult load_module_object(std::string_view bytes)`.
- Produces CLI: `vista <module.vista> --emit module --out-dir <dir>`, writing `<dir>/module.vobj`.

- [ ] **Step 1: Write failing module-object tests**

Assert in `tests/test_vista_link_module.sh`:

```bash
./build/vista examples/vista_link/applicant.vista --emit module --out-dir build/vobj-a
./build/vista examples/vista_link/applicant.vista --emit module --out-dir build/vobj-b
cmp build/vobj-a/module.vobj build/vobj-b/module.vobj
grep -F "VISTA-OBJECT 1" build/vobj-a/module.vobj
grep -F "module Applicant" build/vobj-a/module.vobj
grep -F "export category choice" build/vobj-a/module.vobj
```

Also assert the SHA-256 known vector `sha256("abc") = ba7816bf...15ad`, missing export field returns `MOD002`, duplicate export returns `MOD003`, and a tampered object returns `MOD004`.

- [ ] **Step 2: Run the test to verify failure**

Run: `make && bash tests/test_vista_link_module.sh`

Expected: FAIL because module emission and hashing do not exist.

- [ ] **Step 3: Implement dependency-free SHA-256**

Expose only:

```cpp
std::string sha256_hex(std::string_view input);
```

Keep the compression implementation private to `src/hash.cpp`. Test the empty-string and `abc` vectors before using it elsewhere.

- [ ] **Step 4: Define the module-object contract**

Use these value types in `include/module.hpp`:

```cpp
struct ImportContract { std::string provider, field, alias; SourceSpan span; };
struct ExportContract { std::string field; FieldType type; std::vector<std::string> options; };
struct ModuleObject {
    unsigned format_version;
    std::string name;
    std::string source_sha256;
    std::vector<ImportContract> imports;
    std::vector<ExportContract> exports;
    std::string canonical_source;
    std::string object_sha256;
};
struct ModuleCompileResult { ModuleObject object; std::vector<Diagnostic> diagnostics; bool succeeded() const; };
struct ModuleLoadResult { ModuleObject object; std::vector<Diagnostic> diagnostics; bool succeeded() const; };
```

Normalize CRLF to LF before hashing or embedding source. Serialize sorted imports/exports followed by a byte-counted canonical source payload. Compute `object_sha256` over every preceding canonical byte.

- [ ] **Step 5: Compile local contracts and defer imported-expression typing**

Validate local fields using the existing semantic rules. Validate exports against local symbols. Preserve import references for the linker; expressions involving imported aliases receive full type checking only after the linker supplies their symbols. Do not suppress diagnostics for unknown names that are not declared imports.

- [ ] **Step 6: Add module CLI emission**

Extend the existing argument parser minimally: `--emit module` is a directory mode requiring `--out-dir`. Clear stale `module.vobj` before compilation. Emit no object if scanning, parsing, local semantics, export validation, or hashing fails.

- [ ] **Step 7: Run module and full regression tests**

Run: `make clean && make && bash tests/test_vista_link_module.sh && make test`

Expected: deterministic objects pass; malformed and tampered objects fail closed; old tests pass.

- [ ] **Step 8: Commit**

```bash
git add include/hash.hpp include/module.hpp include/semantic.hpp src/hash.cpp src/module.cpp src/semantic.cpp src/main.cpp Makefile tests/test_vista_link_module.sh examples/vista_link/errors
git commit -m "feat: emit deterministic VISTA module objects"
```

---

### Task 3: Link Typed Module Contracts into Canonical Workflow IR

**Files:**
- Create: `include/workflow.hpp`
- Create: `include/linker.hpp`
- Create: `src/linker.cpp`
- Modify: `include/semantic.hpp:24-62`
- Modify: `src/semantic.cpp:153-580`
- Modify: `src/main.cpp:178-376`
- Modify: `Makefile:12-29`
- Create: `tests/test_vista_link_linker.sh`
- Create: `examples/vista_link/errors/missing_export.workflow.vista`
- Create: `examples/vista_link/errors/type_mismatch.workflow.vista`
- Create: `examples/vista_link/errors/duplicate_alias.workflow.vista`
- Create: `examples/vista_link/errors/invalid_start.workflow.vista`
- Create: `examples/vista_link/errors/import_cycle.workflow.vista`
- Create: `examples/vista_link/errors/unreachable.workflow.vista`
- Create: `examples/vista_link/errors/ambiguous_unconditional.workflow.vista`
- Create: `examples/vista_link/choice_collision.vista`

**Interfaces:**
- Consumes: `WorkflowAst`, workflow source path, `.vista` or `.vobj` module paths, module objects, existing semantic analyser.
- Produces value-owned `IrExpression`, `LinkedField`, `LinkedModule`, `LinkedRoute`, and `LinkedWorkflow` in `include/workflow.hpp`.
- Produces: `LinkResult link_workflow(const WorkflowAst&, const std::filesystem::path& workflow_path)`.
- Produces: `std::string format_linked_workflow(const LinkedWorkflow&)`.
- Produces CLI: `vista <workflow.vista> --emit linked --out-dir <dir>`, writing `<dir>/workflow.ir`.

- [ ] **Step 1: Write failing linker tests**

Assert a valid link produces deterministic `workflow.ir` containing fully qualified `Applicant.category`, resolved import `Certificate.applicant_category -> Applicant.category`, start module, route, module hashes, stable rule IDs, and linked-workflow SHA-256.

Assert focused fixtures return:

```text
LNK001 missing module or unreadable use path
LNK002 unresolved import/export
LNK003 incompatible import use after provider typing
LNK004 duplicate workflow alias
LNK005 invalid start module
LNK006 import or unconditional route cycle
LNK007 unreachable module
LNK008 ambiguous unconditional outgoing routes
```

Add a choice-collision assertion proving a choice option named like another field remains a choice literal.

- [ ] **Step 2: Run the test to verify failure**

Run: `make && bash tests/test_vista_link_linker.sh`

Expected: FAIL because linked IR emission does not exist.

- [ ] **Step 3: Define the canonical linked IR**

Use a recursive value-owned expression so no parser arena pointers survive linking:

```cpp
struct IrExpression {
    ExpressionKind kind;
    ValueType type;
    std::string value;
    std::vector<IrExpression> operands;
};
```

`LinkedField` stores qualified name, type, options, constraints, normalized visibility/requiredness expressions, and stable rule IDs. `LinkedModule` stores alias, object/source hashes, imports, fields, checks, and entry expression. `LinkedRoute` stores source, destination, normalized condition, and rule ID. `LinkedWorkflow` stores name, start alias, sorted modules/routes, and canonical hash.

Define `LinkResult` as `LinkedWorkflow workflow`, `std::vector<Diagnostic> diagnostics`, and `bool succeeded() const`.

- [ ] **Step 4: Resolve paths and contracts**

Resolve relative `use` paths against the workflow file's directory. Accept `.vista` by compiling it in memory and `.vobj` by loading and validating it. Reject duplicate aliases before loading. Resolve each import to the named provider alias and exported field.

- [ ] **Step 5: Re-run module semantics with imported symbols**

Add:

```cpp
SemanticResult analyze_semantics(
    const ModuleAst& module,
    const std::vector<Symbol>& imported_symbols);
```

Imported aliases are read-only symbols carrying the provider's exact type and choice options. Lower names to fully qualified provider fields. Preserve current choice-context behavior when a choice option collides with a field name.

- [ ] **Step 6: Validate workflow structure and emit canonical IR**

Check start, route endpoints, reachability, import cycles, unconditional route cycles, and duplicate unconditional outgoing routes. Sort modules by alias and routes by source/destination/rule ID only for serialization; retain declared route order solely for diagnostics, never as runtime priority.

- [ ] **Step 7: Run linker and regression tests**

Run: `make clean && make && bash tests/test_vista_link_linker.sh && make test`

Expected: all valid and invalid link cases behave as specified; old forms still compile.

- [ ] **Step 8: Commit**

```bash
git add include/workflow.hpp include/linker.hpp include/semantic.hpp src/linker.cpp src/semantic.cpp src/main.cpp Makefile tests/test_vista_link_linker.sh examples/vista_link
git commit -m "feat: link typed VISTA workflow modules"
```

---

### Task 4: Build One Canonical Workflow Evaluator

**Files:**
- Create: `include/evaluator.hpp`
- Create: `src/evaluator.cpp`
- Modify: `include/validation.hpp:12-29`
- Modify: `src/validation.cpp:20-606`
- Modify: `src/main.cpp:178-376`
- Modify: `Makefile:12-40`
- Create: `tests/test_vista_link_evaluator.sh`
- Create: `samples/vista_link/general.data`
- Create: `samples/vista_link/reserved-valid.data`
- Create: `samples/vista_link/reserved-missing-file.data`
- Create: `samples/vista_link/decimal-exponent.data`

**Interfaces:**
- Consumes: `LinkedWorkflow` and `InputValues` parsed from the existing `field = value` format.
- Produces: `InputLoadResult load_input_values(const std::string& path)`.
- Produces: `EvaluationResult evaluate_workflow(const LinkedWorkflow&, const InputValues&)`.
- Produces: `std::string format_observation(const EvaluationObservation&)` and `std::string canonical_observation(const EvaluationObservation&)`.
- Reuses CLI: `vista <workflow.vista> --validate-data <values.data>`.

- [ ] **Step 1: Write failing evaluator tests**

Test these exact outcomes:

- General applicant completes after `Applicant` with no certificate module.
- Reserved applicant routes to `Certificate`.
- Missing required certificate fails in `Certificate`.
- Unknown supplied field is rejected.
- Negative/exponent decimal input is parsed canonically and produces a stable observation.
- Two simultaneously true conditional routes return `LNK008`, not first-route-wins behavior.

The report must contain `Active module`, `Visible fields`, `Required fields`, `Selected route`, `Complete`, `Result`, and stable rule IDs.

- [ ] **Step 2: Run the test to verify failure**

Run: `make && bash tests/test_vista_link_evaluator.sh`

Expected: FAIL because workflow validation is not implemented.

- [ ] **Step 3: Define canonical inputs and observations**

Use sorted maps/vectors in the public representation:

```cpp
using InputValues = std::map<std::string, std::string>;
struct InputLoadResult { InputValues values; std::vector<Diagnostic> diagnostics; bool succeeded() const; };
struct FieldObservation { std::string field; bool visible, required, valid; };
struct EvaluationObservation {
    std::string active_module;
    std::vector<FieldObservation> fields;
    std::string selected_route;
    bool complete;
    std::vector<std::string> diagnostic_codes;
    std::vector<std::string> rule_ids;
};
struct EvaluationResult { EvaluationObservation observation; std::vector<Diagnostic> diagnostics; bool succeeded() const; };
```

- [ ] **Step 4: Extract shared scalar parsing and expression evaluation**

Move reusable internal behavior from `src/validation.cpp` into `src/evaluator.cpp`: Boolean parsing, integer parsing, checked canonical decimal normalization/comparison, ISO-date validation/comparison, text values, choice membership, the `present(file)` operator, and file missing/present state. Make existing `validate_data_file()` delegate to these helpers so old CLI and workflow evaluation cannot drift.

Represent canonical decimals as sign, digit string without leading/trailing zero ambiguity, and decimal scale; compare without converting to `double` or `long double`.

- [ ] **Step 5: Evaluate module state and routes**

For the active module, evaluate entry, visibility, requiredness, field constraints, and checks. A module completes only when visible required fields and checks pass. Evaluate all outgoing route conditions; zero true routes completes the workflow, one selects the destination, and more than one yields `LNK008`.

- [ ] **Step 6: Add workflow `--validate-data` dispatch**

If the parsed root is a workflow, link it and call `evaluate_workflow`; otherwise keep the current form path and report format unchanged.

- [ ] **Step 7: Run evaluator, validation, and regression tests**

Run: `make clean && make && bash tests/test_vista_link_evaluator.sh && bash tests/test_validation.sh && make test`

Expected: new workflow tests pass and all existing data-validation outputs still pass.

- [ ] **Step 8: Commit**

```bash
git add include/evaluator.hpp include/validation.hpp src/evaluator.cpp src/validation.cpp src/main.cpp Makefile tests/test_vista_link_evaluator.sh samples/vista_link
git commit -m "feat: evaluate linked VISTA workflows"
```

---

### Task 5: Derive Complete Behavioral Partitions and Reachable Cases

**Files:**
- Create: `include/partition.hpp`
- Create: `src/partition.cpp`
- Modify: `include/analysis.hpp:12-38`
- Modify: `src/analysis.cpp:1-340`
- Modify: `src/main.cpp:178-376`
- Modify: `Makefile:12-40`
- Create: `tests/test_vista_link_partition.sh`
- Create: `tests/test_partition_unit.cpp`
- Create: `examples/vista_link/all_type_routes.vista`
- Create: `examples/vista_link/all_type_routes.workflow.vista`
- Create: `examples/vista_link/conditional_cycle.workflow.vista`

**Interfaces:**
- Consumes: `LinkedWorkflow` and canonical evaluator.
- Produces: `PartitionResult derive_partitions(const LinkedWorkflow&)`.
- Produces: `RouteSuite enumerate_route_cases(const LinkedWorkflow&, const PartitionResult&)`.
- Produces CLI: `vista <workflow.vista> --emit partitions`.

- [ ] **Step 1: Write failing all-type partition tests**

Assert deterministic output classes for:

- Boolean: `false`, `true`.
- Choice: every option plus invalid-input validation coverage.
- Integer: below, equal, between, and above predicate/constraint constants.
- Decimal: negative, exponent-normalized, exact boundary, between, and above classes.
- Date: before, equal, between, and after valid ISO boundaries.
- Text/textarea: empty, each compared literal, length-boundary representatives, and `OTHER`.
- Email/phone: valid and invalid format representatives combined with relevant literals/length boundaries.
- File: `missing`, `present`, including `present(file)` route conditions.

Run partition generation twice and compare byte-for-byte. In `tests/test_partition_unit.cpp`, construct one minimal `LinkedWorkflow` containing an `IrExpression` with operator `future-op` and assert `derive_partitions()` returns `CERT001`/`UNSUPPORTED`. Assert a conditional cycle terminates and is not reported as successful completion. Assert a choice literal colliding with a field remains correctly partitioned.

- [ ] **Step 2: Run the test to verify failure**

Run: `make && bash tests/test_vista_link_partition.sh`

Expected: FAIL because partition output does not exist and existing analysis reports `ANL900`.

- [ ] **Step 3: Define partition and route-case types**

```cpp
enum class BehaviorClassKind { Missing, Present, Exact, Below, Between, Above, Other, Invalid };
struct BehaviorClass { std::string field, id, representative, description; BehaviorClassKind kind; };
struct FieldPartition { std::string field; std::vector<BehaviorClass> classes; };
struct PartitionResult { std::vector<FieldPartition> fields; std::vector<Diagnostic> diagnostics; bool complete() const; };
struct RouteCase { std::string id; InputValues values; std::vector<std::string> expected_route_prefix; };
struct RouteSuite { std::vector<RouteCase> cases; std::vector<Diagnostic> diagnostics; bool complete() const; };
```

- [ ] **Step 4: Collect only behavior-relevant fields and constants**

Walk entry, route, visibility, requiredness, constraints, and check expressions. Build classes from the operators and literals actually used. Include format-valid/invalid representatives for email and phone because existing validation observes those classes. Exclude presentation-only fields.

- [ ] **Step 5: Generate exact deterministic representatives**

Use canonical decimal and ISO-date helpers from Task 4. For textual `OTHER`, choose the first value from a fixed sequence that differs from every literal. For between-regions, generate a representative only when the region is non-empty. Do not sample randomly.

- [ ] **Step 6: Explore reachable cases with state deduplication**

Use breadth-first exploration keyed by active module plus relevant behavior-class IDs. Stop expanding a repeated state. Return `CERT002` if a route cycle has no completing path for the state. Apply a hard safety ceiling of `100000` explored states; crossing it returns `CERT003`/`UNSUPPORTED`, never `PASS`.

Add the implementation comment: `ponytail: 100000-state safety ceiling; replace with symbolic composition if real workflows exceed it.`

- [ ] **Step 7: Replace the old broad `ANL900` boundary where exact classes now exist**

Keep existing single-form witness diagnostics and tests, but route supported numeric/date/text conditions through the shared partition logic. Reserve `ANL900` only for operators that still lack a partition rule.

- [ ] **Step 8: Run partition and regression tests**

Compile `tests/test_partition_unit.cpp` through a dedicated `build/test-partition-unit` Make target used by `tests/test_vista_link_partition.sh`.

Run: `make clean && make && bash tests/test_vista_link_partition.sh && bash tests/test_analysis.sh && make test`

Expected: all-type classes and cycle behavior pass; existing Boolean/choice witnesses remain stable.

- [ ] **Step 9: Commit**

```bash
git add include/partition.hpp include/analysis.hpp src/partition.cpp src/analysis.cpp src/main.cpp Makefile tests/test_vista_link_partition.sh tests/test_partition_unit.cpp examples/vista_link
git commit -m "feat: derive complete VISTA behavior partitions"
```

---

### Task 6: Generate Workflow HTML and an Executable JavaScript Evaluator

**Files:**
- Modify: `include/codegen.hpp:9-16`
- Modify: `src/codegen.cpp:20-476`
- Modify: `src/main.cpp:22-376`
- Modify: `Makefile:32-42`
- Create: `tests/test_vista_link_codegen.sh`

**Interfaces:**
- Consumes: `LinkedWorkflow` and canonical IR expressions.
- Produces: `std::string generate_workflow_javascript(const LinkedWorkflow&)`.
- Produces: `std::string generate_workflow_html(const LinkedWorkflow&, std::string_view generated_javascript)`.
- JavaScript exports: `vistaEvaluate(input)` in both browser global scope and `module.exports` under Node.
- Produces CLI: `vista <workflow.vista> --emit workflow --out-dir <dir>`, writing `workflow.ir`, `workflow.html`, and `workflow.js`.

- [ ] **Step 1: Write failing workflow-codegen tests**

Assert generated HTML contains accessible module sections, labels, error/status regions, and the exact generated evaluator embedded in its script. Assert generated `workflow.js` can be loaded by Node and:

```javascript
const { vistaEvaluate } = require('./workflow.js');
const result = vistaEvaluate({ 'Applicant.category': 'reserved' });
if (result.selected_route !== 'Applicant->Certificate') process.exit(1);
```

Assert the browser event handler calls `vistaEvaluate` rather than independently reimplementing rules. Existing form HTML snapshots must remain unchanged.

- [ ] **Step 2: Run the test to verify failure**

Run: `make && bash tests/test_vista_link_codegen.sh`

Expected: FAIL because workflow artifacts do not exist.

- [ ] **Step 3: Generate pure JavaScript evaluation from linked IR**

Generate deterministic field/rule tables and one pure `vistaEvaluate(input)` function. Implement canonical decimal comparison in generated JavaScript using strings and integer digit operations matching Task 4. Return the same canonical observation fields and ordering as C++.

- [ ] **Step 4: Build workflow HTML around the generated evaluator**

Render one accessible section per module and show only the active section. Embed the exact string returned by `generate_workflow_javascript`; UI event handlers call `vistaEvaluate`, update visibility/requiredness, render errors, and navigate only when evaluation selects one route.

- [ ] **Step 5: Add workflow emission and stale-artifact clearing**

Treat `--emit workflow` as a directory mode. On failure, remove stale `workflow.ir`, `workflow.html`, and `workflow.js`. Write artifacts only after scan, parse, link, and generation succeed.

- [ ] **Step 6: Run codegen and regression tests**

Run: `make clean && make && bash tests/test_vista_link_codegen.sh && bash tests/test_codegen.sh && make test`

Expected: Node and browser-facing generation use the same evaluator; existing form HTML tests pass.

- [ ] **Step 7: Commit**

```bash
git add include/codegen.hpp src/codegen.cpp src/main.cpp Makefile tests/test_vista_link_codegen.sh
git commit -m "feat: generate executable VISTA workflow targets"
```

---

### Task 7: Execute Both Targets and Emit a Conformance Certificate

**Files:**
- Create: `include/certificate.hpp`
- Create: `src/certificate.cpp`
- Create: `include/certify.hpp`
- Create: `src/certify.cpp`
- Modify: `src/main.cpp:22-376`
- Modify: `include/version.hpp:8-9`
- Modify: `tests/test_scanner.sh:19-21`
- Modify: `Makefile:12-42`
- Create: `tests/test_vista_link_certificate.sh`

**Interfaces:**
- Consumes: `LinkedWorkflow`, `RouteSuite`, generated JavaScript, canonical C++ observations, and an output directory.
- Produces from `include/certify.hpp`: `CertificateResult certify_workflow(const LinkedWorkflow&, const RouteSuite&, std::string_view javascript, const std::filesystem::path& output_directory)`.
- Produces from `include/certificate.hpp`: `std::string serialize_certificate(const Certificate&)` and `CertificateParseResult parse_certificate(std::string_view)`.
- Produces CLI: `vista <workflow.vista> --emit certificate --out-dir <dir>`.
- Produces optional mutation/test CLI: append `--javascript <existing-workflow.js>` to certify that exact supplied target instead of regenerating it.

- [ ] **Step 1: Write failing certificate tests**

Assert the default command creates `workflow.ir`, `workflow.html`, `workflow.js`, and `workflow.vcert`. The certificate must contain:

```text
VISTA-CERT 1
status PASS
workflow ScholarshipApplication
compiler-version 0.8.0
workflow-sha256 <64 lowercase hex characters>
javascript-sha256 <64 lowercase hex characters>
case-count <nonzero integer>
case <id> <input-hash> <cli-observation-hash> <js-observation-hash>
```

Every case's CLI and JS hash must match. Repeated runs must produce byte-identical certificates. Missing Node returns `2` and no certificate. A workflow/output path containing spaces and an apostrophe must certify safely.

- [ ] **Step 2: Run the test to verify failure**

Run: `make && bash tests/test_vista_link_certificate.sh`

Expected: FAIL because certification does not exist.

- [ ] **Step 3: Define certificate evidence types**

```cpp
enum class CertificateStatus { Pass, Fail, Unsupported };
struct CaseEvidence { std::string id, input_sha256, cli_sha256, javascript_sha256; };
struct Certificate {
    unsigned format_version;
    CertificateStatus status;
    std::string compiler_version, workflow, workflow_sha256, javascript_sha256;
    std::vector<std::pair<std::string, std::string>> module_hashes;
    std::vector<std::string> partition_ids;
    std::vector<CaseEvidence> cases;
    std::string counterexample;
    std::string unsupported_rule;
};
struct CertificateParseResult { Certificate certificate; std::vector<Diagnostic> diagnostics; bool succeeded() const; };
struct CertificateResult { Certificate certificate; std::vector<Diagnostic> diagnostics; bool succeeded() const; };
```

Set `kVersion` to `0.8.0` and update the existing scanner/version assertion in the same step so emitted certificate metadata and CLI output agree.

- [ ] **Step 4: Generate and execute the Node probe**

Write a deterministic `probe.js` that imports the exact `workflow.js`, evaluates the route cases, and prints escaped canonical observations in case-ID order. Invoke `node` with safely shell-quoted generated paths; reject embedded NUL characters. Capture the exit status and output file. The path-with-apostrophe test must prove quoting correctness.

- [ ] **Step 5: Compare observations and minimize the first mismatch**

Hash each canonical observation. On mismatch, retain the breadth-first first divergent route trace, then remove assignments one at a time in sorted field order when the mismatch still reproduces. Record the resulting witness and both observations. Never write `PASS` after a missing case, duplicate case, malformed probe line, Node failure, or incomplete partition suite.

- [ ] **Step 6: Serialize deterministic evidence**

Use a line-oriented UTF-8 format with percent-encoded free text. Sort module hashes, partition IDs, and case evidence. The overall status is `PASS` only when the suite is complete and every case matches; it is `UNSUPPORTED` for `CERT001`/`CERT003`, otherwise `FAIL` for a target disagreement.

- [ ] **Step 7: Run certificate and regression tests**

Run: `make clean && make && bash tests/test_vista_link_certificate.sh && make test`

Expected: valid workflow certifies deterministically; runtime and path failures fail closed.

- [ ] **Step 8: Commit**

```bash
git add include/certificate.hpp include/certify.hpp src/certificate.cpp src/certify.cpp src/main.cpp include/version.hpp tests/test_scanner.sh Makefile tests/test_vista_link_certificate.sh
git commit -m "feat: emit VISTA cross-target certificates"
```

---

### Task 8: Add the Independent Certificate Checker

**Files:**
- Create: `src/cert_check_main.cpp`
- Modify: `include/certificate.hpp`
- Modify: `src/certificate.cpp`
- Modify: `Makefile:8-42`
- Modify: `tests/test_vista_link_certificate.sh`
- Create: `examples/vista_link/errors/corrupt.vcert`

**Interfaces:**
- Consumes: `.vcert`, optional `--root <project-directory>`, shared certificate parser, and SHA-256 only.
- Produces executable: `build/vista-cert-check`.
- Produces command: `vista-cert-check <workflow.vcert> [--root <directory>]`.
- Produces: `CertificateVerification verify_certificate(const Certificate&, const std::filesystem::path* root)`.

`CertificateVerification` contains `bool verified`, `std::vector<Diagnostic> diagnostics`, and the recomputed aggregate SHA-256 values used in the final report.

- [ ] **Step 1: Extend failing tests for independent verification**

Assert:

```bash
./build/vista-cert-check build/cert/workflow.vcert --root .
# exit 0; contains: VERIFIED
```

Then assert exit `1` for a changed module, mismatched case hashes, `FAIL`, `UNSUPPORTED`, malformed, truncated, duplicate-case, and corrupt certificates. Assert a `.vobj` with stale embedded-source hash is rejected before its certificate can verify.

- [ ] **Step 2: Run the checker test to verify failure**

Run: `make && bash tests/test_vista_link_certificate.sh`

Expected: FAIL because `build/vista-cert-check` does not exist.

- [ ] **Step 3: Add a separate checker target**

Build `build/vista-cert-check` from `src/cert_check_main.cpp`, `src/certificate.cpp`, `src/hash.cpp`, and diagnostics needed by the codec. It must not link the scanner, parser, linker, evaluator, partitioner, or code generator.

- [ ] **Step 4: Verify evidence rather than trusting status text**

Require valid syntax/version, unique case IDs, nonzero complete case count, matching per-case CLI/JavaScript hashes, matching aggregate fields, valid 64-character lowercase SHA-256 values, and `PASS`. With `--root`, recompute workflow/module/object/JavaScript hashes from named files and reject missing or changed artifacts.

- [ ] **Step 5: Run checker, certificate, and full tests**

Run: `make clean && make && bash tests/test_vista_link_certificate.sh && make test`

Expected: valid evidence verifies; every malformed/stale variant is rejected.

- [ ] **Step 6: Commit**

```bash
git add src/cert_check_main.cpp include/certificate.hpp src/certificate.cpp Makefile tests/test_vista_link_certificate.sh examples/vista_link/errors
git commit -m "feat: verify VISTA certificates independently"
```

---

### Task 9: Prove Detection with Mutations and Finish the End-to-End Product

**Files:**
- Create: `tests/test_vista_link_mutations.sh`
- Modify: `tests/test_pipeline.sh`
- Modify: `Makefile:32-52`
- Modify: `README.md`
- Modify: `DEMO_GUIDE.md`
- Create: `docs/VISTA_LINK_DEMO.md`

**Interfaces:**
- Consumes: completed compiler, generated workflow JavaScript, optional `--javascript`, certificate checker, and valid demo workflow.
- Produces: a cumulative `make test` and a teacher-ready `make vista-link-demo`.

- [ ] **Step 1: Write failing mutation tests**

Generate valid `workflow.js`, copy it per mutation, make one deterministic textual mutation, and certify with `--javascript`:

1. Reverse a Boolean condition.
2. Replace one choice literal.
3. Change an integer/decimal boundary.
4. Change a date boundary.
5. Treat empty text as nonempty.
6. Treat missing file as present.

For every mutation assert status `FAIL`, a nonempty `counterexample`, the differing rule/observation, and a witness that reproduces the mismatch. The unmodified target must remain `PASS`.

- [ ] **Step 2: Run mutation tests to verify failure**

Run: `make && bash tests/test_vista_link_mutations.sh`

Expected: at least one mutation is not yet detected or minimized.

- [ ] **Step 3: Fix only missing comparison or witness coverage**

Extend canonical observations or minimization only where a mutation escapes. Do not add mutation-only production branches or hidden flags.

- [ ] **Step 4: Add final pipeline assertions**

Extend `tests/test_pipeline.sh` with one complete path:

```text
module sources -> module contracts -> linked IR -> partitions
-> CLI cases + generated JavaScript cases -> workflow.vcert
-> vista-cert-check -> VERIFIED
```

Assert that a failed link or certificate run clears stale final artifacts.

- [ ] **Step 5: Update version, Makefile, and documentation**

Document Node.js 18+ as certificate-only, the exact commands, all artifacts, exit codes, `PASS` claim boundary, `FAIL` counterexample, `UNSUPPORTED` behavior, 100000-state ceiling, and rollback branch.

Add `make vista-link-demo` that builds, certifies `examples/vista_link/scholarship.workflow.vista`, verifies the certificate, evaluates general/reserved sample data, then certifies one mutated JavaScript file and shows its counterexample without making the demo target fail.

- [ ] **Step 6: Run every end-to-end check**

Run:

```bash
make clean
make
make test
make vista-link-demo
git diff --check
```

Expected: every test passes; demo shows one `VERIFIED` certificate and one detected mutation; no whitespace errors.

- [ ] **Step 7: Confirm deterministic clean rebuild**

Run the valid certificate command twice from clean output directories and compare `module.vobj`, `workflow.ir`, `workflow.js`, and `workflow.vcert` byte-for-byte.

Expected: all comparisons succeed.

- [ ] **Step 8: Commit**

```bash
git add tests/test_vista_link_mutations.sh tests/test_pipeline.sh Makefile README.md DEMO_GUIDE.md docs/VISTA_LINK_DEMO.md
git commit -m "test: complete VISTA-Link conformance workflow"
```

## Final Acceptance Checklist

- [ ] Existing VISTA form tests pass unchanged.
- [ ] Valid module objects and linked IR are deterministic.
- [ ] Missing exports, type mismatches, aliases, starts, cycles, reachability, and ambiguity fail closed.
- [ ] All existing VISTA field types receive exact behavioral classes for supported operations.
- [ ] Unknown operators and state-ceiling overflow produce `UNSUPPORTED`, never `PASS`.
- [ ] CLI and actual generated JavaScript execute every derived route case.
- [ ] Certificates contain source, module, workflow, JavaScript, partition, case, and observation evidence.
- [ ] `vista-cert-check` verifies evidence without linking compiler/evaluator/codegen code.
- [ ] Six target mutations produce reproducible shortest counterexamples.
- [ ] The valid teacher demonstration ends in `VERIFIED`.
- [ ] The protected baseline branch remains unchanged and available for rollback.
