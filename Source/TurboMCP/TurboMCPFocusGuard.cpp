// Copyright (c) 2026 Florian ESCHALIER (FloStyle)
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "TurboMCPFocusGuard.h"

#include "HAL/PlatformProcess.h"
#include "Misc/App.h"
#include "Misc/CoreDelegates.h"
#include "Framework/Application/SlateApplication.h"

#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include <windows.h>
#include "Windows/HideWindowsPlatformTypes.h"
#endif

#include "Logging.h"

namespace
{
	/** The window that had the foreground BEFORE this editor took it, so it can be handed back. */
	void* GPreviousForeground = nullptr;

	FDelegateHandle GActivationHandle;

	bool GInstalled = false;
}

#if PLATFORM_WINDOWS
namespace
{
	/**
	 * Remembers what currently holds the foreground, IF THIS EDITOR IS NOT THE ONE HOLDING IT.
	 *
	 * The guard must not overwrite its memory with the editor's own window - if it did, the first activation would
	 * record the editor as "what to give the foreground back to" and the guard would spend the rest of the run
	 * handing the foreground to itself.
	 */
	void RememberForegroundBeforeUs()
	{
		const HWND Foreground = ::GetForegroundWindow();
		const HWND Ours = ::GetActiveWindow();
		if (Foreground == nullptr || Foreground == Ours)
		{
			return;
		}

		// Only remember a window that belongs to ANOTHER process. The editor owns several of its own, and handing
		// the foreground to a sibling window would still be taking it from the user.
		DWORD OwnerProcess = 0;
		::GetWindowThreadProcessId(Foreground, &OwnerProcess);
		if (OwnerProcess == ::GetCurrentProcessId())
		{
			return;
		}

		GPreviousForeground = Foreground;
	}

	/**
	 * Hands the foreground back, once, to the window that had it.
	 *
	 * CALLED FROM SLATE'S OWN ACTIVATION NOTIFICATION RATHER THAN FROM A TIMER, so the hand-back lands as soon as
	 * the editor knows it has been activated. A timer would leave a visible interval in which the editor holds the
	 * foreground, which is exactly the interruption the guard exists to prevent.
	 */
	void GiveForegroundBack()
	{
		if (GPreviousForeground == nullptr)
		{
			return;
		}

		HWND Target = static_cast<HWND>(GPreviousForeground);
		if (!::IsWindow(Target))
		{
			// The window that had it is gone - the user closed the game. Forget it rather than retrying forever.
			GPreviousForeground = nullptr;
			return;
		}

		::SetForegroundWindow(Target);
	}
}
#endif

void FTurboMCPFocusGuard::Install()
{
	if (GInstalled)
	{
		return;
	}

	// A HUMAN WHO LAUNCHES THE EDITOR WANTS TO TYPE IN IT. `FApp::IsUnattended()` is true only for an unattended
	// or automation run (`App.cpp:278`), which is the machine running a suite where nobody is looking at the window.
	if (!FApp::IsUnattended())
	{
		UE_LOG(LogTurboMCP, Log,
			TEXT("Focus guard NOT installed: this editor is attended, so it is allowed to take the foreground."));
		return;
	}

	// *** AND SLATE MUST ACTUALLY EXIST, WHICH IS A MEASURED CRASH RATHER THAN A PRECAUTION. ***
	//
	// `StartupModule` RUNS IN `UnrealEditor-Cmd.exe -run=<Commandlet>` TOO, AND A COMMANDLET HAS NO SLATE AT ALL.
	// Calling `FSlateApplication::Get()` there tripped the engine's own assertion:
	//
	//     LogWindows: Error: Assertion failed: CurrentApplication.IsValid()
	//
	// WHICH TOOK OUT **EVERY MATERIAL FIXTURE REBUILD** and with it TEN SUITES - `test-material-tidy`,
	// `test-material-label-promote`, `test-graph-dead`, `test-graph-knots`, `test-graph-tidy`,
	// `check-probe-graphs` and more - so a FOCUS FIX BROKE THE MATERIAL HALF OF THE TEST HARNESS. It is the same
	// trap this project already recorded twice from the other direction: **`-run=` IS NOT THE EDITOR**, and code
	// that assumes an interactive application is exactly the code that a commandlet destroys.
	//
	// `IsInitialized()` IS THE CORRECT GUARD, not a try/catch: an assertion is not an exception, so there would
	// have been nothing to catch.
	if (!FSlateApplication::IsInitialized())
	{
		UE_LOG(LogTurboMCP, Log,
			TEXT("Focus guard NOT installed: Slate is not initialised, so this is not an interactive editor (a ")
			TEXT("-run= commandlet has no Slate at all)."));
		return;
	}

#if PLATFORM_WINDOWS
	// THE CAPTURE HAPPENS ON THE FIRST ACTIVATION NOTIFICATION, AND IT HAS TO BE TRIED REPEATEDLY.
	//
	// MEASURED REASON: this class is installed from `StartupModule`, which runs AFTER the editor has already taken
	// the foreground - so a single capture here would record THE EDITOR'S OWN window as "what to give the foreground
	// back to", and the guard would spend the run handing the foreground to itself. Instead the guard KEEPS TRYING
	// until it sees a moment when the foreground belongs to ANOTHER PROCESS, and only then starts handing it back.
	// During startup the editor is activated more than once, so such a moment arrives.
	static bool GHandingBack = false;

	GActivationHandle = FSlateApplication::Get().OnApplicationActivationStateChanged().AddLambda(
		[](bool bIsActive)
		{
			if (!bIsActive)
			{
				return;
			}

			if (!GHandingBack)
			{
				// Still looking for the user's window. `RememberForegroundBeforeUs` refuses to record anything
				// owned by this process, so this cannot latch onto the editor itself.
				RememberForegroundBeforeUs();
				if (GPreviousForeground != nullptr)
				{
					GHandingBack = true;
					GiveForegroundBack();
				}
				return;
			}

			GiveForegroundBack();
		});

	GInstalled = true;

	UE_LOG(LogTurboMCP, Log,
		TEXT("Focus guard installed: unattended run, so the foreground is handed back whenever this editor takes it."));
#else
	UE_LOG(LogTurboMCP, Log, TEXT("Focus guard not installed: this platform has no implementation."));
#endif
}

void FTurboMCPFocusGuard::Remove()
{
	if (!GInstalled)
	{
		return;
	}

	if (GActivationHandle.IsValid())
	{
		FSlateApplication::Get().OnApplicationActivationStateChanged().Remove(GActivationHandle);
		GActivationHandle.Reset();
	}

	GPreviousForeground = nullptr;
	GInstalled = false;

	UE_LOG(LogTurboMCP, Log, TEXT("Focus guard removed."));
}

bool FTurboMCPFocusGuard::IsActive()
{
	return GInstalled;
}
