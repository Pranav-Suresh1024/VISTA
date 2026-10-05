# VISTA-Link Design Specification

**Project:** VISTA Compiler

**Feature:** Contract-Checked Modular Form Workflows with Cross-Target Conformance Certification

**Date:** 2026-10-05

**Status:** Proposed design for user review

## Purpose

VISTA currently compiles one form definition into a standalone HTML form and supports command-line validation of submitted data. VISTA-Link extends that compiler into a modular workflow compiler without replacing the existing scanner, parser, AST, semantic analysis, validation, or HTML-generation pipeline.

The intended contribution is:

> VISTA-Link independently compiles form modules into hashed typed contracts, links them into a workflow, derives a finite behavior-complete input partition for every current VISTA field type, executes every resulting route against the CLI and generated JavaScript implementations, and emits an independently checkable conformance certificate with the shortest counterexample when the targets disagree.

The implementation must remain feasible for the current C++17, Flex, and Bison codebase and must preserve compatibility with existing single-form `.vista` programs.

## Success Criteria

The work is complete only when all of the following exist:

1. Independently compiled form modules with typed imports and exports.
2. A contract-checked linker for assembling modules into workflows.
3. A deterministic linked workflow representation with stable rule identifiers and hashes.
4. Exact finite behavioral partitions for all predicates expressible by the current VISTA language.
5. Execution of the same generated cases through the CLI evaluator and generated JavaScript evaluator.
6. A machine-readable conformance certificate.
7. A small independent certificate verifier.
8. A shortest counterexample when CLI and JavaScript behavior differs.
9. Mutation tests demonstrating that real target disagreements are detected.
10. Passing regression tests for existing VISTA programs.

## Non-Goals

The first implementation will not add:

- AI or machine-learning features.
- A virtual machine or bytecode runtime.
- An SMT solver.
- Distributed compilation.
- Dynamic module loading at form-runtime.
- Arbitrary user-defined functions.
- Inspection of uploaded file contents.
- Proofs for future predicates whose semantics have not been defined.

Unsupported future operations must produce an explicit `CERT_UNSUPPORTED` result. They must never receive a false `PASS` result.

## Architecture

```text
Module source files
        |
        v
Existing scanner, parser, AST and semantic checks
        |
        v
Deterministic module objects and typed interface contracts
        |
        v
VISTA-Link contract checker
        |
        v
Canonical linked workflow IR
        |---------------------------|
        v                           v
CLI reference evaluator      HTML/JavaScript generator
        |                           |
        |-----------|---------------|
                    v
       Predicate-induced route cases
                    |
                    v
       Cross-target result comparison
                    |
                    v
       Conformance certificate (.vcert)
                    |
                    v
        Independent certificate checker
```

The existing compiler remains the single source of parsing and field semantics. VISTA-Link adds the smallest required structures around that pipeline rather than introducing a second compiler architecture.

## Source Language Extensions

### Module definition

```vista
module Applicant {
    field category : choice(general, reserved) {
        label "Applicant category"
        required
    }

    export category
}
```

A module contains ordinary VISTA field and validation declarations. An exported field becomes part of the module's typed public contract.

### Module imports

```vista
module Certificate {
    import Applicant category as applicant_category

    enter when applicant_category == reserved

    field certificate : file {
        label "Category certificate"
        required
    }
}
```

An import identifies:

- The providing module.
- The exported field name.
- A local alias.
- The expected field type inferred from its uses and checked by the linker.

Imported fields are read-only within the receiving module.

### Workflow definition

```vista
workflow ScholarshipApplication {
    use "applicant.vista" as Applicant
    use "certificate.vista" as Certificate

    start Applicant

    route Applicant -> Certificate
        when Applicant.category == reserved
}
```

A workflow declares module instances, one start module, and conditional routes between modules.

### Completion semantics

VISTA-Link will not introduce a second completion-rule language. A module is complete when all fields currently visible in that module satisfy the existing requiredness and validation rules. Workflow completion occurs when the current module is complete and no outgoing route condition is true.

If multiple outgoing routes are simultaneously true and no explicit priority exists in the language, linking fails as ambiguous. Silent source-order priority is not allowed.

## Module Objects and Contracts

Each independently compiled module produces a deterministic module object. The initial format may be a canonical line-oriented text format to avoid adding a JSON dependency.

The object records:

- Format version.
- Module name.
- Source-content hash.
- Exported field names and types.
- Imported names, providers, aliases, and required types.
- Entry condition.
- Fields and normalized validation rules.
- Stable rule identifiers.
- Canonical module-object hash.

Equivalent source compiled twice with the same compiler version must produce byte-identical module objects.

Stable rule identifiers will be derived from module identity, declaration kind, declared name, and canonical expression position. They must not depend on pointer addresses or nondeterministic container iteration.

## Linker Responsibilities

The linker receives the workflow declaration and compiled module objects. It must:

1. Resolve every workflow module reference.
2. Resolve imports against exported fields.
3. Check import and export type compatibility.
4. Reject missing, duplicate, or ambiguous symbols.
5. Validate the start module.
6. Validate route source and destination modules.
7. Detect module-import cycles that cannot be evaluated safely.
8. Detect unconditional route cycles and statically evident non-terminating cycles.
9. Detect ambiguous simultaneously enabled outgoing routes.
10. Report unreachable modules.
11. Produce one deterministic linked workflow IR.

The linked IR contains fully qualified names so that fields such as `Applicant.category` cannot collide with fields from another module.

## All-Type Behavioral Partitioning

Exhaustively testing every integer, decimal, date, or string is impossible. Instead, VISTA-Link derives a finite quotient from the predicates appearing in the linked workflow. Two concrete values belong to the same class when every relevant VISTA predicate produces the same result for both values.

### Boolean fields

Cases:

```text
false
true
```

### Choice fields

One case is generated for every declared option. Invalid choice input remains covered by the existing validation-error path.

### Integer fields

All integer constants appearing in relevant comparisons are collected and sorted. Cases represent:

- Each constant.
- Values immediately below and above boundaries when representable.
- Non-empty intervals between adjacent constants.
- The regions below the smallest and above the largest constant.

For `age >= 18 && age < 60`, the behavioral classes are:

```text
age < 18
age = 18
18 < age < 60
age = 60
age > 60
```

Only one deterministic representative is required from each non-empty class.

### Decimal fields

Decimal literals used by certified predicates must use canonical checked decimal comparison rather than target-dependent floating-point formatting. The partition uses the same ordered-boundary method as integers.

If a decimal value exceeds the supported canonical representation, certification reports `CERT_UNSUPPORTED` rather than silently rounding it.

### Date fields

Dates are normalized to the existing VISTA date representation. Constants induce:

- Exact boundary dates.
- Dates before and after each boundary when representable.
- Non-empty intervals between boundaries.

### Text fields

For equality and inequality predicates, cases include:

- Empty text.
- Every literal occurring in a relevant predicate.
- One deterministic `OTHER` representative distinct from every listed literal.

If ordered text comparison is supported by the current expression semantics, literals also induce lexicographic intervals. Arbitrary regex and future text functions are unsupported until they receive a sound partition rule.

### File fields

VISTA exposes file presence, not file contents. The certified classes are:

```text
missing
present
```

File-content predicates are outside the language and outside the certificate claim.

### Partition completeness rule

Every operator accepted in a route, visibility, requiredness, or validation expression must have a registered partition rule. If any relevant expression contains an operator without one, certification stops with `CERT_UNSUPPORTED` and identifies the rule.

This makes the certificate complete relative to the current VISTA expression language without claiming completeness for arbitrary future programs.

## Route Exploration

The certificate generator considers only fields that influence:

- Module entry.
- Workflow routing.
- Visibility.
- Requiredness.
- Validation.
- Completion.

It constructs combinations from their behavioral classes and evaluates reachable workflow states. Irrelevant fields are excluded.

The explorer records:

- Input-class selections.
- Active module.
- Visible and required fields.
- Validation result.
- Selected next route.
- Completion result.
- Triggered rule identifiers.

Previously visited canonical states are not expanded again. A state includes the active module and the relevant normalized field classes, preventing endless exploration of repeated workflow cycles.

## Cross-Target Conformance

For each generated route case, VISTA-Link executes:

1. The existing C++ CLI validation and workflow evaluator.
2. A machine-readable probe exposed by the generated JavaScript evaluator.

The browser UI and probe must call the same generated JavaScript validation functions. The probe must not contain a second handwritten interpretation of the rules.

The compared observations are:

- Active module.
- Visible fields.
- Required fields.
- Validation success or failure.
- Validation diagnostic identifiers.
- Selected route.
- Completion state.

A certificate passes only when all compared observations match for every generated case.

## Conformance Certificate

The `.vcert` file is deterministic and machine-readable. It records:

- Certificate-format version.
- Compiler version.
- Workflow name.
- Source and module hashes.
- Linked-workflow hash.
- Participating rule identifiers and hashes.
- Behavioral partitions.
- Number of generated cases.
- Number of reachable states and routes.
- CLI result digest.
- JavaScript result digest.
- Overall status: `PASS`, `FAIL`, or `UNSUPPORTED`.
- Counterexample details when status is `FAIL`.
- Unsupported rule details when status is `UNSUPPORTED`.

The certificate does not claim that arbitrary browser engines, modified generated files, or modules with different hashes were verified.

## Independent Certificate Checker

`vista-cert-check` is a small separate executable. It validates:

- Certificate syntax and version.
- Required sections.
- Source, module, rule, and linked-workflow hashes when corresponding files are supplied.
- Partition coverage metadata.
- Case-count and result-digest consistency.
- Overall conformance status.

It must reject malformed, truncated, stale, or hash-mismatched certificates. It must not trust a plain `PASS` string without validating the recorded evidence.

## Counterexample Generation

When a mismatch is found, VISTA-Link reports a respondent trace containing only the assignments and transitions needed to reproduce the disagreement.

The initial implementation uses deterministic breadth-first exploration. Therefore, the first divergent trace is shortest by number of workflow transitions. Unused field assignments are removed from the displayed witness when doing so preserves the mismatch.

Example:

```text
FAIL: generated JavaScript differs from CLI
Module: Eligibility
Rule: Eligibility.age-rule
Shortest witness:
    age = 18
CLI result: eligible
JavaScript result: ineligible
```

## Diagnostics and Failure Policy

Diagnostics will follow the existing VISTA diagnostic style. The implementation will define exact numeric suffixes without reusing existing meanings.

Required diagnostic categories are:

- `MOD`: malformed module declarations, imports, or exports.
- `LNK`: missing modules, unresolved imports, type mismatches, cycles, ambiguity, or unreachable modules.
- `CERT`: unsupported predicates, execution failures, malformed evidence, hash mismatches, or target divergence.

The system is fail-closed:

- Link errors produce no linked workflow output.
- Certificate execution errors do not produce `PASS`.
- Unsupported semantics produce `UNSUPPORTED`.
- Target disagreement produces `FAIL` and a witness.
- Only complete successful comparison produces `PASS`.

## Backward Compatibility

Existing single-form VISTA files remain valid. When no module or workflow declaration is present, the current compilation path and command-line behavior remain unchanged.

The modular pipeline reuses existing field declarations, expression nodes, semantic checks, runtime validation, and HTML rendering. Unrelated refactoring is not part of this feature.

## Implementation Phases and End-to-End Gates

### Phase 0 — Protected baseline

Work:

- Preserve the current compiler in the baseline branch and commit.
- Perform implementation on a separate `codex/` feature branch.
- Record the result of all existing tests before product-code changes.

End-to-end gate:

```text
Existing source -> scanner -> parser -> semantics -> HTML -> CLI validation
```

All current tests must pass unchanged.

### Phase 1 — Module and workflow syntax

Work:

- Add `module`, `import`, `export`, `workflow`, `use`, `start`, `route`, and `enter` syntax.
- Extend the existing AST with module, workflow, and route nodes.
- Preserve the old form grammar entry path.

End-to-end tests:

1. Parse two valid modules and one workflow.
2. Reject duplicate exports.
3. Reject malformed routes.
4. Compile an existing single-form source successfully.
5. Confirm that parsed names and expressions reach the AST correctly.

Exit criterion: syntax and AST representation work without performing linking.

### Phase 2 — Typed module contracts

Work:

- Compile each module independently.
- Normalize its public contract and rules.
- Emit deterministic module objects and hashes.

End-to-end tests:

```text
applicant.vista   -> applicant.vobj
certificate.vista -> certificate.vobj
```

Compile each module twice and require byte-identical objects and hashes. Reject invalid local semantics before object emission.

Exit criterion: a module can be checked and compiled without loading the entire workflow.

### Phase 3 — Contract-checked linking

Work:

- Resolve workflow module uses and typed imports.
- Check route and dependency structure.
- Emit a deterministic linked workflow IR.

End-to-end tests:

1. Link a valid three-module workflow.
2. Reject a missing export.
3. Reject an imported type mismatch.
4. Reject a duplicate module alias.
5. Reject an invalid start module.
6. Reject a cross-module dependency cycle.
7. Diagnose an unreachable module.
8. Reject simultaneously enabled ambiguous routes.

Exit criterion:

```text
module objects + workflow -> validated linked workflow
```

### Phase 4 — All-type route-space construction

Work:

- Implement predicate-induced classes for Boolean, choice, integer, decimal, date, text, and file-presence semantics.
- Generate deterministic representatives.
- Reject any operator lacking a sound partition rule.

End-to-end tests:

1. Route on every supported field type in one workflow.
2. Exercise values below, at, between, and above numeric boundaries.
3. Exercise dates before, on, between, and after date boundaries.
4. Exercise empty text, known literals, and `OTHER` text.
5. Exercise missing and present files.
6. Generate the route set twice and require identical ordering and cases.

Exit criterion: every current route predicate is partitioned exactly or explicitly reported as unsupported.

### Phase 5 — Dual-target execution

Work:

- Evaluate generated cases through the C++ CLI path.
- Generate a JavaScript conformance probe that calls the real generated validation functions.
- Compare canonical observations.

End-to-end gate:

```text
behavioral case
    |-- C++ CLI evaluator ------> canonical observation
    `-- generated JavaScript ---> canonical observation
                                  |
                                  v
                               compare
```

Tests must compare active module, visibility, requiredness, validation, diagnostics, next route, and completion.

Exit criterion: the same inputs can be executed automatically against both actual targets.

### Phase 6 — Certificate and independent verification

Work:

- Emit deterministic `.vcert` evidence.
- Implement `vista-cert-check`.
- Validate hashes, coverage metadata, digests, and status.

End-to-end tests:

1. Generate and independently verify a passing certificate.
2. Change a module after certification and detect the hash mismatch.
3. Corrupt a certificate and reject it.
4. Truncate a certificate and reject it.
5. Repeat certification and require byte-identical output.

Exit criterion:

```text
workflow -> HTML + .vcert -> independent checker -> VERIFIED
```

### Phase 7 — Mutation and counterexample testing

Work:

- Add controlled target mutations only inside the test harness.
- Confirm that certification detects each mutation.
- Confirm shortest deterministic witness reporting.

Required mutations:

1. Reverse a Boolean condition.
2. Omit one choice branch.
3. Change an integer or decimal boundary.
4. Change a date comparison.
5. Mishandle empty text.
6. Treat a missing file as present.

Exit criterion: every mutation produces `FAIL`, identifies the responsible rule or observation, and includes a reproducible shortest trace.

### Phase 8 — Final regression and teacher demonstration

Run:

- Existing VISTA tests.
- Module-parser tests.
- Module-object determinism tests.
- Linker success and rejection tests.
- All-type partition tests.
- CLI/JavaScript equivalence tests.
- Certificate-verifier tests.
- Mutation tests.
- One complete demonstration workflow.

Demonstration flow:

```text
Applicant module
        |
        v
Eligibility module
        |
        v
Conditional Certificate module
        |
        v
Linked HTML + CLI validator + verified certificate
```

The demonstration must include at least one conditional route, one cross-module imported value, all field-type partition categories across the test suite, a successful certificate, and one intentionally mutated target that produces a minimal counterexample.

## Verification Matrix

| Requirement | Positive evidence | Negative evidence |
|---|---|---|
| Backward compatibility | Existing form compiles and validates | Existing regression failure blocks phase |
| Typed contracts | Correct import links | Wrong type is rejected |
| Determinism | Repeated outputs are byte-identical | Nondeterministic ordering fails test |
| Route completeness | All registered predicate classes explored | Unknown operator yields `UNSUPPORTED` |
| Cross-target agreement | CLI and JavaScript observations match | Mutated JavaScript yields `FAIL` |
| Artifact integrity | Current hashes verify | Modified module or certificate is rejected |
| Counterexample quality | Reproduction follows reported trace | Longer trace is rejected when a shorter one exists |

## Reversion and Commit Strategy

The protected pre-feature baseline is commit:

```text
510059d9c7878e322e6cb5c4cd0822d42861f199
```

It is available through branch:

```text
codex/vista-link-baseline-2026-10-05
```

Implementation will use a separate `codex/` feature branch. Each phase will end in a focused commit only after its end-to-end gate passes. This allows reverting one incomplete phase without discarding previously verified phases.

## Scope Boundary

The certificate claim is deliberately precise:

> For the recorded source hashes, compiler version, supported VISTA predicates, and generated behavioral partition, the recorded CLI evaluator and generated JavaScript evaluator produced identical canonical workflow observations for every behaviorally distinct input class.

It is not a claim of patentability, global uniqueness, arbitrary program equivalence, browser-engine correctness, or correctness after generated artifacts are modified. The research contribution is the combination of typed modular linking, compiler-derived finite behavioral partitioning, actual cross-target execution, independently verifiable evidence, and minimal divergent respondent traces within the VISTA domain.
