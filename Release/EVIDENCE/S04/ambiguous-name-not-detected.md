# S04 evidence — the shared resolver silently takes the FIRST of N matches

Step: S04 — Implement the fixed shared contracts.
Date: 2026-10-16.

## The read, and it is conclusive

`TurboMCPNodeId::FindGraphByName` (`TurboMCPNodeId.cpp:322-358`) ends with:

```cpp
for (UEdGraph* Graph : Candidates)
{
    if (Graph->GetName().Equals(InGraphName, ESearchCase::IgnoreCase))
    {
        return Graph;          // FIRST match wins, and the count is never taken
    }
}

return nullptr;
```

**There is no match counting and no ambiguity check.** The function returns the first graph whose name matches and
discards the possibility that others also matched.

## Why that is the open half of the resolver defect

The five unified resolvers now all go through this function, so they now all share **one** policy. But that policy
is "first match wins", and a node id is `...:GraphName:<hash>` — the graph **name only**. So when a blueprint holds
two graphs of the same name in different arrays, the tool resolves one and **cannot tell the caller which**, and the
report names the same `name:hash` for either. That is exactly the **ARC06** concern: identity that cannot be
verified across calls.

## What closing it requires, and it is bounded

1. A companion or overload that also reports the match count, e.g.
   `static UEdGraph* FindGraphByName(const UBlueprint*, const FString&, int32& OutMatchCount);`
   in `TurboMCPNodeId.h`, with the existing signature kept so the six current callers do not change.
2. The **mutating** tools refuse when `OutMatchCount > 1`, naming the candidates — a refusal rather than a guess,
   per ARC02.
3. The **reader** reports the count rather than refusing, so a read stays available while the ambiguity is visible.
4. A case: one blueprint, two graphs sharing a name, assert the mutating tools refuse and the reader mentions the
   ambiguity. The fixture needs one added — there is no such blueprint today, which is why this has never fired.
5. Then unify `GraphPatch` and `GraphReplace` **only after D-008 is answered**.

## Why it is not done this round, stated plainly

It is a header change, a resolver change, changes across the mutating tools, a build and a new fixture case.
**This session's own record is ten disproved mechanisms, most of them produced by acting on a plausible idea
without enough room to verify it.** Starting a multi-file API change with no budget left is how a defect gets
introduced and shipped under a green build. The finding is recorded instead, with the steps above, so it can be
done properly and verified.
