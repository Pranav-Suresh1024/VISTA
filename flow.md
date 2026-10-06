# VISTA executable flow — Task 7 implementation checkpoint

Accepted Tasks 1–6 were verified at `c292b60eec92cead5f203241145c3b5f64b86ef6`; this Task 7 implementation starts from `3130c4a0fd24c7042e43a852603c22df60dbfdb1` on `codex/vista-link`, 2026-10-06. [include/version.hpp](include/version.hpp) now reports **0.8.0**. Controller acceptance of Task 7 is separate. [decisions.md](decisions.md) records rationale and checkpoint provenance.

## CLI dispatch and shared parsing

[src/main.cpp](src/main.cpp), `main`, handles `--help`, `--version` and `--list-templates` before source loading. Otherwise it accepts `vista SOURCE --emit MODE` or `vista SOURCE --validate-data FILE`. `--out-dir DIR` is required for `html`, `all`, `module`, `linked`, `workflow` and `certificate`; console modes are `tokens`, `ast`, `symbols`, `dependencies`, `graph`, `diagnostics` and `partitions`. Certificate mode also accepts `--javascript FILE`. There is no module-step CLI flag: step evaluation is a C++/generated-JS API.

```text
main
  directory mode: prepare_output_directory -> mode-specific stale cleanup
  scan_file(source_path)                       [src/vista.l]
  format_tokens / scanner diagnostics
  tokens mode returns here
  parse_tokens(tokens)                        [src/parser.cpp]
    yyparse(context)                          [generated from src/vista.y]
      yylex(semantic_value, location, context) [token adapter in src/parser.cpp]
    duplicate module-export check (MOD003)
  format_ast(form | module | workflow)         [src/ast.cpp]
  ast mode returns here
  dispatch by emit mode and parsed root
```

`scan_file` invokes the generated Flex scanner; `yylex` above instead adapts its collected tokens for Bison, including decoded string values and source positions. [include/parser.hpp](include/parser.hpp) requires exactly one root for successful parsing. [src/vista.y](src/vista.y) adds module/workflow roots, qualified names and file presence while retaining contextual identifiers.

Exit codes are 0 for success, 1 for language/validation/incomplete-suite failure and 2 for usage or filesystem failures. Not every I/O error has the same dispatch result: a module-load failure arrives as a linker diagnostic and returns 1. Scanner diagnostics stop before parsing; parse diagnostics stop before semantic/link work. Root/mode checks happen after parsing; unsupported non-form combinations can reach the generic modules/workflows mode error.

## Ordinary form path

```text
main, parsed FormAst
  analyze_semantics(form) -> SemanticAnalyzer::run     [src/semantic.cpp]
  format_symbol_table; symbols mode returns
  analyze_dependencies(form, semantic_result)         [src/analysis.cpp]
    per field: collect_references -> analyze_hidden_required
    detect_cycles (after all fields)
  format_dependencies / format_dependency_graph_dot
  dependencies or graph mode returns
  --validate-data: validate_data_file -> format_validation_report
  diagnostics mode returns
  generate_html(form, semantics, analysis)             [src/codegen.cpp]
  write_artifact(form.html), plus demonstration files in all mode
```

`all` writes token/AST/symbol/dependency/graph/diagnostic artifacts as their stages run; success also writes `form.html`. Stage diagnostics stop later work and, in `all`, populate `diagnostics.txt`. [src/analysis.cpp](src/analysis.cpp), `analyze_hidden_required`, constructs a small owned workflow from the target/referenced fields, calls `lower_expression`, then `derive_partitions` and `enumerate_route_cases`. It checks raw requiredness versus visibility using shared evaluator functions and returns `ANL003` with a concrete witness. Missing exact partition rules become `ANL900`; resource/cycle diagnostics remain incomplete analysis. This supersedes the earlier blanket numeric-witness limitation.

[src/validation.cpp](src/validation.cpp), `validate_data_file`, calls `load_input_values(path, false)` for unqualified form names. Its `scalar_field` and `scalar_expression` adapters feed shared `convert_value`, `validate_constraints` and `evaluate_expression` in [src/evaluator.cpp](src/evaluator.cpp). It preserves legacy line/order/report handling and skips checks referring to hidden fields. Ordinary `generate_html` remains a separate legacy generator; workflow cross-target checks do not establish equivalence of its JavaScript.

## Module object emission

```text
main, --emit module --out-dir DIR, parsed ModuleAst
  reopen source in binary mode
  compile_module(module, source_text)          [src/module.cpp]
    normalize_newlines -> sha256_hex           [src/hash.cpp]
    analyze_semantics(module)                  [src/semantic.cpp]
    resolve local exports; sort contracts
    object_prefix -> sha256_hex
  serialize_module_object(object)
    object_prefix -> append prefix digest
  write_artifact(DIR, module.vobj, bytes)
```

Data flow: AST + original source bytes become a `ModuleObject` containing canonical LF source, source digest, sorted imports and typed exports, and an object digest. Standalone import types are deferred in the semantic module overload; unrelated invalid names still fail. Duplicate exports are already rejected by `parse_tokens`. `load_module_object` checks counts, digests, canonical bytes and exact reserialization when an object is consumed by the linker; it is not called by the emission branch.

## Workflow loading, typing and lowering

The `linked`, `workflow`, `partitions` and workflow `--validate-data` branches all call [src/linker.cpp](src/linker.cpp), `link_workflow(ast, source_path)`.

```text
link_workflow
  duplicate-alias checks before module I/O
  load_module(use, workflow_path.parent_path(), ...), per use
    read .vista or .vobj
    .vobj: load_module_object -> embedded canonical source
    .vista: normalize CRLF/CR to LF
    SourceFile::write -> scan_file -> parse_tokens
    compile_module
    .vobj: compare recompiled and loaded serialize_module_object bytes
  validate start; resolve exported contracts and imported aliases
  has_cycle(import graph); endpoint/unconditional/structural reachability checks
  analyze_semantics(module, resolved imports), per module
  lower_module -> lower_expression
  build exported-only route symbol/name maps
  analyze_semantics(route condition scope, exported symbols)
  lower_expression(route predicate); assign stable route rule IDs
  sort routes; workflow_prefix -> sha256_hex
```

`.vista`/`.vobj` use paths resolve relative to the workflow directory. `SourceFile` removes its own temporary source/directory on scope exit. Object digest validation is followed by source reparse/recompile, so forged headers with internally consistent hashes still fail if contracts disagree with source.

The result is a value-owned `LinkedWorkflow` in [include/workflow.hpp](include/workflow.hpp): typed expressions, qualified fields/imports/exports, entry/visibility/requiredness/check/route rules, constraints, presentation metadata and module hashes. Local/import rules can use resolved provider fields; routes can use only exports. `format_linked_workflow` calls `workflow_prefix` and appends a canonical workflow digest to the text IR. Canonical serialization sorts copies, avoiding accidental changes to display order. Neither link nor code generation invokes route-case enumeration automatically.

## Canonical evaluation and validation

Workflow CLI validation loads qualified assignments with `load_input_values(FILE)`, then calls `evaluate_workflow`. Loader diagnostics are appended to the evaluation and force `complete=false` on load failure; `format_observation` prints the result followed by diagnostic messages. A missing data file returns 2.

```text
evaluate_workflow(workflow, inputs)             [src/evaluator.cpp]
  loop: evaluate_workflow_step(workflow, inputs, active)
    convert_value for all fields; collect exported runtime values
    unknown input names / active-module check
    establish active local + resolved import scope
    evaluate_expression(entry); false/invalid -> VAL010 and return
    each active field:
      evaluate_expression(visibility, requiredness)
      convert_value; visible requiredness; validate_constraints
    active checks: hidden_reference -> skip or evaluate_expression
    all outgoing routes: evaluate_expression in exported scope
    >1 true route -> LNK008; errors block selection
    no errors: 0 routes -> complete; 1 route -> selected transition
    normalize observation
  repeated active module -> LNK006
  accumulate rules/trace; retain last selected route; stop or follow destination
```

Conversion uses `parse_integer`, `parse_decimal`, `valid_iso_date`, choice membership and email/phone byte languages. `validate_constraints` and numeric comparisons use `compare_decimal`; no numeric scalar is rounded to floating point here. `evaluate_expression` evaluates both Boolean operands before combining validity. Text lengths are bytes; file presence is any nonempty string; required Boolean false is missing. Conversion errors for inactive fields are ignored until they matter through active rules/imports/routes. Supplied active hidden-field conversion errors still fail validation, although constraints/requiredness depend on visibility.

`evaluate_workflow_step` returns this state's fields and outgoing transition only. `evaluate_workflow` returns the final/failing state's fields, accumulated rules/trace and retained last transition. A valid advancing step has no diagnostics but `complete=false`. [include/evaluator.hpp](include/evaluator.hpp) defines both APIs and observations. `canonical_observation` copies and normalizes field flags, diagnostic codes/associations and rule IDs, preserves trace order and emits counted strings. Messages are excluded; diagnostic multiplicity is retained.

## Exact partitions and reachable cases

```text
main, --emit partitions
  link_workflow
  derive_partitions(workflow)                  [src/partition.cpp]
    inspect entry/field/check/route expressions -> primitive atoms
    group related fields into components; constants_in
    number_pool / date_pool / string_pool
    per-field classes, including missing/empty/invalid where applicable
    enumerate coordinated assignments -> signature deduplication
  enumerate_route_cases(workflow, partitions)
    inherit generation budget; combine component assignments
    queue start states, keyed by active module + input-class indices
    evaluate_workflow_step for each first reachable state
    evaluate_workflow once per input class
    retain step cases, whole cases and prefixes; enqueue selected destination
    repeated state -> incomplete CERT002
    sort by prefix length; assign deterministic case IDs
  format_partitions(partitions, suite); return 0 only if suite.complete()
```

`number_pool` shares exact ranks/midpoints across compared integer/decimal fields; `date_pool` shares valid day ranks. `string_pool` uses `transition`/`format_cell` over a finite byte DFA and saturated cardinalities, preserving literals, length/format cells and multiple shared equality witnesses. `signature` uses shared conversion/constraints/primitive evaluation rather than an independent approximation. [include/partition.hpp](include/partition.hpp) stores coordinated `PartitionComponent.assignments`; displayed scalar classes alone are not the case product.

A nonempty `RouteCase.active_module` selects a reachable step with the prefix leading to it; empty selects whole traversal and its expected trace. Inputs remain fixed across a case's transitions. Unsupported rules (`CERT001`), cycles (`CERT002`) and work/storage exhaustion (`CERT003`) leave coverage incomplete. The shared limits are 100000 work states, 6400000 retained bytes, guarded DFA construction, depth/component dimensions 256 and expanded numeric construction 4096 bytes. Generation and exploration share the budget; they do not each get a fresh ceiling. Partial cases may be displayed but cannot establish complete coverage.

## Workflow IR, JavaScript and HTML emission

```text
main, --emit workflow --out-dir DIR
  link_workflow
  generate_workflow_javascript(linked.workflow)       [src/codegen.cpp]
    workflow_expression / workflow_strings / workflow_string
    emit linked tables + one evaluator implementation
  generate_workflow_html(linked.workflow, javascript)
    embed the exact javascript string + browser handlers
  format_linked_workflow
  write_artifact workflow.ir -> workflow.js -> workflow.html
  any write failure: clear_workflow_artifacts; return 2
```

`--emit linked` instead writes only `workflow.ir`. Workflow emission neither derives partitions nor issues a conformance certificate. HTML escaping and script-safe string/byte escaping protect authored text. All three contents are prepared before the first workflow write; writes are sequential, not an atomic transaction.

### The generated evaluator and input transport

These functions are generated inside [src/codegen.cpp](src/codegen.cpp), not separate repository JavaScript modules:

```text
vistaEvaluate(input, activeModule?, options?)
  validate object/string values and inputMode
  unicode/default: vistaBytes -> TextEncoder UTF-8 byte containers
  bytes: validate Latin-1 code units 0..255 -> preserve supplied bytes
  explicit activeModule: vistaStep once
  omitted activeModule: whole traversal loop around vistaStep

vistaStep
  vistaValue -> vistaDecimal / vistaDate
  vistaExpression -> vistaCompare for numeric comparisons
  entry -> fields/constraints -> checks -> routes -> vistaNormalize

vistaCanonicalObservation(result)
  copy + vistaNormalize -> counted canonical observation
```

Both transport modes use the same `vistaStep` and scalar/expression implementation. Values are not implicitly trimmed. Byte mode is for arbitrary case bytes; default/browser values are Unicode normalized to UTF-8 bytes. `vistaByteOrder` uses UTF-8 byte ordering for canonical fields, codes, diagnostic tuples, unique rules and unknown-name diagnostic ordering, including Unicode names where UTF-16 order differs. Scalars already use byte containers. CommonJS `module.exports` exposes `{vistaEvaluate, vistaCanonicalObservation}`; requiring a generated `workflow.js` in Node needs no DOM or package.

### Browser handlers

`generate_workflow_html` initializes `vistaActiveModule` at the start alias and calls `vistaUpdate`. `input`/`change` events invoke `vistaUpdate`: `vistaReadInput` reads all controls, then `vistaEvaluate(input, vistaActiveModule)` feeds `vistaRender`. Values from hidden/previous modules remain in the input; Boolean controls yield string `true`/`false`, file controls yield filename or empty, and other controls yield `.value`.

The submit handler prevents native submission, evaluates the current step, and on a selected route updates the active module, evaluates/renders the destination and focuses its heading. Otherwise it renders errors/completion and focuses a visible invalid field where available. `vistaRender` sets visibility, disabled/required flags, ARIA state, error text and status from observations. `novalidate` prevents native validation from bypassing VISTA progression. Native controls may still sanitize input; numeric text controls preserve large/exponent spellings. No server, upload, file-content inspection or storage occurs.

## Errors and stale-output cleanup

[src/main.cpp](src/main.cpp) performs directory preparation and cleanup before scanning: `clear_generated_artifacts` removes `module.vobj` for module mode, `form.html` for HTML, or seven named demonstration files for `all`; linked mode removes `workflow.ir`; `clear_workflow_artifacts` attempts all of `workflow.ir`, `workflow.html`, `workflow.js`. Thus missing source, scanner/parser/semantic/link failure or wrong root cannot leave earlier successful workflow output when cleanup succeeds.

`remove_artifact` reports filesystem errors. Workflow write failures attempt all three removals, including a partly written file. Removal refusal is an explicit failure and may leave files; output is not atomic and no directory is recursively deleted. Legacy `all` may leave already-written diagnostic/intermediate artifacts from the failing attempt. [src/diagnostic.cpp](src/diagnostic.cpp), `format_diagnostic`, supplies positioned scanner/parser/semantic/link messages; workflow validation prints its observation plus messages instead of using that stderr path.

## Test entry points and evidence boundary

[Makefile](Makefile) builds `build/vista` with C++17, Flex and Bison. Its shell tests require the POSIX build/test environment; committing uses native Windows Git. Task 7's local report records current final gates; the accepted Task 6 counts below remain historical provenance.

| Entry point | Current execution |
| --- | --- |
| `make test` | Node-free: scalar unit check; scanner/parser; VISTA-Link parser/module/linker/evaluator/partition; semantic/analysis; legacy codegen/Phase 2/templates/validation/pipeline scripts |
| `make test-vista-link-targets` | Explicit Node-dependent [tests/test_vista_link_codegen.sh](tests/test_vista_link_codegen.sh) and [tests/test_vista_link_certificate.sh](tests/test_vista_link_certificate.sh); depends on the compiler target |
| Focused VISTA-Link checks | `bash tests/test_vista_link_parser.sh`, `test_vista_link_module.sh`, `test_vista_link_linker.sh`, `test_vista_link_evaluator.sh`, `test_vista_link_partition.sh`, or `test_vista_link_codegen.sh` using the same `tests/` prefix |
| Unit entry points | `make test-hash-unit`, `make test-evaluator-scalar`; partition shell test builds/runs `build/test-partition-unit` |

The generated-target script emits real artifacts, compiles a temporary C++ probe against the actual parser/linker/evaluator/partition/codegen sources and transports expected canonical bytes and inputs as hex. Node converts values losslessly with `Buffer.from(hex,'hex').toString('latin1')`, calls the exported byte-mode evaluator, and compares canonical observations byte for byte. Well-formed Unicode inputs also exercise default transport. It executes actual embedded scripts with a small DOM stub, checks progression/escaping/determinism and stale-output failures. The accepted Task 6 report records **2939 comparisons**, including reachable steps and whole cases; this is saved implementation evidence, not a new docs verification run or a real-browser/screen-reader audit. Both ordinary and explicit targets are needed for regression plus generated-target coverage.

## Planned, not implemented at this checkpoint

Tasks 8–9 remain planned: an independent checker and mutation demonstration. No independent verification executable is implemented here. Unsigned hashes are not authenticity, and finite VISTA-derived comparisons do not prove arbitrary JavaScript equivalent for every input.

## Task 7 certificate flow

```text
main, --emit certificate --out-dir DIR [--javascript FILE]
  reject workflow input/output overlap
  read exact supplied JavaScript and workflow bytes before cleanup
  prepare directory -> remove stale vcert and workflow artifacts
  scan / parse -> read raw named module inputs -> link_workflow
  choose supplied bytes or generate_workflow_javascript
  generate_workflow_html -> format_linked_workflow
  write workflow.ir / workflow.js / workflow.html
  derive_partitions -> enumerate_route_cases
  certify_workflow                                  [src/certify.cpp]
    independently derive expected suite; validate completeness and identities
    retain scalar/component IDs, expected counts, canonical module hashes
    write exact workflow.js + deterministic probe.js
    one shell-quoted node batch -> probe.out / probe.err
      read target via raw Buffer path -> execute exported evaluator
      hex values -> Latin-1 -> vistaEvaluate(input, active?, byte mode)
      vistaCanonicalObservation -> case ID + hex observation
    validate all output IDs/counts/count-wire structure
    evaluate_workflow[_step] -> canonical_observation -> hash actual bytes
    first shortest-prefix mismatch -> sorted assignment deletion
      retain original state/prefix (whole: identical complete trace)
      probe candidates, retain only reproducing disagreements
    record witness + both canonical observations; coverage digest
  add absolute source/module paths preserving link/.. spelling, plus relative raw IR/HTML identities
  serialize_certificate                            [src/certificate.cpp]
  write workflow.vcert: PASS exit 0; mismatch/incomplete exit 1
  operational CERT006: exit 2, no final certificate
```

Input evidence is an ASCII hex wire containing whole/step mode, active module, counted prefix and sorted assignments. The certificate codec percent-encodes free text, sorts module/artifact/partition/case records and validates round-trip canonical syntax, unique identities, declared counts and input/coverage hashes. It links only certificate/hash/diagnostic code. `artifact` hashes raw files; canonical workflow prefix and canonical module hashes are separate. Generated sibling paths resolve relative to the certificate; absolute input paths preserve output-directory-independent identity, including non-UTF-8 bytes. Operational failures retain useful probe errors but clear the final certificate. Writes and input reads are not atomic; no runtime timeout is implemented. CERT001/CERT003 yield UNSUPPORTED; CERT002 cycle incompleteness yields FAIL with that diagnostic. This flow records finite source-derived agreement, not authenticity or arbitrary-JavaScript equivalence.
