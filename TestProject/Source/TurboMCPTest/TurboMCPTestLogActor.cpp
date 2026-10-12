// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "TurboMCPTestLogActor.h"

#include "Engine/Engine.h"

/**
 * The markers, defined ONCE so the C++, the map's own settings and the suite cannot drift.
 *
 * `LogTemp` is used deliberately rather than a category of this module's own: it is the category the engine's
 * log already carries, so a suite can find these lines in the log file without the test project having to
 * register a category that a reader would then have to know about.
 */
namespace TurboMCPTestLog
{
	static const TCHAR* const WarningMarker = TEXT("TURBOMCP-TESTLOG-WARN");
	static const TCHAR* const ErrorMarker = TEXT("TURBOMCP-TESTLOG-ERROR");
}

ATurboMCPTestLogActor::ATurboMCPTestLogActor()
{
	PrimaryActorTick.bCanEverTick = true;

	// TICKING STARTS OFF AND IS TURNED ON ONLY WHEN ASKED FOR, so the default fixture cannot add lines a
	// count does not expect. A report whose count depended on frame rate would be unpredictable, and an
	// unpredictable fixture proves nothing about a counter.
	PrimaryActorTick.bStartWithTickEnabled = false;

	// THE ACTOR MUST SURVIVE THE WHOLE WINDOW, AND THE ENGINE'S EMPTY MAP HAS NO FLOOR. Collision is disabled
	// so nothing pushes it, and the repeated case is bounded by the play session rather than by the actor
	// falling out of the world - which would end the logging part-way through a window for a reason no check
	// could see.
	SetActorEnableCollision(false);
}

void ATurboMCPTestLogActor::BeginPlay()
{
	Super::BeginPlay();

	// THE WARNING FIRST, THEN THE ERROR, so the two are distinguishable by order as well as by marker, and
	// exactly one of each so a caller knows what a correct window contains.
	UE_LOG(LogTemp, Warning, TEXT("%s beginplay warning"), TurboMCPTestLog::WarningMarker);
	UE_LOG(LogTemp, Error, TEXT("%s beginplay error"), TurboMCPTestLog::ErrorMarker);

	// TICKING IS ENABLED HERE RATHER THAN IN THE CONSTRUCTOR, so the OBSERVED start of the window and the
	// start of the repeated logging are the same moment.
	SetActorTickEnabled(bRepeatEverySecond);
	SecondsSinceLastPair = 0.0;
	RepeatedPairsLogged = 0;
}

void ATurboMCPTestLogActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!bRepeatEverySecond)
	{
		return;
	}

	SecondsSinceLastPair += static_cast<double>(DeltaSeconds);
	if (SecondsSinceLastPair < 1.0)
	{
		return;
	}
	SecondsSinceLastPair -= 1.0;
	++RepeatedPairsLogged;

	UE_LOG(LogTemp, Warning, TEXT("%s tick warning %d"), TurboMCPTestLog::WarningMarker, RepeatedPairsLogged);
	UE_LOG(LogTemp, Error, TEXT("%s tick error %d"), TurboMCPTestLog::ErrorMarker, RepeatedPairsLogged);
}
