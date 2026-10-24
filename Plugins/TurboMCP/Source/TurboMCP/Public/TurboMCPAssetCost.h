// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once

#include "CoreMinimal.h"

/** Recorded package disk sizes and direct dependency query results. */
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
	/** Failed queries prevent a complete dependency-count report. */
	TArray<FString> QueryErrors;

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

/** Resolves the asset and reads registry metadata without an explicit asset edit. */
class FTurboMCPAssetCostReader
{
public:
	static FString Read(const FString& AssetName, int32 Cap);
};
