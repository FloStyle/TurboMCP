// Copyright FloStyle. All Rights Reserved.

#include "TurboMCPAnalyticsProvider.h"

#include "AnalyticsET.h"
#include "Hash/Blake3.h"
#include "IModelContextProtocolModule.h"
#include "AnalyticsEventAttribute.h"
#include "HAL/CriticalSection.h"
#include "HAL/PlatformTime.h"
#include "Misc/DateTime.h"
#include "Misc/Guid.h"
#include "Serialization/JsonSerializer.h"
#include "ToolsetRegistry/UToolsetRegistry.h"

#include "TurboMCPCallLog.h"

FCriticalSection FTurboMCPAnalyticsProvider::RegistryMutex;

TMap<FString, FString>& FTurboMCPAnalyticsProvider::Registry()
{
	static TMap<FString, FString> Map;
	return Map;
}

FString FTurboMCPAnalyticsProvider::HashToolIdentifier(const FString& InIdentifier)
{
	// THE ENGINE'S OWN HASH, REPRODUCED FROM ITS SOURCE.
	//
	// `ModelContextProtocolAnalytics.cpp` is:
	//
	//     FTCHARToUTF8 Utf8(*Identifier);
	//     const FBlake3Hash Hash = FBlake3::HashBuffer(Utf8.Get(), Utf8.Length());
	//     return LexToString(Hash);
	//
	// The UTF-8 step matters: hashing the TCHAR buffer instead would give a different value for
	// every name that is not pure ASCII. Reproducing it exactly is what lets a hash in the event
	// be turned back into the name that produced it.
	FTCHARToUTF8 Utf8(*InIdentifier);
	const FBlake3Hash Hash = FBlake3::HashBuffer(Utf8.Get(), Utf8.Length());
	return LexToString(Hash);
}

void FTurboMCPAnalyticsProvider::RegisterKnownName(const FString& InName)
{
	if (InName.IsEmpty())
	{
		return;
	}
	FScopeLock Lock(&RegistryMutex);
	// The NAME is stored, so a lookup gives back what was registered rather than the hash again.
	Registry().Add(HashToolIdentifier(InName), InName);
}

void FTurboMCPAnalyticsProvider::RegisterServedToolsetsAndTools()
{
	// THIS PLUGIN'S OWN TOOLSETS, registered by name.
	//
	// THE ENGINE'S TOOLSETS CANNOT BE ENUMERATED, AND THAT IS A MEASURED LIMITATION RATHER THAN AN
	// OVERSIGHT. `UToolsetRegistry` exposes `IsToolsetRegistered(name)`, `GetToolsetJsonSchema` and
	// `GetAllToolsetJsonSchemas` — no list of names and no list of tools. So there is nothing to
	// iterate, and an engine tool's hash cannot be turned back into its name from here.
	//
	// Rather than leave the reader with an opaque 64-character string, the log writes the hash with
	// the word `engine` and the FIRST 12 CHARACTERS, which is enough to tell two engine tools
	// apart in a session and is honestly labelled. `QUESTIONS.md` records the limitation and the
	// one route that would lift it: matching a hash against the toolset schemas' declared names,
	// which needs the schema JSON parsed and is not done here.
	RegisterKnownName(TEXT("TurboMCP.TurboMCPReadToolset"));
	RegisterKnownName(TEXT("TurboMCP.TurboMCPLookToolset"));
	RegisterKnownName(TEXT("TurboMCP.TurboMCPPreviewToolset"));
	RegisterKnownName(TEXT("TurboMCP.TurboMCPEditToolset"));

	// The BARE names too, because `call_tool` takes the bare function name and a future engine
	// version might hash that instead of the qualified one.
	static const TCHAR* const BareToolNames[] = {
		TEXT("ReadTurboMCPStatus"), TEXT("ReadTurboMCPToolIndex"), TEXT("ReadTurboMCPToolHelp"),
		TEXT("ReadTurboMCPAssetSearch"), TEXT("ReadTurboMCPAssetDescribe"),
		TEXT("ReadTurboMCPTypeFind"), TEXT("ReadTurboMCPDirtyList"),
		TEXT("ReadTurboMCPNodeResolve"), TEXT("ReadTurboMCPNodeFind"),
		TEXT("ReadTurboMCPGraphLoad"), TEXT("ReadTurboMCPBlueprintLoad"),
		TEXT("ReadTurboMCPRefs"), TEXT("ReadTurboMCPOverrides"),
		TEXT("ReadTurboMCPCommentList"), TEXT("ReadTurboMCPDataTable"),
		TEXT("ReadTurboMCPCallLog"),
		TEXT("LookTurboMCPLayoutReport"), TEXT("LookTurboMCPWidgetPreview"),
		TEXT("LookTurboMCPCapture"),
		TEXT("PreviewTurboMCPEnumAdd"), TEXT("PreviewTurboMCPVarsAdd"),
		TEXT("PreviewTurboMCPOverrideAdd"), TEXT("PreviewTurboMCPWidgetRetarget"),
		TEXT("EditTurboMCPCompile"), TEXT("EditTurboMCPSave"),
		TEXT("EditTurboMCPEnumAdd"), TEXT("EditTurboMCPVarsAdd"),
		TEXT("EditTurboMCPWidgetsAsVars"), TEXT("EditTurboMCPOverrideAdd"),
		TEXT("EditTurboMCPOverrideRemove"), TEXT("EditTurboMCPFunctionAdd"),
		TEXT("EditTurboMCPFunctionRemove"), TEXT("EditTurboMCPWidgetRetarget"),
		TEXT("EditTurboMCPWidgetBindAdd"), TEXT("EditTurboMCPCallLog"),
		TEXT("EditTurboMCPCallLogClear"),
	};
	for (const TCHAR* Bare : BareToolNames)
	{
		RegisterKnownName(Bare);
	}
}

FString FTurboMCPAnalyticsProvider::AttributeValue(
	const TArray<FAnalyticsEventAttribute>& Attributes, const FString& InName)
{
	for (const FAnalyticsEventAttribute& Attribute : Attributes)
	{
		if (Attribute.GetName().Equals(InName, ESearchCase::IgnoreCase))
		{
			return Attribute.GetValue();
		}
	}
	return FString();
}

namespace
{
	/** The provider installed by this module, so it is not installed twice. */
	TSharedPtr<FTurboMCPAnalyticsProvider, ESPMode::ThreadSafe> GInstalled;

	/** The provider this one wrapped, kept so Uninstall can put it back. */
	TSharedPtr<IAnalyticsProviderET, ESPMode::ThreadSafe> GWrapped;
}

void FTurboMCPAnalyticsProvider::Install()
{
	IModelContextProtocolModule* Module = IModelContextProtocolModule::Get();
	if (Module == nullptr)
	{
		return;
	}

	if (GInstalled.IsValid())
	{
		// Already installed. Installing again would wrap THIS provider and every event would be
		// logged twice, which is worse than not logging at all.
		return;
	}

	// THE PROVIDER ALREADY THERE IS KEPT, NOT DISCARDED. `07_ENGINE_NOTES.md` section 2.4: never
	// replace an existing hook without passing events on to it.
	GWrapped = Module->GetAnalyticsProvider();

	TSharedPtr<FTurboMCPAnalyticsProvider, ESPMode::ThreadSafe> Provider =
		MakeShared<FTurboMCPAnalyticsProvider, ESPMode::ThreadSafe>();
	Provider->Wrapped = GWrapped;
	GInstalled = Provider;

	// The names are registered BEFORE the provider is installed, so the very first event already
	// resolves rather than being written as a hash.
	RegisterServedToolsetsAndTools();

	Module->SetAnalyticsProvider(Provider);
}

void FTurboMCPAnalyticsProvider::Uninstall()
{
	IModelContextProtocolModule* Module = IModelContextProtocolModule::Get();
	if (Module == nullptr || !GInstalled.IsValid())
	{
		return;
	}

	// Only restored when THIS provider is still the one installed: something else may have
	// replaced it, and clobbering that would lose the other hook.
	if (Module->GetAnalyticsProvider() == GInstalled)
	{
		Module->SetAnalyticsProvider(GWrapped);
	}

	GInstalled.Reset();
	GWrapped.Reset();
}

bool FTurboMCPAnalyticsProvider::IsInstalled()
{
	return GInstalled.IsValid();
}

void FTurboMCPAnalyticsProvider::RecordEvent(FString&& EventName,
	const TArray<FAnalyticsEventAttribute>& Attributes)
{
	RecordEvent(MoveTemp(EventName), Attributes, EAnalyticsRecordEventMode::Cached);
}

void FTurboMCPAnalyticsProvider::RecordEvent(FString&& EventName,
	const TArray<FAnalyticsEventAttribute>& Attributes, EAnalyticsRecordEventMode Mode)
{
	// LOGGED FIRST, so an exception or a slow forward cannot lose the record.
	if (EventName.EndsWith(TEXT("ToolCall")))
	{
		const FString SessionId = AttributeValue(Attributes, TEXT("SessionId"));
		const FString ToolsetHash = AttributeValue(Attributes, TEXT("ToolsetNameHash"));
		const FString ToolHash = AttributeValue(Attributes, TEXT("ToolNameHash"));
		const FString Duration = AttributeValue(Attributes, TEXT("Duration"));
		const FString Status = AttributeValue(Attributes, TEXT("ResultStatus"));

		// RESOLVED AGAINST THE REGISTRY, so the log says `blueprint.write_graph_dsl` rather than a
		// 64-character hash. An unresolved hash is written AS the hash with a marker, never
		// invented and never dropped: an opaque identifier is still evidence a call happened.
		FString ToolsetName;
		FString ToolName;
		{
			FScopeLock Lock(&RegistryMutex);
			if (const FString* Found = Registry().Find(ToolsetHash))
			{
				ToolsetName = *Found;
			}
			if (const FString* Found = Registry().Find(ToolHash))
			{
				ToolName = *Found;
			}
		}

		if (ToolsetName.IsEmpty())
		{
			ToolsetName = FString::Printf(TEXT("(engine tool ending %s)"), *ToolsetHash.Left(12));
		}
		if (ToolName.IsEmpty())
		{
			ToolName = FString::Printf(TEXT("(engine tool ending %s)"), *ToolHash.Left(12));
		}

		FTurboMCPCallLog::FEntry Entry;
		Entry.Timestamp = FDateTime::UtcNow().ToIso8601();
		// The engine's event arrives AFTER the call, carrying the duration, so there is no
		// separate start record on this side. The field says which record it is so a reader is not
		// left wondering why engine calls have one line and plugin calls have two.
		Entry.Phase = TEXT("end");
		Entry.Source = TEXT("engine");
		Entry.SessionId = SessionId;
		Entry.Toolset = ToolsetName;
		Entry.Tool = ToolName;
		Entry.DurationMs = FCString::Atod(*Duration) * 1000.0;
		Entry.bError = Status.Equals(TEXT("Error"), ESearchCase::IgnoreCase);
		// The engine's event carries no result text at all, so the log says so rather than
		// implying the result was empty.
		Entry.Result = TEXT("(the engine's event carries no result text)");

		FTurboMCPCallLog::Get().Write(Entry);
	}

	// PASSED ON, ALWAYS, so installing this provider cannot lose an event the previous one wanted.
	if (Wrapped.IsValid())
	{
		Wrapped->RecordEvent(MoveTemp(EventName), Attributes, Mode);
	}
}

// ---------------------------------------------------------------------------------------------
// The pass-through surface. Every one of these forwards to the wrapped provider when there is one,
// and answers something harmless when there is not. This provider SENDS NOTHING over the network:
// it exists to observe and to forward, so a method with nothing to forward has nothing to do.
// ---------------------------------------------------------------------------------------------

void FTurboMCPAnalyticsProvider::SetAppID(FString&& AppID)
{
	if (Wrapped.IsValid()) { Wrapped->SetAppID(MoveTemp(AppID)); }
}

bool FTurboMCPAnalyticsProvider::StartSession(
	const TArray<FAnalyticsEventAttribute>& Attributes)
{
	if (Wrapped.IsValid()) { return Wrapped->StartSession(Attributes); }
	return true;
}

void FTurboMCPAnalyticsProvider::EndSession()
{
	if (Wrapped.IsValid()) { Wrapped->EndSession(); }
}

FString FTurboMCPAnalyticsProvider::GetSessionID() const
{
	return Wrapped.IsValid() ? Wrapped->GetSessionID() : FString();
}

bool FTurboMCPAnalyticsProvider::SetSessionID(const FString& InSessionID)
{
	if (Wrapped.IsValid()) { return Wrapped->SetSessionID(InSessionID); }
	return true;
}

void FTurboMCPAnalyticsProvider::FlushEvents()
{
	// Forwarded, so a caller that flushes through this provider still flushes the real one. This
	// provider holds nothing to flush: it writes to a file and returns.
	if (Wrapped.IsValid()) { Wrapped->FlushEvents(); }
}

void FTurboMCPAnalyticsProvider::SetUserID(const FString& InUserID)
{
	if (Wrapped.IsValid()) { Wrapped->SetUserID(InUserID); }
}

FString FTurboMCPAnalyticsProvider::GetUserID() const
{
	return Wrapped.IsValid() ? Wrapped->GetUserID() : FString();
}

void FTurboMCPAnalyticsProvider::SetDefaultEventAttributes(
	TArray<FAnalyticsEventAttribute>&& Attributes)
{
	if (Wrapped.IsValid()) { Wrapped->SetDefaultEventAttributes(MoveTemp(Attributes)); }
}

TArray<FAnalyticsEventAttribute> FTurboMCPAnalyticsProvider::GetDefaultEventAttributesSafe() const
{
	return Wrapped.IsValid() ? Wrapped->GetDefaultEventAttributesSafe()
		: TArray<FAnalyticsEventAttribute>();
}

int32 FTurboMCPAnalyticsProvider::GetDefaultEventAttributeCount() const
{
	return Wrapped.IsValid() ? Wrapped->GetDefaultEventAttributeCount() : 0;
}

FAnalyticsEventAttribute FTurboMCPAnalyticsProvider::GetDefaultEventAttribute(
	int AttributeIndex) const
{
	// A named placeholder rather than a crash: this provider has no default attributes of its own,
	// and answering one is only needed so a caller that asks does not fall over.
	return Wrapped.IsValid()
		? Wrapped->GetDefaultEventAttribute(AttributeIndex)
		: FAnalyticsEventAttribute(TEXT("TurboMCP.None"), 0);
}

void FTurboMCPAnalyticsProvider::SetAppVersion(FString&& AppVersion)
{
	if (Wrapped.IsValid()) { Wrapped->SetAppVersion(MoveTemp(AppVersion)); }
}

bool FTurboMCPAnalyticsProvider::StartSession(
	FString InSessionID, const TArray<FAnalyticsEventAttribute>& Attributes)
{
	if (Wrapped.IsValid()) { return Wrapped->StartSession(MoveTemp(InSessionID), Attributes); }
	return true;
}

bool FTurboMCPAnalyticsProvider::ShouldRecordEvent(const FString& EventName) const
{
	if (Wrapped.IsValid()) { return Wrapped->ShouldRecordEvent(EventName); }
	return true;
}

void FTurboMCPAnalyticsProvider::SetUrlDomain(
	const FString& Domain, const TArray<FString>& AltDomains)
{
	if (Wrapped.IsValid()) { Wrapped->SetUrlDomain(Domain, AltDomains); }
}

void FTurboMCPAnalyticsProvider::SetUrlPath(const FString& Path)
{
	if (Wrapped.IsValid()) { Wrapped->SetUrlPath(Path); }
}

void FTurboMCPAnalyticsProvider::SetHeader(
	const FString& HeaderName, const FString& HeaderValue)
{
	if (Wrapped.IsValid()) { Wrapped->SetHeader(HeaderName, HeaderValue); }
}

void FTurboMCPAnalyticsProvider::SetEventCallback(const OnEventRecorded& Callback)
{
	if (Wrapped.IsValid()) { Wrapped->SetEventCallback(Callback); }
}

void FTurboMCPAnalyticsProvider::BlockUntilFlushed(float InTimeoutSec)
{
	if (Wrapped.IsValid()) { Wrapped->BlockUntilFlushed(InTimeoutSec); }
}

const FAnalyticsET::Config& FTurboMCPAnalyticsProvider::GetConfig() const
{
	if (Wrapped.IsValid()) { return Wrapped->GetConfig(); }
	return OwnConfig;
}

void FTurboMCPAnalyticsProvider::SetShouldRecordEventFunc(
	const ShouldRecordEventFunction& ShouldRecordEventFunc)
{
	if (Wrapped.IsValid()) { Wrapped->SetShouldRecordEventFunc(ShouldRecordEventFunc); }
}

FOnPreAnalyticsEventProcessed& FTurboMCPAnalyticsProvider::OnPreAnalyticsEventProcessed()
{
	// The wrapped provider's own delegate when there is one, so a caller that registers on this
	// object still hears the engine's filtering. When there is none, a local one is answered so the
	// reference is valid rather than a crash.
	if (Wrapped.IsValid()) { return Wrapped->OnPreAnalyticsEventProcessed(); }
	static FOnPreAnalyticsEventProcessed Local;
	return Local;
}

FOnAnalyticsEventQueued& FTurboMCPAnalyticsProvider::OnAnalyticsEventQueued()
{
	if (Wrapped.IsValid()) { return Wrapped->OnAnalyticsEventQueued(); }
	static FOnAnalyticsEventQueued Local;
	return Local;
}
