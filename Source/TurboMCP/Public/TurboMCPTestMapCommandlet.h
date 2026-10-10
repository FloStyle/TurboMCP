// Copyright (c) 2026 Florian ESCHALIER (FloStyle)
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once

#include "Commandlets/Commandlet.h"
#include "CoreMinimal.h"
#include "UObject/Object.h"

#include "TurboMCPTestMapCommandlet.generated.h"

/**
 * What the built map records about itself, saved INSIDE the map's package.
 *
 * WHY THE MAP CARRIES ITS OWN DESCRIPTION: a `.umap` is an opaque binary, so a reader - or a test - otherwise
 * has to assume which markers the placed actor raises. Storing them makes the fixture self-describing, and it
 * is the same reason the kit commandlet PRINTS its counts: a fixture whose contents are asserted rather than
 * recorded is a fixture that can drift.
 */
UCLASS()
class UTurboMCPTestMapSettings : public UObject
{
	GENERATED_BODY()

public:
	/** The exact string the placed actor's warning carries, so a suite does not have to guess it. */
	UPROPERTY()
	FString WarningMarker;

	/** The exact string the placed actor's error carries. */
	UPROPERTY()
	FString ErrorMarker;

	/** How many log actors the commandlet MEASURED in the persistent level after placing them. */
	UPROPERTY()
	int32 PlacedActorCount = 0;

	/**
	 * How many BLUEPRINT log actors it measured.
	 *
	 * SEPARATE FROM THE NATIVE COUNT ON PURPOSE, because the two prove different things. The native actor
	 * proves the warning and error count can be tested (Q-17); the BLUEPRINT actor is the only one that enters
	 * the Blueprint VM, so it is the only one that can make `FBlueprintCoreDelegates::OnScriptException` fire
	 * and therefore the only one that can close the trace hole (Q-18). A map with one and not the other would
	 * look fine and leave half the reason for building it unmet.
	 */
	UPROPERTY()
	int32 PlacedBlueprintActorCount = 0;

	/** The class it placed, by name, or a note explaining that it found none. */
	UPROPERTY()
	FString ActorClassName;

	/** The BLUEPRINT actor class it placed, by name, or a note explaining that it found none. */
	UPROPERTY()
	FString BlueprintActorClassName;
};

/**
 * Builds the one fixture that closes TWO open holes: a small level containing a placed actor whose code runs
 * during Play-In-Editor.
 *
 * WHY ONE FIXTURE CLOSES TWO HOLES
 * --------------------------------
 * `TestProject/Content` contained **no `.umap` at all**, which was measured rather than assumed. Play-In-Editor
 * therefore ran the engine's empty `/Temp/UEDPIE_0_Untitled_1` world, **nothing in it executed any code**, and
 * two capabilities were blocked by that single missing thing:
 *
 * | Hole | What could not be proved | Why the empty world blocked it |
 * |---|---|---|
 * | `Q-17` | that `R-PLAY-CHECK`'s warning and error COUNT is MEASURED rather than merely shaped | a forged `0` is indistinguishable from a true `0` when nothing runs to raise one |
 * | `Q-18` | that the `E-TRACE` subscriber to `FBlueprintCoreDelegates::OnScriptException` ever fires | the VM never reaches an `EX_Tracepoint` in an empty world |
 *
 * Both need the same thing: a level in which Blueprint or actor code actually executes. This commandlet makes
 * it.
 *
 * WHY A COMMANDLET RATHER THAN A HAND-MADE ASSET
 * ----------------------------------------------
 * The same reason the acceptance asset is built this way: a `.umap` committed as an opaque binary cannot be
 * reviewed, and this project's fixtures are meant to be LEGIBLE. A reader can see exactly which actor is
 * placed and why, and re-running reproduces the map. `06_ACCEPTANCE.md`'s rule about tests building their own
 * fixtures applies to the plugin's TOOLS, and a commandlet is a build step rather than one of the tools.
 *
 * WHAT IT BUILDS
 * --------------
 * `/Game/Tests/TurboMCPTestMap`, a level with one `ATurboMCPTestLogActor` placed at the origin, which raises
 * exactly one Warning and one Error when the level starts.
 */
UCLASS()
class UTurboMCPTestMapCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UTurboMCPTestMapCommandlet();

	virtual int32 Main(const FString& Params) override;
};
