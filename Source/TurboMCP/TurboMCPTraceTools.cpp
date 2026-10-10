// Copyright (c) 2026 Florian ESCHALIER (FloStyle)
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "Tools/TurboMCPToolsets.h"

#include "TurboMCPTrace.h"

/**
 * `E-TRACE`, the write half of the trace-point pair: mark a node by ID, clear one or all.
 *
 * NO ASSET IS CHANGED AND NO GRAPH IS TOUCHED, WHICH IS A MEASURED FACT RATHER THAN A CHOICE. `EX_Tracepoint` is
 * already emitted into the bytecode of every normally-compiled Blueprint
 * (Editor/KismetCompiler/Private/KismetCompilerVMBackend.cpp:1969-1971, suppressed only by
 * `Kismet.bSuppressInstrumentation` at `:1961`, which this project does not set), and the VM raises it through
 * the engine's script exception delegate
 * (Runtime/CoreUObject/Private/UObject/ScriptCore.cpp:2529-2537). So arming is an entry in an in-memory
 * registry and nothing more.
 *
 * THAT REMOVES THE RISKIEST PART THIS CAPABILITY WOULD OTHERWISE HAVE CARRIED. An implementation that inserted
 * a call node before the marked node would have to create a node outside the editor and would owe
 * `AllocateDefaultPins` - the call whose omission has already crashed this project once and silently corrupted
 * a graph once. This tool creates no node, so that trap is not merely avoided by care: it is unreachable.
 *
 * WHY IT IS IN THE EDIT TOOLSET EVEN THOUGH IT WRITES NO FILE. It changes state a caller must be able to
 * undo, and `Spec/03_TOOLS.md` puts every mutating action on the Edit side so a Read toolset can be handed to
 * an agent that must not alter anything. Filing it as a Read tool because it happens not to touch the disk
 * would put a state change in the one toolset this project promises cannot make one.
 */
FString UTurboMCPEditToolset::EditTurboMCPTrace(const FString& Action, const FString& NodeId)
{
	TurboMCPTrace::FPlan Plan;
	FString Error;
	if (!TurboMCPTrace::Plan(Action, NodeId, Plan, Error))
	{
		return FString::Printf(TEXT("error: %s"), *Error);
	}

	// FORCED REBUILD NOTE: a break test left a stale object in the intermediate directory and the link did
	// not re-run, so the loaded binary was the broken one while the source was already correct. Recorded
	// because the source being right and the binary being right are different claims, and only a behaviour
	// test tells them apart.
	return TurboMCPTrace::Apply(Plan);
}
