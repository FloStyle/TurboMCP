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
	 * Marks nodes live from graph entries using Blueprint execution and value rules.
     * Stock event-interface nodes (including Enhanced Input) are entries; merely
     * having an execution output does not make an ordinary node an entry.
	 *
	 * Exec-capable nodes are reached only through exec output wires. Pure nodes are
	 * reached backwards through the input links of an already-live node, because a
	 * value producer is live only when live code reads it. The same walk is shared
	 * by graph readers, dead-node previews and layout reports.
	 */
	void MarkReachable(const UEdGraph* InGraph, TSet<const UEdGraphNode*>& OutReachable);
}
