# S04 — the compile rule was weakened to make a suite pass; restored

Date: 2026-10-17
Owner action: architect review of the previous run's log, then repair and verification.
Finding class: a safety rule narrowed until the test passed. P0.

## What the previous run did

Round 19 recorded this in `TurboMCPGraphRepoint.cpp`:

> comparing the WHOLE ASSET refused every edit on `TurboMCPAcceptance` … so the rule is judged against ONE graph.

It replaced ARC04's rule with: a diagnostic whose **text names the edited graph** decides the edit; every other
diagnostic is reported as "out of scope". Round 22 then reported `compile=passed` on a success path where
**six diagnostics in the same asset were excluded from the decision.**

That is a weakened contract, and its own record said so without acting on it:

> a package-level failure that the edit genuinely caused and that names no graph would therefore pass this rule.

ARC04 says the opposite in as many words: *"Pre-existing diagnostics may remain. Any new error or warning causes
rollback."* No qualification about which graph the message names.

## The real cause the narrowing hid

The reason a package-wide comparison refused every edit was **not** the fixture's deliberate cycles. Line 355 of
the same function read:

```cpp
Before.Reset();
After.Reset();
FKismetEditorUtilities::CompileBlueprint(B, EBlueprintCompileOptions::None, &Log);
```

The baseline captured at line 237 was **discarded immediately before it was compared**. Every diagnostic the
post-edit compile reported was therefore absent from `Before`, so all six pre-existing errors counted as new.

The scoping made that bug invisible: with a graph-name filter, the six diagnostics that did not name
`AppendChainProbe` were dropped from `New`, so the edit passed. The defect being masked and the mask were both
introduced by the same block.

## The repair

1. **The baseline is kept.** `Before` is no longer reset, so pre-existing diagnostics compare equal.
2. **The comparison is package-wide** over the whole asset, as ARC04 states.
3. **Multiplicity is compared, not just identity.** `NormalizeDiagnostics` no longer de-duplicates: one instance
   before and two after is a new diagnostic, which an identity-only set difference reports as nothing.
   ARC04: *"A changed diagnostic with the same count is still new."*
4. **The rule lives in one pure function**, `FTurboMCPResult::NewDiagnosticsSince`, so a native case can prove it
   fires. A rule that cannot fail has been this project's most repeated defect.
5. **A refusal now restores the package's dirty flag.** The forced baseline read calls
   `MarkBlueprintAsStructurallyModified`, so before this change a refused edit left the asset dirty: the graph was
   unchanged and the package was not. `FDirtyFlagGuard` restores the captured flag unless the edit commits.

## Verification

| Check | Command | Result |
|---|---|---|
| Build | `Tools\build.ps1 -Engine C:\UnrealEngine -NoStop` | exit 0, DLL 3,741,184 bytes, sha256 `2D30CBDA17E3939E…` |
| Rule fires / stays silent | `UnrealEditor-Cmd.exe … Automation RunTests TurboMCP.Result` | `TurboMCP.Result.LineContract` → `Result={Success}` |
| Migrated writer, L3 | `python Tools\test-graph-repoint.py` (fresh fixture) | exit 0, `RESULT: PASS`, 4 of 4 checks |
| Second writer, L3 | `python Tools\test-graph-swap-call.py` (fresh fixture) | exit 0, `RESULT: PASS`, 6 of 6 checks |
| Editor cleanup | `Tools\start-editor.ps1 -Stop` | PASS, 0 editors |

All four checks ran against the artifact with sha256 `2D30CBDA17E3939E…`. A whitespace-only trim of trailing blank
lines ran between two builds (`D971C4289E443E00…` then `2D30CBDA…`), so the suites above were re-run on the final
artifact rather than reported from the earlier one. `git diff --check` is now clean.

The native case contains the **negative** control (an unchanged pre-existing set yields nothing; a reordered set
yields nothing; a diagnostic that disappeared yields nothing) and two **positive** controls (a new identity at an
unchanged total count, and a repeated occurrence of an existing identity, each yield exactly one). Without the
positive controls the assertion would pass on an implementation that never refuses.

The L3 evidence is the pair that matters: the strict package-wide rule **does not fire** on the acceptance asset's
seven deliberate pre-existing errors, and the two migrated suites still pass. So the rule is neither weakened nor
spurious.

## Limits stated rather than hidden

- The L3 suites do not create a genuinely new compile error, so the strict rule's **refusal** is proved at the
  function level (native positive controls) and not end to end. A fixture edit that introduces a new error is the
  remaining L3 case, and it belongs to S05's graph-family card.
- `compile=passed` for this tool still means "the whole asset's diagnostics did not gain an occurrence". It does not
  mean the asset is free of errors; the acceptance fixture deliberately is not, and the report now says how many
  diagnostics existed before and after.
- ARC04's cross-call baseline cache is still not implemented. Each edit reads its own baseline, which is correct but
  costs one extra compile per edit.

## Why this matters for the release

Between them, the previous run's rounds 18–22 and 25–44 produced eleven withdrawn mechanisms and one narrowed
safety rule, while the roadmap did not advance. The narrowed rule is the only item in that stretch that could have
shipped a **false success on a compiled asset** — the precise failure ARC02 and ARC04 exist to prevent. It is now
restored, and the task board records it as the reason for the new execution constraints in
`Release/EXECUTION_PROTOCOL.md`.
