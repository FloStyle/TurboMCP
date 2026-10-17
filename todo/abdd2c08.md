# T22 (P3, medium) — One field syntax for all field tools

ID: abdd2c08-6544-4730-b183-e4d856aad868
Status: todo
Priority: P3
Readiness: needs_reconciliation
Owner: unassigned
Design: docs/design/editor-domains.md

## Roadmap disposition

Workstream: results. Reconcile this finding with the current source under the ordered roadmap before implementation. Preserve its acceptance requirements and link duplicate findings to one repair task.

## Scope and acceptance

Preserve the substantive scope below. Before editing, reconcile the finding with current source and identify the focused acceptance check. Do not execute historical board/harness instructions. Record duplicates by task ID without discarding their requirements.

## Imported evidence

Historical paths and completion claims below are not current proof. This task must be made self-contained against the current source before implementation; no private archive is a prerequisite for public contributors.

> Source
>     `Feedbacks/TurboMCP_TaskList_2026-10-13.md:270-277`, group P3. Source IDs: X-10 and SF-03 (S4), DT-01 (S3), SN-02 (S4). This card came from the list. No one measured it on the tree.
>
> done when
>     1. Struct new, struct fields, variables add, function signatures and table rows all accept "Name: Type option=value". They all use the same parser.
>     2. The table tools accept display names first, then internal names. A refusal lists the display names.
>     3. "float" maps to a double-precision real, and the result says so. "real" and "double" are accepted.
>     4. Each help text has one example with options, including "force" for a removal.
>
> Why clause 2 is spelled out
>     A display name is what a person sees. An internal name is what the engine stores. This is the only place in the project where two names exist for one field. `C-P7` says that a refusal must lead to the next step, so a refusal has to name the spelling the caller typed.
>
> Clause 3 is already partly measured
>     `test-vars-add.py:30-32` uses `Speed: float default=250 category=Movement`, and the tool accepts it. `test-types` has a `kind` question on record. The first measurement is therefore a grep of every field parser, not a read of one.

## Verification

Pending. Record command, result, commit, and any blocked acceptance here.
