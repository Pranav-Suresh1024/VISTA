# VISTA-Link decisions — accepted Task 6 checkpoint

Source checkpoint: `c292b60eec92cead5f203241145c3b5f64b86ef6`, branch `codex/vista-link`, 2026-10-06. The current program version is **0.7.0**, from [include/version.hpp](include/version.hpp). Tasks 1–6 are implemented and accepted; Tasks 7–9 are planned, not implemented here. See [flow.md](flow.md) for executable order.

## Provenance and recovery

This is a **retrospective reconstruction**, written after Task 6 acceptance. Reasons and rulings come from the local execution ledger and relevant task-report decision sections; implementation statements were checked against tracked source. It does not claim that undocumented alternatives were considered. Local provenance is under `.superpowers/sdd/2026-10-05-vista-link/`: `progress.md`, `semantic-preflight.md`, and `task-1-report.md` through `task-6-report.md`. These coordination files are ignored, so durable behavior and reasons are summarized here rather than requiring those files in another checkout.

| Checkpoint | Completed commits recorded in the ledger | Evidence in tracked files |
| --- | --- | --- |
| Protected original baseline | `510059d9c7878e322e6cb5c4cd0822d42861f199` | Existing form pipeline and regression scripts |
| Task 1: parser and compatibility | `6165a7e`, corrected by `69814b7` | [parser test](tests/test_vista_link_parser.sh), [semantic test](tests/test_semantic.sh) |
| Task 2: canonical module objects | `6f780ee`, corrected by `247cd24` | [module test](tests/test_vista_link_module.sh), [hash/object unit check](tests/test_hash_unit.cpp) |
| Task 3: typed linker | `e2ed2c5081916547bac58a5d19ffc9260fca5803` | [linker test](tests/test_vista_link_linker.sh) |
| Task 4: canonical evaluator | `1c5e81343e1471c4484c364499deacab3b551fed` | [evaluator test](tests/test_vista_link_evaluator.sh), [scalar check](tests/test_evaluator_scalar.cpp) |
| Task 5: exact behavior partitions | `0b2789766c04624ef2ffa9d8b153618cc314681e` | [partition test](tests/test_vista_link_partition.sh), [partition unit check](tests/test_partition_unit.cpp) |
| Task 6: executable targets | `5f21c00`, corrected by `c292b60eec92cead5f203241145c3b5f64b86ef6` | [generated-target comparisons](tests/test_vista_link_codegen.sh), [Makefile](Makefile) |

**Isolation decision (setup, ledger provenance).** Work started from `f30c348f87b108725935c53cefd7c73017d0915f`, which includes the approved design and plan beyond the protected baseline. The worktree at `C:\Users\prana\.codex\worktrees\vista-link\Compiler Design` protects the original checkout at `C:\Users\prana\OneDrive\Documents\ChatGPT\Compiler Design` and allows separate, reviewable checkpoints. Recovery should first preserve all current working changes and other contributors' work. Inspect or recover a checkpoint in a separate checkout, or make an explicitly scoped follow-up change; a checkpoint is not permission to discard current files. This documentation checkpoint changes no source and performs no restoration.

## Implemented decisions and consequences

### 1. Reuse the existing language tooling; keep new keywords contextual

**Task 1, retrospectively sourced from its report and review corrections.** [src/vista.l](src/vista.l) and [src/vista.y](src/vista.y) extend the existing Flex/Bison grammar; [src/parser.cpp](src/parser.cpp) still feeds positioned tokens into `yyparse`. [Makefile](Makefile) keeps C++17, strict warnings and the existing toolchain. This avoids a second scanner/parser or runtime dependency.

The grammar's `identifier` production accepts the added keyword tokens where names are legal, preserving ordinary forms that already used those words as identifiers. `ParseResult` selects one arena-owned form, module or workflow root; `present(file)` is a unary expression with file-only typing in [src/semantic.cpp](src/semantic.cpp). Duplicate exports remain parser-owned `MOD003`, avoiding duplicate diagnostics during module compilation. The consequence is a shared grammar with contextual naming rules, rather than globally reserving every new word.

### 2. Canonical source and objects carry deterministic identity

**Task 2, including its newline/name-check correction.** [src/module.cpp](src/module.cpp) uses `normalize_newlines` to convert CRLF and lone CR to LF before source hashing/embedding. `compile_module` reuses semantic analysis, verifies local exports and sorts typed contracts. `object_prefix` writes a counted source payload; `sha256_hex` in [src/hash.cpp](src/hash.cpp) hashes the canonical source and object prefix separately. `serialize_module_object` appends the object digest.

`load_module_object` requires format version 1, valid digest syntax, exact counts, canonical newlines, no trailing bytes, matching source/object hashes and canonical reserialization; failures are `MOD004`. In the linker, recompilation of embedded source must also reproduce the object contract. Thus a matching digest alone cannot substitute for a source/contract check. Newline normalization deliberately makes equivalent line-ending encodings share source identity; it is not a raw-file hash policy or a signature.

Standalone compilation defers unresolved **declared imports** and a potentially contextual choice name immediately paired with an import in an equality comparison. It still checks unrelated names recursively. Imported types/options become known at link time; standalone object success does not establish fully resolved workflow typing.

### 3. Exports define the typed public contract; linked IR owns its values

**Task 3, including the controller's exports-only ruling.** `link_workflow` in [src/linker.cpp](src/linker.cpp) resolves imports against provider exports, carries exact field types/choice options into read-only imported symbols, and reruns `analyze_semantics(module, imports)`. Internal rules see locals and resolved imports; workflow route predicates see exported qualified fields only. This preserves module boundaries: a private field needed by a route must be exported.

`lower_module` and `lower_expression` copy expressions, constraints, checks and presentation data into [include/workflow.hpp](include/workflow.hpp). Names map to provider-qualified fields; contextual choice literals remain literals even when they collide with field names. No AST pointers survive in `LinkedWorkflow`, so evaluation/emission remain valid after parser arenas die. This costs copies but eliminates lifetime coupling. Display order remains in field vectors, while `workflow_prefix` sorts a copy for canonical serialization and hashing; stable rule IDs identify observations independently of presentation.

`load_module` supports `.vista` and `.vobj` relative to the workflow file. Its `SourceFile` adapter temporarily writes normalized/validated source to reuse the file-only scanner. Temporary paths never enter artifact identity; a writable system temporary directory is required.

### 4. Routes have no priority; reject ambiguity and nontermination

**Tasks 3–5, report and preflight provenance.** Link-time checks reject duplicate aliases, invalid endpoints/start, missing exports, imported type errors, import cycles, unconditional route cycles, unreachable modules and multiple unconditional outgoing routes. Literal `true` counts as unconditional. Conditional overlaps/cycles remain for evaluation and exploration, because structural linking cannot decide their enabled behavior.

`evaluate_workflow_step` in [src/evaluator.cpp](src/evaluator.cpp) evaluates every outgoing predicate after entry, even when field validation already blocks advancement. Multiple enabled routes produce `LNK008`; there is no first-match rule. Whole traversal detects a repeated active module for fixed inputs as `LNK006`. [src/partition.cpp](src/partition.cpp) reports a reachable nonterminating input class as incomplete `CERT002`, even if other classes terminate. The consequence is explicit rejection rather than silently selecting a route or dropping cyclic cases.

### 5. Share exact scalar semantics and preserve byte behavior

**Task 4; extended to generated targets in Task 6.** [src/validation.cpp](src/validation.cpp) adapts legacy fields/expressions and delegates conversion, constraints and expression execution to [src/evaluator.cpp](src/evaluator.cpp). `CanonicalDecimal` is sign, digit string and scale; `parse_decimal`, `parse_integer` and `compare_decimal` avoid floating-point rounding and exponent expansion. Numeric input is bounded to 4096 bytes and absolute exponent/normalized scale 100000. Failed conversion gives `VAL005`; an unrepresentable runtime constraint gives `VAL011`. Compile-time constraint admission in [src/semantic.cpp](src/semantic.cpp) still uses its existing `parse_finite_number`/`long double` checks; exact runtime comparison does not imply that every bounded decimal spelling is admitted by the compiler.

Text lengths count `std::string` bytes, including UTF-8 bytes for ordinary Unicode text. Email/phone matching preserves the existing byte languages and whole-value matching; missing/empty numeric values keep zero for comparison, missing Boolean is false, and required Boolean false remains missing. `present(file)` means any nonempty supplied string, without inspecting file contents. Data-file loading trims unquoted surrounding whitespace; direct evaluator inputs do not. Both Boolean operands execute, so invalid data cannot disappear through Boolean short-circuiting. Hidden local checks skip, while supplied hidden-field conversion errors remain observable.

### 6. Observe intermediate states as well as final traversal

**Tasks 4–6, controller preflight ruling.** `evaluate_workflow_step` observes exactly one active module. Success means no diagnostics; `complete` means terminal success, not merely a valid advancing step. `evaluate_workflow` follows transitions with fixed inputs, retains the last selected route even if the destination fails, accumulates rule IDs/trace and returns the last state's field flags. Final-only comparison would miss earlier optional visibility/requiredness changes.

`RouteCase.active_module` in [include/partition.hpp](include/partition.hpp) distinguishes reachable steps from whole cases (empty active module). Step cases carry the prefix reaching that state; whole cases carry the traversal trace. `canonical_observation` sorts field flags, preserves diagnostic multiplicity and associations, deduplicates rule IDs and preserves transition order in a byte-counted format. Human message wording is outside this canonical observation contract.

### 7. Coordinate relational witnesses and string cells; bound all generation work

**Task 5, report completeness rationale and preflight provenance.** Independent scalar intervals can miss `x < y` or shared string equality. `derive_partitions` in [src/partition.cpp](src/partition.cpp) groups related fields, shares numeric/date pools across each component and deduplicates assignments by actual evaluator signatures. For N related fields, `number_pool` retains up to N integer ranks per open boundary interval and exact terminating midpoints for decimal subgaps; `date_pool` uses valid day ranks. This preserves feasible equality/order patterns within supported construction limits without an SMT dependency.

`string_pool` combines literal singletons, byte-length cells and email/phone format cells. A product DFA over all 256 byte values counts candidates with saturation at N plus excluded literals, then deterministically extracts up to N distinct nonliteral witnesses, or all when fewer exist. Shared cells retain enough distinct values for equality/inequality patterns; one generic OTHER string would not. `signature` uses actual conversion, constraint and primitive-expression outcomes, including rule activation. The implementation rationale is recorded retrospectively; this is not a claim of an external proof audit.

`enumerate_route_cases` combines coordinated assignments and explores reachable states with fixed input classes, retaining step and whole cases and shortest prefixes. Its budget inherits generation usage: 100000 work states, 6400000 retained payload bytes, guarded DFA allocation, depth/component dimensions of 256 and 4096-byte expanded numeric construction. Conservative limits can reject workflows before 100000 reachable states. `CERT001` means no registered exact rule, `CERT002` a reachable nonterminating cycle, and `CERT003` resource exhaustion; all leave coverage incomplete. Partial output is never complete evidence. Existing form hidden-required analysis reuses these partitions through `analyze_hidden_required`; unsupported exact rules map to `ANL900`, while concrete witnesses remain `ANL003`.

### 8. Generate one evaluator for browser and Node, with explicit byte transport

**Task 6 and accepted correction.** [src/codegen.cpp](src/codegen.cpp) emits linked tables plus `vistaEvaluate`, which shares `vistaStep` for whole and explicit-state modes. `generate_workflow_html` embeds the exact generated JavaScript artifact and adds input/render/progression handlers. The browser calls the same evaluator with its active module; Node receives `vistaEvaluate` and `vistaCanonicalObservation` via `module.exports`.

Default Unicode values pass through `vistaBytes`/`TextEncoder` once. Probe mode `{inputMode:'bytes'}` accepts one Latin-1 code unit per byte, preserving arbitrary C++ case bytes without replacement decoding. Both modes reach the same scalar/expression/state implementation. Source literals/options are emitted as byte escapes. Input-mode/value validation throws `TypeError` for malformed API inputs. `vistaByteOrder`, added in `c292b60`, orders observation strings and unknown input names by UTF-8 bytes, correcting JavaScript UTF-16 ordering differences.

Workflow numeric controls use text plus input hints to preserve precision/exponents. The form uses `novalidate`; VISTA observations govern progression. Label/help/error associations, live status and focus changes are implemented, but native input sanitization and real-browser/screen-reader behavior are not proven by a Node DOM stub. File handling reads the selected filename only; there is no upload or persistence. Escaping protects authored HTML/script text, and runtime messages use `textContent`.

Workflow output clears the three known stale artifacts before compilation and attempts cleanup after a failed write, preventing old success artifacts from surviving a failed request. This is bounded file cleanup, not recursive directory removal; filesystem refusal still prevents a guarantee of complete cleanup.

### 9. Keep ordinary regression tests Node-free

**Task 6 review ruling, implemented in `c292b60`.** [Makefile](Makefile) runs ordinary checks through `make test`; actual generated-target comparisons require explicit `make test-vista-link-targets`. This preserves the compiler's optional-Node runtime boundary, while requiring users to run both targets for cross-target coverage. The local Task 6 report records 2939 canonical C++/generated-JS comparisons and embedded-handler progression, plus ordinary tests with Node absent. These are prior implementation results, not new runs at this docs-only checkpoint.

## Claim and future-work boundaries

There is no AI/ML component. Exact partition completeness is relative to supported VISTA predicates, validation semantics and successful bounded derivation. Finite source-derived comparisons do not prove arbitrary supplied JavaScript equivalent for every input or every browser interaction. Unsigned digest evidence can be regenerated and is not authenticity or tamper-proof execution evidence.

Tasks 7–9 are **planned, not implemented**: certificate generation, an independent checker and mutation demonstration. No certificate CLI, checker executable, certificate PASS or future call order is asserted here. Artifact path/hash metadata and future certificate hash policies remain controller-coordinated work; current `LinkedModule` carries source/object hashes but no input paths.
