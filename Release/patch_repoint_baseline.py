"""Wire ARC04's diagnostic baseline into the graph repoint writer.

Two mechanical edits that PowerShell quoting kept mangling:
  1. the pre-edit baseline now comes from a compile WITH a results log, and the log is normalized to identity
     strings rather than counted;
  2. the `ReadCompileDiagnostics` member is implemented, and the normalizer the edit path already calls is
     declared next to it.
"""

import pathlib

PATH = pathlib.Path(r"C:\Users\Example\Desktop\TurboMCP\Source\TurboMCP\TurboMCPGraphRepoint.cpp")
text = PATH.read_text(encoding="utf-8")

OLD_BEFORE = """	// --- ARC04: the diagnostics BEFORE the edit, on the same object ---
	TArray<FString> Before;
	if (Context.bCompile)
	{
		Before = ReadCompileDiagnostics(B);
	}
"""

NEW_BEFORE = """	// --- ARC04: the diagnostics BEFORE the edit, on the same object ---
	//
	// The baseline is read by COMPILING once WITH a results log. ARC04 allows exactly that ("Use an explicit
	// Compile operation to establish a baseline when no valid baseline exists"), and it is done here rather than
	// once per mutation because this write is about to compile anyway. The cross-call cache ARC04 asks for is
	// NOT implemented yet, and that limit is recorded rather than implied: this compares within one edit.
	TArray<FString> Before;
	TArray<FString> After;
	FCompilerResultsLog Log;
	if (Context.bCompile)
	{
		FKismetEditorUtilities::CompileBlueprint(B, EBlueprintCompileOptions::None, &Log);
		Before = NormalizeDiagnostics(Log);
		Log.Messages.Reset();
	}
"""

OLD_COMPILE = """	// Step 10: compile ONCE per affected compilable asset.
	if (Context.bCompile)
	{
		FKismetEditorUtilities::CompileBlueprint(B);
		Out.CompileRan = true;
		Out.Compile = ETurboMCPResultCompile::Passed;

		TArray<FString> After = ReadCompileDiagnostics(B);
"""

NEW_COMPILE = """	// Step 10: compile ONCE per affected compilable asset, and read what the engine reported.
	if (Context.bCompile)
	{
		FKismetEditorUtilities::CompileBlueprint(B, EBlueprintCompileOptions::None, &Log);
		After = NormalizeDiagnostics(Log);
		Out.CompileRan = true;
		Out.Compile = ETurboMCPResultCompile::Passed;
"""

OLD_APPLY = """bool FTurboMCPGraphRepoint::ApplyTransacted(const FPlan& P, const FApplyContext& Context, FApplyOutcome& Out,
	FString& Report, FString& Error)
{"""

NEW_APPLY = """TArray<FString> FTurboMCPGraphRepoint::ReadCompileDiagnostics(UBlueprint* Blueprint)
{
	// A public member so a caller can compare a baseline WITHOUT an edit, which is what ARC04's explicit Compile
	// tool will need. It compiles and reads the log, and it changes nothing itself.
	FCompilerResultsLog Log;
	if (Blueprint != nullptr)
	{
		FKismetEditorUtilities::CompileBlueprint(Blueprint, EBlueprintCompileOptions::None, &Log);
	}
	return NormalizeDiagnostics(Log);
}

TArray<FString> FTurboMCPGraphRepoint::NormalizeDiagnostics(const FCompilerResultsLog& Log)
{
	// *** IDENTITY, NOT COUNT, AND THAT IS THE WHOLE OF ARC04's DIAGNOSTIC RULE. ***
	//
	// The audit's graph-safety review records that counting errors accepts a CHANGED message at the same count
	// as success - "A changed diagnostic with the same count is still new" - so each message becomes a stable
	// string of its severity and its text, and the two sets are compared as SETS.
	//
	// THE LIMIT IS STATED RATHER THAN HIDDEN: the engine's results log exposes the message and its severity,
	// not the node that produced it, so this detects a new MESSAGE and cannot detect a message whose text is
	// unchanged but whose target moved. ARC04 asks for a node identity "where available"; it is not available
	// through this API on this engine revision, and the honest form is to say so.
	TArray<FString> Out;
	for (const TSharedRef<FTokenizedMessage>& Message : Log.Messages)
	{
		FString Severity;
		switch (Message->GetSeverity())
		{
		case EMessageSeverity::Error:       Severity = TEXT("error"); break;
		case EMessageSeverity::PerformanceWarning:
		case EMessageSeverity::Warning:     Severity = TEXT("warning"); break;
		default:                            continue;  // information and below are not diagnostics for this rule
		}
		Out.Add(FString::Printf(TEXT("%s: %s"), *Severity, *Message->ToText().ToString()));
	}
	// Sorted and de-duplicated, so the comparison does not depend on the order the compiler reported them in -
	// the same reason ARC06 requires unordered sets to be sorted before hashing.
	Out.Sort();
	for (int32 Index = Out.Num() - 1; Index > 0; --Index)
	{
		if (Out[Index] == Out[Index - 1])
		{
			Out.RemoveAt(Index);
		}
	}
	return Out;
}

bool FTurboMCPGraphRepoint::ApplyTransacted(const FPlan& P, const FApplyContext& Context, FApplyOutcome& Out,
	FString& Report, FString& Error)
{"""

for label, old, new in (("before", OLD_BEFORE, NEW_BEFORE),
                        ("compile", OLD_COMPILE, NEW_COMPILE),
                        ("apply", OLD_APPLY, NEW_APPLY)):
    if old not in text:
        print("MISS: %s" % label)
    else:
        text = text.replace(old, new)
        print("patched: %s" % label)

# The normalizer needs declaring on the class, and the compiler results log needs its header.
HEADER = pathlib.Path(r"C:\Users\Example\Desktop\TurboMCP\Source\TurboMCP\Public\TurboMCPGraphRepoint.h")
header = HEADER.read_text(encoding="utf-8")
if "NormalizeDiagnostics" not in header:
    header = header.replace(
        " /** Read the compile diagnostics of one Blueprint, normalized to stable identity strings (ARC04). */\n"
        " static TArray<FString> ReadCompileDiagnostics(UBlueprint* Blueprint);",
        " /** Read the compile diagnostics of one Blueprint, normalized to stable identity strings (ARC04). */\n"
        " static TArray<FString> ReadCompileDiagnostics(UBlueprint* Blueprint);\n"
        " /** Severity + message text for every error and warning, sorted and de-duplicated (ARC04). */\n"
        " static TArray<FString> NormalizeDiagnostics(const struct FCompilerResultsLog& Log);")
    HEADER.write_text(header, encoding="utf-8")
    print("patched: header declaration")

if '#include "Kismet/KismetCompilerLog.h"' not in text and "KismetCompilerLog" not in text:
    # The header that declares FCompilerResultsLog; verified to exist on this engine revision.
    text = text.replace('#include "ScopedTransaction.h"',
                        '#include "Kismet/KismetCompilerLog.h"\n#include "ScopedTransaction.h"')
    print("patched: include")

PATH.write_text(text, encoding="utf-8")
print("done")
