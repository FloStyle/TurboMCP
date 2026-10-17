# Task index

[Repository home](README.md) · [Roadmap](ROADMAP.md) · [Contributing](CONTRIBUTING.md)

Run `python Tools/next-task.py` to select work. The approved queue below defines execution order; historical findings need reconciliation before implementation.

Generated from task files and the approved queue. After editing task status, run `python Tools/update-todo.py`.

## Approved queue

| Task | Track | Kind | Status |
| --- | --- | --- | --- |
| [R01: Repair the test-audit evidence classifications](todo/roadmap-r01.md) | v1 | implementation | done |
| [R02: Make shared engine ownership atomic](todo/roadmap-r02.md) | v1 | implementation | in_progress |
| [R03: Establish fixture and binary reproducibility evidence](todo/roadmap-r03.md) | v1 | investigation | todo |
| [R13: Measure 5.9 native overlap before approving new features](todo/roadmap-r13.md) | v1 | investigation | todo |
| [R16: Determine what prevents an installed-engine plugin](todo/roadmap-r16.md) | v1 | investigation | todo |
| [R04: Audit callable behavior and result contracts](todo/roadmap-r04.md) | v1 | investigation | todo |
| [R05: Specify and verify mutation lifecycle repairs](todo/roadmap-r05.md) | v1 | investigation | todo |
| [R06: Reconcile graph identity and representation repairs](todo/roadmap-r06.md) | v1 | investigation | todo |
| [R07: Reconcile geometry, layout and capture correctness](todo/roadmap-r07.md) | v1 | investigation | todo |
| [R08: Reconcile data and widget state correctness](todo/roadmap-r08.md) | v1 | investigation | todo |
| [R09: Reconcile references and runtime observations](todo/roadmap-r09.md) | v1 | investigation | todo |
| [R17: Expose verified tool preferences in discovery and help](todo/roadmap-r17.md) | v1 | implementation | todo |
| [R18: Specify a clearer editor panel and icon system](todo/roadmap-r18.md) | v1 | investigation | todo |
| [R10: Make discovery and human documentation match observed behavior](todo/roadmap-r10.md) | v1 | documentation | todo |
| [R11: Close the 5.8.3 release acceptance matrix](todo/roadmap-r11.md) | v1 | verification | todo |
| [R15: Automate source and binary release preparation](todo/roadmap-r15.md) | v1 | implementation | todo |
| [R12: Prove installed-engine packaging and isolate development dependencies](todo/roadmap-r12.md) | compatibility | investigation | todo |
| [R14: Implement bounded 5.9 compatibility adapters](todo/roadmap-r14.md) | compatibility | implementation | todo |

## Historical findings

These records preserve original requirements and evidence. Their status does not grant implementation permission.

<details>
<summary>todo (68)</summary>

- **P1** [TR-01 / TR-02 (P1, high) — trace must not claim a node never ran without a proven match](todo/02674939.md)
- **P1** [T-RECON (P1, medium) — Three `plan row` clauses the list dropped: macros (row 6.6), an open tab refusing (row 7.6), E-SESSION/R-EDIT-SESSION bounded (row 9.2)](todo/100b52a6.md)
- **P1** [RF-06 (P1, high) — references direction both must not return a false zero](todo/484d5638.md)
- **P1** [GC-09 / FT-47 (P1, high) — graph append must detect invalid created nodes and roll back](todo/4f1bfd61.md)
- **P1** [X-17/X-16 (P1, high) — feedback reports must retain items and IDs](todo/51678b9f.md)
- **P1** [T14 (P1, low) — Session change list](todo/602feaab.md)
- **P1** [T11 (P1, medium) — Function from text, with its signature](todo/63d5c9e2.md)
- **P1** [LT-04 (P1, high) — lint must not call a linked cast result unnecessary](todo/7bbe9c3c.md)
- **P1** [S04 follow-ups — the double-wrapped return shape, and the failing T04 native test](todo/7de8d185.md)
- **P1** [T12 (P1, high) — Atomic multi-asset batch edit](todo/bbb63d1e.md)
- **P1** [GL-10–GL-13 / GL-11 (P1, high) — graph reads must preserve every node and every pin link](todo/d3897f07.md)
- **P1** [Batch verifier — test the ten concurrent urgent fixes](todo/ee4fde11.md)
- **P1** [T15 (P1, low) — Cleanup helpers](todo/f1a04ef1.md)
- **P1** [T13 (P1, medium) — Pin-level link dump](todo/ff18311b.md)
- **P1** [Establish installed-engine dependency and packaging matrix](todo/foundation-engine-matrix.md)
- **P1** [Make engine scheduling exclusive across worktrees](todo/foundation-shared-lease.md)
- **P1** [Reconcile imported P0/P1 tasks against current code](todo/foundation-task-triage.md)
- **P2** [T18 (P2, high) — One getter rule for the report and the fixer](todo/02ba0f24.md)
- **P2** [WT-07 (P2, medium) — widget subtree write and explicit event binding](todo/1388b93a.md)
- **P2** [X-18/X-21/X-22 (P2, medium) — read-only calls must not silently dirty, save or check out assets](todo/3be553c8.md)
- **P2** [F5-F8/RF-1/AC-1/AC-2/AC-4/GL-4/BL-1/BL-3 (P2, medium) — read and cost outputs must be truthful and bounded](todo/3eae39b1.md)
- **P2** [GC-10–GC-12 / X-33 (P2, medium) — graph writers must accept canonical reader text](todo/4de12720.md)
- **P2** [S16 / G12+R03 — Restricted single-graph batch: one transaction, one compile, resolved per operation](todo/5b6a8e45.md)
- **P2** [X-24/X-26 / FT-37 (P2, medium) — read live widget instances and usable paths](todo/7ae3dd76.md)
- **P2** [T-FIXTURE-01 (P2, high) — The acceptance fixture's saved bytes are NOT reproducible: three sizes from one source, so the tracked asset drifts on every rebuild](todo/9935c0ea.md)
- **P2** [X-27 / FT-41 (P2, medium) — preview and revert unsaved assets safely](todo/9c47994b.md)
- **P2** [X-28 / TH-05 / TH-07 / X-34 / FT-43 (P2, medium) — bounded task-to-tool discovery](todo/9e5d5c9a.md)
- **P2** [BL-09 / LT-03 / LT-06 (P2, medium) — usage analysis must recognize delegates, bound handlers and implicit self](todo/a28a09fd.md)
- **P2** [T21 (P2, low) — Layout report counts](todo/a8341453.md)
- **P2** [S00 follow-up / T-FIXTURE-01 — The tracked fixture bytes change after an editor-free suite run](todo/b4d297e2.md)
- **P2** [T16 (P2, high) — Real node sizes for every layout tool](todo/be9c2edb.md)
- **P2** [T20 (P2, high) — Capture shows what it framed](todo/c421299d.md)
- **P2** [T17 (P2, high) — Layout rules and score that match a person's layout](todo/c5c54253.md)
- **P2** [T19 (P2, high) — Tidy: no false cycle](todo/e6fc78bd.md)
- **P2** [S04 / P2 — the first run after a fixture rebuild fails; later runs pass. Instrument the FAILING run.](todo/efa76989.md)
- **P3** [F9-F13/RF-2/AC-3/BL-2/NF-1/VC-1/MI-1 (P3, low/positive) — keep outputs compact and preserve proven good behavior](todo/04111899.md)
- **P3** [Release 1.0: reconcile the public branch](todo/081329d2.md)
- **P3** [NR-03/WT-04 (P3, medium) — read tools need actionable detail, not counts or cuts](todo/0b84b1d8.md)
- **P3** [AS-01/BL-07/TH-02/TH-03 (P3, low) — filters, focused output and usable help](todo/0e4658ad.md)
- **P3** [BL-10 / X-30 / X-31 / FT-46 (P3, medium) — complete widget overrides and animation state reads](todo/1256778b.md)
- **P3** [T26 (P3, low) — Show the dependents of a struct change](todo/1f5fe672.md)
- **P3** [T-KEEP (P3, medium) — `Keep: works today`: the file's 10 regression checks, which no card owned until now](todo/2f066e6c.md)
- **P3** [TurboMCP release candidate — sequential implementation and acceptance](todo/5f2cee78.md)
- **P3** [T24 (P3, medium) — Struct readers include break and make nodes](todo/7bd81b84.md)
- **P3** [TH-06 / GC-08 / TR-04 / TH-04 (P3, low) — help must show valid actions and graph-edit forms](todo/9263bd95.md)
- **P3** [T25 (P3, low) — Data table create, with nested rows](todo/a4eed2e1.md)
- **P3** [T22 (P3, medium) — One field syntax for all field tools](todo/abdd2c08.md)
- **P3** [T12 prerequisite — Extract transaction-free operation cores](todo/b230e508.md)
- **P3** [T23 (P3, medium) — Table row preview and read-back](todo/ba6df3c8.md)
- **P3** [S04 / D-008 — an empty graph name: mutators refuse, readers scan explicitly (DECIDED, implementation owed)](todo/c1e04796.md)
- **P3** [Release 1.0 gate: safety, build, and public release](todo/c2865829.md)
- **P4** [T30 (P4, low) — Bound handlers are not "never called"](todo/261bb8db.md)
- **P4** [T28 (P4, medium) — Node-type lookup accuracy](todo/9db83209.md)
- **P4** [T29 (P4, low) — Node find covers every graph](todo/ca5bbbfc.md)
- **P4** [FT-25/FT-26/FT-29 (P4, medium) — reduce common asset, component and listener authoring work](todo/d4433d0e.md)
- **P4** [T27 (P4, medium) — One node ID format, readable by a person](todo/e866ba0d.md)
- **P4** [FT-32–FT-36 (P4, low) — add focused read, layout and read-only helper tools](todo/e9a28b8d.md)
- **P5** [T31 (P5, medium) — Short checkout report](todo/1db73082.md)
- **P5** [T33 (P5, low) — Plugin window presentation](todo/9d4624ff.md)
- **P5** [T32 (P5, medium) — Agent entry point and trap notes](todo/e35faf67.md)
- **P6** [T39 (P6) — Curve assets (row 7.3)](todo/4ec9e1cc.md)
- **P6** [T34 (P6) — Widget authoring (rows 5.2 to 5.5): tree, properties, slot layout, animations, bindings in bulk](todo/5b06fa2e.md)
- **P6** [T37 (P6) — Asset operations (row 7.6): create, rename, move, duplicate, with redirectors fixed](todo/66d97dc8.md)
- **P6** [T35 (P6) — Material writing (rows 3.2 to 3.6): full reader, then check, append, replace, patch, then function arguments](todo/b6fa0cf7.md)
- **P6** [T41 (P6) — Input test loop for play sessions: mouse, input action injection, simulated gamepad, menu solver](todo/ba518a2a.md)
- **P6** [T36 (P6) — Source control next to the editor hooks](todo/cdbf31c9.md)
- **P6** [T40 (P6) — Preview of the ordered steps to add a node: create, then link these pins in this order](todo/d0f909a3.md)
- **P6** [T38 (P6) — Test: the material reader keeps constants on unconnected inputs](todo/f434ffba.md)

</details>

<details>
<summary>backlog (24)</summary>

- **P1** [T-AUDIT-22 (P1, medium) — AUDITED BY HAND 22/49: TurboMCPCallCostTools.cpp read to the end — a substituted cap parameter, and reporting discipline the three material writers lack](todo/019cf7af.md)
- **P1** [T-AUDIT-14 (P1, medium) — AUDITED BY HAND 14/49: TurboMCPEnumAdd.cpp — 311 lines, Renamed restates the request while ShownAs beside it is measured](todo/214d6e7a.md)
- **P1** [T-AUDIT-12 (P1, medium) — AUDITED BY HAND 12/49: TurboMCPOverrideRemove.cpp — 182 lines, the count that caught its worst bug is not in the answer](todo/c4485ba2.md)
- **P1** [Complete release acceptance before public repository cutover](todo/public-cutover.md)
- **P1** [Adapt the call recorder to Unreal 5.9 registry identity](todo/ue59-recorder.md)
- **P2** [Reconcile imported tasks with current source and acceptance cases](todo/task-reconciliation.md)
- **P2** [Verify Unreal 5.9 discovery, media and null contracts](todo/ue59-protocol.md)
- **P3** [T-SUITE-01 (P3, medium) — The material suites DO assert the rollback text — but only on the paths the code verifies, which is why 28 unchecked sites are untested](todo/048107d9.md)
- **P3** [T-AUDIT-UNCOVERED (P3, low) — AUDITED: test-uncovered-tools.py — 176 lines, the best statement of C3 in the audit, and one weak check named](todo/5301f48e.md)
- **P3** [T-METHOD-01 (P3, medium) — Every count in every card must state the scope it was taken over — 161 is a floor, not a total](todo/53fb9672.md)
- **P3** [T-SUITE-02 (P3, low) — The FAILS ON convention measures a vocabulary, not a quality — two full suites tested, and one of the 34 silent ones is exemplary](todo/71e2e86a.md)
- **P4** [T-X4-01 (P4, low) — The X4 screen over all of Tools/: zero orphaned symbols, so the pattern lives entirely in Source/ where there are five](todo/4ff1c6d8.md)
- **P4** [T-SUITE-03 (P4, low) — The two obsolete T03 suites are removed (a0b4556) and the final 62-suite sweep is running](todo/5013737a.md)
- **P5** [T-AUDIT-16 (P5, low) — AUDITED BY HAND 16/49: TurboMCPTokenEstimate — 48 correct lines and one false universal (one finding withdrawn after checking the spec)](todo/48608da1.md)
- **P5** [T-AUDIT-32 (P5, low) — AUDITED BY HAND 32/49: TurboMCPNodeType.cpp — 1058 lines, and a note with a written contract that fourteen assignments fill and no caller reads](todo/701a6c3d.md)
- **P5** [T-C3-01 (P5, low) — C3: 17 of the 50 comment-versus-code sites remain — 33 fixed, and this card is the resume point](todo/745637f4.md)
- **P5** [T-AUDIT-10 (P5, low) — AUDITED BY HAND 10/49: TurboMCPWidgetTreeText.cpp — 688 lines, one comment omits the character its sibling calls decisive, and no round-trip defect](todo/81cbc063.md)
- **P5** [T-C3-02 (P5, low) — C3 and C-P10 overlap on 16 sites: the C-P10 card owns them, and C3 owns only the comment question at those lines](todo/8af56e37.md)
- **P5** [T-AUDIT-18-19 (P5, low) — AUDITED BY HAND 18-19/49: TurboMCPSettings.cpp clean; TurboMCPModule.cpp measures the analytics install and only logs it, plus a member read nowhere in the repository](todo/a312b008.md)
- **P5** [T-AUDIT-36 (P5, low) — AUDITED BY HAND 36/49: TurboMCPGraphLoad.cpp — the spec and code are both honest about format 'write'; the plan's count is the only thing that omits it](todo/d03a170f.md)
- **P6** [T-AUDIT-27-29 (P6, low) — AUDITED BY HAND 27-29/49: TurboMCPPlayCheck (.cpp + .h + Tools) — 757 lines, zero defects, and the correct form of what TraceTools does wrong](todo/37d3d49e.md)
- **P6** [T-AUDIT-31 (P6, low) — AUDITED BY HAND 31/49: TurboMCPOverrides.cpp — 402 lines, zero defects, and the fifteenth read-only file with no behaviour defect](todo/77b738a1.md)
- **P6** [T-AUDIT-20 (P6, low) — AUDITED BY HAND 20/49: TurboMCPTraps.cpp — 202 lines, zero defects, and four failure states kept distinct](todo/8beb2deb.md)
- **P6** [T-AUDIT-17 (P6, low) — AUDITED BY HAND 17/49: TurboMCPPureChecks.cpp — 132 lines, zero defects, and the structural reason why](todo/a6a86af7.md)

</details>
