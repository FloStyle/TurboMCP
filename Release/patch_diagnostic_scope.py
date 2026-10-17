"""Route A: scope ARC04's new-diagnostic comparison to the graph the edit touched.

The conflict this resolves, measured:

  * the migrated repoint tool compared the WHOLE ASSET's compile diagnostics before and after;
  * a repoint changes ONE graph;
  * `TurboMCPAcceptance` is DESIGNED to contain non-compiling graphs - `ValueCycleProbe` and `SelfLinkProbe`
    are "pure-value cycles" built so the reader can be tested against a cyclic input (the commandlet says so at
    L851-858 and L1573-1575) - so every edit to any graph was refused for diagnostics that belong to graphs the
    edit never touched.

The correction scopes the comparison to the edited graph, and says so in the message, so the scoping is
explicit rather than a silent weakening. A diagnostic whose text names a DIFFERENT graph still cannot be
attributed, and the code states that limit instead of pretending otherwise.
"""

import pathlib

PATH = pathlib.Path(r"C:\Users\Example\Desktop\TurboMCP\Source\TurboMCP\TurboMCPGraphRepoint.cpp")
text = PATH.read_text(encoding="utf-8")

OLD = """		Out.NewDiagnostics = New;
		if (New.Num() > 0)
		{"""

NEW = """		Out.NewDiagnostics = New;
		if (New.Num() > 0)
		{"""

# The insertion point is the AFTER-comparison block: scope `After` to the edited graph before comparing.
OLD_AFTER = """		FKismetEditorUtilities::CompileBlueprint(B, EBlueprintCompileOptions::None, &Log);
		After = NormalizeDiagnostics(Log);
		Out.bCompileRan = true;
		Out.Compile = ETurboMCPResultCompile::Passed;

		TArray<FString> New;
		for (const FString& Diagnostic : After)
		{
			if (!Before.Contains(Diagnostic))
			{
				New.Add(Diagnostic);
			}
		}"""

NEW_AFTER = """		FKismetEditorUtilities::CompileBlueprint(B, EBlueprintCompileOptions::None, &Log);
		After = NormalizeDiagnostics(Log);
		Out.bCompileRan = true;
		Out.Compile = ETurboMCPResultCompile::Passed;

		// *** THE COMPARISON IS SCOPED TO THE GRAPH THIS EDIT TOUCHED, AND THE MESSAGE SAYS SO. ***
		//
		// MEASURED: comparing the WHOLE ASSET refused every edit on `TurboMCPAcceptance`, because that asset is
		// DESIGNED to contain non-compiling graphs - `ValueCycleProbe` and `SelfLinkProbe` are deliberate
		// pure-value cycles built so the reader can be tested against a cyclic input
		// (TurboMCPAcceptanceAssetCommandlet.cpp L851-858 and L1573-1575). The six diagnostics reported as "the
		// edit would add" belonged to graphs the repoint never touched.
		//
		// A repoint changes ONE graph, so the rule is judged against ONE graph. A diagnostic that names the
		// edited graph is in scope; one that does not is reported as OUT OF SCOPE rather than silently dropped,
		// so a reader can always see what was set aside and why.
		//
		// THE LIMIT IS STATED RATHER THAN HIDDEN: a diagnostic whose text names NO graph cannot be attributed to
		// the edit, so it is treated as out of scope. A package-level failure that the edit genuinely caused and
		// that names no graph would therefore pass this rule - which is why the out-of-scope count is reported.
		const FString ScopeMarker = P.GraphName;
		TArray<FString> InScope;
		TArray<FString> OutOfScope;
		for (const FString& Diagnostic : After)
		{
			// The compiler names the function or graph in messages about it; a message that names the edited
			// graph is about the edited graph.
			if (Diagnostic.Contains(ScopeMarker, ESearchCase::IgnoreCase))
			{
				InScope.Add(Diagnostic);
			}
			else
			{
				OutOfScope.Add(Diagnostic);
			}
		}

		TArray<FString> New;
		for (const FString& Diagnostic : InScope)
		{
			if (!Before.Contains(Diagnostic))
			{
				New.Add(Diagnostic);
			}
		}
		Out.OutOfScopeDiagnostics = OutOfScope;"""

if OLD_AFTER not in text:
    print("MISS: the after-comparison block")
else:
    text = text.replace(OLD_AFTER, NEW_AFTER)
    print("patched: the comparison is now graph-scoped")

# The refusal message must state the scope, so the scoping is explicit and not a silent weakening.
OLD_MSG = """			Error = FString::Printf(
				TEXT("the edit would add %d compile diagnostic(s) to an asset that already had %d. Pre-existing: %s. "
				     "New from this edit: %s"),
				New.Num(), Before.Num(),
				Before.Num() > 0 ? *FString::Join(Before, TEXT(" | ")) : TEXT("(none)"),
				*FString::Join(New, TEXT(" | ")));"""

NEW_MSG = """			Error = FString::Printf(
				TEXT("the edit would add %d compile diagnostic(s) naming graph '%s'. Scoped to that graph, so "
				     "diagnostics in other graphs do not decide this edit; %d other diagnostic(s) in this asset "
				     "were OUT OF SCOPE and are listed below. New from this edit: %s. Out of scope: %s"),
				New.Num(), *ScopeMarker, OutOfScope.Num(),
				*FString::Join(New, TEXT(" | ")),
				OutOfScope.Num() > 0 ? *FString::Join(OutOfScope, TEXT(" | ")) : TEXT("(none)"));"""

if OLD_MSG not in text:
    print("MISS: the refusal message")
else:
    text = text.replace(OLD_MSG, NEW_MSG)
    print("patched: the message states the scope and the out-of-scope count")

# And the success report should say the compile was judged per graph, so a reader knows what was and was not checked.
OLD_OK = """		*FTurboMCPResult::CompileToken(Out.Compile),
		Out.bCompileBaselineUsed ? TEXT(" (compared against the pre-edit diagnostics; no new one)") : TEXT(""));"""
NEW_OK = """		*FTurboMCPResult::CompileToken(Out.Compile),
		Out.bCompileBaselineUsed
			? *FString::Printf(TEXT(" (compared against the pre-edit diagnostics, scoped to graph '%s'; no new "
			                        "diagnostic naming it, %d other diagnostic(s) in this asset out of scope)"),
			                   *ScopeMarker, Out.OutOfScopeDiagnostics.Num())
			: TEXT(""));"""
if OLD_OK not in text:
    print("MISS: the success report")
else:
    text = text.replace(OLD_OK, NEW_OK)
    print("patched: the success report names the scope")

PATH.write_text(text, encoding="utf-8")

# The outcome struct needs the new field.
HEADER = pathlib.Path(r"C:\Users\Example\Desktop\TurboMCP\Source\TurboMCP\Public\TurboMCPGraphRepoint.h")
header = HEADER.read_text(encoding="utf-8")
if "OutOfScopeDiagnostics" not in header:
    header = header.replace(
        "  /** Compile diagnostics the edit INTRODUCED, by normalized identity - not by count (ARC04). */\n"
        "  TArray<FString> NewDiagnostics;",
        "  /** Compile diagnostics the edit INTRODUCED, by normalized identity - not by count (ARC04). */\n"
        "  TArray<FString> NewDiagnostics;\n"
        "  /** Diagnostics in the same asset that name ANOTHER graph, so they cannot decide this edit. Reported,\n"
        "   *  never silently dropped - a reader must be able to see what was set aside and why. */\n"
        "  TArray<FString> OutOfScopeDiagnostics;")
    HEADER.write_text(header, encoding="utf-8")
    print("patched: the header field")
else:
    print("header already has the field")
