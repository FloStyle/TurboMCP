// Copyright FloStyle. All Rights Reserved.

#pragma once

#include "Containers/UnrealString.h"
#include "CoreTypes.h"

/**
 * The shared token estimate, from `Spec/04_FORMATS.md` section 5.
 *
 * WHY NOT "CHARACTERS DIVIDED BY FOUR"
 * ------------------------------------
 * The spec says so outright: outputs are S-expressions, not prose, and most punctuation — brackets,
 * colons, pipes, dots — costs about a token per character, so the naive rule UNDERCOUNTS BY ABOUT
 * HALF. A budget built on it would let through twice what it promised.
 *
 * The rule the spec gives, implemented exactly:
 *
 *  - split the text into runs of letters and digits, and single other visible characters;
 *  - each unbroken run of letters and digits counts `ceil(length / 4)`;
 *  - every other VISIBLE character counts 1;
 *  - whitespace counts 0.
 *
 * It is an estimate, and every tool description that mentions tokens says so.
 */
int32 TurboMCPTokenEstimate(const FString& InText);
