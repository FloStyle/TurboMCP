// Copyright (c) 2026 Florian ESCHALIER (FloStyle)
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "Tools/TurboMCPToolsets.h"

#include "Containers/UnrealString.h"

#include "TurboMCPMaterialTidy.h"

/**
 * `P/E-MAT-TIDY`: lay out a material, one comment box per block.
 *
 * THE PAIR SHARES ONE PLAN, which is the rule this project states for every `P/E` pair and enforces by
 * shape: `Plan` decides, `RenderPreview` prints what would happen, and `Apply` writes it. A Preview that
 * accepted what its Edit twin refused would be worse than no Preview, so there is exactly ONE place
 * that can answer "no" and both tools reach it.
 *
 * THE ERROR PREFIX IS ADDED IN EXACTLY ONE PLACE, with the same `AsError` guard the material readers
 * use, because two layers each assuming the other leaves the prefix off is how a result ends up reading
 * `error: error: ...`.
 */
namespace
{
	/** `error: <what failed>`, with the prefix added exactly once. */
	FString AsError(const FString& InError)
	{
		if (InError.StartsWith(TEXT("error:")))
		{
			return InError;
		}
		return FString::Printf(TEXT("error: %s"), *InError);
	}
}

/**
 * Lay out a material, one comment box per block, and change nothing.
 *
 * Use this before `E-MAT-TIDY` to see what would move, which blocks would be boxed, and what the tool
 * refuses. It reads the material, plans the columns, and names every expression whose position would
 * change. It writes nothing and it does not claim the compiled result is unchanged - that claim is made
 * by the edit, which measures it.
 *
 * The layout follows the same section 6 rules as `P-GRAPH-TIDY`: columns left to right, links pointing
 * rightwards, each block a column, and the boxes sized from the positions rather than the reverse.
 *
 * GEOMETRY IS A FALLBACK AND THE ANSWER SAYS SO. A material expression stores no size at all - it owns
 * only its editor X and Y - so each expression is planned at a documented size and the positions are
 * relative. The plan under-reports the space it needs rather than inventing a measurement.
 *
 * Pass anything in `force` to plan the layout even when the material is already as this pass would
 * leave it.
 */
FString UTurboMCPPreviewToolset::PreviewTurboMCPMaterialTidy(const FString& Asset, const FString& Force)
{
	if (Asset.TrimStartAndEnd().IsEmpty())
	{
		return TEXT("error: no asset given. Pass the material to lay out, for example ")
			TEXT("/Game/Tests/TurboMCPMaterialFixture.M_TurboMCPFixture");
	}

	const bool bForce = !Force.TrimStartAndEnd().IsEmpty();

	FTurboMCPMaterialTidy::FPlan Plan;
	FString Error;
	if (!FTurboMCPMaterialTidy::Plan(Asset, bForce, Plan, Error))
	{
		return AsError(Error);
	}

	if (Plan.Refusals.Num() > 0)
	{
		// A REFUSAL IS THE ANSWER, AND IT IS PRINTED RATHER THAN SWALLOWED. `Plan` answering true means
		// it could reach the material; a refusal means something it was asked to do it will not, and a
		// caller who sees positions without the refusal would believe the whole layout is available.
		FString Out = FString::Printf(TEXT("refused: %d thing(s) this pass will not do"), Plan.Refusals.Num());
		for (const FString& Refusal : Plan.Refusals)
		{
			Out += FString::Printf(TEXT("\n  %s"), *Refusal);
		}
		return Out;
	}

	return FTurboMCPMaterialTidy::RenderPreview(Plan);
}

/**
 * Lay out a material, one comment box per block, AND PROVE IT CHANGED NOTHING BUT GEOMETRY.
 *
 * "Same compiled result" is the requirement and it is MEASURED rather than assumed. A layout moves
 * expressions and adds comment boxes, so it cannot legitimately change what the material computes; if
 * it does, the edit is ROLLED BACK and reported as a failure with the difference named - never as a
 * success with a note.
 *
 * Three statements are made afterwards, and the report says which is which: the SET OF EXPRESSIONS is
 * identical, which a layout cannot change and which is what the compiled result depends on; the engine
 * reports no NEW error; and the engine's own StateId is printed, reported rather than asserted because
 * it can change for reasons a layout does not control.
 *
 * The whole layout is ONE TRANSACTION, so it is a single undo and a rollback is exact.
 */
FString UTurboMCPEditToolset::EditTurboMCPMaterialTidy(const FString& Asset, const FString& Force)
{
	if (Asset.TrimStartAndEnd().IsEmpty())
	{
		return TEXT("error: no asset given. Pass the material to lay out, for example ")
			TEXT("/Game/Tests/TurboMCPMaterialFixture.M_TurboMCPFixture");
	}

	const bool bForce = !Force.TrimStartAndEnd().IsEmpty();

	FTurboMCPMaterialTidy::FPlan Plan;
	FString Error;
	if (!FTurboMCPMaterialTidy::Plan(Asset, bForce, Plan, Error))
	{
		return AsError(Error);
	}

	if (Plan.Refusals.Num() > 0)
	{
		FString Out = FString::Printf(TEXT("unchanged: %d refusal(s), so nothing was written"),
			Plan.Refusals.Num());
		for (const FString& Refusal : Plan.Refusals)
		{
			Out += FString::Printf(TEXT("\n  %s"), *Refusal);
		}
		return Out;
	}

	return FTurboMCPMaterialTidy::Apply(Plan);
}
