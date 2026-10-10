// Copyright (c) 2026 Florian ESCHALIER (FloStyle)
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "Tools/TurboMCPToolsets.h"

#include "Containers/UnrealString.h"

#include "TurboMCPMaterialBlocks.h"

/**
 * `P/E-MAT-BLOCKS`: make labels follow blocks.
 *
 * THE PAIR SHARES ONE PLAN, which is the rule this project states for every `P/E` pair and enforces by
 * shape: `Plan` decides, `RenderPreview` prints what would happen, and `Apply` writes it. A Preview that
 * accepted what its Edit twin refused would be worse than no Preview, so there is exactly ONE place that
 * can answer "no" and both tools reach it.
 *
 * THE ERROR PREFIX IS ADDED IN EXACTLY ONE PLACE, with the same guard the material readers use, because
 * two layers each assuming the other leaves the prefix off is how a result ends up reading
 * `error: error: ...`.
 *
 * THE REFUSAL PATH DIFFERS BETWEEN THE TWO HALVES ON PURPOSE, and `E-GRAPH-REPLACE` recorded why. A
 * material that already follows D-17 is NOT a failure - it is the correct answer for a material whose
 * labels already follow its blocks - so the Preview REPORTS it and the Edit answers `unchanged`.
 * Treating "nothing to do" as an error would make the tool look broken on exactly the materials it
 * should leave alone.
 *
 * AND A MATERIAL WITH NO COMMENT BOX IS A THIRD ANSWER RATHER THAN A SILENT SUCCESS. D-17 is about the
 * boundary between blocks, and a material with one block has no boundary: the refusal says so and names
 * `P/E-MAT-TIDY` as the tool that would create the blocks first.
 */
namespace
{
	/** `error: <what failed>`, with the prefix added exactly once. */
	FString BlocksAsError(const FString& InError)
	{
		if (InError.StartsWith(TEXT("error:")))
		{
			return InError;
		}
		return FString::Printf(TEXT("error: %s"), *InError);
	}
}

/**
 * List what would make a material's labels follow its blocks, and change nothing.
 *
 * THE THREE LISTS ARE THE THREE WAYS D-17 CAN BE VIOLATED, and each is derived from the same geometric
 * block membership the reader uses - not from a second opinion about which box holds what:
 *
 *   - a value read from MORE THAN ONE BLOCK with no label, where D-17 says a value leaving its block
 *     travels as a named reroute;
 *   - a label read through TWO OR MORE usage nodes in ONE block, where D-17 says a block reads each label
 *     through a single usage node;
 *   - a PLAIN REROUTE whose source and readers are all in ONE block, where D-17 says plain reroute dots
 *     are for routing long wires and never stand for a value.
 *
 * Every item is printed with the keys the edit would touch, because "changes exactly the items in the
 * Preview list" is only checkable if the list is in the text.
 *
 * It writes nothing and does not claim the compiled result is unchanged; the edit measures that.
 */
FString UTurboMCPPreviewToolset::PreviewTurboMCPMaterialBlocks(const FString& Asset)
{
	if (Asset.TrimStartAndEnd().IsEmpty())
	{
		return TEXT("error: no asset given. Pass the material to arrange into blocks, for example ")
			TEXT("/Game/Tests/TurboMCPMaterialFixture.M_TurboMCPFixture");
	}

	FTurboMCPMaterialBlocks::FPlan Plan;
	FString Error;
	if (!FTurboMCPMaterialBlocks::Plan(Asset, Plan, Error))
	{
		return BlocksAsError(Error);
	}

	// A MATERIAL THAT ALREADY FOLLOWS D-17 IS AN ANSWER RATHER THAN A REFUSAL, and it is the answer this
	// capability exists to give on a material that is already shaped correctly. `RenderPreview` prints it
	// with the block count and the number of expressions in no block, so "nothing to change" can never be
	// confused with "nothing was looked at".
	return FTurboMCPMaterialBlocks::RenderPreview(Plan);
}

/**
 * Make a material's labels follow its blocks: add the missing labels, merge the redundant usage nodes,
 * and remove the in-block reroutes.
 *
 * *** THIS IS THE ONE MATERIAL WRITER THAT REMOVES, SO IT STATES WHAT IT REMOVES AND DELETES BY IDENTITY.
 * `05_DECISIONS.md` D-45 requires exactly that: "a writer that removes anything lists each removal by key
 * with its class and title, never a count... a count says how much is gone and a list says what." ***
 *
 * "Same compiled result check" IS THE REQUIREMENT AND IT IS MEASURED. The assertion is asymmetric on
 * purpose, because this pass legitimately adds and legitimately removes: NO PRE-EXISTING EXPRESSION WAS
 * LOST EXCEPT THE ONES ON THE PLAN'S OWN REMOVAL LIST, the expressions GAINED are exactly the ones it
 * planned, and the engine reports no NEW error - so a material that already had a compiler error does
 * not make this pass fail. A node that vanished without being listed is a failure even though the
 * material still compiles, WHICH IS WHAT "changes exactly the items in the Preview list" MEANS.
 *
 * EVERY NODE IS RE-RESOLVED BY KEY AT APPLY TIME rather than trusted from the plan, which is the other
 * half of D-45 and is forced by the two-call shape: a plan is made and applied separately, so the
 * material can change in between. An input that no longer points where the plan said is a REFUSAL rather
 * than a rewire, because a stale index would retarget the wrong pin and produce a wrong material that
 * still compiles - the one outcome the compiled-result check cannot tell from a correct edit.
 *
 * The whole edit is ONE TRANSACTION, so it is a single undo and a rollback is exact.
 */
FString UTurboMCPEditToolset::EditTurboMCPMaterialBlocks(const FString& Asset)
{
	if (Asset.TrimStartAndEnd().IsEmpty())
	{
		return TEXT("error: no asset given. Pass the material to arrange into blocks, for example ")
			TEXT("/Game/Tests/TurboMCPMaterialFixture.M_TurboMCPFixture");
	}

	FTurboMCPMaterialBlocks::FPlan Plan;
	FString Error;
	if (!FTurboMCPMaterialBlocks::Plan(Asset, Plan, Error))
	{
		return BlocksAsError(Error);
	}

	return FTurboMCPMaterialBlocks::Apply(Plan);
}
