// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "Tools/TurboMCPToolsets.h"

#include "Containers/UnrealString.h"

#include "TurboMCPWidgetTreeSync.h"

// Read selected widget-tree data through the shared inspector; no writer is invoked here.
FString UTurboMCPReadToolset::ReadTurboMCPWidgetTree(const FString& Asset)
{
	const FString Text = FTurboMCPWidgetTreeSync::Read(Asset);

	// `Read` already prefixes its own refusals, so this only ensures exactly one prefix survives. The guard
	// is on the RESULT rather than on a flag, because a flag can disagree with the string it describes.
	return Text.StartsWith(TEXT("error:")) || Text.StartsWith(TEXT("read:"))
		? Text
		: FString::Printf(TEXT("read: %s"), *Text);
}
