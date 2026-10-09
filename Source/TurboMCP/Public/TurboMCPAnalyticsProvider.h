// Copyright FloStyle. All Rights Reserved.
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once

#include "CoreMinimal.h"
#include "AnalyticsET.h"
#include "IAnalyticsProviderET.h"
#include "Containers/Map.h"
#include "Containers/UnrealString.h"
#include "Templates/SharedPointer.h"

/**
 * The engine-side half of the call log: every tool call, including the ENGINE's own tools.
 *
 * THE ENGINE'S EVENT DOES NOT CARRY THE NAMES
 * -------------------------------------------
 * Measured from `ModelContextProtocolAnalytics.cpp`, a tool-call event carries exactly:
 *
 *     SessionId, ToolsetNameHash, ToolNameHash, Duration, ResultStatus
 *
 * and the event name is `<namespace>.ToolCall`. The names are **BLAKE3 hashes**, and there are no
 * arguments at all. `07_ENGINE_NOTES.md` section 2.4 anticipated this and gave the shape of the
 * answer.
 *
 * So the hashes are RESOLVED rather than left opaque: the engine's own `HashToolIdentifier` is
 * reproduced here (UTF-8 bytes through `FBlake3::HashBuffer`, then `LexToString`), and every known
 * toolset and tool name is hashed once at startup into a lookup. A hash that resolves is written as
 * the real name; one that does not is written AS THE HASH, marked unresolved, rather than being
 * invented or dropped.
 *
 * NEVER REPLACE A HOOK WITHOUT PASSING ITS EVENTS ON. Whatever provider was installed before this
 * one is held and every call is forwarded to it, so installing this cannot lose an event the
 * engine's own telemetry wanted.
 */
class FTurboMCPAnalyticsProvider : public IAnalyticsProviderET
{
public:
	/** Installs the provider over whatever is there now, if it is not already installed. */
	static void Install();

	/** Removes the provider, restoring the one it wrapped. */
	static void Uninstall();

	/** True when this provider is the one installed. */
	static bool IsInstalled();

	/**
	 * Registers a name so its hash can be resolved in the log.
	 *
	 * Called for the plugin's own toolsets and tools at startup, and for the engine's served
	 * toolsets from the registry, so a call to any of them is written with its real name.
	 */
	static void RegisterKnownName(const FString& InName);

	/** Registers every toolset and tool the registry serves. */
	static void RegisterServedToolsetsAndTools();

	// IAnalyticsProviderET
	virtual void SetAppID(FString&& AppID) override;

	// The rest of `IAnalyticsProvider`, all pure virtual and all forwarded. They are listed
	// together because none of them is interesting: this provider observes and passes on, so a
	// method with nothing to observe has nothing to do but forward.
	virtual bool StartSession(const TArray<FAnalyticsEventAttribute>& Attributes) override;
	virtual void EndSession() override;
	virtual FString GetSessionID() const override;
	virtual bool SetSessionID(const FString& InSessionID) override;
	virtual void FlushEvents() override;
	virtual void SetUserID(const FString& InUserID) override;
	virtual FString GetUserID() const override;
	virtual void SetDefaultEventAttributes(TArray<FAnalyticsEventAttribute>&& Attributes) override;
	virtual TArray<FAnalyticsEventAttribute> GetDefaultEventAttributesSafe() const override;
	virtual int32 GetDefaultEventAttributeCount() const override;
	virtual FAnalyticsEventAttribute GetDefaultEventAttribute(int AttributeIndex) const override;
	virtual void SetAppVersion(FString&& AppVersion) override;
	virtual bool StartSession(FString InSessionID,
		const TArray<FAnalyticsEventAttribute>& Attributes) override;
	virtual bool ShouldRecordEvent(const FString& EventName) const override;
	virtual void RecordEvent(FString&& EventName,
		const TArray<FAnalyticsEventAttribute>& Attributes) override;
	virtual void RecordEvent(FString&& EventName,
		const TArray<FAnalyticsEventAttribute>& Attributes,
		EAnalyticsRecordEventMode Mode) override;
	virtual void SetUrlDomain(const FString& Domain, const TArray<FString>& AltDomains) override;
	virtual void SetUrlPath(const FString& Path) override;
	virtual void SetHeader(const FString& HeaderName, const FString& HeaderValue) override;
	virtual void SetEventCallback(const OnEventRecorded& Callback) override;
	virtual void BlockUntilFlushed(float InTimeoutSec) override;
	virtual const FAnalyticsET::Config& GetConfig() const override;
	virtual void SetShouldRecordEventFunc(const ShouldRecordEventFunction& ShouldRecordEventFunc) override;
	virtual FOnPreAnalyticsEventProcessed& OnPreAnalyticsEventProcessed() override;
	virtual FOnAnalyticsEventQueued& OnAnalyticsEventQueued() override;

private:
	/** The provider that was installed before this one, so its events are not lost. */
	TSharedPtr<IAnalyticsProviderET> Wrapped;

	/** A config answered to any caller that asks, since this provider sends nothing itself. */
	FAnalyticsET::Config OwnConfig;

	/** The hash-to-name lookup, guarded because the server records from its own threads. */
	static FCriticalSection RegistryMutex;
	static TMap<FString, FString>& Registry();

	/** The engine's own hash, reproduced. */
	static FString HashToolIdentifier(const FString& InIdentifier);

	/** The value of an attribute by name, or an empty string. */
	static FString AttributeValue(const TArray<FAnalyticsEventAttribute>& Attributes,
		const FString& InName);
};
