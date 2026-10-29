// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "Tools/TurboMCPToolsets.h"
#include "TurboMCPCallRecorder.h"

#include "Containers/UnrealString.h"
#include "Editor.h"
#include "Editor/EditorEngine.h"
#include "EditorAssetLibrary.h"
#include "EditorFramework/AssetImportData.h"
#include "Engine/Blueprint.h"
#include "FileHelpers.h"
#include "Kismet2/CompilerResultsLog.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "UObject/Object.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "UObject/UObjectGlobals.h"
#include "WidgetBlueprint.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "TurboMCPGraphReachability.h"
#include "TurboMCPNodeId.h"

#include "Logging.h"
#include "TurboMCPAssetResolver.h"
#include "TurboMCPLogCapture.h"
#include "TurboMCPSourceControlWrite.h"

/** Compile and save inspect actual per-asset results. Neither operation promises
 * transaction rollback. Compilation can reinstance classes; save writes packages.
 * Save warnings are summarized while compiler diagnostics retain actionable text.
 */
namespace
{
	/** How many assets one compile or save call may handle. */
	constexpr int32 MaxAssetsPerCall = 25;

	/** Why a write was refused, or empty when it can proceed. */
	FString WriteRefusalReason()
	{
		if (!GIsEditor)
		{
			return TEXT("not in the editor, so nothing can be compiled or saved");
		}
		if (GEditor == nullptr)
		{
			return TEXT("the editor is not available yet, try again once it has loaded");
		}

		// D-34: saving during play or simulate failed or half worked, so both are
		// refused with the reason rather than attempted.
		if (GEditor->IsPlaySessionInProgress())
		{
			return TEXT("a play in editor session is running or queued, and saving during ")
				TEXT("play is refused (D-34). Nothing saved or queued. Earlier edits may still be unsaved; this refusal does not undo them. Stop the session, call ReadTurboMCPDirtyList, inspect and compile changed assets, then explicitly save the assets you want to keep.");
		}
		if (GEditor->IsSimulateInEditorInProgress())
		{
			return TEXT("a simulate in editor session is running or queued, and saving during ")
				TEXT("simulate is refused (D-34). Nothing saved or queued. Earlier edits may still be unsaved; this refusal does not undo them. Stop simulation, call ReadTurboMCPDirtyList, inspect and compile changed assets, then explicitly save the assets you want to keep.");
		}
		return FString();
	}

	/** True when the asset kind can be compiled at all. */
	bool CanCompile(const UObject* InAsset, UBlueprint*& OutBlueprint)
	{
		OutBlueprint = const_cast<UBlueprint*>(Cast<UBlueprint>(InAsset));
		return OutBlueprint != nullptr;
	}

	/**
	 * One line per compiler message, tied to the node that caused it where the
	 * engine gives one.
	 *
	 * `E-COMPILE` asks for each warning and error tied to the ID of the node that
	 * caused it. The node GUID is the same value `04_FORMATS.md` section 3 builds
	 * a node ID from, so printing it is what lets a later call act on the error.
	 */
	FString DescribeCompileMessages(const FCompilerResultsLog& InLog, int32 InMaxMessages)
	{
		FString Lines;
		int32 Shown = 0;

		for (const TSharedRef<FTokenizedMessage>& Message : InLog.Messages)
		{
			if (Shown >= InMaxMessages)
			{
				break;
			}

			const EMessageSeverity::Type Severity = Message->GetSeverity();
			const TCHAR* SeverityText =
				Severity == EMessageSeverity::Error ? TEXT("error")
				: Severity == EMessageSeverity::PerformanceWarning ? TEXT("warning")
				: Severity == EMessageSeverity::Warning ? TEXT("warning")
				: TEXT("note");

			FString Text = Message->ToText().ToString();
			Text.TrimStartAndEndInline();
			int32 Newline = INDEX_NONE;
			if (Text.FindChar(TEXT('\n'), Newline))
			{
				Text = Text.Left(Newline).TrimEnd();
			}

			// The message text already names the node or property that caused it,
			// because the engine builds it that way. No message token is read
			// here: the object token type the engine uses internally is not part
			// of its public headers, so reaching for it does not compile.
			Lines += FString::Printf(TEXT("\n    %s: %s"), SeverityText, *Text);
			++Shown;
		}

		if (InLog.Messages.Num() > Shown)
		{
			Lines += FString::Printf(TEXT("\n    (%d more message(s) not shown)"),
				InLog.Messages.Num() - Shown);
		}

		return Lines;
	}

	/** True when the package holding this object still has unsaved changes. */
	bool HasUnsavedChanges(const FString& InPackagePath)
	{
		const UPackage* Package = FindPackage(nullptr, *InPackagePath);
		return Package != nullptr && Package->IsDirty();
	}
}

FString UTurboMCPEditToolset::EditTurboMCPCompile(const FString& Assets)
{
	const TArray<FString> Parts = FTurboMCPAssetResolver::SplitList(Assets);
	if (Parts.Num() == 0)
	{
		return TEXT("error: no asset given. Pass one or more assets separated by commas or pipes.");
	}
	if (Parts.Num() > MaxAssetsPerCall)
	{
		return FString::Printf(
			TEXT("error: %d assets given, and one call handles at most %d. Split the list."),
			Parts.Num(), MaxAssetsPerCall);
	}

	// Public tool dispatch refuses asset edits during PIE. Internal callers still
	// require an available editor before entering the compiler.
	if (!GIsEditor || GEditor == nullptr)
	{
		return FString::Printf(TEXT("error: %s."), *WriteRefusalReason());
	}

	// Compilation can reinstance dependent Widget classes. An enclosing script's
	// undo cannot reliably restore that object graph, so refuse before loading assets.
	if (GEditor->IsTransactionActive())
	{
		return TEXT("error: compile refused while an editor transaction is active. ")
			TEXT("Finish the edit batch and verify its outcome, then compile in a separate call. ")
			TEXT("Nothing was compiled; the caller's transaction was not ended or undone.");
	}

	FString Result = FString::Printf(TEXT("compiling %d asset(s)"), Parts.Num());

	int32 CleanCount = 0;
	int32 WarningCount = 0;
	int32 ErrorCount = 0;
	int32 RefusedCount = 0;
	FString Lines;
	FString Errors;

	// Warnings raised by the compile pass itself are counted so a compile that
	// logs a warning says so, without any of the text reaching the result.
	FTurboMCPLogCapture Capture;

	for (const FString& Part : Parts)
	{
		FTurboMCPAssetResolver::FResolved Resolved;
		const FString ResolveError = FTurboMCPAssetResolver::Resolve(Part, Resolved);
		if (!ResolveError.IsEmpty())
		{
			Errors += TEXT("\n") + ResolveError;
			++RefusedCount;
			continue;
		}

		// The object must be loaded to compile. Loading is what an Edit may do;
		// it does not change the asset.
		UObject* Loaded = FindObject<UObject>(nullptr, *Resolved.ObjectPath);
		if (Loaded == nullptr)
		{
			Loaded = LoadObject<UObject>(nullptr, *Resolved.ObjectPath);
		}

		if (Loaded == nullptr)
		{
			// P1: name why rather than reporting a bare failure.
			Errors += FString::Printf(
				TEXT("\nerror: %s could not be loaded, so it could not be compiled."),
				*Resolved.ObjectPath);
			++RefusedCount;
			continue;
		}

		UBlueprint* Blueprint = nullptr;
		if (!CanCompile(Loaded, Blueprint) || Blueprint == nullptr)
		{
			Errors += FString::Printf(
				TEXT("\nerror: %s is a %s, which cannot be compiled. ")
				TEXT("Only Blueprints and widgets compile."),
				*Resolved.ObjectPath, *Loaded->GetClass()->GetName());
			++RefusedCount;
			continue;
		}

		// *** THE MARK IS TAKEN BEFORE THE COMPILE SO THE CAPTURE CAN ATTRIBUTE WHAT FOLLOWS TO THIS ASSET. ***
		// TWO MARKS, BECAUSE THEY INDEX TWO DIFFERENT STORES: `Mark()` counts warnings, `ErrorMark()` counts
		// errors, and reading one as the other would attribute the wrong messages to this asset.
		const int32 CompileMark = Capture.Mark();
		const int32 CompileErrorMark = Capture.ErrorMark();

		FCompilerResultsLog CompileLog;
		CompileLog.bLogInfoOnly = false;
		FKismetEditorUtilities::CompileBlueprint(Blueprint, EBlueprintCompileOptions::None, &CompileLog);

		const bool bHasErrors = CompileLog.NumErrors > 0;
		// *** THE SENTENCE BELOW WAS DECIDED BY `CompileLog` ALONE, AND `FCompilerResultsLog` CARRIES ONLY WHAT THE
		// COMPILER REPORTED. AN ENSURE IS NOT A COMPILER MESSAGE: THE ENGINE LOGS IT TO THE OUTPUT LOG AT `Error` OR
		// `Warning` (`AssertionMacros.cpp:679` AND `:683`), WHICH IS WHY A COMPILE THAT RAISED ONE WAS STILL ANSWERED
		// `clean, 0 warnings`. *** *** THE CAPTURE IS ALREADY HERE AND ALREADY READ LATER FOR THE SAVE'S WARNINGS
		// (`:285`), SO THE VERDICT CONSULTS IT TOO - AND `Capture.WarningCountSince(CompileMark)` IS WHAT ATTRIBUTES
		// THE ENGINE'S MESSAGES TO THIS ASSET RATHER THAN TO ANOTHER. ***
		// BOTH SEVERITIES ARE READ, NOT JUST THE WARNINGS: `GEnsuresAreErrors` logs an ensure at `Error`, so a
		// capture read for warnings alone answers `clean` for exactly the compile this task names.
		const int32 CapturedWarnings = Capture.WarningCountSince(CompileMark);
		const int32 CapturedErrors = Capture.ErrorCountSince(CompileErrorMark);
		const bool bHasWarnings = CompileLog.NumWarnings > 0 || CapturedWarnings > 0;
		// AN ENGINE ERROR ALONE IS NOT A COMPILER ERROR, SO IT DOES NOT MAKE THE COMPILE FAIL - BUT IT IS NOT
		// `clean` EITHER, WHICH IS THE ONE THING `T04` FORBIDS.
		const bool bEngineLoggedSomething = CapturedWarnings > 0 || CapturedErrors > 0;

		if (bHasErrors)
		{
			++ErrorCount;
		}
		else if (bHasWarnings || bEngineLoggedSomething)
		{
			++WarningCount;
		}
		else
		{
			++CleanCount;
		}

		// THE SENTENCE NAMES THE SOURCE OF EACH NUMBER, because "3 warnings" cannot be told apart from a
		// capture that counted three lines it never read, and this task exists exactly because a verdict
		// that reads one source reported nothing about another.
		const FString EngineNote = bEngineLoggedSomething
			? FString::Printf(TEXT(", %d warning(s)/%d error(s) from the engine log"),
				CapturedWarnings, CapturedErrors)
			: FString();

		const FString Status = bHasErrors
			? FString::Printf(TEXT("error (%d errors, %d warnings)"), CompileLog.NumErrors, CompileLog.NumWarnings)
			: bHasWarnings
				? FString::Printf(TEXT("clean with %d warning(s)"), CompileLog.NumWarnings + CapturedWarnings) + EngineNote
				: bEngineLoggedSomething
					? FString::Printf(TEXT("not clean, the compiler reported nothing but the engine log did%s"), *EngineNote)
					: TEXT("clean");

		// T-STRUCT-7: an unsaved asset must say it compiled but is not saved, so
		// the caller does not believe a compile put the work on disk.
		const bool bDirty = HasUnsavedChanges(Resolved.PackagePath);

		Lines += FString::Printf(TEXT("\n%s  [%s]"), *Resolved.ObjectPath, *Status);
		Lines += bDirty ? TEXT("  unsaved changes; compile did not save")
			: TEXT("  package not marked dirty; disk content not verified; compile did not save");

        // X-41: compiler success is not evidence of a connected execution path.
        TArray<UEdGraph*> Graphs;
        Blueprint->GetAllGraphs(Graphs);
        TSet<const UEdGraph*> SeenGraphs;
        int32 UnreachableExecNodes = 0;
        int32 ShownExecNodes = 0;
        FString ReachabilityDetails;
        for (const UEdGraph* Graph : Graphs)
        {
            if (!Graph || SeenGraphs.Contains(Graph) || !Cast<UEdGraphSchema_K2>(Graph->GetSchema())) continue;
            SeenGraphs.Add(Graph);
            TSet<const UEdGraphNode*> Reachable;
            TurboMCPGraphReachability::MarkReachable(Graph, Reachable);
            for (const UEdGraphNode* Node : Graph->Nodes)
            {
                if (!Node || Reachable.Contains(Node)) continue;
                bool HasExecutionInput = false;
                for (const UEdGraphPin* Pin : Node->Pins)
                    if (Pin && !Pin->bOrphanedPin && Pin->Direction == EGPD_Input && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec)
                        HasExecutionInput = true;
                if (!HasExecutionInput) continue;
                ++UnreachableExecNodes;
                if (ShownExecNodes++ < 20)
                    ReachabilityDetails += FString::Printf(TEXT("\n  %s: %s (%s)"), *Graph->GetName(),
                        *TurboMCPNodeId::MakeForNode(Node, true), *TurboMCPNodeId::GetNodeTitle(Node));
            }
        }
        Lines += FString::Printf(TEXT("\n  structural reachability: %d execution-input node(s) not reached from recognized entries; separate from compiler verdict; not runtime proof"), UnreachableExecNodes);
        Lines += ReachabilityDetails;
        if (UnreachableExecNodes > 20)
            Lines += FString::Printf(TEXT("\n  %d more; use ReadTurboMCPGraphLoad for graph detail"), UnreachableExecNodes - 20);

		// THE COMPILER'S OWN MESSAGES ARE TIED TO THEIR NODE IDS; THE ENGINE'S ARE TIED TO THEIR SPAN.
		// Both are listed, because a count with no text cannot be acted on and `T04` asks for each one.
		if (bHasErrors || bHasWarnings)
		{
			Lines += DescribeCompileMessages(CompileLog, /*InMaxMessages=*/6);
		}

		if (CapturedWarnings > 0)
		{
			Lines += FString::Printf(TEXT("\n    the engine logged %d warning(s) during this compile:"),
				CapturedWarnings);
			Lines += Capture.SummariseSince(CompileMark);
		}

		if (CapturedErrors > 0)
		{
			Lines += FString::Printf(TEXT("\n    the engine logged %d error(s) during this compile:"),
				CapturedErrors);
			Lines += Capture.SummariseErrorsSince(CompileErrorMark);
		}
	}

	Result += FString::Printf(TEXT(": %d clean, %d with warnings, %d with errors"),
		CleanCount, WarningCount, ErrorCount);
	if (RefusedCount > 0)
	{
		Result += FString::Printf(TEXT(", %d refused"), RefusedCount);
	}

	// P4: a result opens with either `unchanged` and the reason, or a summary of
	// what changed. A compile that refused everything changed nothing, so it must
	// not announce a change. A compile does not alter the asset's meaning, so the
	// honest opening is the outcome rather than the bare word "changed".
	const int32 CompiledCount = CleanCount + WarningCount + ErrorCount;
	if (CompiledCount == 0)
	{
		Result = FString::Printf(
			TEXT("unchanged: nothing was compiled, %d asset(s) refused"), RefusedCount);
	}
	else
	{
		Result = FString::Printf(TEXT("changed: compiled %d of %d asset(s)"),
			CompiledCount, Parts.Num());
	}

	Result += FString::Printf(TEXT("\n%d clean, %d with warnings, %d with errors"),
		CleanCount, WarningCount, ErrorCount);
	if (RefusedCount > 0)
	{
		Result += FString::Printf(TEXT(", %d refused"), RefusedCount);
	}

	Result += Lines;

	if (Capture.WarningCount() > 0)
	{
		// Counted, not pasted, so P6 holds while the caller still learns a
		// warning was raised.
		Result += FString::Printf(TEXT("\nengine warnings raised during compile: %d"),
			Capture.WarningCount());
	}

	// THE ERRORS GET A LINE OF THEIR OWN, AND THEY HAVE TO: a compile that logged an engine error and no
	// compiler message has no other place to say so, and `T04` requires that it never reads as clean.
	if (Capture.ErrorCount() > 0)
	{
		Result += FString::Printf(TEXT("\nengine errors raised during compile: %d"),
			Capture.ErrorCount());
	}

	Result += Errors;

	return Result;
}

/**
 * The save itself, shared by `E-SAVE` and `E-SAVE-WITHOUT-THUMBNAIL`.
 *
 * *** ONE BODY, TWO TOOLS, AND THE MECHANISM IS THE ONLY DIFFERENCE. *** A retry that quietly went back
 * to the other mechanism would defeat the tool the caller chose, and the caller would have no way to
 * see it, so the choice is made once and used by both attempts.
 *
 * A file-scope function rather than a member: the toolset's public surface is the two declared tools,
 * and a third entry point on the class would be published as one.
 */
static FString SaveTurboMCPAssets(const FString& Assets, bool bWithoutThumbnail)
{
	const TArray<FString> Parts = FTurboMCPAssetResolver::SplitList(Assets);
	if (Parts.Num() == 0)
	{
		return TEXT("error: no asset given. Pass one or more assets separated by commas or pipes.");
	}
	if (Parts.Num() > MaxAssetsPerCall)
	{
		return FString::Printf(
			TEXT("error: %d assets given, and one call handles at most %d. Split the list."),
			Parts.Num(), MaxAssetsPerCall);
	}

	// D-34: refuse while play or simulate runs, with the reason.
	const FString Refusal = WriteRefusalReason();
	if (!Refusal.IsEmpty())
	{
		return FString::Printf(TEXT("unchanged: %s."), *Refusal);
	}
	if (GEditor->IsTransactionActive())
	{
		return TEXT("error: save refused while an editor transaction is active. ")
			TEXT("Finish the batch and verify its outcome, then save separately. Nothing was saved.");
	}


	// =============================================================================================
	// ROW 10.2 / D-57: THE AUTOMATIC CHECKOUT THIS WRITE TOOL PERFORMS BEFORE IT WRITES.
	//
	// WHY HERE AND NOT IN THE SAVE LOOP. `E-SAVE` IS THE TOOL THAT ACTUALLY WRITES A FILE, so it is the
	// one place where taking a lock can matter - and the engine's own save path ALREADY issues a
	// checkout on every call without reading either fact that says whether one can mean anything:
	// `UEditorAssetLibrary::SaveLoadedAsset` -> `UEditorLoadingAndSavingUtils::SavePackages` ->
	// `FileHelpers.cpp:6019-6023` -> `InternalCheckoutAndSavePackages`, whose `:5925-5929` calls
	// `FEditorFileUtils::CheckoutPackages` whenever `ISourceControlModule::Get().IsEnabled()`, ignoring
	// the operation's result. THIS DOES THE SAME OPERATION EARLIER, AND RECORDS THE ANSWER. That is why
	// row 10.2 exists, and it is why the block below is printed rather than acted on silently.
	//
	// IT IS PLACED AFTER THE THREE REFUSALS ABOVE, so a call that will not write anything asks no
	// provider anything, and BEFORE the loop, because every asset the caller named is one the caller
	// declared an intent to write - the engine's own path checks out every package it is handed too.
	//
	// *** THE ANSWER IS A REPORT AND NEVER A GATE. *** D-57's own reason for the ordering is that an
	// automatic checkout must not turn a source control problem into an "every tool reports failure"
	// problem. On Git (and on an editor with no provider) a gate would refuse every save in this project.
	// =============================================================================================
	const FString CheckoutReport = FTurboMCPSourceControlWrite::EnsureCheckedOut(
		FTurboMCPSourceControlWrite::Plan(Parts), TEXT("E-SAVE"));

	// Warnings raised during the saves are counted and attributed, never pasted.
	FTurboMCPLogCapture Capture;

	FString Result = FString::Printf(TEXT("saving %d asset(s)"), Parts.Num());

	int32 SavedCount = 0;
	int32 NoChangeCount = 0;
	int32 FailedCount = 0;
	FString Lines;
	FString Errors;

	for (const FString& Part : Parts)
	{
		FTurboMCPAssetResolver::FResolved Resolved;
		const FString ResolveError = FTurboMCPAssetResolver::Resolve(Part, Resolved);
		if (!ResolveError.IsEmpty())
		{
			Errors += TEXT("\n") + ResolveError;
			++FailedCount;
			continue;
		}

		// Explicit save requests also write packages whose dirty flag is clear.
        // Native pin setters can change memory without setting that flag.

		const int32 MarkBeforeSave = Capture.Mark();
		const int32 ErrorMarkBeforeSave = Capture.ErrorMark();

		// The asset object is needed for the save call, so it is loaded when it
		// is not already in memory. Loading is not an edit.
		UObject* AssetObject = FindObject<UObject>(nullptr, *Resolved.ObjectPath);
		if (AssetObject == nullptr)
		{
			AssetObject = LoadObject<UObject>(nullptr, *Resolved.ObjectPath);
		}
		if (AssetObject == nullptr)
		{
			Errors += FString::Printf(
				TEXT("\nerror: %s could not be loaded for saving."), *Resolved.ObjectPath);
			++FailedCount;
			continue;
		}

        // Resolve the owning package after loading the requested asset. Registry
        // resolution alone does not make an unloaded package available in memory.
        UPackage* Package = AssetObject->GetOutermost();
        if (!Package || Package->GetName() != Resolved.PackagePath)
        {
            Errors += FString::Printf(TEXT("\nerror: %s resolved to an unexpected package; save not attempted."), *Resolved.ObjectPath);
            ++FailedCount;
            continue;
        }

        // Widget thumbnail rendering runs design-time Blueprint code inside the MCP
        // exception handler. Its script errors can hide a successful save result.
        // Use direct package serialization for widgets on both save entry points.
        const bool bSkipThumbnail = bWithoutThumbnail || Cast<UWidgetBlueprint>(AssetObject) != nullptr;
        FString WithoutThumbnailFileName;
        if (bSkipThumbnail)
        {
            const FString SavePackageName = Package->GetName();
            if (SavePackageName.StartsWith(TEXT("/Game/")))
            {
                WithoutThumbnailFileName = FPaths::ConvertRelativePathToFull(FPaths::Combine(
                    FPaths::ProjectContentDir(), SavePackageName.RightChop(6) + FPackageName::GetAssetPackageExtension()));
            }
            else
            {
                if (!FPackageName::TryConvertLongPackageNameToFilename(SavePackageName, WithoutThumbnailFileName,
                    FPackageName::GetAssetPackageExtension()))
                {
                    Errors += FString::Printf(TEXT("\nerror: cannot resolve mounted filename for %s; save not attempted"), *SavePackageName);
                    ++FailedCount;
                    continue;
                }
                WithoutThumbnailFileName = FPaths::ConvertRelativePathToFull(WithoutThumbnailFileName);
            }
        }

		bool bSaved = false;
		if (AssetObject != nullptr)
		{
			// *** THE MECHANISM IS CHOSEN ONCE AND USED BY BOTH ATTEMPTS, INCLUDING THE RETRY BELOW. ***
			auto SaveOnce = [&]() -> bool
			{
				if (!bSkipThumbnail)
				{
					// UEditorAssetLibrary::SaveLoadedAsset is the same save path the
					// engine's own editor toolset uses. FEditorFileUtils was tried first
					// and fails in a headless editor because it goes through source
					// control checkout, which reported failure on every attempt while
					// nothing was written. That is the D-33 trap from the other side: a
					// save call's own answer is not the truth.
					//
					// One asset at a time, so a failure cannot be blamed on its
					// neighbour: D-33 records that saving several at once silently saved
					// only some of them.
					return UEditorAssetLibrary::SaveLoadedAsset(AssetObject, /*bOnlyIfIsDirty=*/false);
				}

				// *** THE OBJECT MUST CARRY THESE FLAGS BEFORE THE CALL, OR THE ENGINE REFUSES AND
				// REPORTS DATA LOSS. *** `TurboMCPTestMapCommandlet.cpp:350-361` records the engine's own
				// warning and the silent non-write that followed it: `TopLevelFlags` describe what the
				// TOP-LEVEL ASSET must carry, so they have to be ON that asset.
				AssetObject->SetFlags(RF_Public | RF_Standalone);
				FSavePackageArgs SaveArgs;
				SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
				// NO `SAVE_NoError`: a failure must surface through the return value, which is checked. The
				// commandlet sets it and can afford to, because its next line logs an error; a tool that
				// reports to an agent cannot.
				//
				// `bSlowTask` is off because a tool call must not raise the editor's progress dialog. The
				// engine's own exec path passes `SILENT=true` for the same reason (`FileHelpers.cpp:1332`).
				SaveArgs.bSlowTask = false;
				return UPackage::SavePackage(Package, AssetObject, *WithoutThumbnailFileName, SaveArgs);
			};
			bSaved = SaveOnce();

			// A failed return is not proof of a write, even if the dirty flag cleared.
			if (!bSaved || Package->IsDirty())
			{
				// Retry once, which the spec asks for, in case the first attempt lost a race with a
				// background writer. THE SAME MECHANISM: a retry that quietly went back to the thumbnail
				// path would defeat the tool the caller chose.
				bSaved = SaveOnce();
			}
		}

		const bool bStillDirty = Package->IsDirty();

		const int32 WarningsHere = Capture.WarningCountSince(MarkBeforeSave);
		const int32 ErrorsHere = Capture.ErrorCountSince(ErrorMarkBeforeSave);

		if (bSaved && !bStillDirty)
		{
			// Report the save API outcome plus dirty state; this is not reload verification.
			Lines += FString::Printf(TEXT("\n%s  package write: saved"), *Resolved.ObjectPath);
			++SavedCount;
			Lines += bSkipThumbnail
				? TEXT("\n    thumbnail/design-time work: skipped (written without the thumbnail draw)")
				: TEXT("\n    thumbnail/design-time work: ran through the editor's save path (written by the editor's save)");
		}
		else
		{
			// Name the engine's own outcome when it was a refusal rather than a
			// failure, because "you declined" and "it failed" need different
			// fixes. This is a statement about the call, not log text.
			const TCHAR* CallOutcome = bSaved
				? TEXT("the save reported success but the asset is still marked unsaved")
				: TEXT("the save reported failure");

			Errors += FString::Printf(
				TEXT("\nerror: %s has no confirmed successful save. %s. ")
				TEXT("Disk outcome is unconfirmed; do not assume nothing was written.%s"),
				*Resolved.ObjectPath,
				CallOutcome,
				*Capture.SummariseSince(MarkBeforeSave, /*InMax=*/1));
            if (!bSkipThumbnail && Cast<UWidgetBlueprint>(AssetObject) != nullptr)
            {
                Errors += TEXT("\n    Widget save may execute thumbnail/design-time code. Use EditTurboMCPSaveWithoutThumbnail with the same asset to save without drawing a new thumbnail; inspect its result before retrying again.");
            }
            ++FailedCount;
        }

        // *** SV-03, AND IT IS THE BRANCH THAT NEEDS NO NEW PARAMETER. *** The report's design is an OR:
		// "add an option to save without a new thumbnail, OR SAY IN THE RESULT THAT THE THUMBNAIL DRAW
		// RUNS WIDGET CODE." This is the second half, said for the mechanism that actually ran, so a
		// caller who sees a warning during this save knows what drew the asset's own code.
		//
		// WHY IT IS TRUE OF THE EDITOR'S SAVE: it is an `OBJ SAVEPACKAGE` exec (`FileHelpers.cpp:1332`),
		// which runs the engine's thumbnail step (`UnrealEdSrv.cpp:680-682`) and renders when the cached
		// thumbnail is missing or stale (`:439-446`, `:478-485`); for a Widget Blueprint the renderer
		// instantiates the widget with `Designing | ExecutePreConstruct`
		// (`WidgetBlueprintThumbnailRenderer.cpp:189-191`). The direct write calls none of it.
		if (bSaved && !bStillDirty && Cast<UWidgetBlueprint>(AssetObject) != nullptr)
		{
			Lines += bSkipThumbnail
				? TEXT("\n    ; the package was written directly, so the engine's thumbnail step did not")
					TEXT(" run for this save, and this widget's design-time code did not run for a draw.")
				: TEXT("\n    ; the editor's save runs the engine's thumbnail step before it writes, and a")
					TEXT(" thumbnail draw for a Widget Blueprint instantiates the widget and runs its")
					TEXT(" design-time code (`WidgetBlueprintThumbnailRenderer.cpp:189-191`, frequency")
					TEXT(" `OnAssetSave` at `:216`). No option on this tool skips that draw;")
					TEXT(" `EditTurboMCPSaveWithoutThumbnail` writes the package without it.");
		}

		if (bSaved && !bStillDirty && !bSkipThumbnail && Cast<UWidgetBlueprint>(AssetObject) != nullptr && ErrorsHere > 0)
		{
			// The write is already proven above. A widget thumbnail may execute design-time script, so report
			// that work as a warning instead of allowing its error to obscure the saved package.
			Lines += FString::Printf(
				TEXT("\n    warning: package write succeeded, but thumbnail/design-time work raised %d engine error(s)"),
				ErrorsHere);
		}

		if (WarningsHere > 0)
		{
			// P6: the count and the owning asset, never the log text.
			Lines += FString::Printf(TEXT("\n    %d engine warning(s) raised by this save"),
				WarningsHere);
			Lines += Capture.SummariseSince(MarkBeforeSave);
		}
	}

	// P4: the opening word states what actually happened. Nothing saved means
	// nothing changed, whatever the caller asked for, so it must not claim a
	// change.
	if (SavedCount == 0)
	{
		Result = FailedCount > 0
			? FString::Printf(TEXT("unconfirmed: no successful saves confirmed, %d failed; disk may have changed"), FailedCount)
			: FString::Printf(
				TEXT("unchanged: nothing to save, all %d asset(s) already had no changes"),
				NoChangeCount);
	}
	else
	{
		Result = FString::Printf(TEXT("changed: saved %d of %d asset(s)"),
			SavedCount, Parts.Num());
	}

	Result += FString::Printf(TEXT("\n%d saved, %d had no changes, %d failed"),
		SavedCount, NoChangeCount, FailedCount);
	Result += Lines;
	Result += Errors;

	if (Capture.ErrorCount() > 0)
	{
		Result += FString::Printf(TEXT("\nengine errors raised during save: %d"),
			Capture.ErrorCount());
	}

	// APPENDED, NEVER PREPENDED: this tool's answer opens with "changed:" or "unchanged:", and callers
	// and suites read that first word. The block also never uses the word "saved", so it cannot be
	// mistaken for, or confused with, this tool's own "N saved" count.
	Result += TEXT("\n\n");
	Result += CheckoutReport;

	return Result;
}

FString UTurboMCPEditToolset::EditTurboMCPSave(const FString& Assets)
{
	const FString Report = SaveTurboMCPAssets(Assets, /*bWithoutThumbnail=*/false);
    CaptureTurboMCPSaveOutcome(Report);
    return Report;
}

FString UTurboMCPEditToolset::EditTurboMCPSaveWithoutThumbnail(const FString& Assets)
{
	const FString Report = SaveTurboMCPAssets(Assets, /*bWithoutThumbnail=*/true);
    CaptureTurboMCPSaveOutcome(Report);
    return Report;
}
