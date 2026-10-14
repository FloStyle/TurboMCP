// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "TurboMCPTestCompileFixture.h"

#include "Engine/Blueprint.h"
#include "HAL/PlatformTLS.h"
#include "Kismet2/CompilerResultsLog.h"
#include "Logging/LogMacros.h"
#include "Misc/AssertionMacros.h"
#include "Misc/OutputDeviceRedirector.h"

// THE COMPILER PATH THIS FIXTURE DRIVES EXISTS ONLY WITH THE EDITOR, and this module is loaded in both
// shapes, so the fixture compiles everywhere and answers honestly where it cannot act.
#if WITH_EDITOR
#include "BlueprintCompilationManager.h"
#include "Editor.h"
#include "Editor/EditorEngine.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "UObject/UObjectGlobals.h"
#endif

bool UTurboMCPTestCompileFixture::bHookArmed = false;
int32 UTurboMCPTestCompileFixture::HookFireCount = 0;
int32 UTurboMCPTestCompileFixture::EnsureCount = 0;
int32 UTurboMCPTestCompileFixture::EnsureLogCount = 0;
FString UTurboMCPTestCompileFixture::EnsureLogText;
FString UTurboMCPTestCompileFixture::EnsureLogVerbs;
FString UTurboMCPTestCompileFixture::LastEnsureMessage;
TWeakObjectPtr<UBlueprint> UTurboMCPTestCompileFixture::QueuedTarget = nullptr;

void FTurboMCPTestCompileLogDevice::Serialize(
	const TCHAR* InMessage, ELogVerbosity::Type InVerbosity, const FName& InCategory)
{
	// *** A RE-ENTRANCY GUARD, BECAUSE NOTHING IN THE LOG PATH IS RE-ENTRANT. *** A log write from inside a
	// log call is how the first version of this fixture killed the editor, and a device that is itself
	// writing log lines is the same hazard from the other side. This costs one bool and removes the class.
	if (bInsideSerialize)
	{
		return;
	}
	TGuardValue<bool> ReentrancyGuard(bInsideSerialize, true);

	++CallsSeen;

	const bool bIsWarning = InVerbosity == ELogVerbosity::Warning;
	const bool bIsError = InVerbosity == ELogVerbosity::Error || InVerbosity == ELogVerbosity::Fatal;

	// ONE DIAGNOSTIC LINE, ON THE FIRST CALL ONLY, BECAUSE THE ANSWER DECIDES A FIX RATHER THAN A COMMENT: is
	// the log delivered on the game thread (and the thread id comparison was what failed), or is it delivered
	// on some other thread entirely (and a thread filter cannot work)?
	if (CallsSeen == 1)
	{
		FirstCallOwnerId = OwnerThreadId;
		FirstCallThisId = FPlatformTLS::GetCurrentThreadId();
		bFirstCallInGameThread = IsInGameThread();
	}

	// *** NO THREAD FILTER, AND THAT IS A MEASURED DECISION WITH A MEASURED SURPRISE UNDER IT. ***
	//
	// The first version copied `FTurboMCPLogCapture`'s rule - record only the creating thread's messages - and
	// then, when that recorded nothing, asked `IsInGameThread()` instead. BOTH FILTERS THREW AWAY THE WHOLE
	// COMPILE WINDOW, and the device's own counters are what said so: `deviceSaw=83 (offThread=83)`, every
	// single call rejected, while the engine's line was plainly in the editor's log.
	//
	// WHAT THAT MEANS: **`GLog` HANDS A LOG CALL TO ITS DEVICES FROM A THREAD THAT IS NEITHER THE CALLER NOR
	// THE GAME THREAD.** That is worth recording rather than working around, because `FTurboMCPLogCapture`
	// applies exactly this rule (`TurboMCPLogCapture.cpp:62-68`) and therefore CANNOT SEE AN ENGINE'S LINE
	// raised during a compile - which is the very message `T04` is about, and which the commit before this
	// one claimed the capture would now report. The claim was wrong about the mechanism.
	//
	// SO THE SPAN IS THE ATTRIBUTION HERE, not the thread: the device is attached for exactly one compile and
	// removed immediately after it, so what it counts is what happened during that compile.
	if (!bIsWarning && !bIsError)
	{
		return;
	}

	++UTurboMCPTestCompileFixture::EnsureLogCount;
	UTurboMCPTestCompileFixture::EnsureLogVerbs +=
		FString::Printf(TEXT("%s "), bIsError ? TEXT("Error") : TEXT("Warning"));
	if (UTurboMCPTestCompileFixture::EnsureLogText.IsEmpty() && InMessage != nullptr)
	{
		UTurboMCPTestCompileFixture::EnsureLogText = FString(InMessage);
		UTurboMCPTestCompileFixture::EnsureLogText.ReplaceInline(TEXT("\n"), TEXT(" | "));
	}
}

FString UTurboMCPTestCompileFixture::CompileWithQueuedRequest(
	const FString& AssetPath, const FString& AlsoQueuePath)
{
#if !WITH_EDITOR
	return TEXT("error: this fixture needs the editor, and the engine was built without it");
#else
	if (AssetPath.IsEmpty())
	{
		return TEXT("error: no asset path given");
	}
	if (GEditor == nullptr)
	{
		return TEXT("error: the editor is not available, so nothing can be compiled");
	}

	// AN OBJECT PATH IS ACCEPTED AS WELL AS A PACKAGE PATH, because the caller reads the asset's path out of
	// E-COMPILE's own report and that report prints `/Game/Tests/BP_NodeTest.BP_NodeTest`. Loading either
	// form has to work or the suite would have to rewrite the tool's own output to call this.
	auto LoadBlueprint = [](const FString& InPath) -> UBlueprint*
	{
		if (UBlueprint* Found = FindObject<UBlueprint>(nullptr, *InPath))
		{
			return Found;
		}
		if (UBlueprint* Loaded = LoadObject<UBlueprint>(nullptr, *InPath))
		{
			return Loaded;
		}
		return Cast<UBlueprint>(StaticLoadObject(UBlueprint::StaticClass(), nullptr, *InPath));
	};

	UBlueprint* Target = LoadBlueprint(AssetPath);
	if (Target == nullptr)
	{
		return FString::Printf(TEXT("error: %s could not be loaded as a Blueprint"), *AssetPath);
	}

	// EMPTY MEANS "QUEUE THE SAME ASSET AGAIN", which is still a compile requested while a compile runs, and
	// keeps the caller from having to name a second asset just to arm the condition.
	const FString QueuePath = AlsoQueuePath.IsEmpty() ? AssetPath : AlsoQueuePath;
	UBlueprint* ToQueue = LoadBlueprint(QueuePath);
	if (ToQueue == nullptr)
	{
		return FString::Printf(TEXT("error: %s could not be loaded as a Blueprint"), *QueuePath);
	}

	// ARMED ONLY FOR THE COMPILE BELOW, so a compile another part of the editor runs cannot be counted as
	// this fixture's doing. The count is reset with it, for the same reason.
	bHookArmed = true;
	HookFireCount = 0;
	EnsureCount = 0;
	EnsureLogCount = 0;
	EnsureLogText.Reset();
	EnsureLogVerbs.Reset();
	LastEnsureMessage.Reset();
	QueuedTarget = ToQueue;

	// THE DEVICE IS ATTACHED FOR THE COMPILE AND REMOVED AFTER IT, which is the same span semantics
	// `E-COMPILE` uses. `GLog` routes to every registered device, so the editor's own log output is
	// untouched by this.
	//
	// *** A PLAIN STRUCT RATHER THAN THE FIXTURE ITSELF, AND THAT IS A FIX RATHER THAN A TASTE. *** The first
	// version inherited `FOutputDevice` INTO the `UCLASS` and made the device with
	// `static UTurboMCPTestCompileFixture Device;`, which killed the editor with
	// `FObjectInitializer::Get() can only be used inside of UObject-derived class constructor`
	// (`UObjectGlobals.cpp:4886`): a `UObject` may only be created through `NewObject`. A function-local
	// static of a non-UObject type is exactly right here - it outlives the calls that use it, and nothing
	// collects it.
	static FTurboMCPTestCompileLogDevice Device;
	Device.OwnerThreadId = FPlatformTLS::GetCurrentThreadId();
	Device.CallsSeen = 0;
	if (GLog != nullptr)
	{
		GLog->AddOutputDevice(&Device);
	}

	// THE HOOK IS THE WHOLE FIXTURE. `OnBlueprintPreCompile` is broadcast from INSIDE the compile window, so
	// the request made here arrives while the engine's own `GCompilingBlueprint` guard is up - which is the
	// condition `BlueprintCompilationManager.cpp:331` exists to catch.
	FDelegateHandle Handle = GEditor->OnBlueprintPreCompile().AddLambda(
		[ToQueue](UBlueprint* BlueprintBeingCompiled)
		{
			if (!bHookArmed)
			{
				return;
			}
			++HookFireCount;

			// THE ENGINE'S OWN QUEUE, NOT A COMPILE OF OUR OWN: `QueueForCompilation` is the call the session
			// 2 log shows arriving from elsewhere while an asset compiled, and its own `ensureMsgf` is the
			// line the report under test has to notice.
			FBlueprintCompilationManager::QueueForCompilation(ToQueue);
			UE_LOG(LogTemp, Display,
				TEXT("TURBOMCP-TESTFIXTURE-QUEUED %s while %s was compiling"),
				*ToQueue->GetName(), *BlueprintBeingCompiled->GetName());
		});

	FCompilerResultsLog Results;
	Results.bLogInfoOnly = false;

	// *** THE ENSURE IS EXPECTED, SO IT IS HANDLED RATHER THAN LEFT TO STOP THE TEST. *** MEASURED: without
	// this, `Tools/run-suites.ps1` sees the fixture's ensure and marks the AUTOMATION TEST `Fail` - the
	// engine's `FAutomationTestFramework` turns a handled ensure raised inside a test into an error, and it
	// did so for BOTH lines the engine writes (`BlueprintCompilationManager.cpp:331` and `:2243`). A test
	// that always fails cannot carry a passing verdict, and the condition under test is the POINT of the
	// run rather than an accident in it.
	TFunction<bool(const FEnsureHandlerArgs&)> PreviousHandler = SetEnsureHandler(
		[](const FEnsureHandlerArgs& Args)
		{
			++EnsureCount;
			LastEnsureMessage = Args.Message != nullptr ? FString(Args.Message) : FString();
			LastEnsureMessage.ReplaceInline(TEXT("\n"), TEXT(" | "));

			// *** THE HANDLER RETURNS FALSE, SO THE ENGINE WRITES ITS OWN LINE - AND THAT IS THE POINT. ***
			// Returning true here would SKIP the engine's `UE_LOGFMT`, so no line would reach the log at all
			// and the only thing recorded would be this fixture's own count. MEASURED: with `return true` the
			// fixture reported `engineLog=0 [ ]`, which is not evidence about `T04` - it is evidence that the
			// fixture suppressed the very line the task is about. Letting the engine write means the level is
			// the ENGINE's (`Error` when `GEnsuresAreErrors` is set, `Warning` otherwise) and the suite outside
			// can read it. The test around this fixture therefore declares the engine's two lines as expected,
			// because an `Error` line raised inside a test is otherwise a test failure.
			return false;
		});

	FKismetEditorUtilities::CompileBlueprint(Target, EBlueprintCompileOptions::None, &Results);

	// THE HANDLER IS PUT BACK WHATEVER THE COMPILE DID, because a leftover handler would silently swallow a
	// real ensure in a later test - which is a worse defect than the one this fixture exists to expose.
	SetEnsureHandler(PreviousHandler);

	// THE DEVICE COMES OFF HERE, so nothing after this compile is counted as this fixture's doing.
	if (GLog != nullptr)
	{
		GLog->RemoveOutputDevice(&Device);
	}

	GEditor->OnBlueprintPreCompile().Remove(Handle);
	bHookArmed = false;

	// THE ANSWER SAYS WHETHER THE CONDITION WAS EVEN BUILT, because a suite that measured nothing would
	// otherwise look exactly like a suite that measured silence. `queuedFromHook` is the number of compile
	// requests this fixture made from inside a compile, `compilerMessages` is what the compiler's own
	// results log carried - the second number is the one that must stay 0 for the test to mean anything,
	// because a non-zero one would mean the compiler had reported the problem itself - `ensures` is how many
	// times THE ENGINE raised its own assertion while this compile ran, and `engineLog` is how many of the
	// engine's lines arrived at a severity the capture records, WITH THE SEVERITY THE ENGINE USED.
	return FString::Printf(
		TEXT("fixture: compiled %s (%d compiler message(s); %d warning(s), %d error(s) from the compiler); ")
		TEXT("queuedFromHook=%d; queuedTarget=%s; ensures=%d; engineLog=%d [%s]; deviceSaw=%d; firstCall owner=%u this=%u inGame=%s"),
		*Target->GetName(), Results.Messages.Num(), Results.NumWarnings, Results.NumErrors,
		HookFireCount, *QueuePath, EnsureCount, EnsureLogCount, *EnsureLogVerbs,
		Device.CallsSeen, Device.FirstCallOwnerId, Device.FirstCallThisId,
		Device.bFirstCallInGameThread ? TEXT("yes") : TEXT("no"));
#endif
}

FString UTurboMCPTestCompileFixture::EnsureSeverity()
{
	return FString::Printf(TEXT("engineLog=%d [%s] %s"), EnsureLogCount, *EnsureLogVerbs, *EnsureLogText);
}

bool UTurboMCPTestCompileFixture::ProbeWhetherDeliveryFollowsTheCaller()
{
	if (GLog == nullptr)
	{
		return false;
	}

	// A SEPARATE DEVICE FROM THE ONE THE COMPILE USES, so this probe cannot disturb a measurement and cannot
	// be disturbed by one. It records nothing but its own call count, and its owner thread is whatever thread
	// calls this function - which is the whole point.
	struct FDeliveryProbeDevice : public FOutputDevice
	{
		int32 Count = 0;
		virtual void Serialize(const TCHAR*, ELogVerbosity::Type, const FName&) override { ++Count; }
	};

	static FDeliveryProbeDevice Probe;
	Probe.Count = 0;

	GLog->AddOutputDevice(&Probe);
	// ONE LINE AT `Warning`, the lowest severity any capture records, written from this thread.
	UE_LOG(LogTemp, Warning, TEXT("TURBOMCP-TESTFIXTURE-PROBE delivery probe from the calling thread"));

	// *** THE FIRST VERSION OF THIS PROBE REMOVED THE DEVICE IMMEDIATELY AFTER THE LOG CALL AND REPORTED
	// "NOT RECORDED" - WHICH MEASURED THE PROBE RATHER THAN THE ENGINE. If `GLog` hands a line to its devices
	// later, a device that is detached one statement after the write cannot see its own line, and the answer
	// says nothing about the delivery thread. So the queue is FLUSHED while the device is still attached,
	// which is the same operation a writer uses to make a log line visible before reading it back.
	GLog->Flush();
	GLog->RemoveOutputDevice(&Probe);

	// THE PROBE'S OWN LINE IS LOGGED AT `Log` AND THEREFORE IGNORED BY THE DEVICE ABOVE, which counts every
	// call regardless of severity - so a count above zero means the call reached a device attached in this
	// same callstack, on this same thread.
	return Probe.Count > 0;
}

FString UTurboMCPTestCompileFixture::HookState()
{
	return FString::Printf(TEXT("hook armed=%s, fired %d time(s), last queued target=%s, ensures=%d"),
		bHookArmed ? TEXT("yes") : TEXT("no"),
		HookFireCount,
		QueuedTarget.IsValid() ? *QueuedTarget->GetName() : TEXT("(none)"),
		EnsureCount);
}
