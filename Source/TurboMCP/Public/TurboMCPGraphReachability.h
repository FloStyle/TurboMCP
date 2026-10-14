// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once

#include "Containers/Set.h"

class UEdGraph;
class UEdGraphNode;

/** The shared graph-liveness walk used by graph readers and dead-node previews. */
namespace TurboMCPGraphReachability
{
	/**
	 * Marks every node connected to an entry by any graph wire.
	 *
	 * Graph execution flows forward on exec wires, while a value producer is live
	 * because an entry-reachable consumer reads it. Walking every pin in both
	 * directions expresses both cases and keeps reader/disconnected reporting in
	 * lockstep with P-GRAPH-DEAD.
	 */
	void MarkReachable(const UEdGraph* InGraph, TSet<const UEdGraphNode*>& OutReachable);
}
