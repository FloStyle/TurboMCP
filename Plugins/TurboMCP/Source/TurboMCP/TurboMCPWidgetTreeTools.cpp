// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "Tools/TurboMCPToolsets.h"

#include "Containers/UnrealString.h"

#include "TurboMCPWidgetTreeSync.h"

/**
 * `R-WIDGET-TREE`: a Widget Blueprint's tree, in slot order, as the dialect text.
 *
 * THE ERROR PREFIX IS ADDED IN EXACTLY ONE PLACE, with the same guard the material and block readers use,
 * because two layers each assuming the other leaves the prefix off is how a result ends up reading
 * `error: error: ...`.
 *
 * THIS IS A READER AND ITS TOOLSET IS THE READ TOOLSET, which is the structural reason it cannot write:
 * the class it is declared on has no writer helpers, so this function could not mutate a tree even if it
 * tried. The writers live in `P/E-WIDGET-TREE` and are separate entry points on purpose.
 */
FString UTurboMCPReadToolset::ReadTurboMCPWidgetTree(const FString& Asset)
{
	const FString Text = FTurboMCPWidgetTreeSync::Read(Asset);

	// `Read` already prefixes its own refusals, so this only ensures exactly one prefix survives. The guard
	// is on the RESULT rather than on a flag, because a flag can disagree with the string it describes.
	return Text.StartsWith(TEXT("error:")) || Text.StartsWith(TEXT("read:"))
		? Text
		: FString::Printf(TEXT("read: %s"), *Text);
}
