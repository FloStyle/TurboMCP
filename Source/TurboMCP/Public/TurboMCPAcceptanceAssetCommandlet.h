// Copyright (c) 2026 Florian ESCHALIER (FloStyle)
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once

#include "Commandlets/Commandlet.h"
#include "CoreMinimal.h"

#include "TurboMCPAcceptanceAssetCommandlet.generated.h"

/**
 * Builds the acceptance asset that three capabilities need and no writer could produce.
 *
 * WHY THIS EXISTS
 * ---------------
 * Three phase 6 capabilities operate on structures that **nothing available could build**:
 *
 * | Capability | Structure it needs | Why the engine's `write_graph_dsl` cannot make it |
 * |---|---|---|
 * | `P/E-GRAPH-KNOTS` | a reroute knot | the DSL **refuses** `\|RerouteNode` outright |
 * | `P/E-GRAPH-DEAD` | a disconnected group | the DSL needs a top-level `event`/`fn` and wires every node into its exec chain |
 * | `P/E-LOCAL-VARS` | a pure node read **twice** | a DSL expression is a **tree**, so every sub-expression has exactly one consumer |
 *
 * Measured in `QUESTIONS.md` Q-9, Q-12 and Q-13. One asset carrying all three closes all three.
 *
 * WHY A COMMANDLET RATHER THAN A HAND-MADE ASSET
 * ----------------------------------------------
 * `06_ACCEPTANCE.md` says the structure the acceptance tests measure should be built by hand, because
 * an asset built by a tool proves nothing about a tool — and that rule is about **what the plugin's
 * own tools may do**, which is nothing here. A commandlet is not one of the plugin's tools; it is a
 * build step, like the `.cpp` next to it. Writing the asset from a commandlet rather than committing
 * an opaque binary means **the structure is legible in the repository**: a reader can see exactly
 * which nodes were made and how they were wired, and re-running it reproduces the asset.
 *
 * WHAT IT BUILDS
 * --------------
 * `/Game/Tests/TurboMCPAcceptance`, an Actor Blueprint with four function graphs:
 *
 * 1. `KnotProbe` — two nodes far apart with three knots between them, so the knot capability has
 *    something to find.
 * 2. `DeadProbe` — a group of nodes with **every link broken**, so nothing reaches it.
 * 3. `LocalVarProbe` — a pure node whose value is read by **two** consumers, one fed by a function
 *    input (SAFE) and one fed by a member variable (UNSAFE), so both classifications are exercised.
 * 4. `SaneProbe` — a small ordinary chain, so the layout passes have a graph that SHOULD NOT be
 *    disturbed, which is what a rollback test needs.
 */
UCLASS()
class UTurboMCPAcceptanceAssetCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UTurboMCPAcceptanceAssetCommandlet();

	virtual int32 Main(const FString& Params) override;
};
