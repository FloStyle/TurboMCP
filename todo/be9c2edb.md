# T16 (P2, high) — Real node sizes for every layout tool

ID: be9c2edb-2826-4421-941e-f02b00041b4d
Status: todo
Priority: P2

## Scope and acceptance

Preserve the substantive scope below. Before editing, reconcile the finding with current source and identify the focused acceptance check. Do not execute historical board/harness instructions. Record duplicates by task ID without discarding their requirements.

## Imported evidence

Historical paths and completion claims below are not current proof. Old documents are recoverable from the private backup commit recorded in docs/migration.md.

> Source
>     `Feedbacks/TurboMCP_TaskList_2026-10-13.md:208-218`, group P2. Source IDs: X-01, LR-05 and KN-01 (S1), plus the capture design note (S1). This card came from the list. No one measured it on the tree.
>
> The problem, in the list's words
>     "every layout tool used a default square per node. The tools reported success on overlapping layouts."
>
> done when
>     1. Layout tools get real node sizes before they plan. One way is to draw the graph off screen.
>     2. Without sizes, a layout edit refuses unless the caller forces it.
>     3. Every layout report says at the top when the sizes are estimated.
>
> Verify
>     Run tidy on a graph that has never been opened. The result has 0 overlaps with real outlines.
>
> The shape this card shares with T03 and T04
>     This is a tool that reports a measurement it did not make. "0 overlaps" from a default square is true about the square and false about the graph. T04 has the same shape, where a compile verdict came from the compiler's log alone. T03 has it too, where a save verdict came from a return value. T04's fix is the pattern to copy: read the source that actually knows, and say which source answered.
>
> The cheapest clause
>     Clause 3 is the cheapest of the three, and it is the one that makes the other two honest. A report that says "sizes are estimated" cannot be mistaken for one taken from real outlines. `TurboMCPCallCostTools.cpp` already does this with "(%d row(s), each with its call count)". Its comment says why: "a count with no denominator cannot be told apart from a reader that looked at nothing."

## Verification

Pending. Record command, result, commit, and any blocked acceptance here.
