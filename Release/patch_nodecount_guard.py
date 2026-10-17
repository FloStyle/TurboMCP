"""Add the NodeCountBefore guard, and put it AFTER the transaction's commit.

THE PROBLEM THE RESTRUCTURE SOLVES, and it is why the guard could not simply be added at the end.

`FScopedTransaction Tx` is declared at L215 and lives until the FUNCTION returns (L424). On this engine a
transaction is committed by being DESTROYED WITHOUT `Cancel()`, so the commit happens when `Tx` goes out of scope
- which is AFTER the success report is built at L413-423. A node-count check placed in the success block would
therefore read the graph BEFORE the commit, which is the one moment it cannot be wrong.

So the successful return is wrapped in its own scope. `Tx` is destroyed when that inner scope ends, and the guard
then reads the graph in its committed state.
"""

import pathlib

PATH = pathlib.Path(r"C:\Users\Example\Desktop\TurboMCP\Source\TurboMCP\TurboMCPGraphRepoint.cpp")
text = PATH.read_text(encoding="utf-8")

OLD = """	// The transaction is committed by NOT cancelling it, and it ends here - exactly once (finding G04).
	Out.Outcome = ETurboMCPResultOutcome::Changed;
	Out.Code = TEXT("ok");
	Out.Rollback = ETurboMCPResultRollback::NotNeeded;
	Out.Save = Context.bSave ? ETurboMCPResultSave::Saved : ETurboMCPResultSave::NotRequested;
	Out.ElapsedMs = (FPlatformTime::Seconds() - StartSeconds) * 1000.0;

	const FString TargetId = P.bCreateTarget ? TurboMCPNodeId::MakeForNode(Target, true) : P.TargetNodeId;
	Report = FString::Printf(TEXT("changed: repointed %d reader(s) in %s; target %s.%s; old source %s\\n"
		"compile: %s%s"),
		P.Readers.Num(), *P.GraphName, *TargetId, *P.TargetPin,
		P.bKeepSource ? TEXT("retained") : TEXT("deleted"),
		*FTurboMCPResult::CompileToken(Out.Compile),
		Out.bCompileBaselineUsed
			? *FString::Printf(TEXT(" (compared against the pre-edit diagnostics, scoped to graph '%s'; no new "
			                        "diagnostic naming it, %d other diagnostic(s) in this asset out of scope)"),
			                   *P.GraphName, Out.OutOfScopeDiagnostics.Num())
			: TEXT(""));
	return true;
}"""

NEW = """	// -----------------------------------------------------------------------------------------------
	// *** THE SUCCESS PATH IS SCOPED SO THE COMMIT HAPPENS BEFORE THE GRAPH IS VERIFIED. ***
	//
	// MEASURED, and it is why this block exists: `FScopedTransaction Tx` lives until the FUNCTION returns, and on
	// this engine a transaction is committed by being DESTROYED WITHOUT `Cancel()`. So the commit happened AFTER
	// the report was built - which meant the report described a state nobody had checked. `TargetId` was computed
	// from a LIVE POINTER before the commit, and the tool then reported "changed ... compile: passed" while the
	// graph, measured afterwards by node CLASS, was missing the created node AND had lost another one (3 -> 2
	// K2Node_CallFunction, id 554569 gone).
	//
	// A node-count check placed inside the old success block would have read the graph at the one moment it could
	// not be wrong. Closing the scope here destroys `Tx`, which IS the commit, so everything below runs against
	// the committed graph.
	// -----------------------------------------------------------------------------------------------
	{
		Out.Outcome = ETurboMCPResultOutcome::Changed;
		Out.Code = TEXT("ok");
		Out.Rollback = ETurboMCPResultRollback::NotNeeded;
		Out.Save = Context.bSave ? ETurboMCPResultSave::Saved : ETurboMCPResultSave::NotRequested;
		Out.ElapsedMs = (FPlatformTime::Seconds() - StartSeconds) * 1000.0;

		CreatedTargetId = P.bCreateTarget ? TurboMCPNodeId::MakeForNode(Target, true) : P.TargetNodeId;
	}

	// *** THE GUARD: THE PLAN'S NodeCountBefore IS USED, AND BOTH COUNTS ARE REPORTED. ***
	//
	// The plan recorded `NodeCountBefore` (L45) and NEVER used it, so nothing in this function compared the
	// intended change with the graph's real final state. This is the check that turns "the tool confidently
	// reported a clean success while the graph disagreed" into a refusal.
	//
	// EXACT, NOT `>=`: a comparison that cannot express its claim is how `GetUndoCount() >= 0` and
	// `$anyForeign.Count -eq 0` both hid real defects. Both operands are printed on failure.
	{
		int32 NodeCountAfter = -1;
		if (UEdGraph* GAfter = FindGraph(B, P.GraphName))
		{
			NodeCountAfter = GAfter->Nodes.Num();
		}
		int32 NodeCountExpected = P.NodeCountBefore;
		if (P.bCreateTarget) { NodeCountExpected += 1; }
		if (!P.bKeepSource) { NodeCountExpected -= 1; }

		if (NodeCountAfter < 0 || NodeCountAfter != NodeCountExpected)
		{
			// ARC05: the change may or may not have landed, and a caller must never be able to read "nothing
			// happened" from an unverified outcome.
			Out.Outcome = ETurboMCPResultOutcome::OutcomeUnknown;
			Out.Code = TEXT("graph_nodecount_mismatch");
			Out.bChanged = false;
			Out.Changed = 2;  // tri-state: 2 == unknown
			Out.Rollback = ETurboMCPResultRollback::NotSupported;
			Error = FString::Printf(
				TEXT("the graph's node count does not match the plan, so the outcome cannot be vouched for. ")
				TEXT("Expected %d (before %d, +%d created, -%d removed); found %s. ")
				TEXT("A count that disagrees with the plan means nodes were created or destroyed that the ")
				TEXT("report cannot account for."),
				NodeCountExpected, P.NodeCountBefore, P.bCreateTarget ? 1 : 0, P.bKeepSource ? 0 : 1,
				NodeCountAfter < 0 ? TEXT("(the graph could not be resolved after the edit)") : *FString::FromInt(NodeCountAfter));
			Report = FString::Printf(
				TEXT("outcome_unknown: the edit completed but the graph's node count does not match the plan\\n")
				TEXT("nodes: before %d, created %d, removed %d, expected %d, found %d\\n")
				TEXT("target the report would have named: %s"),
				P.NodeCountBefore, P.bCreateTarget ? 1 : 0, P.bKeepSource ? 0 : 1, NodeCountExpected,
				NodeCountAfter, *CreatedTargetId);
			return false;
		}

		Report = FString::Printf(TEXT("changed: repointed %d reader(s) in %s; target %s.%s; old source %s\\n"
			"nodes: before %d, created %d, removed %d, after %d\\n"
			"compile: %s%s"),
			P.Readers.Num(), *P.GraphName, *CreatedTargetId, *P.TargetPin,
			P.bKeepSource ? TEXT("retained") : TEXT("deleted"),
			P.NodeCountBefore, P.bCreateTarget ? 1 : 0, P.bKeepSource ? 0 : 1, NodeCountAfter,
			*FTurboMCPResult::CompileToken(Out.Compile),
			Out.bCompileBaselineUsed
				? *FString::Printf(TEXT(" (compared against the pre-edit diagnostics, scoped to graph '%s'; no new "
				                        "diagnostic naming it, %d other diagnostic(s) in this asset out of scope)"),
				                   *P.GraphName, Out.OutOfScopeDiagnostics.Num())
				: TEXT(""));
	}
	return true;
}"""

if OLD not in text:
    print("MISS: the success block")
else:
    text = text.replace(OLD, NEW)
    print("patched: the guard runs after the commit, and both counts are reported")

# The id must survive the inner scope, so it is declared where the guard can see it.
OLD_DECL = "\tUEdGraphNode* Target = nullptr;\n\tif (P.bCreateTarget)"
NEW_DECL = "\tFString CreatedTargetId;\n\tUEdGraphNode* Target = nullptr;\n\tif (P.bCreateTarget)"
if OLD_DECL in text:
    text = text.replace(OLD_DECL, NEW_DECL, 1)
    print("patched: CreatedTargetId declared at function scope")
else:
    print("MISS: the Target declaration")

PATH.write_text(text, encoding="utf-8")
