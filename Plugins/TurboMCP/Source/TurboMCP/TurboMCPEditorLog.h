// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
#pragma once
#include "CoreMinimal.h"
#include "Misc/OutputDevice.h"

/** Bounded enabled-session editor output, separate from tool-call recording.
 * The output device never logs from Serialize or while holding its mutex.
 * A local instance can exercise retention without registering with GLog.
 */
class FTurboMCPEditorLog : public FOutputDevice
{
public:
    static FTurboMCPEditorLog& Get();
    void Start();
    void Stop();
    virtual void Serialize(const TCHAR* Message, ELogVerbosity::Type Verbosity, const FName& Category) override;
    virtual bool CanBeUsedOnAnyThread() const override { return true; }
    virtual bool CanBeUsedOnMultipleThreads() const override { return true; }
    FString Read(const FString& Category, const FString& Contains, const FString& Since, const FString& Cursor, int32 Limit);
private:
    struct FRecord
    {
        uint64 Sequence = 0;
        FDateTime Time;
        FString Category, Message;
        ELogVerbosity::Type Verbosity = ELogVerbosity::Log;
        bool Truncated = false;
    };
    FCriticalSection Mutex;
    TArray<FRecord> Records;
    FGuid Session = FGuid::NewGuid();
    uint64 Sequence = 0;
    int32 Head = 0;
    bool Attached = false;
};
