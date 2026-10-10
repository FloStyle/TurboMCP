// Copyright (c) 2026 Florian ESCHALIER (FloStyle)
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "Tools/TurboMCPToolsets.h"

#include "Containers/UnrealString.h"

#include "TurboMCPMaterialLabelsPromote.h"

/**
 * `P/E-MAT-LABEL-PROMOTE`: give values names.
 *
 * THE PAIR SHARES ONE PLAN, which is the rule this project states for every `P/E` pair and enforces by
 * shape: `Plan` decides, `RenderPreview` prints what would happen, and `Apply` writes it. A Preview that
 * accepted what its Edit twin refused would be worse than no Preview, so there is exactly ONE place
 * that can answer "no" and both tools reach it.
 *
 * THE ERROR PREFIX IS ADDED IN EXACTLY ONE PLACE, with the same `AsError` guard the material readers
 * use, because two layers each assuming the other leaves the prefix off is how a result ends up reading
 * `error: error: ...`.
 *
 * THE REFUSAL PATH DIFFERS BETWEEN THE TWO HALVES ON PURPOSE, and `E-GRAPH-REPLACE` recorded why. A
 * material where nothing is worth promoting is NOT a failure - it is the correct answer for a material
 * that is already named, or one whose values are each read in a single place - so the Preview REPORTS
 * it and the Edit answers `unchanged`. Treating "nothing to do" as an error would make the tool look
 * broken on exactly the materials it should leave alone.
 */
namespace
{
	/** `error: <what failed>`, with the prefix added exactly once. */
	FString LabelPromoteAsError(const FString& InError)
	{
		if (InError.StartsWith(TEXT("error:")))
		{
			return InError;
		}
		return FString::Printf(TEXT("error: %s"), *InError);
	}
}

/**
 * List the values in a material worth a name, and change nothing.
 *
 * A VALUE IS WORTH A NAME WHEN IT IS READ IN SEVERAL PLACES. Distinct READERS are counted rather than
 * inputs, so one expression reading the same value into two of its own inputs counts once - the spec
 * asks for values used "in several boxes", and a single reader is one place however many of its pins
 * point at the value.
 *
 * EACH CANDIDATE CARRIES A PROPOSED NAME TAKEN FROM CONTEXT, which is the part of the specification a
 * bare list of values would not satisfy. The name is derived from the expression's own caption, which is
 * what the material editor draws, and it is printed with the places that would read it and with whether
 * the name already exists.
 *
 * It writes nothing and does not claim the compiled result is unchanged; the edit measures that.
 */
FString UTurboMCPPreviewToolset::PreviewTurboMCPMaterialLabelPromote(const FString& Asset)
{
	if (Asset.TrimStartAndEnd().IsEmpty())
	{
		return TEXT("error: no asset given. Pass the material to name values in, for example ")
			TEXT("/Game/Tests/TurboMCPMaterialFixture.M_TurboMCPFixture");
	}

	FTurboMCPMaterialLabelsPromote::FPlan Plan;
	FString Error;
	if (!FTurboMCPMaterialLabelsPromote::Plan(Asset, Plan, Error))
	{
		return LabelPromoteAsError(Error);
	}

	// A MATERIAL WITH NOTHING TO PROMOTE IS AN ANSWER RATHER THAN A REFUSAL, and it is the answer this
	// capability exists to give on a material that is already named. `RenderPreview` prints it with the
	// count of values examined, so "no label is worth adding" can never be confused with "nothing was
	// looked at".
	return FTurboMCPMaterialLabelsPromote::RenderPreview(Plan);
}

/**
 * Give a material values names: create the declaration and rewire the readers.
 *
 * A MATERIAL LABEL IS A NAMED REROUTE, so this creates a declaration that forwards to the value and
 * re-points every input that read that value directly at the declaration instead. It ADDS and REWIRES;
 * it never removes anything.
 *
 * "The material's compiled result must be the same" IS THE REQUIREMENT AND IT IS MEASURED. Three
 * statements are made afterwards and the answer says which is which: no pre-existing expression was
 * LOST, the expressions GAINED are exactly the declarations this pass planned, and the engine reports
 * no NEW error, so a material that already had one does not make this pass fail. The engine's own
 * StateId is printed, REPORTED rather than asserted because it can change for reasons a label promotion
 * does not control.
 *
 * The whole promotion is ONE TRANSACTION, so it is a single undo and a rollback is exact.
 */
FString UTurboMCPEditToolset::EditTurboMCPMaterialLabelPromote(const FString& Asset)
{
	if (Asset.TrimStartAndEnd().IsEmpty())
	{
		return TEXT("error: no asset given. Pass the material to name values in, for example ")
			TEXT("/Game/Tests/TurboMCPMaterialFixture.M_TurboMCPFixture");
	}

	FTurboMCPMaterialLabelsPromote::FPlan Plan;
	FString Error;
	if (!FTurboMCPMaterialLabelsPromote::Plan(Asset, Plan, Error))
	{
		return LabelPromoteAsError(Error);
	}

	return FTurboMCPMaterialLabelsPromote::Apply(Plan);
}
