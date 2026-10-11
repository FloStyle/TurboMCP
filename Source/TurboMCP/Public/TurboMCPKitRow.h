// Copyright (c) 2026 Florian ESCHALIER (FloStyle)
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"

#include "TurboMCPKitRow.generated.h"

/**
 * THE ROW TYPE FOR `TA-14`, AND IT EXISTS BECAUSE A `UUserDefinedStruct` CANNOT BE BUILT HERE.
 *
 * `Spec/06_ACCEPTANCE.md` defines TA-14 as *"a data table asset with a project-owned row struct - 3 rows, 4 fields,
 * one field of a struct type"*, and the kit commandlet has recorded since TA-10 that IT CANNOT MAKE A
 * `UUserDefinedStruct`:
 *
 *     FStructureEditorUtils::AddVariable()  ->  FMemberVariableNameHelper::Generate()
 *                                           ->  CastChecked<UUserDefinedStructEditorData>   CRASHES
 *
 * because a `NewObject<UUserDefinedStruct>` DOES NOT CREATE ITS `UUserDefinedStructEditorData` - THE EDITOR FACTORY
 * INSTALLS IT, which is why the engine only ever makes one through `UUserDefinedStructFactory`. See
 * `TurboMCPKitCommandlet.cpp:1420-1447` for the full measurement.
 *
 * *** AND THE DISTINCTION THAT UNBLOCKS TA-14 IS THAT NOTHING REQUIRES THE ROW STRUCT TO BE USER-DEFINED. ***
 * `Spec/06_ACCEPTANCE.md` says *"project-owned"*, and A NATIVE `USTRUCT` IN A PROJECT MODULE IS PROJECT-OWNED - it is
 * reflected, it is a `UScriptStruct`, and it is exactly what a DataTable's `RowStruct` must be. **A `UUserDefinedStruct`
 * IS ONE WAY TO GET A PROJECT-OWNED STRUCT AND IT IS THE ONLY WAY THAT IS BLOCKED.**
 *
 * It is ONE struct with FOUR fields, ONE of them itself a struct type, which is what the acceptance row asks for.
 * The nested type is separate rather than inline so that the "one field of a struct type" clause is a real nested
 * struct rather than a second scalar wearing a different name.
 */

/** The nested type, so one field can genuinely be of a struct type. */
USTRUCT(BlueprintType)
struct FTurboMCPKitNested
{
	GENERATED_BODY()

	/** A scalar inside the nested struct, so a reader has something to print at depth two. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TurboMCP Kit")
	float Amount = 0.0f;

	/** A name inside the nested struct, so the nested value is not one number. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TurboMCP Kit")
	FName Tag;
};

/**
 * The row type of `TA-14`. FOUR FIELDS, the fourth being `FTurboMCPKitNested`, which is the "one field of a struct
 * type" the acceptance row requires.
 */
USTRUCT(BlueprintType)
struct FTurboMCPKitRow
{
	GENERATED_BODY()

	/** Field one: a scalar. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TurboMCP Kit")
	float Weight = 0.0f;

	/** Field two: a name, which is what a row is keyed by in a table that needs stable identity. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TurboMCP Kit")
	FName Label;

	/** Field three: an integer. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TurboMCP Kit")
	int32 Count = 0;

	/** Field four: A STRUCT TYPE, which is the clause the acceptance row calls out. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TurboMCP Kit")
	FTurboMCPKitNested Nested;
};
