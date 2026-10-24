# Roadmap to a dependable v1

[Repository home](README.md) | [Documentation](docs/README.md)

The v1 plan starts with trustworthy tests, then audits existing tools and repairs confirmed defects. Release follows only after the acceptance gates pass. Compatibility work is tracked separately.

See [project status](docs/status.md) for current support and release limits. The [capability inventory](docs/audit/capabilities.md) covers all 101 tool declarations; [task dispositions](roadmap/task-dispositions.json) account for the 92 historical/foundation records. The [complete task review](docs/audit/task-review.md) covers all 110 records and separates source-confirmed defects from pending runtime evidence.

## Give an agent one instruction

> Read AGENTS.md. Run `python Tools/next-task.py`. Work only on the returned task, follow its scope and stop conditions, and record evidence. If it says STOP, explain the blocker; do not skip ahead or invent work.

The selector reads the approved sequence and refuses blocked, unready, already-owned/in-progress or incomplete-prerequisite work. It does not execute commands, grant a lease, assign credentials or publish. Use `python Tools/next-task.py --claim <owner-name>` in the worker's clean worktree. A shared durable claim prevents duplicate dispatch; the integration owner releases it only after review. Concurrent agents require distinct worktrees and explicitly assigned independent tasks; the default queue is intentionally serial.

## Investment decisions

Fix false success, data-loss risk, misleading observations and unusable tests before adding features. Keep 5.8.3 pinned. Compare 5.9 native tools before expanding enum/struct/source-control/widget-animation or building a proxy. Do not remove existing useful workflows merely because native names overlap. General multi-asset batching and broad new authoring stay deferred until the native comparison and a concrete missing workflow justify their maintenance cost.

## Baseline execution order

| Stage | Work | Exit condition |
| --- | --- | --- |
| [R01](todo/roadmap-r01.md) | Make test-audit classifications truthful | Parser regressions distinguish evidence from references and false positives |
| [R02](todo/roadmap-r02.md) | Shared engine/editor/port ownership | Concurrent-process contention and cleanup tests pass |
| [R03](todo/roadmap-r03.md) | Fixture and binary reproducibility | Owned fixture recovery and loaded-binary identity are verified; historical intermittent/serialization findings remain open |
| [R13](todo/roadmap-r13.md) | Compare pinned 5.9 native interfaces early | Adopt/adapter/retain/defer decisions; unknown runtime parity stays explicit |
| [R16](todo/roadmap-r16.md) | Establish installed-engine integration constraints early | Dependency and rebuild-boundary evidence; bounded adapter/package proposals |
| [R04](todo/roadmap-r04.md) | Audit every callable and result contract | Every tool has schema/effect/refusal/evidence status and bounded repair specifications |
| [R05](todo/roadmap-r05.md) | Mutation, rollback and restricted batch | Exact lifecycle gaps and required repairs are specified against ARC03-ARC09 |
| [R06](todo/roadmap-r06.md) | Graph identity and representation | Findings resolved or specified with concrete negative controls |
| [R07](todo/roadmap-r07.md) | Geometry, layout and capture | Measured geometry/framing requirements and fixes |
| [R08](todo/roadmap-r08.md) | Data/material/widget correctness | Domain read-back and restoration requirements resolved |
| [R09](todo/roadmap-r09.md) | References and runtime observation | Unknown/unmatched data cannot masquerade as measured absence |
| [R17](todo/roadmap-r17.md) | Make verified tool preferences discoverable | Live help/schema and client-choice evidence; no refusal bypass |
| [R18](todo/roadmap-r18.md) | Design the editor panel and icon improvements | Reviewed layouts, states and bounded implementation proposals |
| [R10](todo/roadmap-r10.md) | Human docs, help and discovery | Descriptions and examples agree with observed behavior |
| [R11](todo/roadmap-r11.md) | Complete 5.8.3 acceptance | All 97 required cases mapped and passing on the candidate |
| [R15](todo/roadmap-r15.md) | Release automation | Required CI and release dry-run failure controls pass |

Investigations produce reviewed repair specifications, not unlimited implementation permission. Confirmed defects must be assigned bounded repair tasks by the integration owner and inserted before their acceptance gate; the executor cannot silently close a finding or expand its own scope. This queue is the audit and foundation plan, not a claim that all 5.8.3 product repair tasks are already specified.

## Vision tools and rendering investigation — 2026-10-24

Owner: Codex. User-requested classification and local testing; DSH remains paused. This belongs to R07, with tool-output quality under R04. Existing issue c421299d tracks capture framing; CP-14 tracks the contrast/shadow artifact. Treat vision-tool involvement as the field report, not a proven root cause. Do not create a duplicate bug or close either from text-only checks.

| Current capability | Tools | Boundary |
| --- | --- | --- |
| Graph, region, node, comment and neighbourhood images | LookTurboMCPCapture | Requires rendered editor; may open UI/change view or selection; produces PNG. Selector modes and Size semantics differ. |
| Screen and supported asset-editor images | LookTurboMCPCapture | Captures editor presentation; not proof of asset correctness or general support for every asset editor. |
| Widget image with sample values | LookTurboMCPWidgetPreview | Executes project code with Execution=allow; transient instance is not a sandbox. |
| Geometry and layout observations | LookTurboMCPLayoutReport, LookTurboMCPGraphPositions | Text measurements/heuristics, not images or rendered-wire proof. |
| Combined layout report and image | LookTurboMCPLayoutDoneCheck | Review aid; not a correctness/pass verdict. |
| Window discovery | LookTurboMCPToolset | Lists live windows; does not capture an image. |

### Missing evidence and candidate improvements

- Missing acceptance evidence: captures consistently contain the requested nodes, including frame edges and small requested sizes; returned paths are accessible and image dimensions match reported facts.
- Missing acceptance evidence: capture and geometry tools leave the live graph presentation usable. Reproduce CP-14 before selecting a rendering fix.
- Review candidate: separate capture dimensions from neighbourhood depth/count rather than overloading Size. This is a proposed API improvement, not an approved schema change.
- Review candidate: consistent image provenance (asset, graph, selector, bounds, dimensions, clipping/partial status) across modes. Inventory existing output first; do not assume every field is absent.
- Review candidate: explicit preservation/reporting of changed view, focus and selection. No claim of existing restoration without checks.

### Codex-owned local test plan

Use the supervised pinned editor and disposable TestProject fixtures, not workplace assets. Record build identity, logs and images. Start with an open graph containing execution and data nodes, a comment box and nodes at frame edges. Capture a baseline, then call graph Capture, LayoutReport, GraphPositions and LayoutDoneCheck individually. Inspect the live graph after each call and repeat captures to distinguish image-only artifacts from persistent editor rendering changes. If contrast/shadows change, preserve evidence before a controlled node-create action; compare whether that action clears the artifact. Record dirty state, selection, zoom and focus before/after without automatically saving or clearing dirty flags.

Then exercise node/box/region/selection/neighbour selectors, invalid selectors and undersized frames. Separately render a disposable Widget with explicit execution consent and known text/colour sample values. Inspect actual pixels and result text, not merely file existence or RPC success. Existing test-capture-selectors and test-capture suites are starting coverage, not full visual acceptance. Fix confirmed bugs first; retain unreproduced cases as open investigations. Validation is a flag, never a substitute feature/task.

Status: classified and planned; no new editor execution or visual acceptance in this documentation update.

## Optional compatibility track

R16 investigates integration constraints early; R12 proves installed-engine packaging. R14 implements and verifies the 5.9 adapter. Run `python Tools/next-task.py --track compatibility` when the baseline prerequisites and target engine are available. Missing optional engine distributions do not block baseline safety fixes and do not become support claims.

## Cost and drift controls

Read the selected task, relevant design and exact source/test files; do not reload all historical audits each turn. Reuse existing fixtures and focused suites. Do not run a clean engine build to answer a source-only question. After two identical unexplained failures, diagnose instead of repeating. Record invalidated assumptions and stop on missing prerequisites. Changes to release scope, public schema or approved architecture require an explicit design decision. No automatic publication from a development push.

For each completed task, retain the observable outcome, source/engine/binary identity, commands, exits and logs. A statement of confidence or a generated document is not acceptance evidence. See [audit findings](docs/audit/findings.md), [technical decisions](docs/audit/technical-decisions.md) and [native overlap](docs/audit/native-overlap.md).

See the [implementation handoff](docs/agent-handoff.md), [three-way comparison](docs/tool-comparison.md), [tool-selection design](docs/design/tool-selection.md) and [editor experience requirements](docs/design/editor-experience.md).
