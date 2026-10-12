// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "TurboMCPAnalyticsProvider.h"

#include "AnalyticsET.h"
#include "Logging.h"
#include "ModelContextProtocolAnalytics.h"
#include "IModelContextProtocolTool.h"
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
	// THE ENGINE'S OWN, CALLED RATHER THAN REPRODUCED.
	//
	// `UE::ModelContextProtocol::Analytics::HashToolIdentifier` is exported `UE_API` from
	// `ModelContextProtocolAnalytics.h`, so there was never a reason to reimplement it. The first
	// version did — UTF-8 bytes through `FBlake3::HashBuffer`, then `LexToString` — and that was a
	// liability rather than a virtue: a reimplementation is only correct while it matches, and
	// NOTHING WOULD HAVE SAID SO when it stopped matching.
	//
	// The same function generated the hashes in the log, so calling it makes the lookup exact by
	// construction rather than by agreement.
	return UE::ModelContextProtocol::Analytics::HashToolIdentifier(InIdentifier);
}

void FTurboMCPAnalyticsProvider::RegisterKnownName(const FString& InName)
{
	// AN EMPTY NAME IS A REAL KEY, not a mistake to skip. `ParseToolsetName` returns the empty
	// string for a tool with no dot in its name, and the engine's own mapping commandlet registers
	// that empty entry so rows for top-level tools decode. The first version returned early on an
	// empty name, which would have left every top-level tool unresolved.
	FScopeLock Lock(&RegistryMutex);
	Registry().Add(HashToolIdentifier(InName), InName);
}

void FTurboMCPAnalyticsProvider::RegisterServedToolsetsAndTools()
{
	// THE ENGINE'S OWN LIST, WHICH REMOVES THE LIMITATION THE FIRST VERSION HAD AND CLOSES Q-10.
	//
	// The first version listed this plugin's tool names BY HAND and could not name an engine tool
	// at all, because `UToolsetRegistry` exposes no list of served toolsets or tools. That is still
	// true of `UToolsetRegistry` — but it was the wrong place to look. `IModelContextProtocolModule
	// ::GetTools()` returns every registered tool with its identifier, and it is exactly what the
	// engine's own `ModelContextProtocolToolHashMappingCommandlet` uses to build the very mapping
	// this provider needs. Reading it is strictly better than a hand-kept table: a tool added
	// anywhere resolves without this file being touched.
	IModelContextProtocolModule* Module = IModelContextProtocolModule::Get();
	if (Module == nullptr)
	{
		return;
	}

	TSet<FString> Names;
	for (const TSharedRef<IModelContextProtocolTool>& Tool : Module->GetTools())
	{
		Names.Add(Tool->GetName());
	}

	// THE META-TOOLS ARE NOT IN `GetTools()`, AND THAT WAS THE OTHER HALF OF Q-10.
	//
	// In tool-search mode `call_tool` receives the call and then emits a SECOND event for the
	// inner tool it dispatched — measured at `ModelContextProtocolServer.cpp:887` and `:907`,
	// where the comment says the second emission exists so per-toolset analytics for the real
	// target stay visible. So each of this plugin's calls produced TWO lines: one for `call_tool`,
	// whose hashes did not resolve, and one for the inner tool, whose did.
	//
	// `call_tool`, `list_toolsets` and `describe_toolset` never appear in `GetTools()` because the
	// tool-search adapter registers them, which is precisely why the engine's own commandlet adds
	// the same three by hand (`ModelContextProtocolToolHashMappingCommandlet.cpp` lines 54-56).
	Names.Add(TEXT("list_toolsets"));
	Names.Add(TEXT("describe_toolset"));
	Names.Add(TEXT("call_tool"));

	for (const FString& Name : Names)
	{
		RegisterKnownName(Name);

		// The TOOLSET PREFIX, through the engine's own parser, which splits at the LAST dot. A
		// top-level tool has no dot, so its prefix is the empty string — and that empty entry is
		// what makes `call_tool`'s own toolset resolve.
		RegisterKnownName(UE::ModelContextProtocol::Analytics::ParseToolsetName(Name));
	}

	// THE TOOLSETS THE SERVER ACTUALLY SERVES, UNDER BOTH NAMES THEY GO BY.
	//
	// `GetTools()` DID NOT HELP WITH THESE, and that is measured rather than assumed: in
	// tool-search mode it returns exactly THREE identifiers — `call_tool`, `describe_toolset` and
	// `list_toolsets`, all with an empty toolset prefix — because in that mode the toolset's own
	// tools are DISPATCHED THROUGH `call_tool` rather than registered. The engine says so itself in
	// `ModelContextProtocolToolsetRegistryAdapter.h`: "In tool-search mode ... toolset tools are
	// dispatched through call_tool without being registered."
	//
	// So the inner event's toolset hash is over the string the CALLER passed as `toolset_name`, and
	// that is the bare toolset name. Measured: with `TurboMCP.TurboMCPReadToolset` registered, the
	// event's toolset hash `025927b6f59b` did not resolve, and with the bare `TurboMCPReadToolset`
	// it does. Both forms are registered, because a caller may pass either and the qualified form
	// is what `describe_toolset` reports.
	//
	// `UToolsetRegistry` still exposes no list of served toolsets, so these four are named — but
	// they are the plugin's own, and `check-mcp-server.py` fails if a fifth toolset is registered
	// without a line here, so the list cannot silently drift.
	static const TCHAR* const OwnToolsets[] = {
		TEXT("TurboMCPReadToolset"),
		TEXT("TurboMCPLookToolset"),
		TEXT("TurboMCPPreviewToolset"),
		TEXT("TurboMCPEditToolset"),
	};
	for (const TCHAR* Toolset : OwnToolsets)
	{
		RegisterKnownName(Toolset);
		RegisterKnownName(FString::Printf(TEXT("TurboMCP.%s"), Toolset));
	}

	// THE INNER TOOL IS HASHED OVER ITS FULL NAME, NOT ITS BARE ONE.
	//
	// `ModelContextProtocolServer.cpp:904` builds `ResolvedFullName` as
	// `toolset_name + "." + tool_name` and passes THAT to `RecordToolCallEvent`, so the tool slot
	// of an inner event holds the hash of the whole qualified string. Registering the bare tool
	// name is not enough, and measured it was not: the slot stayed unresolved with the bare names
	// registered and the toolsets resolved.
	//
	// Every combination is registered, because the `toolset_name` a caller passes may be either
	// form and the pairing between a tool and its toolset is not discoverable here for the same
	// reason the toolset list is not. The combinations are cheap: one hash each, once at startup.
	static const TCHAR* const OwnToolNames[] = {
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
	for (const TCHAR* Tool : OwnToolNames)
	{
		RegisterKnownName(Tool);
		for (const TCHAR* Toolset : OwnToolsets)
		{
			// The qualified form FIRST, because that is the one the server builds.
			RegisterKnownName(FString::Printf(TEXT("TurboMCP.%s.%s"), Toolset, Tool));
			// The bare-toolset form, which is what a caller passes as `toolset_name`.
			RegisterKnownName(FString::Printf(TEXT("%s.%s"), Toolset, Tool));
		}
	}

	// Registered explicitly so the entry exists even if the set above came out empty.
	RegisterKnownName(FString());

	UE_LOG(LogTurboMCP, Log,
		TEXT("TurboMCP call log: registered %d tool identifier(s) and their toolsets for hash resolution"),
		Names.Num());

	// The identifiers are logged, bounded, because the SHAPE of an identifier is the thing that had
	// to be discovered here. A reader of the next puzzle wants it without rebuilding.
	{
		TArray<FString> Sorted = Names.Array();
		Sorted.Sort();
		for (int32 Index = 0; Index < Sorted.Num() && Index < 12; ++Index)
		{
			UE_LOG(LogTurboMCP, Log, TEXT("  identifier[%d]: '%s' -> toolset '%s'"),
				Index, *Sorted[Index],
				*UE::ModelContextProtocol::Analytics::ParseToolsetName(Sorted[Index]));
		}
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

		// RESOLVED AGAINST THE REGISTRY, so the log says a real name rather than a 64-character
		// hash. An unresolved hash is written AS the hash with a marker, never invented and never
		// dropped: an opaque identifier is still evidence a call happened.
		//
		// RESOLVED AND EMPTY ARE DIFFERENT ANSWERS, and conflating them was a real bug. A top-level
		// tool has an EMPTY toolset prefix — the empty string is a legitimate key in the registry,
		// which is why the engine's own mapping commandlet registers it — so `call_tool` resolved
		// to the empty string and the `IsEmpty()` test below then reported it as UNRESOLVED:
		//
		//     (engine tool ending af1349b9f5f9)  .  call_tool
		//
		// a line saying it could not name a toolset it had just named. So RESOLUTION is tracked by
		// whether the key was FOUND, not by whether the value is empty.
		FString ToolsetName;
		FString ToolName;
		bool bToolsetResolved = false;
		bool bToolResolved = false;
		{
			FScopeLock Lock(&RegistryMutex);
			if (const FString* Found = Registry().Find(ToolsetHash))
			{
				ToolsetName = *Found;
				bToolsetResolved = true;
			}
			if (const FString* Found = Registry().Find(ToolHash))
			{
				ToolName = *Found;
				bToolResolved = true;
			}
		}

		if (!bToolsetResolved)
		{
			ToolsetName = FString::Printf(TEXT("(engine tool ending %s)"), *ToolsetHash.Left(12));
		}
		else if (ToolsetName.IsEmpty())
		{
			// THE EMPTY PREFIX IS A REAL ANSWER AND IT IS WRITTEN AS ONE. A top-level tool — the
			// tool-search meta-tools and anything else registered without a toolset — hashes
			// against the empty prefix. Written as `(top-level)` so a reader sees a name rather
			// than an empty column, and so it cannot be mistaken for a failed lookup.
			ToolsetName = TEXT("(top-level)");
		}
		if (!bToolResolved)
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