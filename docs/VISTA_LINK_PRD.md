# VISTA-Link Product Requirements Document

**Product:** VISTA-Link

**Target release:** VISTA 0.8.0

**Date:** 2026-10-05

**Status:** Approved scope, pending implementation

## Product Summary

VISTA-Link extends the VISTA form compiler from single forms to modular multi-step form workflows. It lets a developer compile modules independently, connect them through typed imports and exports, generate a browser workflow, validate the same workflow through the command line, and obtain a machine-checkable certificate showing that both generated targets behave identically over every behaviorally distinct supported input.

## Problem

Large forms are difficult to maintain as one source file. Splitting a workflow into files introduces new risks:

- A module may import a field that does not exist or has the wrong type.
- Routes may contain cycles, unreachable modules, or ambiguous destinations.
- The CLI validator and browser JavaScript may interpret the same rule differently.
- Ordinary example-based testing cannot try every integer, decimal, date, or text value.
- A successful compiler run does not provide portable evidence of what was checked.

VISTA-Link must address these risks without replacing the working VISTA compiler or adding AI/ML components.

## Primary Users

### Form workflow developer

Creates reusable VISTA modules, links them into a workflow, generates HTML, and needs actionable compiler diagnostics.

### Reviewer or instructor

Needs a clear demonstration that module contracts were checked and that the CLI and generated browser target agree.

### Certificate verifier

Receives generated artifacts and needs to detect stale sources, modified JavaScript, malformed evidence, or target disagreement without trusting a printed `PASS` message.

## Product Goals

1. Make modular VISTA workflows practical and type-safe.
2. Preserve existing single-form behavior and commands.
3. Cover every current VISTA field type in conformance analysis.
4. Compare the actual C++ CLI behavior with the actual generated JavaScript behavior.
5. Produce deterministic evidence that can be checked by a separate executable.
6. Give the shortest reproducible respondent trace when targets disagree.

## Non-Goals

- AI or machine-learning integration.
- Patentability or global-uniqueness claims.
- General-purpose program equivalence.
- Arbitrary user-defined functions or regular expressions.
- Inspection of uploaded file contents.
- Dynamic runtime module loading.
- A new virtual machine, bytecode format, or SMT solver.
- Cloud submission, storage, accounts, or analytics.

## Core User Stories

### Modular authoring

As a workflow developer, I can export fields from one module and import them into another so that the linker rejects missing or incompatible contracts before generating output.

### Workflow routing

As a workflow developer, I can declare a start module and conditional routes so that VISTA generates one coherent multi-step workflow.

### Complete supported-case analysis

As a reviewer, I can see that Boolean, choice, number, date, text, email, phone, textarea, and file-presence behavior was covered without relying on random samples.

### Cross-target certification

As a reviewer, I can generate a certificate showing that the CLI evaluator and generated JavaScript returned the same canonical result for every derived case.

### Independent verification

As a verifier, I can run a separate checker that rejects modified modules, JavaScript, malformed certificates, incomplete evidence, or mismatched observations.

### Actionable failure

As a developer, when the targets disagree, I receive a minimal input and route trace that reproduces the difference.

## Functional Requirements

### FR-1: Source modules

- The language shall support `module`, `import`, `export`, and `enter when` declarations.
- The language shall support the built-in Boolean expression `present(file_field)` for file-presence rules and routes.
- A module shall reuse existing VISTA field, property, expression, and check syntax.
- Imported fields shall be read-only.
- Duplicate or missing exports shall be rejected.

### FR-2: Workflow source

- The language shall support `workflow`, `use`, `start`, and `route ... when ...`.
- Paths in `use` declarations shall resolve relative to the workflow source.
- A workflow shall have exactly one valid start module.
- Module aliases shall be unique.

### FR-3: Typed linking

- Imports shall resolve to exported fields with their exact field type and choice options.
- The linker shall reject unresolved contracts, incompatible uses, invalid endpoints, cycles covered by the specification, unreachable modules, and ambiguous enabled routes.
- Linked names shall be fully qualified.
- Linked output shall be deterministic.

### FR-4: Completion

- A module shall complete only when its visible required fields and active checks pass.
- If exactly one outgoing route is true, it shall become the next module.
- If no outgoing route is true, the workflow shall complete.
- If multiple outgoing routes are true, evaluation shall fail as ambiguous.

### FR-5: Behavioral partitions

- Boolean fields shall cover `false` and `true`.
- Choice fields shall cover every option and invalid choice validation.
- Integer and decimal fields shall cover exact constants and non-empty ordered regions induced by predicates and constraints.
- Date fields shall cover exact boundary dates and non-empty intervals.
- Text and textarea fields shall cover empty text, compared literals, length boundaries, and an `OTHER` class.
- Email and phone fields shall additionally cover valid and invalid format classes.
- File fields shall cover missing and present states.
- Unsupported operators shall produce `UNSUPPORTED` with a rule identifier.

### FR-6: Dual-target evaluation

- The compiler shall evaluate every generated case through the C++ workflow evaluator.
- The generated browser UI and Node probe shall call the same generated JavaScript evaluator.
- Both targets shall report canonical active module, field visibility, requiredness, validation, diagnostics, selected route, completion, and rule IDs.

### FR-7: Certificate

- Certificate status shall be `PASS`, `FAIL`, or `UNSUPPORTED`.
- `PASS` shall require a complete partition suite and matching observations for every case.
- The certificate shall record compiler version, source/module/workflow/JavaScript hashes, partitions, case count, and per-case observation hashes.
- `FAIL` shall include a reproducible shortest divergent trace.
- Repeated equivalent builds shall produce byte-identical certificates.

### FR-8: Independent checker

- A separate `vista-cert-check` executable shall parse and validate the certificate.
- It shall validate format, hashes, unique and complete cases, and matching target evidence rather than trusting status text.
- With a supplied root directory, it shall reject changed or missing artifacts.

### FR-9: Backward compatibility

- Existing `.vista` form sources and commands shall remain valid.
- Existing generated single-form HTML and CLI reports shall not change unless required by a shared correctness fix.
- Existing regression tests shall continue to pass.

## Command-Line Experience

```bash
# Existing form commands remain valid
./build/vista examples/valid_scholarship.vista --emit html --out-dir out/form

# Compile one module object
./build/vista examples/vista_link/applicant.vista --emit module --out-dir out/applicant

# Inspect the linked workflow
./build/vista examples/vista_link/scholarship.workflow.vista --emit linked --out-dir out/workflow

# Inspect derived behavioral partitions
./build/vista examples/vista_link/scholarship.workflow.vista --emit partitions

# Generate workflow HTML and JavaScript
./build/vista examples/vista_link/scholarship.workflow.vista --emit workflow --out-dir out/workflow

# Generate the conformance certificate
./build/vista examples/vista_link/scholarship.workflow.vista --emit certificate --out-dir out/workflow

# Verify the certificate independently
./build/vista-cert-check out/workflow/workflow.vcert --root .
```

## Output Artifacts

| Artifact | Purpose |
|---|---|
| `module.vobj` | Deterministic independently compiled module contract and source payload |
| `workflow.ir` | Canonical linked workflow representation |
| `workflow.html` | Browser-facing modular workflow |
| `workflow.js` | Actual generated evaluator used by browser and certificate probe |
| `workflow.vcert` | Machine-readable conformance evidence |

## Quality Requirements

- C++17 compilation with `-Wall -Wextra -Wpedantic -Werror`.
- No third-party C++ dependencies.
- Node.js 18+ required only for certificate execution.
- Author-provided text must remain safely escaped in HTML and JavaScript.
- Invalid source, linking, partitioning, runtime, or probe states must fail closed.
- Artifact ordering and hashes must be deterministic.
- Route exploration must terminate through canonical-state deduplication.
- Exploration above 100000 states must return `UNSUPPORTED`, not an incomplete `PASS`.

## Acceptance Criteria

The release is accepted when:

1. A three-module workflow compiles, links, generates HTML/JavaScript, certifies, and verifies.
2. Existing VISTA regression tests pass.
3. All current field types appear in behavioral-partition tests.
4. Missing exports, wrong types, cycles, unreachable modules, and ambiguous routes are rejected.
5. Repeated clean builds produce byte-identical objects, IR, JavaScript, and certificates.
6. A modified source or generated JavaScript invalidates verification.
7. Six controlled target mutations are detected.
8. Every detected target disagreement includes a reproducible shortest trace.
9. Unsupported semantics and the state ceiling cannot produce `PASS`.
10. The teacher demonstration shows one verified workflow and one detected mutation.

## Delivery Phases

| Phase | Deliverable | Release gate |
|---|---|---|
| 0 | Protected baseline | Existing test suite passes |
| 1 | Module/workflow grammar and AST | Valid and invalid syntax tests pass |
| 2 | Deterministic module objects | Repeated objects are byte-identical |
| 3 | Typed linker and canonical IR | Contract and graph failures are rejected |
| 4 | Canonical workflow evaluator | CLI executes complete routes correctly |
| 5 | All-type behavioral partitions | Every supported predicate is exact or unsupported |
| 6 | Browser/Node JavaScript target | Actual generated evaluator matches expected cases |
| 7 | Certificate generation | Complete evidence produces deterministic status |
| 8 | Independent verification | Stale and malformed evidence is rejected |
| 9 | Mutation and full regression | Valid target verifies; all mutations fail with witnesses |

## Risks and Mitigations

| Risk | Mitigation |
|---|---|
| Cartesian case growth | Include only behavior-relevant fields, explore reachable states, deduplicate states, and fail unsupported at 100000 states |
| Decimal differences between C++ and JavaScript | Use canonical string-based decimal comparison in both targets |
| Browser and probe drift | Generate one JavaScript evaluator used by both |
| False confidence from a `PASS` string | Require per-case evidence and independent verification |
| Stale or modified artifacts | Record and recompute SHA-256 hashes |
| Existing compiler regressions | Run the full existing suite after every implementation task |
| Node unavailable | Keep normal compilation functional and return command error only for certificate mode |

## Claim Boundary

VISTA-Link certifies the recorded sources, compiler version, supported VISTA predicates, generated behavioral partitions, and recorded CLI/JavaScript artifacts. It does not claim arbitrary program equivalence, browser-engine correctness, patentability, global uniqueness, or correctness after an artifact changes.

## Rollback

The pre-feature compiler is preserved at commit `510059d9c7878e322e6cb5c4cd0822d42861f199` on branch `codex/vista-link-baseline-2026-10-05`. Implementation shall occur on a separate `codex/vista-link` branch with one verified commit per phase.
