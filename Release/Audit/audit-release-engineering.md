> Archived audit evidence from 2026-10-16. Follow the [sequential roadmap](<C:/Users/Example/Desktop/TurboMCP/Release/ROADMAP.md>) for execution. Historical parallel-agent instructions do not apply.

# Release engineering, API, tests, and trust audit

Audit date: 2026-10-16. This report uses source inspection and isolated checks. It does not certify runtime behavior.

## Snapshot

- Plugin HEAD: `f045c1313f1db04ddcac269d6910be9ad6befb11`.
- Engine HEAD: `396c9f059903aed5fec78ecd3d437a40c6415368`, local commit subject `5.8.3 release`.
- The engine reported no tracked modifications. Its version file says 5.8.3.
- The plugin had five modified tracked files and one untracked batch implementation. These changes predate this audit.
- The working header declares 101 tools: Read 30, Look 4, Preview 29, Edit 38.
- The runner discovers 74 suites. Its editor-free list contains 10 entries, including commandlet suites. Editor-free does not mean read-only.
- The source directory contains 205 files. This is an inventory, not a claim that every line received independent review.
- No editor process appeared in the initial process check. No full build or live editor suite ran during this audit.

## R01 — Release gate can report success without successful acceptance

**Priority: release blocker. Evidence: confirmed source defect, with isolated classifier reproduction.**

[Runner process handling:88–116](<C:/Users/Example/Desktop/TurboMCP/Tools/run-suites.ps1#L88-L116>) waits for a child but discards its exit code. [Verdict handling:343–357](<C:/Users/Example/Desktop/TurboMCP/Tools/run-suites.ps1#L343-L357>) uses printed text instead. The isolated real classifier accepted `RESULT: PASS` followed by a traceback as passing.

[Final status:600–619](<C:/Users/Example/Desktop/TurboMCP/Tools/run-suites.ps1#L600-L619>) increments a blocked count but exits nonzero only for failed suites. A run with blocked suites and zero failures exits zero. [Selection:395–401](<C:/Users/Example/Desktop/TurboMCP/Tools/run-suites.ps1#L395-L401>) only warns when no suite matches. It can produce a successful empty run. A partly misspelled selection silently omits unknown names.

**Required correction:** require a nonempty, fully matched selection. Preserve child exit codes. Require both an accepted structured verdict and exit zero. Return nonzero for required blocked, missing, or timed-out suites. Preserve each child's complete log.

**Acceptance:** test exit 7 after printed PASS, blocked-only runs, zero matches, one valid plus one invalid suite, timeout, and malformed verdict. None may count as acceptance success.

## R02 — Client retries mutations after ambiguous failures

**Priority: release blocker for the supplied client. Evidence: confirmed control flow and mocked-I/O reproduction.**

[Client retry:62–87](<C:/Users/Example/Desktop/TurboMCP/Tools/tmcp.py#L62-L87>) catches every exception, reconnects, and repeats the original request. This includes writes whose response disappeared after the editor changed the asset. [Transport:104–116](<C:/Users/Example/Desktop/TurboMCP/Tools/tmcp.py#L104-L116>) can fail on timeout, decoding, or a JSON-RPC error.

The [isolated probe](<C:/Users/Example/Desktop/TurboMCP/Release/Audit/audit_retry_probe.py>) used the real retry method and mocked I/O. One lost response caused two simulated effects and one reconnect. No editor mutation occurred in the probe.

**Required correction:** do not automatically replay a mutation after an ambiguous transport failure. Report `outcome_unknown`. Reconnect without replay. Add operation IDs and server-side deduplication if safe retries are required. Do not assume every Read/Look operation is side-effect-free.

**Acceptance:** discard a response after a committed edit. The client must not execute that edit twice. A caller must recover the outcome by operation ID or independent state inspection.

## R03 — Unfinished batch edit is publicly reflected

**Priority: release blocker if included. Evidence: working-tree source, not a committed release defect.**

The [batch implementation:66–81](<C:/Users/Example/Desktop/TurboMCP/Source/TurboMCP/TurboMCPBatchEditTools.cpp#L66-L81>) disables compile and save for each operation. It then returns `changed` without a final compile, save, or independent comparison. Its only supported kind is `graphRepoint` at lines 39–47.

All plans are computed before any operation applies. Later operations can depend on nodes that earlier operations remove or alter. The coordinator does not model those dependencies. On failure, line 77 prints `unchanged` even when the rollback result says `failed`. The reported rolled-back list omits the failing operation's possible partial mutation.

The [public declarations](<C:/Users/Example/Desktop/TurboMCP/Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L2074-L2076>) expose the feature. Static coverage found no suite calling either batch tool.

**Recommendation:** exclude this unfinished capability from the release candidate. Finish the transaction-free cores separately. Do not expand the batch to more operation kinds next week.

**Design issue:** an editor undo transaction cannot guarantee atomic writes across several package files. The batch board card combines one undo with save-all atomicity. That contract needs separate in-memory and disk failure semantics before implementation.

## R04 — Test gates are heuristic, not semantic coverage

**Priority: high. Evidence: measured checks and inspected implementation.**

Measured results:

| Check | Result | Meaning |
|---|---|---|
| Coverage audit | FAIL | Two new batch tools have no callers. The FunctionAdd parameter report also contains a parser false positive. |
| Suite-quality audit | FAIL, two candidates | Repoint and swap suites use checks that the heuristic does not recognize. They are not empty tests. |
| Plugin check | PASS, 11 checks | 1606 includes resolve. Engine API names and 101 reflected declarations pass this static checker. This is not a compile. |
| Renamed-plugin static test | PASS | Source contains expected lookup forms. No renamed install ran. |
| Nested-widget static test | PASS | Source and a text fixture contain expected traversal forms. No nested widget ran. |
| Git whitespace check | PASS | Tracked diff has no reported whitespace defect. |

[Coverage parsing:73–79](<C:/Users/Example/Desktop/TurboMCP/Tools/audit-tool-coverage.py#L73-L79>) takes the last whitespace token as a parameter name. Defaulted `Inputs` and `Outputs` become `TEXT("")`. This explains that failure without proving missing function coverage.

[Coverage matching:113–129](<C:/Users/Example/Desktop/TurboMCP/Tools/audit-tool-coverage.py#L113-L129>) can credit names or calls present in source without executing them. [Parameter matching:214–223](<C:/Users/Example/Desktop/TurboMCP/Tools/audit-tool-coverage.py#L214-L223>) accepts a matching key anywhere in a calling file. These checks cannot establish successful behavior, branch coverage, undo, rollback, or persistence.

[Quality heuristic:74–80](<C:/Users/Example/Desktop/TurboMCP/Tools/audit-suite-quality.py#L74-L80>) equates a list that receives append calls with a failure recorder. That is neither necessary nor sufficient.

The [nested-widget test:25–46](<C:/Users/Example/Desktop/TurboMCP/Tools/test-widget-tree-nested.py#L25-L46>) searches source strings and an authored text fixture. The [renamed-plugin test:25–40](<C:/Users/Example/Desktop/TurboMCP/Tools/test-traps-plugin-identity.py#L25-L40>) does the same. Keep these guards, but do not label them runtime acceptance.

**Required correction:** distinguish source guards, pure behavior tests, editor integration tests, and release acceptance. Measure assertions against independent state. Add negative controls for semantic defects, not only source shapes.

## R05 — Four toolset classes do not enforce read-only behavior

**Priority: high safety-contract defect. Evidence: confirmed architecture.**

The [README:35–37](<C:/Users/Example/Desktop/TurboMCP/README.md#L35-L37>) says readers are structurally unable to write. The [header:16–34](<C:/Users/Example/Desktop/TurboMCP/Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L16-L34>) offers class placement and code review as the mechanism.

All classes live in one editor module with unrestricted mutable engine APIs. A static member in a class named Read can load assets, call mutators, compile, execute Blueprint code, or change editor state. Naming a class does not establish a capability boundary.

The domain report identifies widget preview execution as a concrete counterexample to the stronger no-side-effect claim.

**Required correction:** describe these labels as contracts, not enforced isolation. Classify asset mutation, dirty-state changes, UI changes, script execution, filesystem writes, and network access separately. Test each claimed boundary.

## R06 — Tool failures can be MCP successes

**Priority: high for autonomous consumers. Evidence: confirmed source path, not live wire capture.**

TurboMCP returns `FString`, including strings beginning with `error:`. The [engine invocation:494–499](<C:/ExampleUnrealengine/Engine/Plugins/Experimental/ToolsetRegistry/Source/ToolsetRegistry/Private/ToolsetRegistry/ObjectFunctionToolCall.cpp#L494-L499>) serializes a normal return value as success. The [MCP adapter:258–267](<C:/ExampleUnrealengine/Engine/Plugins/Experimental/ModelContextProtocol/Source/ModelContextProtocolEditor/Private/ModelContextProtocolToolsetRegistryAdapter.cpp#L258-L267>) sends that value with `MakeTextResult`.

Therefore a plugin-level refusal string is not automatically a protocol-level error. The [supplied CLI:225–227](<C:/Users/Example/Desktop/TurboMCP/Tools/tmcp.py#L225-L227>) adds a textual error heuristic. Other clients do not inherit it. The batch's `unchanged\nrefused:` format does not match that heuristic either.

**Required correction:** define a versioned result envelope with explicit outcome, changed assets, warnings, diagnostics, rollback status, save status, and verification state. Keep text as a rendering. Integrate protocol error signaling where the engine API permits it. Until then, document exactly how clients must interpret the envelope.

## R07 — String-only inputs and incomplete help increase agent mistakes

**Priority: high usability and automation debt. Evidence: confirmed API.**

[Tool help declaration:102–109](<C:/Users/Example/Desktop/TurboMCP/Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L102-L109>) explains that optional strings appear required. Callers must send empty strings. Booleans, numbers, action enums, selectors, and nested operations frequently use strings rather than schema types.

[Help rendering:461–493](<C:/Users/Example/Desktop/TurboMCP/Source/TurboMCP/TurboMCPAgentHelp.cpp#L461-L493>) does print parameter types. Old feedback claiming it prints no types is too broad. It does not print required status, defaults, enums, or structured examples in this path.

[Tool index:276–303](<C:/Users/Example/Desktop/TurboMCP/Source/TurboMCP/TurboMCPAgentHelp.cpp#L276-L303>) lists all tools across all toolsets without a query, pagination, or overall budget. [Help limit:533–541](<C:/Users/Example/Desktop/TurboMCP/Source/TurboMCP/TurboMCPAgentHelp.cpp#L533-L541>) appends a warning when over budget instead of enforcing the promised limit.

**Required correction:** add bounded task-word search and schema-derived examples. Establish one casing and optional-argument rule. Give every action a valid example and a refusal example. Use typed schemas where possible. Do not launch a full API rewrite during the release week.

## R08 — Packaging dependencies remain incomplete

**Priority: release gate for clean installation. Evidence: confirmed declarations, build impact unmeasured.**

[Build rules:57–61](<C:/Users/Example/Desktop/TurboMCP/Source/TurboMCP/TurboMCP.Build.cs#L57-L61>) depend on `EditorScriptingUtilities`. [Build rules:148–160](<C:/Users/Example/Desktop/TurboMCP/Source/TurboMCP/TurboMCP.Build.cs#L148-L160>) depend on `PythonScriptPlugin`. Neither appears in the [plugin dependency list:27–43](<C:/Users/Example/Desktop/TurboMCP/TurboMCP.uplugin#L27-L43>).

Both modules belong to separate engine plugins: [EditorScriptingUtilities descriptor](<C:/ExampleUnrealengine/Engine/Plugins/Editor/EditorScriptingUtilities/EditorScriptingUtilities.uplugin>) and [Python descriptor](<C:/ExampleUnrealengine/Engine/Plugins/Experimental/PythonScriptPlugin/PythonScriptPlugin.uplugin>). The [test project:45–47](<C:/Users/Example/Desktop/TurboMCP/TestProject/TurboMCPTest.uproject#L45-L47>) explicitly enables Python, which can hide a standalone dependency gap.

Niagara is now declared. Do not repeat the old Niagara finding as an open code defect.

**Required correction:** either declare the remaining plugin dependencies or move fixture-only commandlets into a test module. Verify a minimal fresh project with only documented dependencies. Treat engine-installed and project-installed forms as separate installation tests.

## R09 — Setup is not a reproducible release install

**Priority: high adoption risk. Evidence: confirmed scripts and missing release evidence.**

[Setup:74–79](<C:/Users/Example/Desktop/TurboMCP/Tools/setup-engine.ps1#L74-L79>) defaults to the moving `release` branch. [Version handling:134–137](<C:/Users/Example/Desktop/TurboMCP/Tools/setup-engine.ps1#L134-L137>) prints the version but does not reject a mismatch there. Dependency readiness relies on a directory's existence at lines 144–149.

[Setup:170–176](<C:/Users/Example/Desktop/TurboMCP/Tools/setup-engine.ps1#L170-L176>) accepts an existing junction without checking whether it points to this repository. A user can build a different checkout while believing the new one is mounted.

[Setup:125–127](<C:/Users/Example/Desktop/TurboMCP/Tools/setup-engine.ps1#L125-L127>) recommends putting an access token in a Git URL. This can expose the token through shell history, configuration, logs, or process inspection. Use a credential manager instead.

The [README:89–93](<C:/Users/Example/Desktop/TurboMCP/README.md#L89-L93>) restricts support to a source-built Windows engine. That is an acceptable preview limitation, but it sharply limits the audience. The engine source build, disk requirements, account access, Python, and PowerShell requirements need a short verified onboarding procedure.

No tracked CI or release packaging file matched the audit's release-infrastructure query. This is not proof that external CI does not exist. No fresh-clone, independently provisioned release acceptance was available during this audit.

## R10 — Lifecycle scripts can stop unrelated editors

**Priority: high safety and test isolation risk. Evidence: confirmed fallback paths.**

[Runner recovery:177–181](<C:/Users/Example/Desktop/TurboMCP/Tools/run-suites.ps1#L177-L181>) stops all processes with editor and trace-server names after a failed health check. [Runner cleanup:217–225](<C:/Users/Example/Desktop/TurboMCP/Tools/run-suites.ps1#L217-L225>) also stops commandlets and trace servers by name.

An earlier owner check does not make these later global stop operations safe against a new process or a foreign editor. The board already records test failures caused by shared editor and build contention.

**Required correction:** one explicit owner for the project, engine target, port, fixture set, and process tree. Require a lease before build or live tests. Kill only owned descendants. Abort safely if ownership cannot be established. Give parallel agents separate worktrees and one serialized integration lane.

## R11 — Build success can still mean no plugin artifact

**Priority: high release gate defect. Evidence: confirmed source.**

[Build result:102–113](<C:/Users/Example/Desktop/TurboMCP/Tools/build.ps1#L102-L113>) prints that the plugin DLL is missing, then still returns PASS and exit zero. A timestamp is printed when present, but it is not tied to a source manifest or loaded editor process.

**Required correction:** fail if the expected artifact is absent. Record source commit, dirty diff hash, engine commit, target, toolchain, artifact hash, and loaded module identity. Do not require a timestamp change for a legitimate no-op build. Require artifact provenance instead.

## R12 — Logging is useful, but not a reliable mutation ledger

**Priority: medium, high if used as recovery evidence. Evidence: confirmed source.**

Logging starts off in [settings:24–39](<C:/Users/Example/Desktop/TurboMCP/Source/TurboMCP/Public/TurboMCPSettings.h#L24-L39>). That is a sensible privacy default. [Serialization:101–107](<C:/Users/Example/Desktop/TurboMCP/Source/TurboMCP/TurboMCPCallLog.cpp#L101-L107>) writes complete arguments and results when present. Users need a retention and redaction warning.

[File writes:185–213](<C:/Users/Example/Desktop/TurboMCP/Source/TurboMCP/TurboMCPCallLog.cpp#L185-L213>) do not check delete, move, or save results. Rotation can silently fail. A single oversized record can exceed the claimed approximate file ceiling. [Write ordering:223–232](<C:/Users/Example/Desktop/TurboMCP/Source/TurboMCP/TurboMCPCallLog.cpp#L223-L232>) writes the file before the in-memory mutex. Concurrent safety requires proof of callback threading or a lock around rotation and append.

A call log records requests and responses. It does not independently prove which nodes changed or whether an undo restored them. Do not use it as the sole recovery ledger.

## R13 — Security boundary must be local and explicit

**Priority: release documentation and configuration gate. Evidence: inspected defaults, not a penetration test.**

The [HTTP listener default:10–21](<C:/ExampleUnrealengine/Engine/Source/Runtime/Online/HTTPServer/Private/HttpServerConfig.h#L10-L21>) binds localhost. The engine MCP implementation contains Origin validation. These are real positive controls. This audit does not claim that the server is exposed to the network by default.

The test project [automatically starts MCP](<C:/Users/Example/Desktop/TurboMCP/TestProject/Config/DefaultEditorPerProjectUserSettings.ini#L10-L30>). The inspected settings and dispatch path do not provide per-client authorization for TurboMCP edits. Any authorized local tool caller can reach mutating operations. Host approval settings are not plugin-side enforcement.

**Required release policy:** trusted local clients only. No public exposure, reverse proxy, or port forwarding without a separate authentication design. Document that tool calls can modify valuable project assets and that widget preview can execute project code. Treat project-authored text as untrusted data, not agent instructions. Verify actual socket binding on the release machine.

No full secret-history scan, hostile-input fuzzing, crash fuzzing, or network security test ran in this audit.

## R14 — Documentation and board state are not release evidence

**Priority: high process defect. Evidence: complete non-archived metadata inventory and document comparison.**

The [README:3–10](<C:/Users/Example/Desktop/TurboMCP/README.md#L3-L10>) claims 93 tools and a 58-suite measured release. The same document later describes 65 suites. Current discovery yields 101 and 74. The [handover:3–4](<C:/Users/Example/Desktop/TurboMCP/HANDOVER.md#L3-L4>) says the worktree is clean. It is not clean in this snapshot.

The [working plan:49–55](<C:/Users/Example/Desktop/TurboMCP/Spec/WORKING_PLAN.md#L49-L55>) repeats old counts. [Row 1.3](<C:/Users/Example/Desktop/TurboMCP/Spec/WORKING_PLAN.md#L91>) contains conflicting trailing states. Large historical narratives occupy fields that should contain one current verdict.

Board revision 1632 contains 111 non-archived cards: 59 todo, 20 backlog, 32 done. Six done cards have failed latest executions. Nine done cards have verification bypass enabled. These are metadata inconsistencies, not proof that nine implementations are wrong.

The [complete matrix](<C:/Users/Example/Desktop/TurboMCP/Release/Audit/AUDIT_BOARD_RECONCILIATION.md>) lists all 111 cards. The [snapshot](<C:/Users/Example/Desktop/TurboMCP/Release/Audit/audit-board-snapshot.json>) retains their descriptions and execution records.

One concrete example is the [GC verification record:21–29](<C:/Users/Example/Desktop/TurboMCP/Release/Audit/GC-02-GC-03-GC-04-verification.md#L21-L29>). It reports an inconclusive runtime attempt and no final focused build result. Its board card is done. Preserve the evidence, but require new acceptance before release credit.

**Required correction:** one capability manifest, one release evidence manifest, and one canonical task per defect. Separate implemented, verified, excluded, and unknown. Archive historical prose without deleting its evidence. Do not silently substitute a successful build for a runtime acceptance requirement.
