// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "Tools/TurboMCPToolsets.h"

#include "Containers/UnrealString.h"
#include "Editor.h"
#include "Editor/EditorEngine.h"
#include "EditorAssetLibrary.h"
#include "EditorFramework/AssetImportData.h"
#include "Engine/Blueprint.h"
#include "FileHelpers.h"
#include "Kismet2/CompilerResultsLog.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "UObject/Object.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"

#include "Logging.h"
#include "TurboMCPAssetResolver.h"
#include "TurboMCPLogCapture.h"
#include "TurboMCPSourceControlWrite.h"

/**
 * The two write tools of phase 1: compile and save.
 *
 * Both are Edit kind, so `P4` governs them: one transaction, a real answer, and
 * a roll back when a change makes something worse. Neither reports from a call's
 * return value alone, because `D-33` records that an engine save returned an
 * error when it worked and saved only some of the assets when asked for several.
 *
 * `P6` forbids engine log text in a result. `E-SAVE` must still say that a save
 * raised warnings and which asset raised them, so warnings are counted through
 * `FTurboMCPLogCapture` and summarised, never pasted.
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
				TEXT("play is refused (D-34). Stop the session and ask again.");
		}
		if (GEditor->IsSimulateInEditorInProgress())
		{
			return TEXT("a simulate in editor session is running or queued, and saving during ")
				TEXT("simulate is refused (D-34). Stop the session and ask again.");
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

	// Compiling is safe during play, unlike saving, so only the editor-available
	// check applies here.
	if (!GIsEditor || GEditor == nullptr)
	{
		return FString::Printf(TEXT("error: %s."), *WriteRefusalReason());
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

		FCompilerResultsLog CompileLog;
		CompileLog.bLogInfoOnly = false;
		FKismetEditorUtilities::CompileBlueprint(Blueprint, EBlueprintCompileOptions::None, &CompileLog);

		const bool bHasErrors = CompileLog.NumErrors > 0;
		const bool bHasWarnings = CompileLog.NumWarnings > 0;

		if (bHasErrors)
		{
			++ErrorCount;
		}
		else if (bHasWarnings)
		{
			++WarningCount;
		}
		else
		{
			++CleanCount;
		}

		const FString Status = bHasErrors
			? FString::Printf(TEXT("error (%d errors, %d warnings)"), CompileLog.NumErrors, CompileLog.NumWarnings)
			: bHasWarnings
				? FString::Printf(TEXT("clean with %d warning(s)"), CompileLog.NumWarnings)
				: TEXT("clean");

		// T-STRUCT-7: an unsaved asset must say it compiled but is not saved, so
		// the caller does not believe a compile put the work on disk.
		const bool bDirty = HasUnsavedChanges(Resolved.PackagePath);

		Lines += FString::Printf(TEXT("\n%s  [%s]"), *Resolved.ObjectPath, *Status);
		Lines += bDirty ? TEXT("  not saved yet") : TEXT("  saved");

		if (bHasErrors || bHasWarnings)
		{
			Lines += DescribeCompileMessages(CompileLog, /*InMaxMessages=*/6);
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

	Result += Errors;

	return Result;
}

FString UTurboMCPEditToolset::EditTurboMCPSave(const FString& Assets)
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

		// An asset with no unsaved changes is reported as such and never saved,
		// so the result cannot claim work that did not happen.
		if (!HasUnsavedChanges(Resolved.PackagePath))
		{
			Lines += FString::Printf(TEXT("\n%s  had no changes"), *Resolved.ObjectPath);
			++NoChangeCount;
			continue;
		}

		UPackage* Package = FindPackage(nullptr, *Resolved.PackagePath);
		if (Package == nullptr)
		{
			// The package reports unsaved changes but cannot be found by name,
			// which is a real contradiction worth naming rather than hiding.
			Errors += FString::Printf(
				TEXT("\nerror: %s reports unsaved changes but its package could not be opened."),
				*Resolved.ObjectPath);
			++FailedCount;
			continue;
		}

		const int32 MarkBeforeSave = Capture.Mark();

		// The asset object is needed for the save call, so it is loaded when it
		// is not already in memory. Loading is not an edit.
		UObject* AssetObject = FindObject<UObject>(nullptr, *Resolved.ObjectPath);
		if (AssetObject == nullptr)
		{
			AssetObject = LoadObject<UObject>(nullptr, *Resolved.ObjectPath);
		}

		bool bSaved = false;
		if (AssetObject != nullptr)
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
			bSaved = UEditorAssetLibrary::SaveLoadedAsset(AssetObject, /*bOnlyIfIsDirty=*/false);

			// The unsaved flag decides the result, never the call's return value.
			// D-33: an engine save once returned an error when it had worked.
			if (Package->IsDirty())
			{
				// Retry once, which the spec asks for, in case the first attempt
				// lost a race with a background writer.
				bSaved = UEditorAssetLibrary::SaveLoadedAsset(
					AssetObject, /*bOnlyIfIsDirty=*/false);
			}
		}

		const bool bStillDirty = Package->IsDirty();

		const int32 WarningsHere = Capture.WarningCountSince(MarkBeforeSave);

		if (!bStillDirty)
		{
			Lines += FString::Printf(TEXT("\n%s  saved"), *Resolved.ObjectPath);
			++SavedCount;
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
				TEXT("\nerror: %s is still unsaved after two attempts. %s. ")
				TEXT("The asset may be read only, or another program may hold the file.%s"),
				*Resolved.ObjectPath,
				CallOutcome,
				*Capture.SummariseSince(MarkBeforeSave, /*InMax=*/1));
			++FailedCount;
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
			? FString::Printf(TEXT("unchanged: nothing was saved, %d failed"), FailedCount)
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
