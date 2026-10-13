// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "TurboMCPPanelProbeCommandlet.h"

#include "Containers/UnrealString.h"
#include "Misc/Paths.h"

#include "TurboMCPPanelToolRunner.h"

// THE COMPLETE TYPE IS REQUIRED, NOT JUST THE FORWARD DECLARATION THE RUNNER'S HEADER CARRIES.
//
// MEASURED: without this include the build fails in the engine's own templates rather than here:
//
//     PointerIsConvertibleFromTo.h(19,3): error C2027: use of undefined type
//         'UToolCallAsyncResultString'
//     PointerIsConvertibleFromTo.h(19,3): error C2338: static assertion failed:
//         'TPointerIsConvertibleFromTo must not be instantiated with incomplete types'
//     StrongObjectPtrTemplates.h(54,82): error C2338: static assertion failed:
//         'TStrongObjectPtr can only be constructed with UObject types'
//
// The cause is that `FTurboMCPPanelToolRunner` HOLDS a `TStrongObjectPtr<UToolCallAsyncResultString>`
// as a member, so constructing the runner by value needs the complete type - a forward declaration is
// enough to declare a pointer but not to instantiate the owning class. The error surfaces three levels
// down in Core, which is why the three messages name templates rather than this file.
#include "ToolsetRegistry/ToolCallAsyncResultString.h"

#include "Logging.h"
#include "TurboMCPCallTimeline.h"

// THE PANEL ITSELF, so the probe can build the window rather than only the logic behind it.
#include "Framework/Application/SlateApplication.h"
#include "Panel/STurboMCPPanel.h"

#define LOCTEXT_NAMESPACE "TurboMCPPanelProbe"

namespace
{
	/** Folds a name so `TurboMCP.TurboMCPReadToolset` matches `TurboMCPReadToolset`. */
	FString Fold(const FString& InName)
	{
		FString Out;
		Out.Reserve(InName.Len());
		for (const TCHAR Character : InName)
		{
			if (Character != TEXT('.') && Character != TEXT('_') && Character != TEXT('-'))
			{
				Out.AppendChar(FChar::ToLower(Character));
			}
		}
		return Out;
	}

	/** The signature a probe script parses, kept in one place so it cannot drift from the reader. */
	void Line(const FString& InText)
	{
		UE_LOG(LogTurboMCP, Display, TEXT("TurboMCP: %s"), *InText);
	}
}

const TArray<FString>& UTurboMCPPanelProbeCommandlet::TurboMCPToolsetNames()
{
	// THE CLASS NAMES, WHICH IS WHAT THE REGISTRY PUBLISHES, and the probe folds them for matching so
	// a name carrying the module prefix is found either way.
	static const TArray<FString> Names = {
		TEXT("TurboMCPReadToolset"),
		TEXT("TurboMCPLookToolset"),
		TEXT("TurboMCPPreviewToolset"),
		TEXT("TurboMCPEditToolset"),
	};
	return Names;
}

UTurboMCPPanelProbeCommandlet::UTurboMCPPanelProbeCommandlet()
{
	IsClient = false;
	IsServer = false;
	IsEditor = true;
	LogToConsole = true;
}

int32 UTurboMCPPanelProbeCommandlet::Main(const FString& Params)
{
	Line(TEXT("probing the panel's tool runner"));
	Line(FString::Printf(TEXT("  project: %s"), *FPaths::GetProjectFilePath()));

	// ---------------------------------------------------------------------------------------------
	// 1. THE REGISTRY, WHICH EVERYTHING ELSE DEPENDS ON
	// ---------------------------------------------------------------------------------------------
	FString Why;
	if (!FTurboMCPPanelToolRunner::IsRegistryAvailable(Why))
	{
		// A REFUSAL WITH THE REASON, not a silent zero. An unavailable registry would otherwise make
		// every count below read as "this toolset has no tools", which is a different and much worse
		// answer than "the registry is not answering".
		Line(FString::Printf(TEXT("panel-registry: UNAVAILABLE  %s"), *Why));
		Line(TEXT("panel-summary: registry=unavailable toolsets=0 tools=0 partial=0 livecall=skipped"));
		UE_LOG(LogTurboMCP, Error, TEXT("TurboMCP: the toolset registry is not available: %s"), *Why);
		return 1;
	}
	Line(TEXT("panel-registry: available"));

	TArray<FTurboMCPPanelToolRunner::FToolsetInfo> Toolsets;
	FString Error;
	if (!FTurboMCPPanelToolRunner::ListToolsets(Toolsets, Error))
	{
		Line(FString::Printf(TEXT("panel-toolsets: FAILED  %s"), *Error));
		Line(TEXT("panel-summary: registry=available toolsets=0 tools=0 partial=0 livecall=skipped"));
		UE_LOG(LogTurboMCP, Error, TEXT("TurboMCP: could not list the toolsets: %s"), *Error);
		return 1;
	}

	// ---------------------------------------------------------------------------------------------
	// 2. THE TOOLSET LIST, AND THE FOUR THIS PLUGIN OWNS
	// ---------------------------------------------------------------------------------------------
	Line(FString::Printf(TEXT("panel-toolsets: %d published"), Toolsets.Num()));

	// THE FOUR ARE MATCHED BY FOLDED NAME, so a registry that publishes
	// `TurboMCP.TurboMCPReadToolset` is found from the short form and a future rename that only adds a
	// prefix does not silently make all four vanish from this probe.
	TMap<FString, FString> ResolvedNames;   // folded plugin name -> the registry's own name
	int32 MissingToolsets = 0;
	for (const FString& Wanted : TurboMCPToolsetNames())
	{
		const FString WantedFold = Fold(Wanted);
		const FString* Found = nullptr;
		for (const FTurboMCPPanelToolRunner::FToolsetInfo& Info : Toolsets)
		{
			if (Fold(Info.Name) == WantedFold || Fold(Info.Name).EndsWith(WantedFold))
			{
				Found = &Info.Name;
				break;
			}
		}
		if (Found == nullptr)
		{
			++MissingToolsets;
			Line(FString::Printf(TEXT("panel-toolset: MISSING  %s"), *Wanted));
		}
		else
		{
			ResolvedNames.Add(Wanted, *Found);
			Line(FString::Printf(TEXT("panel-toolset: present  %s"), **Found));
		}
	}

	// ---------------------------------------------------------------------------------------------
	// 3 AND 4. EVERY TOOLSET'S TOOLS, AND EVERY TOOL'S SAMPLE
	// ---------------------------------------------------------------------------------------------
	int32 TotalTools = 0;
	int32 TotalPartial = 0;
	int32 FailedToolsets = 0;
	for (const FString& Wanted : TurboMCPToolsetNames())
	{
		const FString* RegistryName = ResolvedNames.Find(Wanted);
		if (RegistryName == nullptr)
		{
			++FailedToolsets;
			continue;
		}

		int32 ToolCount = 0;
		int32 PartialCount = 0;
		if (!ProbeToolset(*RegistryName, ToolCount, PartialCount))
		{
			++FailedToolsets;
			continue;
		}
		TotalTools += ToolCount;
		TotalPartial += PartialCount;
	}

	// THE COUNT PER TOOLSET IS THE CHECK THAT STOPS A TOOLSET SILENTLY SHRINKING. Every other check in
	// this project counts what IS there; none of them can know what SHOULD be. A toolset that lost a
	// tool to a bad merge still lists its remaining tools happily.
	Line(FString::Printf(TEXT("panel-tools: %d total, %d with a partial sample"),
		TotalTools, TotalPartial));

	// ---------------------------------------------------------------------------------------------
	// 5. ONE REAL CALL, MEASURED RATHER THAN ASSUMED
	// ---------------------------------------------------------------------------------------------
	ProbeLiveCall();

	// ---------------------------------------------------------------------------------------------
	// 6. THE CALL TIMELINE'S ROWS, FILTERS AND EXPANSION, ON LITERAL RECORDS
	// ---------------------------------------------------------------------------------------------
	ProbeCallTimeline();

	// ---------------------------------------------------------------------------------------------
	// THE SUMMARY LINE A SCRIPT PARSES
	// ---------------------------------------------------------------------------------------------
	Line(FString::Printf(
		TEXT("panel-summary: registry=available toolsets=%d missing=%d tools=%d partial=%d ")
		TEXT("failedtoolsets=%d"),
		Toolsets.Num(), MissingToolsets, TotalTools, TotalPartial, FailedToolsets));

	// A MISSING TOOLSET OR A FAILED ONE IS A NON-ZERO EXIT, so a script cannot read a broken probe as a
	// pass. The partial-sample count is REPORTED rather than failed on: a partial sample is a tool a
	// human cannot try in one click, which is a real shortfall, but it is a property of that tool's
	// schema rather than a broken probe. The script asserts it separately.
	if (MissingToolsets > 0 || FailedToolsets > 0 || TotalTools == 0)
	{
		UE_LOG(LogTurboMCP, Error,
			TEXT("TurboMCP: the panel probe found %d missing toolset(s), %d failed toolset(s) ")
			TEXT("and %d tool(s) in total"),
			MissingToolsets, FailedToolsets, TotalTools);
		return 1;
	}

	return 0;
}

bool UTurboMCPPanelProbeCommandlet::ProbeToolset(const FString& InToolsetName,
	int32& OutToolCount, int32& OutPartialCount)
{
	TArray<FTurboMCPPanelToolRunner::FToolInfo> Tools;
	FString Error;
	if (!FTurboMCPPanelToolRunner::ListTools(InToolsetName, Tools, Error))
	{
		Line(FString::Printf(TEXT("panel-toolset: FAILED   %s  %s"), *InToolsetName, *Error));
		OutToolCount = 0;
		OutPartialCount = 0;
		return false;
	}

	int32 PartialCount = 0;
	int32 NoSample = 0;
	for (const FTurboMCPPanelToolRunner::FToolInfo& Tool : Tools)
	{
		if (Tool.SampleJson.IsEmpty())
		{
			// A TOOL WITH NO SAMPLE IS A TOOL A HUMAN CANNOT TRY, which is the user's requirement
			// failing for that tool. It is named individually so the report says WHICH tool, rather
			// than only that some count was short.
			++NoSample;
			Line(FString::Printf(TEXT("panel-nosample: %s"), *Tool.BareName));
			continue;
		}
		if (Tool.bSampleIsPartial)
		{
			++PartialCount;
			Line(FString::Printf(TEXT("panel-partial: %s  params=[%s]"),
				*Tool.BareName, *FString::Join(Tool.Parameters, TEXT(", "))));
		}
	}

	// THE COUNT AND THE SAMPLE COVERAGE TOGETHER, on one line, so a script can assert both without
	// parsing a list.
	//
	// THE BARE CLASS NAME IS PRINTED BESIDE THE REGISTRY NAME, and that was added after the FIRST RUN
	// OF THE PROBE SCRIPT FAILED EIGHT CHECKS FOR THE WRONG REASON. The registry publishes
	// `TurboMCP.TurboMCPReadToolset` while the header that declares the tools names the class
	// `UTurboMCPReadToolset`, so a script matching on the class name found nothing on this line and
	// reported "the toolset has no tools" when it had twenty-one. The line carries BOTH names so a
	// reader can match whichever it holds, which is cheaper than making every reader know the
	// difference between a registry name and a class name.
	const FString BareClass = InToolsetName.Contains(TEXT("."))
		? InToolsetName.RightChop(InToolsetName.Find(TEXT("."), ESearchCase::CaseSensitive,
			ESearchDir::FromEnd) + 1)
		: InToolsetName;

	Line(FString::Printf(
		TEXT("panel-toolset-tools: %s %s %d tool(s), %d partial, %d with no sample"),
		*InToolsetName, *BareClass, Tools.Num(), PartialCount, NoSample));

	OutToolCount = Tools.Num();
	OutPartialCount = PartialCount;

	// A TOOLSET WITH NO TOOLS IS A FAILURE RATHER THAN A ZERO. `ListTools` succeeding with an empty
	// array is exactly the silent shrink this probe exists to catch.
	return Tools.Num() > 0;
}

void UTurboMCPPanelProbeCommandlet::ProbeLiveCall()
{
	// THE TOOL IS CHOSEN BECAUSE IT NEEDS NO EDITOR STATE AND NO ARGUMENT. `LookTurboMCPToolset` reads
	// the live Slate window list and takes `{}`. A tool that needed an asset would make this probe
	// depend on the test project's contents, and a failure would then have two possible causes.
	//
	// THE TIMEOUT IS SHORT ON PURPOSE. `CallAndWait` sleeps on the CALLING thread while the completion
	// is delivered by the editor's tick, and a `-run=` commandlet has no editor tick. If that makes the
	// call impossible, a two-second measurement reports it; a ten-second one would only waste eight
	// seconds arriving at the same answer.
	const double TimeoutSeconds = 2.0;

	FTurboMCPPanelToolRunner Runner;
	const double Started = FPlatformTime::Seconds();
	const FString Answer = Runner.CallAndWait(
		TEXT("TurboMCP.TurboMCPLookToolset"), TEXT("LookTurboMCPToolset"), TEXT("{}"),
		TimeoutSeconds);
	const double Elapsed = FPlatformTime::Seconds() - Started;

	const FTurboMCPPanelToolRunner::FCallResult& Result = Runner.GetResult();

	// A DISTINCT LINE PER OUTCOME, because "it timed out" and "it never ran" and "it answered" are
	// three different facts and a script has to be able to tell them apart.
	if (Result.bNeverRan)
	{
		Line(FString::Printf(
			TEXT("panel-livecall: NEVER-RAN  %.2fs  %s"), Elapsed, *Result.Error));
	}
	else if (Result.bIsError && Elapsed >= TimeoutSeconds)
	{
		// A TIMEOUT IS THE EXPECTED OUTCOME IF THERE IS NO EDITOR TICK, and it is reported as a
		// MEASUREMENT rather than as a failure of the probe: it is a fact about what a commandlet can
		// do, not about whether the runner works.
		Line(FString::Printf(
			TEXT("panel-livecall: TIMEOUT  %.2fs  the registry did not answer without an editor ")
			TEXT("tick, which is a measured LIMIT of probing a call from a commandlet rather than a ")
			TEXT("defect in the runner"), Elapsed));
	}
	else if (Result.bIsError)
	{
		Line(FString::Printf(TEXT("panel-livecall: ERROR  %.2fs  %s"), Elapsed, *Result.Error));
	}
	else
	{
		// THE ANSWER IS PRINTED IN FULL AND ITS CHARACTER COUNT, because the point of the call is that
		// the panel would have shown this text. An empty value here would be the "tool that returned
		// nothing" case, which is what `P1` forbids, so it is called out.
		Line(FString::Printf(TEXT("panel-livecall: ANSWER  %.2fs  %d character(s)"),
			Elapsed, Result.CharacterCount));
		Line(FString::Printf(TEXT("panel-livecall-value: %s"),
			*Answer.Replace(TEXT("\n"), TEXT(" | "))));
		if (Result.CharacterCount == 0)
		{
			Line(TEXT("panel-livecall-note: the tool answered NOTHING, which P1 forbids"));
		}
	}
}

void UTurboMCPPanelProbeCommandlet::ProbeCallTimeline()
{
	// *** A TYPE ALIAS AND NOT A `using FTurboMCPCallLog::FEntry;` DECLARATION, WHICH MSVC REFUSES AT
	// FUNCTION SCOPE: `error C2885: invalid using-declaration at a scope that is not a class`. *** The
	// alias is the same type by another name and is legal here.
	using FEntry = FTurboMCPCallLog::FEntry;

	// LITERAL RECORDS, WRITTEN HERE RATHER THAN READ FROM THE LOG. See the header for why: a check
	// that reads whatever happened to be recorded passes by finding nothing in a quiet session.
	//
	// THE SHAPE IS ONE OF EACH THING THE TIMELINE HAS TO GET RIGHT:
	//   call 1  a plugin start and end, PLUS the engine's own event for the same call - four records
	//           that must collapse to ONE row, or every total in `R-CALL-COST` doubles
	//   call 2  an engine-only call: the `call_tool` meta-tool, which has no start record of its own
	//   call 3  a plugin start with NO end: the crash shape, which must render as `running`
	FEntry Start1;
	Start1.Timestamp = TEXT("2026-01-01T00:00:01.000Z");
	Start1.Phase = TEXT("start");
	Start1.Source = TEXT("plugin");
	Start1.Toolset = TEXT("TurboMCPReadToolset");
	Start1.Tool = TEXT("ReadTurboMCPStatus");
	Start1.Arguments = TEXT("{\"asset\":\"BP_NodeTest\"}");

	FEntry End1;
	End1.Timestamp = TEXT("2026-01-01T00:00:01.100Z");
	End1.Phase = TEXT("end");
	End1.Source = TEXT("plugin");
	End1.Toolset = TEXT("TurboMCPReadToolset");
	End1.Tool = TEXT("ReadTurboMCPStatus");
	End1.Arguments = TEXT("{\"asset\":\"BP_NodeTest\"}");
	End1.DurationMs = 100.0;
	End1.ResultChars = 336;
	End1.TokenEstimate = 111;
	End1.Result = TEXT("read: the status\nline two\nline three");

	FEntry EngineInner;
	EngineInner.Timestamp = TEXT("2026-01-01T00:00:01.200Z");
	EngineInner.Phase = TEXT("end");
	EngineInner.Source = TEXT("engine");
	EngineInner.SessionId = TEXT("session-a");
	EngineInner.Toolset = TEXT("TurboMCP.TurboMCPReadToolset");
	EngineInner.Tool = TEXT("TurboMCP.TurboMCPReadToolset.ReadTurboMCPStatus");
	EngineInner.DurationMs = 120.0;

	FEntry EngineOnly;
	EngineOnly.Timestamp = TEXT("2026-01-01T00:00:02.000Z");
	EngineOnly.Phase = TEXT("end");
	EngineOnly.Source = TEXT("engine");
	EngineOnly.SessionId = TEXT("session-b");
	EngineOnly.Toolset = TEXT("(top-level)");
	EngineOnly.Tool = TEXT("call_tool");
	EngineOnly.DurationMs = 2.0;
	EngineOnly.bError = true;

	FEntry LostStart;
	LostStart.Timestamp = TEXT("2026-01-01T00:00:03.000Z");
	LostStart.Phase = TEXT("start");
	LostStart.Source = TEXT("plugin");
	LostStart.Toolset = TEXT("TurboMCPEditToolset");
	LostStart.Tool = TEXT("EditTurboMCPSave");
	LostStart.Arguments = TEXT("{\"assets\":\"/Game/Tests/BP_NodeTest\"}");

	TArray<FEntry> Records;
	Records.Add(Start1);
	Records.Add(End1);
	Records.Add(EngineInner);
	Records.Add(EngineOnly);
	Records.Add(LostStart);

	const TArray<FEntry> Calls = FTurboMCPCallTimeline::Select(Records,
		FTurboMCPCallTimeline::FFilter());

	// *** FIVE RECORDS, THREE CALLS: THE ENGINE'S EVENT FOR A PLUGIN CALL IS MERGED, NOT COUNTED. ***
	Line(FString::Printf(TEXT("panel-timeline: %d call(s) from %d record(s)"),
		Calls.Num(), Records.Num()));

	// THE ROW TEXT COMES FROM THE SAME RENDERER THE PANEL'S ROWS USE.
	for (const FEntry& Call : Calls)
	{
		Line(FString::Printf(TEXT("panel-timeline-row: %s"), *FTurboMCPCallTimeline::RenderRow(Call)));
	}

	// THE CRASH SHAPE: a start with no end reads `running` rather than being dropped.
	int32 Running = 0;
	for (const FEntry& Call : Calls)
	{
		if (Call.Phase == TEXT("start"))
		{
			++Running;
		}
	}
	Line(FString::Printf(TEXT("panel-timeline-running: %d"), Running));

	// THE FOUR FILTERS, EACH AGAINST ITS OWN COLUMN - and each one's kept count is arithmetic, so a
	// filter that matched everything by accident is visible as `kept=3` where 1 was expected.
	auto ReportFilter = [&Calls, &Records](const TCHAR* InName, const FTurboMCPCallTimeline::FFilter& InFilter)
	{
		const int32 Kept = FTurboMCPCallTimeline::Select(Records, InFilter).Num();
		Line(FString::Printf(TEXT("panel-timeline-filter-%s: kept=%d of %d"),
			InName, Kept, Calls.Num()));
	};

	FTurboMCPCallTimeline::FFilter ByToolset;
	ByToolset.Toolset = TEXT("ReadToolset");
	ReportFilter(TEXT("toolset"), ByToolset);

	FTurboMCPCallTimeline::FFilter ByTool;
	ByTool.Tool = TEXT("EditTurboMCPSave");
	ReportFilter(TEXT("tool"), ByTool);

	FTurboMCPCallTimeline::FFilter ByAsset;
	ByAsset.Asset = TEXT("BP_NodeTest");
	ReportFilter(TEXT("asset"), ByAsset);

	FTurboMCPCallTimeline::FFilter ByStatus;
	ByStatus.Status = TEXT("error");
	ReportFilter(TEXT("status"), ByStatus);

	// THE GROUPING: a heading row per session rather than the session repeated on every row. Two
	// sessions in the literal records above - the plugin call carries `session-a` through the engine's
	// own event for it, and the engine-only call carries `session-b` - so the list is five entries:
	// heading, call, heading, call, call.
	const TArray<FTurboMCPCallTimeline::FListEntry> List =
		FTurboMCPCallTimeline::ListEntries(Records, FTurboMCPCallTimeline::FFilter());
	int32 Headings = 0;
	for (const FTurboMCPCallTimeline::FListEntry& Entry : List)
	{
		if (Entry.bHeading)
		{
			++Headings;
		}
	}
	Line(FString::Printf(TEXT("panel-timeline-list: %d entries, %d heading(s), %d call(s)"),
		List.Num(), Headings, List.Num() - Headings));

	// THE EXPANSION: the full arguments and the first lines of the result, with what it left out said
	// out loud rather than left to look complete.
	const FString Expanded = FTurboMCPCallTimeline::Expand(End1, /*InResultLines=*/2);
	Line(FString::Printf(TEXT("panel-timeline-expand: arguments=%s result_lines=%d truncated=%s"),
		Expanded.Contains(TEXT("{\"asset\"")) ? TEXT("yes") : TEXT("no"),
		Expanded.Contains(TEXT("line two")) ? 2 : 0,
		Expanded.Contains(TEXT("more line(s)")) ? TEXT("yes") : TEXT("no")));

	ProbeTimelineWidget();
}

void UTurboMCPPanelProbeCommandlet::ProbeTimelineWidget()
{
	// *** THE WIDGET ITSELF, WHICH UNTIL NOW HAD NEVER BEEN BUILT BY ANYTHING BUT A HUMAN OPENING THE
	// WINDOW. *** `QUESTIONS.md` Q-11 says the widget tree cannot be read, and that is still true - but
	// "cannot be inspected" is not "cannot be EXECUTED", and the difference is the whole of this
	// method: `STurboMCPPanel::Construct` builds the filter row, the list and the empty-state text, and
	// a mistake in any of those - a bad lambda, a list reading from a dead array - only appears the day
	// a human opens the window, which is the worst time to find it.
	//
	// A PROCESS WITH NO SLATE IS REPORTED RATHER THAN SILENT: a commandlet run with `-nullrhi` may have
	// no Slate application at all, and a skip that says so is a measurement while a skip that says
	// nothing is the vacuous pass this project blocks suites for.
	if (!FSlateApplication::IsInitialized())
	{
		Line(TEXT("panel-widget: SKIPPED (this process has no Slate application, so the window cannot be built here)"));
		return;
	}

	TSharedRef<STurboMCPPanel> Panel = SNew(STurboMCPPanel);
	Line(FString::Printf(TEXT("panel-widget: BUILT  timeline_rows=%d  count_text=\"%s\""),
		Panel->GetTimelineRowCount(), *Panel->GetTimelineCountText().ToString()));
}

#undef LOCTEXT_NAMESPACE