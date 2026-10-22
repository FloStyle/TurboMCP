// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "TurboMCPCallRecorder.h"

#include "Async/Future.h"
#include "Editor.h"
#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "Templates/SharedPointer.h"
#include "Templates/SubclassOf.h"
#include "Templates/ValueOrError.h"
#include "UObject/Class.h"
#include "UObject/Package.h"
#include "UObject/UObjectIterator.h"
#include "Containers/Set.h"

#include "ToolsetRegistry/Toolset.h"
#include "ToolsetRegistry/ToolsetRegistry.h"
#include "ToolsetRegistry/ToolsetRegistrySubsystem.h"
#include "ToolsetRegistry/UToolsetRegistry.h"

#include "Logging.h"
#include "Tools/TurboMCPToolsets.h"
#include "TurboMCPCallLog.h"

namespace
{
	using UE::ToolsetRegistry::FToolset;
	using FToolResult = TValueOrError<FString, FString>;

    // Called only on the editor game thread. Store names rather than object pointers across a call.
    TSet<FString> DirtyPackages()
    {
        TSet<FString> Names;
        for (TObjectIterator<UPackage> It; It; ++It)
            if (It->IsDirty() && !It->HasAnyFlags(RF_Transient)) Names.Add(It->GetName());
        return Names;
    }

    FString DirtyDelta(const TSet<FString>& Before)
    {
        if (!IsInGameThread()) return TEXT("\ndirty_packages: after snapshot unavailable off game thread; no cleanliness claim");
        const TSet<FString> After = DirtyPackages();
        TArray<FString> Added, Cleared;
        for (const FString& Name : After) if (!Before.Contains(Name)) Added.Add(Name);
        for (const FString& Name : Before) if (!After.Contains(Name)) Cleared.Add(Name);
        Added.Sort(); Cleared.Sort();
        FString Report = FString::Printf(TEXT("\ndirty_packages: before=%d after=%d newly_dirty=%d no_longer_dirty=%d"), Before.Num(), After.Num(), Added.Num(), Cleared.Num());
        for (const FString& Name : Added) Report += TEXT("\n  newly_dirty: ") + Name;
        for (const FString& Name : Cleared) Report += TEXT("\n  no_longer_dirty: ") + Name;
        if (!Added.IsEmpty()) Report += TEXT("\nReview newly dirty packages against the requested targets; this report does not save or discard them. Dependency/editor side effects may be included.");
        if (!Cleared.IsEmpty()) Report += TEXT("\nA cleared dirty flag alone is not proof of a successful disk save.");
        return Report;
    }

	/**
	 * The wrappers this plugin installed.
	 *
	 * TWO JOBS, AND THE SECOND IS THE ONE THAT MATTERS: `InstallTurboMCPCallRecording` can be called
	 * more than once - `RegisterTurboMCPToolsets` is called from the module's loading-phase callback -
	 * and wrapping a wrapper would write TWO start records for one call and TWO end records. The check
	 * is a POINTER COMPARISON against this list rather than a `StaticCastSharedPtr`, because the
	 * registry's handlers are `FFunctionLibraryToolset` instances and a downcast from the base would
	 * be undefined behaviour that HAPPENS to answer non-null.
	 */
	TArray<TSharedPtr<FToolset>> GInstalledWrappers;

	bool IsAlreadyWrapped(const TSharedPtr<FToolset>& InHandler)
	{
		for (const TSharedPtr<FToolset>& Existing : GInstalledWrappers)
		{
			if (Existing.Get() == InHandler.Get())
			{
				return true;
			}
		}
		return false;
	}

	/**
	 * The last dotted segment of a name.
	 *
	 * *** THE SAME RULE `TurboMCPCallCost.cpp:114-119` ALREADY APPLIES, AND FOR THE SAME MEASURED
	 * REASON: the engine's event writes `TurboMCP.TurboMCPReadToolset` while this side records the
	 * bare `TurboMCPReadToolset`, and appending one spelling to the other produced a whole path
	 * appearing twice. The last segment collapses both spellings, so no prefix list has to be kept in
	 * step with anything. ***
	 */
	FString BareName(const FString& InName)
	{
		int32 LastDot = INDEX_NONE;
		if (!InName.FindLastChar(TEXT('.'), LastDot) || LastDot < 0 || LastDot + 1 >= InName.Len())
		{
			return InName;
		}
		return InName.Mid(LastDot + 1);
	}

	/**
	 * One toolset handler, wrapped so that a call through it writes the two records `T02` asks for.
	 *
	 * THE FORWARDING IS THE WHOLE TRICK: `FToolset::ExecuteTool` is PUBLIC AND NON-VIRTUAL
	 * (`Toolset/Toolset.h:28-47`) and it reaches the engine's own dispatch through the virtual
	 * `ExecuteToolInternal` (`:72`). So the wrapped handler is called through its OWN public door -
	 * enable checks, tool filters and the reflection dispatch to the `AICallable` UFUNCTION all still
	 * happen exactly once, and none of it is reimplemented here. The wrapper only adds the records.
	 */
	class FRecordingToolset final : public FToolset
	{
	public:
		explicit FRecordingToolset(TSharedPtr<FToolset> InInner)
			: Inner(MoveTemp(InInner))
		{
		}

		// THE IDENTITY IS THE INNER HANDLER'S, so everything that asks a toolset what it is -
		// `ForEachToolset`, the schema listing, `GetToolsetClass` - receives the engine's own answer
		// rather than a copy that could drift from it.
		virtual FString GetToolsetName() const override { return Inner->GetToolsetName(); }
		virtual FString GetToolsetVersion() const override { return Inner->GetToolsetVersion(); }
		virtual FString GetToolsetDescription() const override { return Inner->GetToolsetDescription(); }
		virtual UClass* GetToolsetClass() const override { return Inner->GetToolsetClass(); }

	protected:
		virtual FString GetJsonSchemaInternal() const override { return Inner->GetJsonSchema(); }

		virtual TFuture<FToolResult> ExecuteToolInternal(
			const FString& ToolName, const FString& JsonInput) override
		{
			const FString RecordedToolset = BareName(Inner->GetToolsetName());

			// *** THE START RECORD, WRITTEN AND FLUSHED BEFORE THE CALL RUNS. *** The scope object is
			// held by a shared pointer rather than on the stack, because the end record is written from
			// the future's continuation - which may run on another thread, and may run after this
			// function has returned. `FScopedCall`'s destructor is the backstop: a call whose future is
			// destroyed without a result still writes an end record saying so, rather than leaving a
			// `start` that reads like a crash.
			TSharedPtr<FTurboMCPCallLog::FScopedCall> Call =
				MakeShared<FTurboMCPCallLog::FScopedCall>(RecordedToolset, ToolName, JsonInput);

            const bool bAssetEdit = ToolName.StartsWith(TEXT("EditTurboMCP"))
                && !ToolName.StartsWith(TEXT("EditTurboMCPCallLog"))
                && ToolName != TEXT("EditTurboMCPTrace") && ToolName != TEXT("EditTurboMCPVcsCheckout");
            if (bAssetEdit
                && (!IsInGameThread() || !GEditor || GEditor->IsTransactionActive() || GEditor->IsPlaySessionInProgress()))
            {
                const FString Error = TEXT("unchanged: edit refused; requires the editor game thread, no play session and no active transaction. Run individual edits outside script transactions.");
                Call->Finish(Error, true);
                return MakeFulfilledPromise<FToolResult>(MakeError(Error)).GetFuture();
            }

			const TSet<FString> BeforeDirty = bAssetEdit ? DirtyPackages() : TSet<FString>();
			return Inner->ExecuteTool(ToolName, JsonInput).Then(
				[Call, bAssetEdit, BeforeDirty](TFuture<FToolResult> Future) -> FToolResult
				{
					FToolResult Result = Future.Get();
                    if (bAssetEdit)
                    {
                        const FString Delta = DirtyDelta(BeforeDirty);
                        if (Result.HasError()) Result = MakeError(Result.GetError() + Delta);
                        else Result = MakeValue(Result.GetValue() + Delta);
                    }

					// THE SIZE AND THE TOKEN ESTIMATE COME FROM THE RESULT ITSELF, WHICH IS THE POINT OF
					// MEASURING AT THIS BOUNDARY: the engine's event for the same call carries neither,
					// and `FScopedCall::Finish` counts the FULL string even though it stores only the
					// first 4000 characters of it.
					const bool bError = Result.HasError();
					Call->Finish(bError ? Result.GetError() : Result.GetValue(), bError);
					return Result;
				});
		}

	private:
		TSharedPtr<FToolset> Inner;
	};
}

void InstallTurboMCPCallRecording()
{
	// The registry is the engine's, and `IsAvailable` is its own check for whether the subsystem that
	// holds it exists yet. Asking it first is what keeps this from logging a spurious error during the
	// loading phase the module already waits for.
	if (!UToolsetRegistry::IsAvailable())
	{
		UE_LOG(LogTurboMCP, Warning,
			TEXT("Call recording not installed: the engine's toolset registry is not available yet."));
		return;
	}

	auto Subsystem = UToolsetRegistrySubsystem::Get();
	if (Subsystem.HasError())
	{
		UE_LOG(LogTurboMCP, Warning, TEXT("Call recording not installed: %s"), *Subsystem.GetError());
		return;
	}

	UE::ToolsetRegistry::FToolsetRegistry& Registry = Subsystem.GetValue()->ToolsetRegistry;

	int32 WrappedCount = 0;
	for (const TSubclassOf<UToolsetDefinition>& Class : TurboMCPToolsets::GetToolsetClasses())
	{
		// *** THE HANDLER IS FOUND BY ITS CLASS AND NOT BY ITS NAME, WHICH IS A MEASURED CHOICE: the
		// name an agent sees (`TurboMCP.TurboMCPReadToolset`) is built by the engine from the class's
		// `ScriptName` metadata while `TurboMCPToolsets::GetToolsetName` reproduces that rule by hand.
		// Matching on the class pointer is the one identification that cannot disagree with the
		// registry, because it is the same value the registry was given. ***
		TSharedPtr<FToolset> Inner;
		Registry.ForEachToolset(
			[&Registry, &Class, &Inner](const FString& Name, const FToolset& Toolset)
			{
				if (Toolset.GetToolsetClass() == Class.Get())
				{
					Inner = Registry.Find(Name);
				}
			});

		// A toolset the registry does not hold is either not registered yet or blocked by the
		// engine's own name filters, and neither is a call this plugin could record.
		if (!Inner.IsValid() || IsAlreadyWrapped(Inner))
		{
			continue;
		}

		// THE SWAP IS THE PUBLIC PAIR. `RegisterToolset` refuses a name that is already taken
		// (`ToolsetRegistry.cpp:129-135`), so the engine's handler has to come out before the wrapper
		// goes in - and it is held by `Inner` throughout, so nothing the engine built is lost.
		if (!Registry.UnregisterToolset(Inner))
		{
			continue;
		}

		TSharedPtr<FRecordingToolset> Wrapper = MakeShared<FRecordingToolset>(Inner);
		if (!Registry.RegisterToolset(Wrapper))
		{
			// PUT THE ENGINE'S OWN HANDLER BACK, so a failed swap leaves no toolset missing. A toolset
			// that cannot be reached is worse than one whose calls are not recorded.
			Registry.RegisterToolset(Inner);
			continue;
		}

		GInstalledWrappers.Emplace(Wrapper);
		++WrappedCount;
	}

	UE_LOG(LogTurboMCP, Log,
		TEXT("TurboMCP call recording installed on %d of %d toolset(s): a start record is now written ")
		TEXT("before every call and an end record after it, on the agent's path as well as the plugin's."),
		WrappedCount, TurboMCPToolsets::GetToolsetClasses().Num());
}

void RemoveTurboMCPCallRecording()
{
	// The registry is unregistered first by the caller (`UnregisterTurboMCPToolsets`), so this only
	// drops the plugin's own references. It is called anyway, because a static array holding engine
	// handlers across a module unload is a reference to something that is being torn down.
	GInstalledWrappers.Reset();
}
