// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "TurboMCPMaterialTidy.h"

#include "TurboMCPRollback.h"
#include "Logging.h"
#include "Public/TurboMCPMaterial.h"
#include "TurboMCPAssetResolver.h"

#include "Containers/UnrealString.h"
#include "Editor/UnrealEd/Public/Editor.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionComment.h"
#include "Materials/MaterialFunctionInterface.h"
#include "MaterialEditingLibrary.h"
#include "Misc/Guid.h"
#include "ScopedTransaction.h"
#include "UObject/Package.h"

namespace
{
	/**
	 * The size this pass uses for an expression, and whether it is a substitution.
	 *
	 * A MATERIAL EXPRESSION STORES NO SIZE AT ALL. `MaterialExpressionEditorX` and
	 * `MaterialExpressionEditorY` (`Runtime/Engine/Public/Materials/MaterialExpression.h:159,162`) are the
	 * only geometry an expression owns, so there is nothing to measure and the fallback is ALWAYS in use.
	 * It is a documented substitution and every report that carries a number derived from it says so.
	 */
	void SizeForLayout(const UMaterialExpression* InExpression,
		double& OutWidth, double& OutHeight, bool& bOutFallback)
	{
		OutWidth = FTurboMCPMaterialTidy::FallbackWidth;
		OutHeight = FTurboMCPMaterialTidy::FallbackHeight;
		bOutFallback = true;

		// A COMMENT HAS A REAL SIZE, and using the fallback for it would be wrong in a way that shows:
		// boxes would be planned around a rectangle the engine does not agree with. So a comment's own
		// SizeX and SizeY are read where they exist, and THIS IS THE ONE PLACE the fallback is not used.
		if (const UMaterialExpressionComment* AsComment = Cast<UMaterialExpressionComment>(InExpression))
		{
			if (AsComment->SizeX > 0 && AsComment->SizeY > 0)
			{
				OutWidth = static_cast<double>(AsComment->SizeX);
				OutHeight = static_cast<double>(AsComment->SizeY);
				bOutFallback = false;
			}
		}
	}

	/** True when the string is usable as a comment box's text. */
	bool HasText(const FString& InText)
	{
		return !InText.TrimStartAndEnd().IsEmpty();
	}
}

bool FTurboMCPMaterialTidy::Plan(const FString& InAsset, bool bInForce,
	FPlan& OutPlan, FString& OutError)
{
	// THE READER DOES THE RESOLUTION, DELIBERATELY. `FTurboMCPMaterial::Plan` already refuses a
	// non-material asset, names the alternative where it can, and gathers the expressions and the
	// comments in a stable order. A second resolver here would be the second-dialect mistake in a new
	// place, and it would drift from the four readers the moment either changed.
	FTurboMCPMaterial::FPlan Read;
	if (!FTurboMCPMaterial::Plan(InAsset, FTurboMCPMaterial::ERead::Blocks, FString(),
		/*bInIncludeInstanceOverrides=*/false, Read, OutError))
	{
		return false;
	}

	OutPlan.Asset = Read.Asset;
	OutPlan.AssetName = Read.AssetName;
	OutPlan.Material = Read.Material;
	OutPlan.Function = Read.Function;
	OutPlan.ParentPath = Read.ParentPath;
	OutPlan.Comments = Read.Comments;

	// A MATERIAL INSTANCE HAS NO GRAPH TO LAY OUT, AND THAT IS A REFUSAL RATHER THAN AN EMPTY ANSWER.
	// Its expressions live in the parent, so laying it out would move the parent's nodes - an edit the
	// caller did not ask for. `RenderBlocks` already says a material instance "has no blocks"; here that
	// becomes a refusal, because a writer must not proceed on something a reader merely reports.
	if (OutPlan.Material == nullptr && OutPlan.Function == nullptr)
	{
		OutError = OutPlan.ParentPath.IsEmpty()
			? FString(TEXT("this asset has no material graph to lay out"))
			: FString::Printf(TEXT("this is a Material Instance, whose graph lives in its parent %s. ")
				TEXT("Lay the parent out instead, because moving a parent's expressions through an ")
				TEXT("instance would edit an asset the caller did not name."), *OutPlan.ParentPath);
		return false;
	}

	// THE COMPILED-RESULT BASELINE IS TAKEN BEFORE ANYTHING IS PLANNED. Measuring afterwards and calling
	// the result "before" is how a pass reports success on a material it already changed.
	if (OutPlan.Material != nullptr)
	{
		OutPlan.StateIdBefore = OutPlan.Material->StateId;
		OutPlan.CompileErrorsBefore = UMaterialEditingLibrary::RecompileMaterial(OutPlan.Material);
	}

	// EVERY EXPRESSION BECOMES A NODE, in the reader's stable order.
	TMap<const UMaterialExpression*, int32> IndexByExpression;
	for (UMaterialExpression* Expression : Read.Expressions)
	{
		if (Expression == nullptr)
		{
			continue;
		}

		FNode Node;
		Node.Expression = Expression;
		Node.Key = FTurboMCPMaterial::KeyOf(Expression);
		Node.X = Expression->MaterialExpressionEditorX;
		Node.Y = Expression->MaterialExpressionEditorY;
		SizeForLayout(Expression, Node.Width, Node.Height, Node.bSizeWasFallback);
		// THE SIGNAL IS READ, NOT OVERWRITTEN. `SizeForLayout` decides whether it had to substitute,
		// and a comment carries a REAL size where every other expression does not. Forcing the flag
		// true here would have made the report claim a fallback was in use even on a material whose
		// geometry was genuinely measured, which is the opposite of the honesty this flag exists for.
		Node.PlannedX = static_cast<double>(Node.X);
		Node.PlannedY = static_cast<double>(Node.Y);

		OutPlan.KeysBefore.Add(Node.Key);
		OutPlan.bUsedFallbackGeometry = OutPlan.bUsedFallbackGeometry || Node.bSizeWasFallback;
		IndexByExpression.Add(Expression, OutPlan.Nodes.Num());
		OutPlan.Nodes.Add(MoveTemp(Node));
	}

	OutPlan.ExistingBoxes = OutPlan.Comments.Num();

	// ---------------------------------------------------------------------------------------------
	// COLUMNS: THE DEPTH OF EACH EXPRESSION FROM THE OUTPUT ROOTS.
	//
	// A COLUMN IS A BLOCK, and that is what makes "one comment box per block" well defined on a material
	// that has no boxes: the block structure comes from the dependency graph rather than from the
	// editor's geometry, so this pass can CREATE it as well as respect it.
	//
	// DEPTH IS TAKEN FROM THE CONSUMER, NOT FROM THE PRODUCER. An expression's column is one more than
	// the greatest column of anything that reads it, so an output property's own source is column 0 and
	// its inputs are column 1 and so on - which places every link LEFT TO RIGHT, the direction section 6
	// requires. Taking it from the producers instead would put the outputs on the right.
	// ---------------------------------------------------------------------------------------------
	{
		// Seed: the source of every connected output is column 0.
		TArray<FString> Outputs = FTurboMCPMaterial::ConnectedOutputs(Read);
		for (const FString& Property : Outputs)
		{
			if (UMaterialExpression* Source = FTurboMCPMaterial::OutputSource(Read, Property))
			{
				if (const int32* Index = IndexByExpression.Find(Source))
				{
					OutPlan.Nodes[*Index].Column = 0;
				}
			}
		}

		// Relax outward: an input sits at least one column right of everything that reads it. This is a
		// bounded relaxation rather than a recursion, so a malformed graph with a cycle terminates
		// instead of overflowing the stack - the same guard `RenderExpression` carries for the readers.
		const int32 MaxPasses = OutPlan.Nodes.Num() + 1;
		for (int32 Pass = 0; Pass < MaxPasses; ++Pass)
		{
			bool bChanged = false;
			for (int32 Index = 0; Index < OutPlan.Nodes.Num(); ++Index)
			{
				const UMaterialExpression* Expression = OutPlan.Nodes[Index].Expression;
				for (int32 InputIndex = 0; InputIndex < Expression->CountInputs(); ++InputIndex)
				{
					const FExpressionInput* Input = Expression->GetInput(InputIndex);
					if (Input == nullptr || Input->Expression == nullptr)
					{
						continue;
					}
					const int32* InputNodeIndex = IndexByExpression.Find(Input->Expression);
					if (InputNodeIndex == nullptr)
					{
						continue;
					}
					const int32 Wanted = OutPlan.Nodes[Index].Column + 1;
					if (OutPlan.Nodes[*InputNodeIndex].Column < Wanted)
					{
						OutPlan.Nodes[*InputNodeIndex].Column = Wanted;
						bChanged = true;
					}
				}
			}
			if (!bChanged)
			{
				break;
			}
		}

		// An expression nothing reaches gets its own column to the right of everything laid out, and it
		// is MARKED. A layout that quietly folded it into a neighbouring column would be claiming it
		// belongs somewhere it does not, and a reader would then believe the material is tidier than it
		// is.
		int32 MaxColumn = 0;
		for (const FNode& Node : OutPlan.Nodes)
		{
			MaxColumn = FMath::Max(MaxColumn, Node.Column);
		}
		for (FNode& Node : OutPlan.Nodes)
		{
			if (Node.Column == 0)
			{
				// Column 0 is the output sources. Anything else still at 0 that nothing reads is unreached.
				bool bReadByAnything = false;
				for (const FNode& Other : OutPlan.Nodes)
				{
					if (Other.Expression == Node.Expression)
					{
						continue;
					}
					for (int32 InputIndex = 0; InputIndex < Other.Expression->CountInputs(); ++InputIndex)
					{
						const FExpressionInput* Input = Other.Expression->GetInput(InputIndex);
						if (Input != nullptr && Input->Expression == Node.Expression)
						{
							bReadByAnything = true;
							break;
						}
					}
					if (bReadByAnything)
					{
						break;
					}
				}
				if (!bReadByAnything)
				{
					Node.bUnreached = true;
					Node.Column = MaxColumn + 1;
				}
			}
		}
	}

	// ---------------------------------------------------------------------------------------------
	// POSITIONS: COLUMN BY COLUMN, LEFT TO RIGHT, EACH COLUMN STARTING AT THE SAME TOP.
	//
	// A MATERIAL COLUMN IS TALL, so the top is shared rather than centred. Centring would make two
	// columns of very different heights stagger against each other, and section 6's own rule is that a
	// reader follows a row.
	// ---------------------------------------------------------------------------------------------
	{
		TMap<int32, TArray<int32>> ByColumn;
		for (int32 Index = 0; Index < OutPlan.Nodes.Num(); ++Index)
		{
			ByColumn.FindOrAdd(OutPlan.Nodes[Index].Column).Add(Index);
		}

		TArray<int32> Columns;
		ByColumn.GetKeys(Columns);
		Columns.Sort();

		double ColumnLeft = 0.0;
		for (const int32 Column : Columns)
		{
			TArray<int32>& Members = ByColumn[Column];
			// SORTED BY THE ENGINE'S OWN Y, so a material that was already partly ordered keeps that
			// order. Re-sorting by class or by key would rearrange a hand-laid graph for no reason and
			// make the pass's diff larger than the change it made.
			Members.Sort([&OutPlan](const int32 A, const int32 B)
			{
				if (OutPlan.Nodes[A].Y != OutPlan.Nodes[B].Y)
				{
					return OutPlan.Nodes[A].Y < OutPlan.Nodes[B].Y;
				}
				return OutPlan.Nodes[A].Key < OutPlan.Nodes[B].Key;
			});

			double Top = 0.0;
			double ColumnWidth = 0.0;
			for (const int32 Index : Members)
			{
				FNode& Node = OutPlan.Nodes[Index];
				Node.PlannedX = ColumnLeft;
				Node.PlannedY = Top;
				Top += Node.Height + VerticalNodeGap;
				ColumnWidth = FMath::Max(ColumnWidth, Node.Width);
			}

			FBlock Block;
			Block.Column = Column;
			Block.MemberIndices = Members;
			Block.Left = ColumnLeft - FitPadding;
			Block.Top = 0.0 - FitPadding - TitleBarHeight;
			Block.Right = ColumnLeft + ColumnWidth + FitPadding;
			Block.Bottom = (Top - VerticalNodeGap) + FitPadding;

			// THE BOX NAMES WHAT IT HOLDS. A box labelled only "column 3" tells a reader nothing, and the
			// whole value of a block box is that it says what the block of logic is.
			{
				TArray<FString> Captions;
				for (const int32 Index : Members)
				{
					const FString Caption = OutPlan.Nodes[Index].Expression != nullptr
						? OutPlan.Nodes[Index].Expression->GetClass()->GetName()
						: FString(TEXT("(unknown)"));
					Captions.AddUnique(Caption);
				}
				Captions.Sort();
				Block.Text = Captions.Num() > 0
					? FString::Printf(TEXT("block %d: %s"), Column, *FString::Join(Captions, TEXT(", ")))
					: FString::Printf(TEXT("block %d"), Column);
			}

			OutPlan.Blocks.Add(MoveTemp(Block));
			ColumnLeft = OutPlan.Blocks.Last().Right + BoxGap;
		}
	}

	// ALREADY TIDY IS A REAL ANSWER AND IT IS MEASURED RATHER THAN GUESSED: every expression is where
	// this pass would put it. Saying `unchanged` for a material it would not move is what makes the
	// pass safe to run twice.
	{
		bool bAllInPlace = OutPlan.Blocks.Num() > 0 && OutPlan.Nodes.Num() > 0;
		for (const FNode& Node : OutPlan.Nodes)
		{
			if (FMath::Abs(Node.PlannedX - static_cast<double>(Node.X)) > 1.0
				|| FMath::Abs(Node.PlannedY - static_cast<double>(Node.Y)) > 1.0)
			{
				bAllInPlace = false;
				break;
			}
		}
		OutPlan.bAlreadyTidy = bAllInPlace;
	}

	if (OutPlan.bAlreadyTidy && !bInForce)
	{
		// NOT AN ERROR. `P1` requires an answer and this is the answer: the material is as this pass
		// would leave it, and rewriting the same numbers would be a change with no content.
		return true;
	}

	return true;
}

FString FTurboMCPMaterialTidy::RenderPreview(const FPlan& InPlan)
{
	FString Out = FString::Printf(TEXT("preview: tidy %s : %d expression(s) in %d block(s)"),
		*InPlan.Asset, InPlan.Nodes.Num(), InPlan.Blocks.Num());

	if (InPlan.bAlreadyTidy)
	{
		// SAID IN THE FIRST TWO LINES, because a caller reading only the head must not conclude an edit
		// is coming. This mirrors `FTurboMCPGraphTidy`'s own `unchanged` answer.
		Out += TEXT("\nunchanged: every expression is already where this pass would put it.");
		return Out;
	}

	if (InPlan.bUsedFallbackGeometry)
	{
		// STATED EVERY TIME, because the numbers below are derived from it. MEASURED: EVERY expression
		// in the fixture needed the substitution - a material expression stores no size at all - so
		// this note is not a corner case, it is the normal answer. Under-reporting is acceptable here;
		// letting a reader believe these sizes were measured is not.
		int32 Substituted = 0;
		for (const FNode& Node : InPlan.Nodes)
		{
			if (Node.bSizeWasFallback)
			{
				++Substituted;
			}
		}
		Out += FString::Printf(
			TEXT("\n; GEOMETRY IS A FALLBACK: %d of %d expression(s) store no size, so they are planned as ")
			TEXT("%.0f x %.0f. The positions are therefore RELATIVE, and the plan under-reports the space ")
			TEXT("it needs rather than inventing a measurement."),
			Substituted, InPlan.Nodes.Num(), FallbackWidth, FallbackHeight);
	}

	Out += FString::Printf(TEXT("\n%d existing box(es) will be used rather than added to"),
		InPlan.ExistingBoxes);

	for (const FBlock& Block : InPlan.Blocks)
	{
		Out += FString::Printf(TEXT("\n  block %d: %d expression(s) at (%.0f,%.0f) box [%.0f,%.0f .. %.0f,%.0f]"),
			Block.Column, Block.MemberIndices.Num(), Block.Left + FitPadding, Block.Top + FitPadding + TitleBarHeight,
			Block.Left, Block.Top, Block.Right, Block.Bottom);
	}

	for (const FNode& Node : InPlan.Nodes)
	{
		if (FMath::Abs(Node.PlannedX - static_cast<double>(Node.X)) <= 1.0
			&& FMath::Abs(Node.PlannedY - static_cast<double>(Node.Y)) <= 1.0)
		{
			continue;
		}
		Out += FString::Printf(TEXT("\n  %s (%d,%d) -> (%.0f,%.0f)%s"),
			*Node.Key, Node.X, Node.Y, Node.PlannedX, Node.PlannedY,
			Node.bUnreached ? TEXT("  it reaches no output, so it is placed to the right of the rest") : TEXT(""));
	}

	if (InPlan.Refusals.Num() > 0)
	{
		Out += FString::Printf(TEXT("\nrefused (%d):"), InPlan.Refusals.Num());
		for (const FString& Refusal : InPlan.Refusals)
		{
			Out += FString::Printf(TEXT("\n  %s"), *Refusal);
		}
	}

	Out += TEXT("\n; the preview changes NOTHING, and it does NOT claim the compiled result is unchanged - ")
		TEXT("that claim is made by the edit, which measures it.");
	return Out;
}

FString FTurboMCPMaterialTidy::Apply(const FPlan& InPlan)
{
	if (InPlan.bAlreadyTidy)
	{
		return FString::Printf(TEXT("unchanged: %s is already laid out the way this pass would lay it out"),
			*InPlan.Asset);
	}

	if (InPlan.Material == nullptr)
	{
		return FString::Printf(TEXT("error: %s has no material to write into"), *InPlan.Asset);
	}

	// ONE TRANSACTION, so the whole layout is a single undo step and a rollback is exact.
	FScopedTransaction Transaction(NSLOCTEXT("TurboMCP", "MaterialTidy", "TurboMCP: tidy a material"));

	int32 Moved = 0;
	int32 BoxesWritten = 0;

	UMaterial* Material = InPlan.Material;
	FMaterialExpressionCollection& Collection = Material->GetExpressionCollection();

	// ---------------------------------------------------------------------------------------------
	// POSITIONS FIRST, BOXES SECOND, AND THE ORDER IS DELIBERATE.
	//
	// A BOX'S RECTANGLE IS COMPUTED FROM THE POSITIONS. Writing the boxes first would size them against
	// the OLD geometry, so every box would be drawn around where the expressions used to be - which is
	// the one outcome that makes a layout pass look like it worked while leaving the material wrong.
	// ---------------------------------------------------------------------------------------------
	for (const FNode& Node : InPlan.Nodes)
	{
		if (Node.Expression == nullptr)
		{
			continue;
		}
		const int32 NewX = FMath::RoundToInt(Node.PlannedX);
		const int32 NewY = FMath::RoundToInt(Node.PlannedY);
		if (NewX == Node.X && NewY == Node.Y)
		{
			continue;
		}
		Node.Expression->Modify();
		Node.Expression->MaterialExpressionEditorX = NewX;
		Node.Expression->MaterialExpressionEditorY = NewY;
		++Moved;
	}

	// ONE BOX PER BLOCK. An existing box at a block's column is REUSED rather than duplicated, because
	// adding a second box around the same expressions is the failure mode `R-MAT-BLOCKS` would report as
	// two blocks where the author meant one.
	{
		// Index the existing boxes by their own left edge, so a second run finds the box it made.
		TMap<int32, UMaterialExpressionComment*> ExistingByColumn;
		for (UMaterialExpressionComment* Comment : Collection.EditorComments)
		{
			if (Comment == nullptr)
			{
				continue;
			}
			ExistingByColumn.Add(FMath::RoundToInt(static_cast<double>(Comment->MaterialExpressionEditorX)), Comment);
		}

		for (const FBlock& Block : InPlan.Blocks)
		{
			const int32 BoxX = FMath::RoundToInt(Block.Left);
			const int32 BoxY = FMath::RoundToInt(Block.Top);
			const int32 BoxW = FMath::RoundToInt(Block.Right - Block.Left);
			const int32 BoxH = FMath::RoundToInt(Block.Bottom - Block.Top);

			if (UMaterialExpressionComment* const* Found = ExistingByColumn.Find(BoxX))
			{
				UMaterialExpressionComment* Comment = *Found;
				Comment->Modify();
				Comment->MaterialExpressionEditorX = BoxX;
				Comment->MaterialExpressionEditorY = BoxY;
				Comment->SizeX = BoxW;
				Comment->SizeY = BoxH;
				if (!HasText(Comment->Text))
				{
					Comment->Text = Block.Text;
				}
				++BoxesWritten;
				continue;
			}

			// A COMMENT IS NOT CREATED LIKE AN EXPRESSION, AND THIS IS A MEASURED TRAP.
			// `UMaterialEditingLibrary::CreateMaterialExpressionEx` calls `AddExpression`, and a comment
			// belongs in `EditorComments` - a DIFFERENT array. The fixture commandlet records that using
			// the helper for a comment made `R-MAT-BLOCKS` answer `blocks: 0 comment box(es)` against a
			// material that HAD boxes. So this follows the engine's own editor
			// (`FMaterialEditor::CreateNewMaterialExpressionComment`, `MaterialEditor.cpp:6060`) and calls
			// `AddComment` directly.
			UMaterialExpressionComment* Comment = NewObject<UMaterialExpressionComment>(
				Material, UMaterialExpressionComment::StaticClass(), NAME_None, RF_Transactional);
			if (Comment == nullptr)
			{
				continue;
			}
			Comment->Material = Material;
			Comment->MaterialExpressionEditorX = BoxX;
			Comment->MaterialExpressionEditorY = BoxY;
			Comment->SizeX = BoxW;
			Comment->SizeY = BoxH;
			Comment->Text = Block.Text;
			Collection.AddComment(Comment);
			++BoxesWritten;
		}
	}

	// ---------------------------------------------------------------------------------------------
	// THE COMPILED-RESULT CHECK. "Same compiled result" IS THE REQUIREMENT AND IT IS MEASURED HERE.
	//
	// THE ASSERTION RESTS ON THE EXPRESSION SET RATHER THAN ON `StateId`, AND THE REASON IS MEASURED
	// RATHER THAN PREFERRED: `StateId` can legitimately change for reasons that have nothing to do with
	// a layout - a background shader compile, a platform switch, a material function the material calls
	// being recompiled by something else. Asserting on it would make this pass fail for other people's
	// reasons. So the STRONGEST STATEMENT AVAILABLE is made instead:
	//
	//   - the SET OF EXPRESSIONS is identical, which a layout cannot change and which is what "same
	//     compiled result" actually depends on, and
	//   - the engine reports NO NEW ERROR, so nothing was broken, and
	//   - `StateId` is REPORTED, so a reader can see it and judge.
	//
	// A LAYOUT PASS THAT CANNOT PROVE IT PRESERVED THE MATERIAL IS A PASS THAT MIGHT HAVE BROKEN IT
	// SILENTLY, so a failure here is reported as a FAILURE with the difference named - never as a
	// success with a note.
	// ---------------------------------------------------------------------------------------------
	Material->PostEditChange();



	TArray<FString> CompileErrorsAfter = UMaterialEditingLibrary::RecompileMaterial(Material);

	// GATHER THE SET AGAIN, through the SAME reader the baseline used, so the two cannot disagree about
	// what "the expressions" means.
	FString ReadError;
	FTurboMCPMaterial::FPlan ReadAfter;
	TArray<FString> KeysAfter;
	const bool bReadBack = FTurboMCPMaterial::Plan(InPlan.Asset, FTurboMCPMaterial::ERead::Blocks,
		FString(), /*bInIncludeInstanceOverrides=*/false, ReadAfter, ReadError);
	if (bReadBack)
	{
		for (const UMaterialExpression* Expression : ReadAfter.Expressions)
		{
			if (Expression != nullptr)
			{
				KeysAfter.Add(FTurboMCPMaterial::KeyOf(Expression));
			}
		}
	}

	// A SET COMPARISON IN BOTH DIRECTIONS, so an ADDED expression and a REMOVED one are both caught. One
	// direction alone would miss half the ways this can go wrong, and a layout that added an expression
	// is exactly as broken as one that dropped it.
	bool bSameSet = bReadBack && KeysAfter.Num() == InPlan.KeysBefore.Num();
	TArray<FString> Lost;
	TArray<FString> Gained;
	if (bReadBack)
	{
		TSet<FString> Before(InPlan.KeysBefore);
		TSet<FString> After(KeysAfter);
		for (const FString& Key : InPlan.KeysBefore)
		{
			if (!After.Contains(Key))
			{
				Lost.Add(Key);
			}
		}
		for (const FString& Key : KeysAfter)
		{
			if (!Before.Contains(Key))
			{
				Gained.Add(Key);
			}
		}
		bSameSet = Lost.Num() == 0 && Gained.Num() == 0;
	}

	// A NEW ERROR IS THE FAILURE SIGNAL, not any error. A material that ALREADY had a compiler error
	// before this pass must not make this pass fail - that would be blaming the layout for something it
	// found.
	TSet<FString> Before(InPlan.CompileErrorsBefore);

	TArray<FString> NewErrors;
	for (const FString& Error : CompileErrorsAfter)
	{
		if (!Before.Contains(Error))
		{
			NewErrors.Add(Error);
		}
	}

	FString Out = FString::Printf(TEXT("%d expression(s) in %d block(s)"),
		InPlan.Nodes.Num(), InPlan.Blocks.Num());

	if (NewErrors.Num() > 0)
	{
		FTurboMCPRollback::EndAndUndoActiveTransaction();
		return FString::Printf(
			TEXT("FAILED: the layout introduced %d compile error(s) that were not there before, so it was ")
			TEXT("ROLLED BACK. A layout cannot legitimately change what a material computes. First: %s"),
			NewErrors.Num(), *NewErrors[0]);
	}

	if (!bReadBack)
	{
		FTurboMCPRollback::EndAndUndoActiveTransaction();
		return FString::Printf(
			TEXT("FAILED: the material could not be read back after the layout (%s), so the compiled result ")
			TEXT("could NOT be verified and the layout was ROLLED BACK."), *ReadError);
	}

	if (!bSameSet)
	{
		FTurboMCPRollback::EndAndUndoActiveTransaction();
		return FString::Printf(
			TEXT("FAILED: the layout changed the SET OF EXPRESSIONS, so it was ROLLED BACK. ")
			TEXT("%d lost (%s), %d gained (%s). A layout moves geometry; it does not add or remove ")
			TEXT("expressions, and a difference here means the compiled result cannot be trusted."),
			Lost.Num(), Lost.Num() > 0 ? *Lost[0] : TEXT("none"),
			Gained.Num(), Gained.Num() > 0 ? *Gained[0] : TEXT("none"));
	}

	Out += FString::Printf(TEXT("\n%d expression(s) moved, %d comment box(es) written"),
		Moved, BoxesWritten);
	Out = FString::Printf(TEXT("changed: tidied %s : %s"), *InPlan.Asset, *Out);

	// THE COMPILED-RESULT EVIDENCE, STATED WITH ITS OWN LIMITS RATHER THAN AS A BARE CLAIM.
	Out += FString::Printf(
		TEXT("\nCOMPILED RESULT: the expression set is IDENTICAL (%d of %d, none added, none removed) and the ")
		TEXT("engine reports %d error(s), none of them new. StateId %s -> %s (reported rather than asserted: ")
		TEXT("it can change for reasons a layout does not control)."),
		KeysAfter.Num(), InPlan.KeysBefore.Num(), CompileErrorsAfter.Num(),
		*InPlan.StateIdBefore.ToString(), *Material->StateId.ToString());

	if (InPlan.Refusals.Num() > 0)
	{
		Out += FString::Printf(TEXT("\n; %d refusal(s) were planned and are NOT part of this edit"),
			InPlan.Refusals.Num());
	}

	return Out;
}
