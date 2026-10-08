# VISTA-Link teacher demonstration

VISTA 0.8.0 links typed modules and compares the real C++ and generated-JavaScript evaluator on a complete bounded source-derived suite. Node.js 18+ is required for certification and generated-target tests; compilation, workflow generation, CLI validation and the independent checker remain Node-free. Local tests use Node v22.23.3, not a separate Node 18 compatibility run.

All-type tests derive three smaller source fixtures from the original module declarations: numeric/date (896 cases), textual/format (1216), and choice/file (280). Every group retains Boolean routing and the Finish module. Their field-declaration union must equal the original ten field declarations. The combined original fixture's 124416 assignments exceed unchanged limits and explicitly test deterministic CERT003/UNSUPPORTED rejection. Group completeness does not claim exhaustive acceptance of that over-budget full Cartesian product.

The generated-target check executes 3613 canonical C++/JS cases in total, including those 2392 grouped cases, the 33-case original two-module workflow, scalar/format/byte adversaries and one concrete full-source combined case. It also checks default Unicode mode for well-formed byte inputs and embedded browser handlers; this is not a real-browser or screen-reader audit.

## One-command demonstration

Run from this implementation checkout in WSL:

```bash
cd "/mnt/c/Users/prana/.codex/worktrees/vista-link/Compiler Design"
make
make vista-link-demo
```

The source `examples/vista_link/scholarship-demo.workflow.vista` uses three modules:

```text
Applicant -- category chosen --> Eligibility -- reserved only --> Certificate
                                general: completes here
```

Applicant exports required `category: choice { general, reserved }`. New Eligibility imports it into a supported rule and requires integer age 18–120. Certificate keeps its original imported-category entry and required file field. The existing two-module `scholarship.workflow.vista` fixture and both old module files are unchanged.

The target writes `out/vista-link-demo/valid/workflow.vcert`, verifies it with named artifacts, validates general and reserved sample data, creates a separate generated-JS mutant changing minimum age 18 to 19, and certifies it through `--javascript`. The mutant must exit 1 with FAIL and nonempty counterexample/observations; that expected detection makes the demonstration target succeed. Operational or unexpected mutant results fail the target.

## Editable CLI inputs

After the target runs, show/edit `out/vista-link-demo/general.data`:

```text
Applicant.category = general
Eligibility.age = 18
```

General input completes at Eligibility with trace `Applicant->Eligibility`. Reserved input in `out/vista-link-demo/reserved.data` is:

```text
Applicant.category = reserved
Eligibility.age = 18
Certificate.certificate = certificate.pdf
```

It completes at Certificate with trace `Applicant->Eligibility Eligibility->Certificate`.

```bash
./build/vista examples/vista_link/scholarship-demo.workflow.vista --validate-data out/vista-link-demo/general.data
./build/vista examples/vista_link/scholarship-demo.workflow.vista --validate-data out/vista-link-demo/reserved.data
```

Change age to 17, use a malformed integer, or remove the reserved certificate value to demonstrate validation failure. A file field is any nonempty string; no disk-file existence, contents or upload is checked. Rerunning the demo overwrites its two sample data files, so copy them elsewhere to retain teacher edits.

## Individual stages and artifacts

```bash
./build/vista examples/vista_link/applicant.vista --emit module --out-dir out/contracts/applicant
./build/vista examples/vista_link/eligibility.vista --emit module --out-dir out/contracts/eligibility
./build/vista examples/vista_link/certificate.vista --emit module --out-dir out/contracts/certificate
./build/vista examples/vista_link/scholarship-demo.workflow.vista --emit linked --out-dir out/linked
./build/vista examples/vista_link/scholarship-demo.workflow.vista --emit partitions
./build/vista examples/vista_link/scholarship-demo.workflow.vista --emit workflow --out-dir out/workflow
./build/vista examples/vista_link/scholarship-demo.workflow.vista --emit certificate --out-dir out/cert
./build/vista-cert-check out/cert/workflow.vcert --root .
./build/vista examples/vista_link/scholarship-demo.workflow.vista --emit certificate --out-dir out/supplied --javascript out/workflow/workflow.js
```

| Mode | Files / output |
| --- | --- |
| module | `module.vobj`: canonical LF source, typed imports/exports, source/object SHA-256 |
| linked | `workflow.ir`: owned typed linked rules and module/workflow hashes |
| partitions | Console classes, coordinated assignments, reachable step/whole cases and completeness diagnostics |
| workflow | `workflow.ir`, `workflow.js`, standalone `workflow.html` |
| certificate | Those three artifacts plus `workflow.vcert`; local `probe.js`, `probe.out`, `probe.err` |
| validate-data | Console active state, visible/required/valid fields, route, completion, rules and diagnostics |
| vista-cert-check | VERIFIED plus recomputed hashes when named files are checked |

Module paths resolve relative to the workflow source, not the terminal directory. `.vobj` can replace a `.vista` in a workflow `use`; the linker validates its digests and reparses/recompiles its embedded source contract. No edits to existing fixtures are needed for this demo.

Routes accept exported file-presence conditions such as `route A -> B when present(A.attachment)`. Missing and supplied-empty values are absent; any nonempty value is present. Local/imported `present(attachment)` and contextual identifiers remain supported. The focused `make test-vista-link-final-fixes` target checks this source-to-Node/certificate path, filesystem aliases, read failures and numeric admission; it requires Node and is included in the explicit target gate.

`workflow.js` exports `vistaEvaluate(input, activeModule?, options?)` and `vistaCanonicalObservation(result)`. Omit activeModule for full traversal; supply an alias for one step. Input values are strings. Default Unicode mode converts values to UTF-8 bytes; `{inputMode:'bytes'}` accepts Latin-1 containers for lossless arbitrary-byte testing. Browser handlers use this same generated evaluator. The checker does not execute this API.

## Meaning and limits of evidence

PASS records agreement over the complete generated finite suite of declared workflow fields, supported scalar/predicate classes and reachable states. Both whole traversal and intermediate steps execute. It is not proof of arbitrary supplied-JavaScript equivalence for all inputs, arbitrary unknown-key maps, every browser behavior, global originality or cryptographic authenticity. Inputs/observations and named files have deterministic unsigned hashes; an adversary can rewrite evidence self-consistently.

FAIL with CERT004 records the first mismatch by stable shortest-prefix ordering, a deterministic sorted assignment-deletion reduction preserving that state/prefix, and both actual canonical observations. Reduction is not a global search for the smallest possible values. A reachable nonterminating cycle produces incomplete FAIL with CERT002; it need not contain a semantic mismatch witness.

CERT001 (unknown exact rule) and CERT003 (resource exhaustion) produce UNSUPPORTED with incomplete coverage, never PASS. All field types—text, textarea, email, phone, integer, decimal, Boolean, date, choice and file—have supported validation classes. The shared ceiling is 100000 generation/exploration work states and 6400000 retained payload bytes, with guarded DFA allocation, 256 depth/component dimensions and 4096-byte numeric construction. Scalar numeric input has 4096 bytes and absolute exponent/normalized scale 100000 bounds. Conservative construction limits can reject a workflow before 100000 route states; these limits are not subprocess timeouts.

Supported-language inspection applies the canonical numeric parser to every source numeric literal, including constant-only predicates, entry/check/field rules and rules not reached at runtime. Unrepresentable literals produce CERT001/UNSUPPORTED; agreement on invalid source semantics cannot PASS. Supported respondent conversion/validation failures remain ordinary certifiable observations.

The independent executable links only checker, certificate codec, hash and diagnostic sources. It validates canonical syntax, complete recorded case/manifest consistency, matching observation hashes and named identities. With `--root .`, it reopens all recorded source/module/object/generated files and recomputes hashes. It does not parse source semantics, recreate partitions, rebuild source contracts, rerun either evaluator or authenticate execution. Without `--root` it checks recorded consistency only.

Version 1 records raw absolute source/object paths, preserving byte spelling and filesystem-significant symlink/`..` components. `--root` must be an existing directory; it enables file checks and neither relocates absolute inputs nor confines reads beneath root. Generated sibling paths resolve relative to the certificate. Raw CRLF input hashes differ from canonical LF module-source hashes. Writes/reads are sequential, not atomic snapshots.

Supplied JavaScript executes as trusted local Node code with the process's access. There is no sandbox or subprocess deadline; nonterminating code can block indefinitely. Do not use this path to execute untrusted third-party targets.

## Exits and cleanup

| Exit | Meaning |
| --- | --- |
| 0 | Successful operation; PASS certificate or VERIFIED checker |
| 1 | Source/validation failure, FAIL or UNSUPPORTED certificate, malformed/rejected evidence or changed/missing recorded artifact |
| 2 | Usage, input/output identity overlap, or operational file/runtime/probe error, including module/data reads |

Every emitting mode checks the top-level source and declared module input filesystem identities against its exact touched outputs before cleanup or writes, including hardlinks, symlinks and parent components. Overlap refusal leaves inputs and outputs untouched. Certificate mode captures supplied JS before cleanup, so intentional `--javascript DIR/workflow.js` snapshots remain supported. Exception-safe source/module/JS/data reads return operational exit 2 and clear stale certificate evidence when cleanup succeeds; malformed source/object/data remains exit 1. Scanner/parser/linker failure clears stale final workflow artifacts when cleanup succeeds; runtime/malformed-probe failure leaves no final certificate, but generated IR/JS/HTML/probe diagnostics can remain for investigation. No recursive user-output directory deletion occurs. Filesystem refusal prevents a universal cleanup guarantee; concurrent file mutation and atomic publication are outside this contract. `make clean` separately removes this checkout's `build` and `out`; preserve wanted files there before using it.

## Verification and deterministic rebuild

```bash
make clean
make
make test
make test-vista-link-targets
make vista-link-demo
git diff --check
```

Ordinary `make test` stays Node-free. The explicit target runs codegen comparisons, certificate/checker adversaries, six real mutations (Boolean, choice, numeric/date boundaries, supplied-empty text, missing file) and actual C++/JS replay, followed by `bash tests/test_pipeline.sh --vista-link`. That explicit pipeline compiles three module objects and compares two fresh output directories byte-for-byte for each object, linked IR, generated JS and certificate, along with partition output. It validates both CLI/JS demo paths and known-artifact cleanup. Missing Node is a failure, never a silent skip. The original two-module workflow enumerates 33 cases; the new three-module teacher certificate records 219 cases (129 step, 90 whole). Historical pre-presence counts are not current counts.

## Local WSL runtime

This worktree retains an ignored, verified Node v22.23.3 runtime and wrapper:

```bash
bash .superpowers/sdd/2026-10-05-vista-link/runtime/with-node.sh node --version
bash .superpowers/sdd/2026-10-05-vista-link/runtime/with-node.sh make test-vista-link-targets
bash .superpowers/sdd/2026-10-05-vista-link/runtime/with-node.sh make vista-link-demo
```

It adjusts PATH only for the child command. It is local recovery material, not committed setup or a production Makefile path; another checkout needs Node 18+ on PATH. Keep ignored runtime and RED/GREEN evidence through handoff; cleanup or relocation requires a separate decision.

## Rollback and review

Implementation remains on `codex/vista-link`. Protected branch `codex/vista-link-baseline-2026-10-05` points to `510059d9c7878e322e6cb5c4cd0822d42861f199`; the original main checkout remains available. Inspect with native Windows Git (`git show codex/vista-link-baseline-2026-10-05:README.md`) or recover into a separate checkout after preserving current changes. Do not reset or overwrite this worktree to demonstrate rollback. No merge/push is part of this task. Task review spans `316dc2d` through final Task 9 HEAD; main coordinates subsequent reviews.
