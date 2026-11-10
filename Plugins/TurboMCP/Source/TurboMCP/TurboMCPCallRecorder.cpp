// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "TurboMCPCallRecorder.h"
#include "TurboMCPToolPolicy.h"

#include "Algo/LevenshteinDistance.h"
#include "Async/Future.h"
#include "Editor.h"
#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "Templates/SharedPointer.h"
#include "Templates/SubclassOf.h"
#include "Templates/ValueOrError.h"
#include "Templates/UnrealTemplate.h"
#include "UObject/Class.h"
#include "UObject/UnrealType.h"
#include "JsonObjectConverter.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
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

    FString InsertDirtyReport(const FString& Body, const FString& Delta)
    {
        const FString Trimmed = Body.TrimEnd();
        int32 LastNewline = INDEX_NONE;
        Trimmed.FindLastChar(TEXT('\n'), LastNewline);
        const int32 FinalStart = LastNewline + 1;
        if (Trimmed.Mid(FinalStart).StartsWith(TEXT("; tmcp-result-v1:")))
        {
            // Preserve the existing authoritative line verbatim; do not infer an outcome.
            return Trimmed.Left(FinalStart) + Delta.TrimStart() + TEXT("\n")
                + Trimmed.Mid(FinalStart);
        }
        return Body + Delta;
    }

    // Preserve the engine's JSON return envelope when adding human-readable telemetry.
    // Appending after the closing brace produces neither valid JSON nor plain tool text.
    FString AppendDirtyReport(const FString& Value, const FString& Delta)
    {
        TSharedPtr<FJsonObject> Object;
        const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Value);
        FString Body;
        if (FJsonSerializer::Deserialize(Reader, Object) && Object.IsValid()
            && Object->TryGetStringField(TEXT("returnValue"), Body))
        {
            Object->SetStringField(TEXT("returnValue"), InsertDirtyReport(Body, Delta));
            FString Encoded;
            const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Encoded);
            if (FJsonSerializer::Serialize(Object.ToSharedRef(), Writer)) return Encoded;
            // Preserve the report even if encoding unexpectedly fails.
            return InsertDirtyReport(Value, Delta);
        }
        return InsertDirtyReport(Value, Delta);
    }

    thread_local FString* ActiveSaveOutcome = nullptr;

    bool NormalizeParameterAliases(const UFunction* Function,
        const FString& Input, FString& Output, FString& Error)
    {
        Output = Input;
        TSharedPtr<FJsonObject> Object;
        if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Input), Object) || !Object) return true;
        TSet<FString> Parameters;
        TSet<FString> StringParameters;
        for (TFieldIterator<FProperty> It(Function); It; ++It)
            if (It->HasAnyPropertyFlags(CPF_Parm) && !It->HasAnyPropertyFlags(CPF_ReturnParm))
            {
                const FString Name = FJsonObjectConverter::StandardizeCase(It->GetName());
                Parameters.Add(Name);
                if (CastField<FStrProperty>(*It)) StringParameters.Add(Name);
            }
        const TCHAR* Pairs[][2] = {
            {TEXT("asset"), TEXT("assets")}, {TEXT("toolset"), TEXT("toolsetName")},
            {TEXT("tool"), TEXT("toolName")}, {TEXT("nodeId"), TEXT("nodeIds")}
        };
        bool Changed = false;
        for (const auto& Pair : Pairs)
            for (int32 Direction = 0; Direction < 2; ++Direction)
            {
                const FString Canonical = Pair[Direction];
                const FString Alias = Pair[1 - Direction];
                // Do not reinterpret two distinct declared parameters or array inputs.
                if (!StringParameters.Contains(Canonical) || Parameters.Contains(Alias) || !Object->HasField(Alias)) continue;
                if (Object->HasField(Canonical))
                {
                    Error = FString::Printf(TEXT("unchanged: supply only '%s' or its alias '%s', not both"), *Canonical, *Alias);
                    return false;
                }
                FString Value;
                if (!Object->TryGetStringField(Alias, Value))
                {
                    Error = FString::Printf(TEXT("unchanged: alias '%s' requires the string value declared for '%s'"), *Alias, *Canonical);
                    return false;
                }
                Object->SetStringField(Canonical, Value);
                Object->RemoveField(Alias);
                Changed = true;
            }
        if (Changed)
        {
            Output.Reset();
            FJsonSerializer::Serialize(Object.ToSharedRef(), TJsonWriterFactory<>::Create(&Output));
        }
        return true;
    }


    // Called only on the editor game thread. Store names rather than object pointers across a call.
    TSet<FString> DirtyPackages()
    {
        TSet<FString> Names;
        for (TObjectIterator<UPackage> It; It; ++It)
            if (It->IsDirty() && !It->HasAnyFlags(RF_Transient)) Names.Add(It->GetName());
        return Names;
    }

    FString DirtyDelta(const TSet<FString>& Before, bool OnlyChanges)
    {
        if (!IsInGameThread()) return TEXT("\ndirty_packages: after snapshot unavailable off game thread; no cleanliness claim");
        const TSet<FString> After = DirtyPackages();
        TArray<FString> Added, Cleared;
        for (const FString& Name : After) if (!Before.Contains(Name)) Added.Add(Name);
        for (const FString& Name : Before) if (!After.Contains(Name)) Cleared.Add(Name);
        Added.Sort(); Cleared.Sort();
        if (OnlyChanges && Added.IsEmpty() && Cleared.IsEmpty()) return FString();
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

    FString ToolSearchStem(const FString& Name)
    {
        const int32 PrefixEnd = Name.Find(TEXT("TurboMCP"), ESearchCase::IgnoreCase);
        return (PrefixEnd == INDEX_NONE ? Name : Name.Mid(PrefixEnd + 8)).ToLower();
    }

    FString UnknownToolError(const FToolset& Wrapped, const FToolset& Inner, const FString& ToolName)
    {
        constexpr int32 MaxNameChars = 96;
        const FString ToolsetName = Wrapped.GetToolsetName();
        const FString DisplayName = ToolName.Left(MaxNameChars).Replace(TEXT("\r"), TEXT("\\r")).Replace(TEXT("\n"), TEXT("\\n"));
        FString Error = FString::Printf(TEXT("Unknown tool '%s%s' in %s."),
            *DisplayName, ToolName.Len() > MaxNameChars ? TEXT("...") : TEXT(""), *ToolsetName);

        const FString Query = ToolSearchStem(BareName(ToolName.Left(MaxNameChars)));
        if (ToolName.Len() <= MaxNameChars && Query.Len() >= 3)
        {
            const FString Prefix = ToolsetName + TEXT(".");
            const int32 MaxDistance = FMath::Clamp(Query.Len() / 4, 1, 4);
            TArray<TPair<int32, FString>> Matches;
            for (const FString& FullName : Inner.ListToolNames())
            {
                if (!FullName.StartsWith(Prefix, ESearchCase::CaseSensitive)
                    || !Wrapped.IsToolEnabled(FullName) || !Inner.IsToolEnabled(FullName))
                {
                    continue;
                }

                const FString Bare = FullName.Mid(Prefix.Len());
                FString Reason;
                if (!TurboMCPToolPolicy::IsAllowed(Bare, Reason))
                {
                    continue;
                }

                const FString Stem = ToolSearchStem(Bare);
                const int32 Distance = Algo::LevenshteinDistance(FStringView(Query), FStringView(Stem));
                if (Distance <= MaxDistance)
                {
                    Matches.Emplace(Distance, Bare);
                }
            }
            Matches.Sort([](const TPair<int32, FString>& A, const TPair<int32, FString>& B)
            {
                return A.Key == B.Key ? A.Value < B.Value : A.Key < B.Key;
            });
            if (!Matches.IsEmpty())
            {
                Error += TEXT(" Did you mean");
                const int32 Count = FMath::Min(3, Matches.Num());
                for (int32 Index = 0; Index < Count; ++Index)
                {
                    Error += (Index == 0 ? TEXT(" ") : TEXT(", ")) + Matches[Index].Value;
                }
                Error += TEXT("?");
            }
        }
        Error += FString::Printf(TEXT(" Use ReadTurboMCPToolHelp with ToolsetName=%s and an empty ToolName, then call a listed bare tool name."), *ToolsetName);
        return Error;
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

            const UClass* ToolsetClass = Inner->GetToolsetClass();
            const UFunction* Function = ToolsetClass != nullptr && ToolName.Len() < NAME_SIZE
                ? ToolsetClass->FindFunctionByName(FName(*ToolName, FNAME_Find)) : nullptr;
            if (Function == nullptr || !Function->HasMetaData(TEXT("AICallable"))
                || !Function->GetName().Equals(ToolName, ESearchCase::IgnoreCase))
            {
                const FString Error = UnknownToolError(*this, *Inner, ToolName);
                Call->Finish(Error, true);
                return MakeFulfilledPromise<FToolResult>(MakeError(Error)).GetFuture();
            }

            FString PolicyReason;
            if (!TurboMCPToolPolicy::IsAllowed(ToolName, PolicyReason))
            {
                const FString Error = TEXT("unchanged: ") + PolicyReason + TEXT(" This call did not execute.");
                Call->Finish(Error, true);
                return MakeFulfilledPromise<FToolResult>(MakeError(Error)).GetFuture();
            }
            FString NormalizedInput, AliasError;
            if (!NormalizeParameterAliases(Function, JsonInput, NormalizedInput, AliasError))
            {
                Call->Finish(AliasError, true);
                return MakeFulfilledPromise<FToolResult>(MakeError(AliasError)).GetFuture();
            }

            const bool bAssetEdit = ToolName.StartsWith(TEXT("EditTurboMCP"))
                && !ToolName.StartsWith(TEXT("EditTurboMCPCallLog"))
                && ToolName != TEXT("EditTurboMCPPlayInput") && ToolName != TEXT("EditTurboMCPMenuActivate")
                && ToolName != TEXT("EditTurboMCPPlayInvoke") // Exact PIE runtime target and one-use preview confirmation.
                && ToolName != TEXT("EditTurboMCPDiagnosticConsole") // Explicit diagnostic target and confirmation; no asset editing.
                && ToolName != TEXT("EditTurboMCPTrace") && ToolName != TEXT("EditTurboMCPVcsCheckout");
            if (bAssetEdit
                && (!IsInGameThread() || !GEditor || GEditor->IsTransactionActive() || GEditor->IsPlaySessionInProgress()))
            {
                FString Error = TEXT("unchanged: edit refused; requires the editor game thread, no play session and no active transaction. Run individual edits outside script transactions.");
                if (IsInGameThread() && GEditor && GEditor->IsPlaySessionInProgress())
                {
                    Error += TEXT("\nThis call made no changes and was not queued. Earlier edits made by other tools may still be unsaved; this refusal does not undo them. TurboMCP does not control the engine property writer. Stop play, call ReadTurboMCPDirtyList and inspect the changed assets, then compile and explicitly save the assets you want to keep.");
                }
                Call->Finish(Error, true);
                return MakeFulfilledPromise<FToolResult>(MakeError(Error)).GetFuture();
            }

            // Read/preview paths can load assets and trigger dependency work too.
            // Report their deltas without pretending an absent before snapshot was clean.
            const bool bObserveDirty = bAssetEdit || ToolName.StartsWith(TEXT("ReadTurboMCP"))
                || ToolName.StartsWith(TEXT("PreviewTurboMCP")) || ToolName.StartsWith(TEXT("LookTurboMCP"));
            const bool bDirtyBeforeAvailable = bObserveDirty && IsInGameThread();
            const TSet<FString> BeforeDirty = bDirtyBeforeAvailable ? DirtyPackages() : TSet<FString>();
            FString SaveOutcome;
            const bool bSave = Inner->GetToolsetClass() == UTurboMCPEditToolset::StaticClass()
                && (ToolName == TEXT("EditTurboMCPSave") || ToolName == TEXT("EditTurboMCPSaveWithoutThumbnail"));
            // These FString-returning save functions are invoked synchronously by
            // FObjectFunctionToolCall before its future is returned. Keep the sink
            // scoped to that invocation, never across asynchronous completion.
            auto InnerFuture = [&]()
            {
                TGuardValue<FString*> CaptureScope(ActiveSaveOutcome, bSave ? &SaveOutcome : nullptr);
                return Inner->ExecuteTool(ToolName, NormalizedInput);
            }();
            return InnerFuture.Then(
                [Call, bAssetEdit, bObserveDirty, bDirtyBeforeAvailable, BeforeDirty, SaveOutcome](TFuture<FToolResult> Future) -> FToolResult
				{
					FToolResult Result = Future.Get();
                    if (Result.HasError() && !SaveOutcome.IsEmpty())
                    {
                        // Keep the error carrier and its diagnostics. A completed
                        // report may say saved, failed or unconfirmed: do not infer
                        // success from an exception or from a changed dirty flag.
                        Result = MakeError(TEXT("save outcome (reported before wrapper diagnostics):\n")
                            + SaveOutcome + TEXT("\nscript/tool diagnostics:\n") + Result.GetError());
                    }
                    if (bObserveDirty)
                    {
                        const FString Delta = bDirtyBeforeAvailable
                            ? DirtyDelta(BeforeDirty, !bAssetEdit)
                            : TEXT("\ndirty_packages: before snapshot unavailable off game thread; changes cannot be attributed to this call");
                        if (!Delta.IsEmpty())
                        {
                            if (Result.HasError()) Result = MakeError(Result.GetError() + Delta);
                            else Result = MakeValue(AppendDirtyReport(Result.GetValue(), Delta));
                        }
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

void CaptureTurboMCPSaveOutcome(const FString& Report)
{
    if (IsInGameThread() && ActiveSaveOutcome) *ActiveSaveOutcome = Report;
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
