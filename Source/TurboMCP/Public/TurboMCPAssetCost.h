// Copyright (c) 2026 Florian ESCHALIER (FloStyle)
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once

#include "CoreMinimal.h"

/**
 * `R-ASSET-COST`: the size map and hard references of one asset.
 *
 * *** THE WHOLE POINT IS `D-52`: A SIZE IS REPORTED WITH ITS PROVENANCE. *** The decision rejects *"one total size"*
 * because *"the total names no cause and cannot be acted on. The check that matters is 'a heavy hard reference where
 * a soft one would do', and that fact does not exist inside a total."* So this report NEVER prints a single number
 * where a cause is available: it prints the asset's OWN size, then what it pulls in SPLIT BY KIND, then the largest
 * contributors EACH WITH WHY IT IS REFERENCED.
 *
 * *** AND `P1` GOVERNS EVERY FIGURE: A SIZE THAT CANNOT BE READ IS REPORTED AS UNKNOWN WITH THE REASON, NEVER AS
 * ZERO. *** The spec says it twice - *"A size that cannot be read is reported as unknown with the reason, never as
 * zero (`P1`, `D-05`)"* - and the sibling reader `R-CALL-COST` states the same rule in its own output as *"THIS IS
 * NOT ZERO COST"* and *"an absent measurement, not zero cost"*. **A total that silently counts an unreadable
 * package as zero is worse than no total, because it looks complete.**
 */
struct FTurboMCPAssetCost
{
	/** One referenced package, with the provenance `D-52` requires. */
	struct FContributor
	{
		FString Package;
		/** The registry's own recorded size for the package, or INDEX_NONE when it could not be read. */
		int64 Bytes = INDEX_NONE;
		/** Why the reference exists, from the query's own terms, not a guess. */
		FString Reason;
	};

	FString Asset;

	/** The asset's OWN serialised size, or INDEX_NONE with `OwnReason` set. */
	int64 OwnBytes = INDEX_NONE;
	/** Set whenever `OwnBytes` is INDEX_NONE. A reader must be told WHY rather than shown a zero. */
	FString OwnReason;

	/** What the asset pulls in, SPLIT BY KIND. That split IS `D-52`. */
	TArray<FContributor> Hard;
	TArray<FContributor> Soft;
	TArray<FContributor> EditorOnly;

	/** Entries whose size could not be read, with the reason. NEVER folded into a total as zero. */
	TArray<FContributor> Unknown;

	/** The largest contributors, capped, each carrying its size and its reason. */
	TArray<FContributor> Largest;

	/** How many dependency entries were examined, so every figure above has a denominator. */
	int32 Examined = 0;

	/** Renders the report. Provenance is preserved in the TEXT because that is what an agent reads. */
	FString Render(int32 Cap) const;
};

/** The tool entry point. A reader, so it lives in a toolset that is structurally unable to write. */
class FTurboMCPAssetCostReader
{
public:
	static FString Read(const FString& AssetName, int32 Cap);
};
