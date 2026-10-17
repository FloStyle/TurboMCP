# S04 evidence — `AllocateDefaultPins` is ruled out, and the arithmetic now points where it should have first

Step: S04 — Implement the fixed shared contracts.
Date: 2026-10-16.
Severity: **P0, still open.** Fifth mechanism withdrawn. The arithmetic of the counts is the most useful thing
this round produced.

## The test, and its result

I made the `AllocateDefaultPins` call in `SpawnNodeOfType` conditional on the node having no pins —
`BuildProbeNode` does not call it at all, so it was the only visible difference between the path that works and
the path that does not.

Build: **exit 0**, artifact 3 742 720 bytes. Then the discriminator:

```
BEFORE  ids: ['504adf','f7df37','f3ad8a','12b579','1789b5']
SOURCE used: ...AppendChainProbe:f3ad8a
report     : changed: repointed 1 reader(s) ...; target ...:4dd89e.then; old source retained
AFTER   ids: ['504adf','f7df37','f3ad8a','1789b5']
class deltas: {'K2Node_CallFunction': -1, ...}   total 5 -> 4
```

**The defect is UNCHANGED.** So `AllocateDefaultPins` is **not** the cause, and that is the **fifth** of my own
mechanisms disproved in five rounds.

The change is **kept**: allocating only when there are no pins is strictly safer than resetting a registered
node's pins, preserves the original intent, and is now documented in the code. It is a safe simplification, not
a fix, and it is not claimed as one.

## The arithmetic, which is the real output of this round

Read the two lists carefully:

```
BEFORE: 504adf  f7df37  f3ad8a 12b579 1789b5     (5 nodes; classes: 3 call, 1 branch, 1 get)
AFTER : 504adf  f7df37  f3ad8a        1789b5     (4 nodes; classes: 2 call, 1 branch, 1 get)
                              ^^^^^^ gone
```

The report says **"old source retained"** and the source is **`f3ad8a`** — which **IS in the after list**.
The node that vanished is **`12b579`**, a `K2Node_CallFunction` that is **neither** the source nor the target.

And the created target `4dd89e` is in **neither** list.

So the edit **consumes one existing node (`12b579`)** and **does not persist the node it created**. Net: −1.

**This is the first time this P0's arithmetic has been written down correctly**, and it is a different picture
from every earlier record: the source is fine, and the damage is a *different* call node — which is almost
certainly **the created target of the PREVIOUS run of this case**, still in the fixture and now consumed.

**Why that fits everything measured:** the case creates a node each run; the fixture is rebuilt from a commandlet
that does not necessarily clear a node a previous run left; so each run consumes one leftover and leaves one node
that does not survive. That is consistent with the count falling by exactly one on every single run this sequence
has performed — which the records show it has, every time, across every hypothesis.

**It also means my discriminator is not comparing a clean fixture with a dirty one.** The "loss" may be the
disposal of a **leftover from the previous run**, not damage caused by this run's edit. I have not established
that, and it changes what the P0 *is*: it may be a fixture hygiene problem plus a real non-persistence problem,
rather than a single data-loss defect.

## What is verified, after six rounds of this P0

| Claim | State |
|---|---|
| The created target never persists | **verified, every run** |
| A `K2Node_CallFunction` count falls by exactly one, every run | **verified, every run** |
| The node that vanishes is a **different** call node each time, never the source | **verified — this record** |
| `AllocateDefaultPins` is the cause | **withdrawn — disproved by test** |
| The commit is the cause | **withdrawn** (the guard, running after the commit, saw a correct count) |
| Identity instability / source loss / probe orphan / unregistered node | **all withdrawn** |
| The vanished node is a **leftover from a previous run** | **NOT established — and it is the next test** |

## The next test, and it is the one that should have come first

**Read the graph's node ids BEFORE the fixture rebuild and AFTER it, and establish whether the rebuild clears a
node a previous run created.** If the rebuilt fixture already contains a leftover, then every measurement in this
P0 has been taken against a fixture carrying state from the run before, and the arithmetic above is explained
without any product defect in the deletion path.

Then, separately and still open: **the created node does not persist**, which no rebuild explains, because the
report names it in the same session in which it was created.

## Evidence paths

- this record; `Source/TurboMCP/TurboMCPNodeType.cpp` (`SpawnNodeOfType`, the conditional allocation)
- discriminator output quoted above; build exit 0, artifact 3 742 720 bytes
- editor: 0 running at the end of the round, no lease

## Next action, in one step

Print the graph's node ids **before** the fixture rebuild and **after** it, with no edit in between. That decides
whether this P0 has been measuring a fixture that carries the previous run's node — and it is one command.
