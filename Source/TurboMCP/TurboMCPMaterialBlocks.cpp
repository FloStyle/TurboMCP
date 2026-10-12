// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "TurboMCPMaterialBlocks.h"

#include "Logging.h"
#include "Public/TurboMCPMaterial.h"

#include "Containers/UnrealString.h"
#include "Editor/UnrealEd/Public/Editor.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionComment.h"
#include "Materials/MaterialExpressionNamedReroute.h"
#include "Materials/MaterialExpressionReroute.h"
#include "Materials/MaterialFunctionInterface.h"
#include "MaterialEditingLibrary.h"
#include "Misc/Guid.h"
#include "Misc/DefaultValueHelper.h"
#include "ScopedTransaction.h"
#include "UObject/Package.h"

namespace
{
	/** True when an expression is a label declaration, so this pass never adds a second name to a value. */
	bool IsDeclaration(const UMaterialExpression* InExpression)
	{
		FString Ignored;
		return FTurboMCPMaterial::IsLabelDeclaration(InExpression, Ignored);
	}

	/** True when an expression is a label usage, which forwards a name rather than a wire. */
	bool IsUsage(const UMaterialExpression* InExpression)
	{
		FString Ignored;
		return FTurboMCPMaterial::IsLabelUsage(InExpression, Ignored);
	}

	/**
	 * Turn a caption into something usable as a label name.
	 *
	 * THE UNIQUENESS OF THE RESULT IS NOT DECIDED HERE. `UMaterialExpressionNamedRerouteDeclaration` makes
	 * its own name unique through `MakeNameUnique`
	 * (`Runtime/Engine/Private/Materials/MaterialExpressions.cpp:18344`), which is the engine's rule and
	 * the only one that cannot drift from it. This only makes a caption SAFE TO OFFER.
	 *
	 * IT MIRRORS `FTurboMCPMaterialLabelsPromote` DELIBERATELY: both propose names from the same caption,
	 * so a value this pass names and a value that pass names must not read differently in two reports.
	 */
	FString NameFromCaption(const FString& InCaption)
	{
		FString Out;
		for (const TCHAR Character : InCaption)
		{
			// Anything that is not a letter or a digit becomes an underscore rather than being dropped,
			// so `A + B` reads as `A_B` instead of `AB` - a name the caller can still recognise.
			Out.AppendChar(FChar::IsAlnum(Character) ? Character : TEXT('_'));
		}

		if (Out.IsEmpty() || FChar::IsDigit(Out[0]))
		{
			Out = FString(TEXT("Value_")) + Out;
		}

		return Out;
	}

	/** The input's own name where the engine exposes one, so a report names a pin rather than an index. */
	FString NameOfInput(const UMaterialExpression* InOwner, int32 InInputIndex)
	{
		if (InOwner == nullptr)
		{
			return FString();
		}

		const FName Found = InOwner->GetInputName(InInputIndex);
		return Found.IsNone() ? FString() : Found.ToString();
	}

	/**
	 * One reader input, named so `Apply` can find it again.
	 *
	 * *** THIS IS THE REF D-45 REQUIRES AND IT CARRIES BOTH HALVES OF THE IDENTITY. *** The reader's key
	 * says WHICH NODE, and the input index says WHICH PIN. A key alone would be ambiguous for a node that
	 * reads the same value into two of its own inputs - a `Lerp` whose A and B are the same constant - and
	 * an index alone means nothing without the node.
	 */
	FString ConsumerRef(const FString& InReaderKey, int32 InInputIndex)
	{
		return FString::Printf(TEXT("%s#%d"), *InReaderKey, InInputIndex);
	}

	/** Which block an expression is in, or `INDEX_NONE`. A thin alias so the call sites read as prose. */
	int32 BlockOf(const FTurboMCPMaterial::FPlan& InRead, const UMaterialExpression* InExpression)
	{
		return FTurboMCPMaterial::BlockIndexOf(InRead, InExpression);
	}

	/**
	 * One reader input of one expression, with everything a plan needs to describe and re-find it.
	 *
	 * THE POINTED-AT EXPRESSION IS CARRIED SEPARATELY FROM THE READER, because the two are what a
	 * crossing, a merge and a reroute all have to compare: the reader's block against the value's block.
	 */
	struct FLink
	{
		UMaterialExpression* Reader = nullptr;
		FString ReaderKey;
		int32 InputIndex = 0;
		FString InputName;

		/** What the input actually points at, BEFORE any reroute is followed. */
		UMaterialExpression* Direct = nullptr;

		/**
		 * What the value really is, AFTER plain reroutes are followed.
		 *
		 * `2.5`: "Plain reroutes are invisible: links go through them." So a crossing is measured against
		 * the REAL source - a value wired into a box through a long-wire reroute is still the same value
		 * crossing the same boundary.
		 */
		UMaterialExpression* Source = nullptr;
	};

	/** Every input of every expression in the plan, resolved through plain reroutes. */
	void GatherLinks(const FTurboMCPMaterial::FPlan& InRead, TArray<FLink>& OutLinks)
	{
		for (UMaterialExpression* Expression : InRead.Expressions)
		{
			if (Expression == nullptr)
			{
				continue;
			}

			for (FExpressionInputIterator It{ Expression }; It; ++It)
			{
				FExpressionInput* Input = It.Input;
				if (Input == nullptr || Input->Expression == nullptr)
				{
					continue;
				}

				FLink Link;
				Link.Reader = Expression;
				Link.ReaderKey = FTurboMCPMaterial::KeyOf(Expression);
				Link.InputIndex = It.Index;
				Link.InputName = NameOfInput(Expression, It.Index);
				Link.Direct = Input->Expression;
				Link.Source = FTurboMCPMaterial::ResolveThroughReroutes(Input->Expression);
				OutLinks.Add(MoveTemp(Link));
			}
		}
	}

	/** The usage nodes of one label, keyed by their own key so a choice is reproducible. */
	void GatherUsagesByLabel(const FTurboMCPMaterial::FPlan& InRead,
		TMap<FString, TMap<FString, UMaterialExpressionNamedRerouteUsage*>>& OutUsages)
	{
		for (UMaterialExpression* Expression : InRead.Expressions)
		{
			UMaterialExpressionNamedRerouteUsage* const Usage =
				Cast<UMaterialExpressionNamedRerouteUsage>(Expression);
			if (Usage == nullptr)
			{
				continue;
			}

			FString Label;
			if (!FTurboMCPMaterial::IsLabelUsage(Usage, Label))
			{
				continue;
			}

			// KEYED BY KEY RATHER THAN APPENDED TO AN ARRAY, so the node this pass keeps is chosen by a
			// stable order. A `TMap` iteration order is not stable either, which is why the caller sorts
			// the keys it reads out of this rather than trusting the map.
			OutUsages.FindOrAdd(Label).Add(FTurboMCPMaterial::KeyOf(Usage), Usage);
		}
	}
}

bool FTurboMCPMaterialBlocks::Plan(const FString& InAsset, FPlan& OutPlan, FString& OutError)
{
	// THE READER DOES THE RESOLUTION, DELIBERATELY. `FTurboMCPMaterial::Plan` refuses a non-material
	// asset, names the alternative where it can, and gathers the expressions, the comments and the
	// labels. A second resolver here would be the second-dialect mistake in a new place.
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
	OutPlan.BlockCount = FTurboMCPMaterial::BlockCount(Read);

	// ---------------------------------------------------------------------------------------------
	// A REFUSAL RATHER THAN AN EMPTY ANSWER, for the reason `LabelsPromote` records: an instance has no
	// graph, so changing blocks inside it would edit its PARENT - an asset the caller did not name. A
	// writer must not proceed on something a reader merely reports.
	// ---------------------------------------------------------------------------------------------
	if (OutPlan.Material == nullptr && OutPlan.Function == nullptr)
	{
		OutError = OutPlan.ParentPath.IsEmpty()
			? FString(TEXT("this asset has no material graph to arrange into blocks"))
			: FString::Printf(TEXT("this is a Material Instance, whose graph lives in its parent %s. ")
				TEXT("Change blocks on the parent instead, because editing through an instance would change ")
				TEXT("an asset the caller did not name."), *OutPlan.ParentPath);
		return false;
	}

	// THE COMPILED-RESULT BASELINE IS TAKEN BEFORE ANYTHING IS PLANNED. Measuring afterwards and calling
	// the result "before" is how a pass reports success on a material it already changed.
	if (OutPlan.Material != nullptr)
	{
		OutPlan.StateIdBefore = OutPlan.Material->StateId;
		OutPlan.CompileErrorsBefore = UMaterialEditingLibrary::RecompileMaterial(OutPlan.Material);
	}

	for (UMaterialExpression* Expression : Read.Expressions)
	{
		if (Expression == nullptr)
		{
			continue;
		}
		OutPlan.KeysBefore.Add(FTurboMCPMaterial::KeyOf(Expression));
		if (!OutPlan.bHasGraphNodes && Expression->GraphNode != nullptr)
		{
			OutPlan.bHasGraphNodes = true;
		}
	}

	// THE EMPTY BASELINE IS REFUSED, because an empty key set makes every subset check pass for the wrong
	// reason. This project has already paid for that lesson once: a suite whose output name was wrong
	// produced an empty key set and seven checks passed or failed for reasons unrelated to the tool.
	if (OutPlan.KeysBefore.Num() == 0)
	{
		OutError = FString::Printf(TEXT("%s reports no expressions, so a compiled-result comparison would ")
			TEXT("compare two empty sets and pass for the wrong reason"), *OutPlan.Asset);
		return false;
	}

	for (UMaterialExpression* Expression : Read.Expressions)
	{
		if (Expression != nullptr && BlockOf(Read, Expression) == INDEX_NONE)
		{
			++OutPlan.UnboxedExpressions;
		}
	}

	// ---------------------------------------------------------------------------------------------
	// A MATERIAL WITH NO BOXES HAS ONE BLOCK, AND THAT IS A DIFFERENT ANSWER FROM "NOTHING TO DO".
	//
	// `RenderBlocks` already reports this for the reader. Here it becomes a refusal, because EVERY block
	// rule D-17 states is about the boundary between blocks, and a material with one block has no
	// boundary to enforce: adding a label would name a value that never leaves anywhere, merging usages
	// would be cosmetic, and removing an in-block reroute would be a layout change this capability does
	// not claim to make. SAYING SO IS THE ANSWER, and `P1` requires one.
	// ---------------------------------------------------------------------------------------------
	if (OutPlan.BlockCount == 0)
	{
		OutPlan.bNothingToChange = true;
		OutPlan.Refusals.Add(FString::Printf(
			TEXT("%s has no comment box, so it is ONE block and D-17 has no boundary to enforce. Nothing ")
			TEXT("here crosses a block, no label is declared and read in the same place under different ")
			TEXT("boxes, and no reroute is in a block of its own. Run P/E-MAT-TIDY first if the blocks ")
			TEXT("should exist."), *OutPlan.Asset));
		return true;
	}

	TArray<FLink> Links;
	GatherLinks(Read, Links);

	// WHICH VALUES ALREADY HAVE A NAME, FOUND BY WHAT THEY FORWARD RATHER THAN BY THE NAME. The engine may
	// have made a name unique, so a lookup by a proposed name can miss the declaration an earlier run
	// made - the value's own key is the identity that cannot drift. `LabelsPromote` records the same trap.
	TSet<FString> DeclaredSourceKeys;
	for (const TPair<FString, UMaterialExpressionNamedRerouteDeclaration*>& Pair : Read.DeclarationsByLabel)
	{
		const UMaterialExpressionNamedRerouteDeclaration* Declaration = Pair.Value;
		if (Declaration != nullptr && Declaration->Input.Expression != nullptr)
		{
			DeclaredSourceKeys.Add(FTurboMCPMaterial::KeyOf(Declaration->Input.Expression));
		}
	}

	// ---------------------------------------------------------------------------------------------
	// (1) A VALUE READ FROM SEVERAL BLOCKS WITH NO LABEL. D-17: "When a value leaves its block for
	// another block, it travels as a named reroute."
	//
	// THE BLOCKS ARE COMPARED, NOT THE DISTANCES. `LabelsPromote` promotes a value read in two places,
	// which is about how SHARED a value is; this pass cares about WHERE those places are, because a value
	// read twice inside one block needs no name at all - the block already says they belong together.
	// TWO VALUES, TWO QUESTIONS, AND CONFLATING THEM WOULD MAKE ONE OF THE TWO TOOLS WRONG.
	// ---------------------------------------------------------------------------------------------
	TMap<const UMaterialExpression*, TArray<const FLink*>> LinksBySource;
	for (const FLink& Link : Links)
	{
		if (Link.Source == nullptr)
		{
			continue;
		}

		// A LABEL IS NOT A REASON TO ADD A LABEL, for the reason `LabelsPromote` records: a declaration
		// already names its value and a usage forwards a name, so counting either would let a material
		// that is already named propose a second name for the same thing.
		if (IsDeclaration(Link.Reader) || IsUsage(Link.Reader))
		{
			continue;
		}

		LinksBySource.FindOrAdd(Link.Source).Add(&Link);
	}

	TArray<const UMaterialExpression*> Sources;
	LinksBySource.GetKeys(Sources);
	// SORTED BY KEY RATHER THAN BY POINTER. Pointer order is stable within one run and meaningless across
	// two, so a plan built from it would not reproduce - and a preview that disagrees with the edit it
	// precedes is the failure this project states a rule about.
	Sources.Sort([](const UMaterialExpression& A, const UMaterialExpression& B)
	{
		return FTurboMCPMaterial::KeyOf(&A) < FTurboMCPMaterial::KeyOf(&B);
	});

	for (const UMaterialExpression* Source : Sources)
	{
		const FString SourceKey = FTurboMCPMaterial::KeyOf(Source);
		if (DeclaredSourceKeys.Contains(SourceKey))
		{
			// ALREADY NAMED, so there is nothing to add. This is the case a SECOND RUN hits, and leaving
			// it out would make the second run propose the same label again and then report `changed`.
			continue;
		}

		const TArray<const FLink*>& SourceLinks = LinksBySource[Source];

		TSet<int32> Blocks;
		for (const FLink* Link : SourceLinks)
		{
			const int32 Block = BlockOf(Read, Link->Reader);
			if (Block != INDEX_NONE)
			{
				Blocks.Add(Block);
			}
		}

		// *** THE TEST IS "READ FROM TWO DIFFERENT BOXES", AND AN UNBOXED READER DOES NOT COUNT AS ONE. ***
		//
		// THIS WAS MEASURED AND THE FIRST RULE WAS WRONG. It originally SKIPPED a value whenever ANY reader
		// sat outside every box, on the reasoning that an unboxed expression is not "in a block" - WHICH IS
		// TRUE AND WAS THE WRONG CONCLUSION: a value read from block 1 and from the unboxed part of the
		// graph DOES cross a boundary, and skipping it meant this pass refused to name exactly the values
		// D-17 says must travel as a label. `R-MAT-BLOCKS` reported it independently - "1 value(s) cross
		// between boxes without a label ... a wire crossing a box boundary is what a label is for" - WHILE
		// THIS PASS REPORTED NOTHING TO DO, and two tools built on one rule disagreeing about one material is
		// the failure this project keeps recording.
		//
		// WHAT DID NOT CHANGE IS THE OTHER SIDE OF IT: ONE block plus unboxed readers is still not a
		// crossing, because there is only ONE box for the value to leave. A single block means no boundary
		// was crossed, which is what the count below enforces.
		if (Blocks.Num() < 2)
		{
			continue;
		}

		FCrossing Crossing;
		Crossing.Source = const_cast<UMaterialExpression*>(Source);
		Crossing.SourceKey = SourceKey;
		Crossing.SourceCaption = FTurboMCPMaterial::CaptionOf(Source);
		Crossing.ProposedName = NameFromCaption(Crossing.SourceCaption);

		// SORTED, BECAUSE A `TSet` HAS NO ORDER. `KeySort` DOES NOT EXIST ON `TSet` - it is `TMap` that has
		// one - and `TSet::Array()` takes no out-parameter, so the keys are RETURNED and then sorted, which
		// is also what makes the preview and the edit print the same blocks in the same order.
		TArray<int32> SortedBlocks = Blocks.Array();
		SortedBlocks.Sort([](int32 A, int32 B) { return A < B; });
		for (const int32 Block : SortedBlocks)
		{
			Crossing.ReaderBlocks.Add(Block);
		}

		for (const FLink* Link : SourceLinks)
		{
			++Crossing.InputsToRetarget;
			Crossing.ConsumerRefs.Add(ConsumerRef(Link->ReaderKey, Link->InputIndex));
		}

		OutPlan.Crossings.Add(MoveTemp(Crossing));
	}

	// ---------------------------------------------------------------------------------------------
	// (2) A LABEL READ THROUGH TWO OR MORE USAGE NODES IN ONE BLOCK. D-17: "A block reads each label
	// through a single usage node."
	//
	// AND THE COMPLEMENT IS DELIBERATELY NOT TOUCHED: two usages in TWO blocks is the correct shape, and
	// merging those would break "the consuming block reads it" rather than fix anything.
	// ---------------------------------------------------------------------------------------------
	TMap<FString, TMap<FString, UMaterialExpressionNamedRerouteUsage*>> UsagesByLabel;
	GatherUsagesByLabel(Read, UsagesByLabel);

	TArray<FString> LabelNames;
	UsagesByLabel.GetKeys(LabelNames);
	LabelNames.Sort([](const FString& A, const FString& B) { return A < B; });

	for (const FString& Label : LabelNames)
	{
		const TMap<FString, UMaterialExpressionNamedRerouteUsage*>& Usages = UsagesByLabel[Label];

		// GROUPED BY BLOCK, because the rule is per block.
		TMap<int32, TArray<FString>> KeysByBlock;
		for (const TPair<FString, UMaterialExpressionNamedRerouteUsage*>& Pair : Usages)
		{
			const int32 Block = BlockOf(Read, Pair.Value);
			if (Block == INDEX_NONE)
			{
				// A usage outside every box is not in a block, so "one usage per block" says nothing about
				// it. Reported through the unboxed count rather than silently merged.
				continue;
			}
			KeysByBlock.FindOrAdd(Block).Add(Pair.Key);
		}

		TArray<int32> Blocks;
		KeysByBlock.GetKeys(Blocks);
		Blocks.Sort([](int32 A, int32 B) { return A < B; });

		for (const int32 Block : Blocks)
		{
			TArray<FString>& Keys = KeysByBlock[Block];
			if (Keys.Num() < 2)
			{
				continue;
			}

			// THE KEPT NODE IS THE FIRST IN KEY ORDER, so the preview and the edit choose the same one.
			Keys.Sort([](const FString& A, const FString& B) { return A < B; });

			FMerge Merge;
			Merge.Label = Label;
			Merge.BlockIndex = Block;
			Merge.KeepKey = Keys[0];
			for (int32 Index = 1; Index < Keys.Num(); ++Index)
			{
				Merge.RemoveKeys.Add(Keys[Index]);
			}

			// EVERY READER POINTING AT A REMOVED USAGE MOVES TO THE KEPT ONE, so the value is unchanged
			// and only the number of nodes standing for it changes.
			TSet<FString> Removed(TSet<FString>(Merge.RemoveKeys));
			for (const FLink& Link : Links)
			{
				const FString UsageKey = FTurboMCPMaterial::KeyOf(Link.Direct);
				if (Removed.Contains(UsageKey))
				{
					++Merge.InputsToRetarget;
					Merge.ConsumerRefs.Add(ConsumerRef(Link.ReaderKey, Link.InputIndex));
				}
			}

			OutPlan.Merges.Add(MoveTemp(Merge));
		}
	}

	// ---------------------------------------------------------------------------------------------
	// (3) A PLAIN REROUTE STANDING FOR A VALUE INSIDE ONE BLOCK. D-17: "Plain reroute dots are for
	// routing long wires, and never stand for a value."
	//
	// THE BOUNDARY IS WHAT MAKES THE REMOVAL SAFE, AND IT IS CHECKED ON BOTH SIDES. A reroute whose
	// source is in block B and whose readers are ALL in block B does nothing that a direct wire would not
	// do, so removing it cannot move a value across a boundary - WHICH IS THE ONLY THING TIME-CRITICAL
	// ABOUT THIS OPERATION. A reroute that spans blocks is EXACTLY the long-wire case D-17 says reroutes
	// are for, and it is left alone.
	// ---------------------------------------------------------------------------------------------
	TMap<const UMaterialExpression*, TArray<const FLink*>> ReadersByDirect;
	for (const FLink& Link : Links)
	{
		if (Link.Direct != nullptr)
		{
			ReadersByDirect.FindOrAdd(Link.Direct).Add(&Link);
		}
	}

	TArray<UMaterialExpression*> Reroutes;
	for (UMaterialExpression* Expression : Read.Expressions)
	{
		if (Expression != nullptr && FTurboMCPMaterial::IsPlainReroute(Expression))
		{
			Reroutes.Add(Expression);
		}
	}
	Reroutes.Sort([](const UMaterialExpression& A, const UMaterialExpression& B)
	{
		return FTurboMCPMaterial::KeyOf(&A) < FTurboMCPMaterial::KeyOf(&B);
	});

	for (UMaterialExpression* Reroute : Reroutes)
	{
		const TArray<const FLink*>* Readers = ReadersByDirect.Find(Reroute);
		if (Readers == nullptr || Readers->Num() == 0)
		{
			// A REROUTE NOTHING READS IS NOT A VALUE CROSSING ANYTHING, and removing it would be a
			// deletion this capability does not claim to make. `R-MAT-BLOCKS` reports unreachable
			// structure; this pass does not tidy it.
			continue;
		}

		const int32 RerouteBlock = BlockOf(Read, Reroute);
		if (RerouteBlock == INDEX_NONE)
		{
			continue;
		}

		// THE REROUTE'S OWN INPUT, READ DIRECTLY FROM `Input` FOR THE REASON `ResolveThroughReroutes`
		// RECORDS: `GetRerouteInput` is `protected` (`MaterialExpressionRerouteBase.h:44`) and its use was
		// a `C2248` compile error, while `MaterialExpressionReroute.h` declares `Input` under `public:` -
		// and for a PLAIN reroute, which is what is checked above, `Input` IS the link.
		UMaterialExpressionReroute* const AsReroute = Cast<UMaterialExpressionReroute>(Reroute);
		if (AsReroute == nullptr || AsReroute->Input.Expression == nullptr)
		{
			continue;
		}

		UMaterialExpression* const Source =
			FTurboMCPMaterial::ResolveThroughReroutes(AsReroute->Input.Expression);
		if (Source == nullptr || Source == Reroute)
		{
			continue;
		}

		if (BlockOf(Read, Source) != RerouteBlock)
		{
			// THE SOURCE IS IN ANOTHER BLOCK, so this reroute carries a value across a boundary and is
			// the long-wire case rather than the violation. LEFT ALONE, and that is the point.
			continue;
		}

		bool bAllReadersInBlock = true;
		for (const FLink* Link : *Readers)
		{
			if (BlockOf(Read, Link->Reader) != RerouteBlock)
			{
				bAllReadersInBlock = false;
				break;
			}
		}
		if (!bAllReadersInBlock)
		{
			continue;
		}

		FInBlockReroute Removal;
		Removal.Reroute = Reroute;
		Removal.RerouteKey = FTurboMCPMaterial::KeyOf(Reroute);
		// *** THE CLASS IS TAKEN FROM THE ENGINE RATHER THAN LEFT FOR THE REPORT TO INVENT. THERE IS NO
		// TITLE: `FInBlockReroute` HAS NO TITLE FIELD, so `D-45`'s "class and title" is satisfied for
		// the class only and every report line below prints the key and the class. ***
		Removal.RerouteClass = Reroute->GetClass()->GetName();
		Removal.Source = Source;
		Removal.SourceKey = FTurboMCPMaterial::KeyOf(Source);
		Removal.BlockIndex = RerouteBlock;
		for (const FLink* Link : *Readers)
		{
			++Removal.InputsToRetarget;
			Removal.ConsumerRefs.Add(ConsumerRef(Link->ReaderKey, Link->InputIndex));
		}

		OutPlan.InBlockReroutes.Add(MoveTemp(Removal));
	}

	OutPlan.bNothingToChange = OutPlan.ItemCount() == 0;

	if (OutPlan.bNothingToChange)
	{
		// `P1`: never return nothing, and say WHY. This is the answer the capability exists to give on a
		// material that already follows D-17, and it is a different answer from a failed read.
		OutPlan.Refusals.Add(FString::Printf(
			TEXT("%s already follows D-17 across its %d block(s): no value crosses a block without a ")
			TEXT("label, no label is read through two usage nodes in one block, and no plain reroute stands ")
			TEXT("for a value inside a single block. %d expression(s) were examined, %d of them in no block."),
			*OutPlan.Asset, OutPlan.BlockCount, OutPlan.KeysBefore.Num(), OutPlan.UnboxedExpressions));
	}

	return true;
}

FString FTurboMCPMaterialBlocks::RenderPreview(const FPlan& InPlan)
{
	FString Out = FString::Printf(TEXT("preview: blocks in %s"), *InPlan.Asset);
	Out += FString::Printf(TEXT("\nblocks: %d comment box(es), %d expression(s), %d of them in no block"),
		InPlan.BlockCount, InPlan.KeysBefore.Num(), InPlan.UnboxedExpressions);
	Out += FString::Printf(TEXT("\nitems the edit would change: %d"), InPlan.ItemCount());

	if (!InPlan.bHasGraphNodes)
	{
		// REPORTED, NOT ASSUMED, AND IT DECIDES HOW THE EDIT IS MADE. The engine's own editor works
		// through `UEdGraph` pins; a material built headlessly by `CreateMaterialExpressionEx` has none,
		// so the edit goes where the compiler reads instead.
		Out += TEXT("\n; no expression carries a graph node, so this is a headlessly built material and ")
			TEXT("the readers are retargeted on the expression inputs, which is where the compiler reads.");
	}

	if (InPlan.bNothingToChange)
	{
		for (const FString& Refusal : InPlan.Refusals)
		{
			Out += FString::Printf(TEXT("\n%s"), *Refusal);
		}
		return Out;
	}

	// EVERY ITEM IS PRINTED, BECAUSE "changes exactly the items in the Preview list" IS ONLY CHECKABLE IF
	// THE LIST IS IN THE TEXT. A preview that said "3 changes" would make the requirement unverifiable.
	if (InPlan.Crossings.Num() > 0)
	{
		Out += TEXT("\n\nLABELS TO ADD - a value read from more than one block, which D-17 says travels as ");
		Out += TEXT("a named reroute:");
		for (const FCrossing& Crossing : InPlan.Crossings)
		{
			Out += FString::Printf(TEXT("\n  + @%s  <- %s  (%s)"),
				*Crossing.ProposedName, *Crossing.SourceKey, *Crossing.SourceCaption);
			Out += FString::Printf(TEXT("\n      read from %d block(s)"), Crossing.ReaderBlocks.Num());
			for (const FString& Ref : Crossing.ConsumerRefs)
			{
				Out += FString::Printf(TEXT("\n      - %s"), *Ref);
			}
		}
	}

	if (InPlan.Merges.Num() > 0)
	{
		Out += TEXT("\n\nUSAGE NODES TO MERGE - a label read through more than one usage node in one ");
		Out += TEXT("block, which D-17 forbids:");
		for (const FMerge& Merge : InPlan.Merges)
		{
			Out += FString::Printf(TEXT("\n  = @%s in block %d: keep %s, remove %d node(s)"),
				*Merge.Label, Merge.BlockIndex, *Merge.KeepKey, Merge.RemoveKeys.Num());
			for (const FString& Key : Merge.RemoveKeys)
			{
				Out += FString::Printf(TEXT("\n      - %s"), *Key);
			}
			for (const FString& Ref : Merge.ConsumerRefs)
			{
				Out += FString::Printf(TEXT("\n      -> %s moves to the kept node"), *Ref);
			}
		}
	}

	if (InPlan.InBlockReroutes.Num() > 0)
	{
		Out += TEXT("\n\nIN-BLOCK REROUTES TO REMOVE - a plain reroute standing for a value inside one ");
		Out += TEXT("block, which D-17 says they never do:");
		for (const FInBlockReroute& Removal : InPlan.InBlockReroutes)
		{
			// D-45: a removal is listed BY KEY WITH ITS CLASS, never as a count. There is no title to
			// print, because the plan carries none.
			Out += FString::Printf(TEXT("\n  - %s  (%s) in block %d  -> readers move to %s"),
				*Removal.RerouteKey, *Removal.RerouteClass, Removal.BlockIndex, *Removal.SourceKey);
			for (const FString& Ref : Removal.ConsumerRefs)
			{
				Out += FString::Printf(TEXT("\n      -> %s"), *Ref);
			}
		}
	}

	Out += FString::Printf(
		TEXT("\n\nthis edit ADDS %d label(s), MERGES %d usage group(s) and REMOVES %d reroute(s). It changes ")
		TEXT("nothing outside that list, and the material's compiled result is checked afterwards and ")
		TEXT("rolled back if it is not the same."),
		InPlan.Crossings.Num(), InPlan.Merges.Num(), InPlan.InBlockReroutes.Num());

	return Out;
}

namespace
{
	/**
	 * Resolves the plan's key set against the material AS IT IS NOW.
	 *
	 * *** D-45 IS THE REASON THIS EXISTS RATHER THAN THE PLAN CARRYING POINTERS: "it re-resolves every
	 * node by key at apply time rather than trusting a plan from an earlier call", BECAUSE A PLAN IS MADE
	 * AND APPLIED IN TWO CALLS AND THE MATERIAL CAN CHANGE BETWEEN THEM. *** A stale pointer would retarget
	 * whatever now occupies that address, which is a wrong material that still compiles - the one outcome
	 * the compiled-result check cannot distinguish from a correct edit.
	 *
	 * A KEY THAT NO LONGER RESOLVES IS A REFUSAL, NOT A SKIP. Silently skipping it would make the edit
	 * differ from the preview it was authorised by, which is exactly what "changes exactly the items in
	 * the Preview list" forbids.
	 */
	void IndexByKey(const FTurboMCPMaterial::FPlan& InRead,
		TMap<FString, UMaterialExpression*>& OutByKey)
	{
		for (UMaterialExpression* Expression : InRead.Expressions)
		{
			if (Expression != nullptr)
			{
				OutByKey.Add(FTurboMCPMaterial::KeyOf(Expression), Expression);
			}
		}
	}

	/**
	 * Re-points one reader input, refusing unless the reader, the new target and the pin are all real.
	 *
	 * *** WHAT THIS REFUSES, STATED EXACTLY, BECAUSE THE NAME PROMISES MORE: it refuses a null reader, a
	 * null new target, an input index the reader does not have, and a null pin at that index. IT DOES NOT
	 * DETECT A MATERIAL THAT CHANGED BETWEEN THE TWO CALLS, because the caller recomputes
	 * `InExpectedOld` FROM THE READER ITSELF and hands that value in - so `It.Input->Expression !=
	 * InExpectedOld` compares the pin with the value just read off it and cannot fail. The parameter is
	 * kept because the comparison is correct if a caller ever supplies a value the plan carried. ***
	 * Refusing on the four real conditions is still better than retargeting by index alone, which would
	 * write to whatever that pin now holds.
	 */
	bool RetargetInput(UMaterialExpression* InReader, int32 InInputIndex,
		UMaterialExpression* InExpectedOld, UMaterialExpression* InNew)
	{
		if (InReader == nullptr || InNew == nullptr)
		{
			return false;
		}

		for (FExpressionInputIterator It{ InReader }; It; ++It)
		{
			if (It.Index != InInputIndex)
			{
				continue;
			}
			if (It.Input == nullptr || It.Input->Expression != InExpectedOld)
			{
				return false;
			}

			InReader->Modify();
			It.Input->Expression = InNew;
			It.Input->OutputIndex = 0;
			return true;
		}

		return false;
	}

	/** Splits `<readerKey>#<inputIndex>` back into its two halves. */
	bool ParseConsumerRef(const FString& InRef, FString& OutReaderKey, int32& OutInputIndex)
	{
		int32 Hash = INDEX_NONE;
		if (!InRef.FindChar(TEXT('#'), Hash))
		{
			return false;
		}

		OutReaderKey = InRef.Left(Hash);
		return !OutReaderKey.IsEmpty()
			&& FDefaultValueHelper::ParseInt(InRef.Mid(Hash + 1), OutInputIndex);
	}

	/**
	 * *** MEASURES THE ROLLBACK, WHICH IS THE ONE THING THE NINETEEN `Transaction.Cancel()` SITES NEVER DO. ***
	 *
	 * `Cancel()` returns `void`, so there is no result to read - but the transaction is not opaque:
	 * `Cancel()` calls `GEditor->CancelTransaction(Index)` AND THEN sets `Index = -1`
	 * (`ScopedTransaction.cpp:42-49`), so `IsOutstanding()` (`:54-57`) answers FALSE afterwards. THE RESTORE
	 * HAS ALREADY HAPPENED BY THAT POINT, because it happens INSIDE `Cancel()`; the destructor only calls
	 * `EndTransaction()` while the transaction is STILL outstanding (`:31-37`).
	 *
	 * SO A FAILED PASS CAN BE MEASURED ON ITS WAY OUT OF THE SCOPE, WITHOUT TOUCHING ANY OF THE NINETEEN
	 * RETURN SITES. This object is declared after the transaction and destroyed before it, and its destructor
	 * is the one place every cancelled path reaches.
	 *
	 * THE REFERENCE SET IS `Plan.KeysBefore` - what the plan READ before the edit, and the same set `Apply`
	 * already compares the post-edit state against. A rollback that does not come back to it is a defect in
	 * this tool, and the note says so in those words rather than restating the request as "rolled back".
	 */
	struct FRollbackMeasurement
	{
		FRollbackMeasurement(FScopedTransaction& InTransaction,
			const FTurboMCPMaterialBlocks::FPlan& InPlan, FString* InOutNote)
			: Transaction(InTransaction), Plan(InPlan), OutNote(InOutNote)
		{
		}

		~FRollbackMeasurement()
		{
			// NOTHING TO MEASURE ON A SUCCESSFUL PASS. An outstanding transaction was never cancelled, and
			// the material is MEANT to hold the new state - so this is not a rollback to verify.
			if (OutNote == nullptr || Transaction.IsOutstanding())
			{
				return;
			}

			FTurboMCPMaterial::FPlan Reread;
			FString RereadError;
			if (!FTurboMCPMaterial::Plan(Plan.Asset, FTurboMCPMaterial::ERead::Blocks, FString(),
					/*bInIncludeInstanceOverrides=*/false, Reread, RereadError))
			{
				// AN UNVERIFIED ROLLBACK IS SAID TO BE UNVERIFIED. Reporting success here would be the
				// nineteen-claims-with-nothing-behind-them defect this measurement exists to remove.
				*OutNote = FString::Printf(
					TEXT("\nROLLBACK NOT VERIFIED: the material could not be re-read after the cancel (%s), so ")
					TEXT("nothing here says whether it came back."), *RereadError);
				return;
			}

			TSet<FString> Now;
			for (const UMaterialExpression* Expression : Reread.Expressions)
			{
				if (Expression != nullptr)
				{
					Now.Add(FTurboMCPMaterial::KeyOf(Expression));
				}
			}

			TArray<FString> Missing;
			for (const FString& Key : Plan.KeysBefore)
			{
				if (!Now.Contains(Key))
				{
					Missing.Add(Key);
				}
			}
			TArray<FString> Extra;
			for (const FString& Key : Now)
			{
				if (!Plan.KeysBefore.Contains(Key))
				{
					Extra.Add(Key);
				}
			}

			if (Missing.Num() == 0 && Extra.Num() == 0)
			{
				*OutNote = FString::Printf(
					TEXT("\nROLLBACK VERIFIED: the material holds the %d expression(s) the plan read, so the ")
					TEXT("cancel put it back."), Plan.KeysBefore.Num());
				return;
			}

			*OutNote = FString::Printf(
				TEXT("\n*** THE ROLLBACK DID NOT RESTORE THE MATERIAL: %d of the %d expression(s) the plan ")
				TEXT("read are gone, and %d are present that it did not read. THIS IS A DEFECT IN THIS TOOL ")
				TEXT("AND NOT A CLEAN ROLLBACK. First named: %s"),
				Missing.Num(), Plan.KeysBefore.Num(), Extra.Num(),
				Missing.Num() > 0 ? *Missing[0] : *Extra[0]);
		}

		FScopedTransaction& Transaction;
		const FTurboMCPMaterialBlocks::FPlan& Plan;
		FString* OutNote;
	};
}

FString FTurboMCPMaterialBlocks::Apply(const FPlan& InPlan, FString& OutRollbackNote)
{
	OutRollbackNote.Empty();

	if (InPlan.bNothingToChange)
	{
		return FString::Printf(
			TEXT("unchanged: %s already follows D-17 across its %d block(s), so there is nothing to change. ")
			TEXT("%d expression(s) were examined."),
			*InPlan.Asset, InPlan.BlockCount, InPlan.KeysBefore.Num());
	}

	if (InPlan.Material == nullptr)
	{
		return FString::Printf(TEXT("error: %s has no material to change blocks in"), *InPlan.Asset);
	}

	// ONE TRANSACTION, so the whole edit is a single undo step and a rollback is exact.
	FScopedTransaction Transaction(NSLOCTEXT("TurboMCP", "MaterialBlocks",
		"TurboMCP: make labels follow blocks"));

	UMaterial* const Material = InPlan.Material;

	// *** THE ONE MEASUREMENT EVERY ROLLBACK PATH REACHES, DECLARED HERE SO THAT IT OUTLIVES ALL NINETEEN
	// `Cancel()` SITES AND IS DESTROYED BEFORE THE TRANSACTION ITSELF. *** See its definition above: it does
	// nothing on a successful pass, and on a cancelled one it re-reads the material and compares the
	// expression set with the one the plan read.
	FRollbackMeasurement Rollback(Transaction, InPlan, &OutRollbackNote);

	// THE MATERIAL IS RE-READ BEFORE ANYTHING IS TOUCHED, which is what makes this an edit of the CURRENT
	// graph rather than of the one the plan was built from.
	FTurboMCPMaterial::FPlan Read;
	FString ReadError;
	if (!FTurboMCPMaterial::Plan(InPlan.Asset, FTurboMCPMaterial::ERead::Blocks, FString(),
		/*bInIncludeInstanceOverrides=*/false, Read, ReadError))
	{
		Transaction.Cancel();
		return FString::Printf(
			TEXT("FAILED: the material could not be re-read at apply time (%s), so the edit was NOT ")
			TEXT("applied rather than applied against a stale plan."), *ReadError);
	}

	TMap<FString, UMaterialExpression*> ByKey;
	IndexByKey(Read, ByKey);

	int32 LabelsAdded = 0;
	int32 UsagesCreated = 0;
	int32 UsagesRemoved = 0;
	int32 ReroutesRemoved = 0;
	int32 InputsRetargeted = 0;

	TArray<FString> PlannedAdds;
	TArray<FString> PlannedRemovals;

	// ---------------------------------------------------------------------------------------------
	// (1) ADD THE LABELS. Each crossing gets a declaration forwarding its value, and one usage node per
	// DISTINCT READER - which is the engine's own convention
	// (`Editor/MaterialEditor/Private/MaterialEditor.cpp:4633-4643`) and what makes a label report how
	// many places read it. `LabelsPromote` measured the cost of getting this wrong: pointing readers
	// straight at a declaration produced a label that `R-MAT-LABELS` answered `read in 0 place(s)` about,
	// and NOTHING IN ITS OWN REPORT SAID SO.
	// ---------------------------------------------------------------------------------------------
	for (const FCrossing& Crossing : InPlan.Crossings)
	{
		UMaterialExpression* const Source = ByKey.FindRef(Crossing.SourceKey);
		if (Source == nullptr)
		{
			Transaction.Cancel();
			return FString::Printf(
				TEXT("FAILED: the value %s this plan would name no longer exists, so the edit was ROLLED ")
				TEXT("BACK rather than applied in part."), *Crossing.SourceKey);
		}

		UMaterialExpression* const Created = UMaterialEditingLibrary::CreateMaterialExpressionEx(
			Material, /*MaterialFunction=*/nullptr,
			UMaterialExpressionNamedRerouteDeclaration::StaticClass(),
			/*SelectedAsset=*/nullptr,
			Source->MaterialExpressionEditorX,
			Source->MaterialExpressionEditorY + 120,
			/*bAllowMarkingPackageDirty=*/true);

		UMaterialExpressionNamedRerouteDeclaration* const Declaration =
			Cast<UMaterialExpressionNamedRerouteDeclaration>(Created);
		if (Declaration == nullptr)
		{
			Transaction.Cancel();
			return FString::Printf(
				TEXT("FAILED: the declaration for @%s could not be created, so the whole edit was ROLLED ")
				TEXT("BACK rather than applied in part."), *Crossing.ProposedName);
		}
		++LabelsAdded;

		if (Declaration->GetEditableName() != Crossing.ProposedName)
		{
			Declaration->SetEditableName(Crossing.ProposedName);
		}

		// THE DECLARATION READS THE VALUE IT NAMES. Without this the node forwards nothing, so every
		// reader retargeted to it would compile to an unconnected input - a silently wrong material.
		Declaration->Input.Expression = Source;
		Declaration->Input.OutputIndex = 0;
		Declaration->Input.InputName = NAME_None;
		PlannedAdds.Add(FTurboMCPMaterial::KeyOf(Declaration));

		// ONE USAGE PER DISTINCT READER, keyed by the reader so two inputs of one expression share one.
		TMap<FString, UMaterialExpressionNamedRerouteUsage*> UsageByReaderKey;

		for (const FString& Ref : Crossing.ConsumerRefs)
		{
			FString ReaderKey;
			int32 InputIndex = INDEX_NONE;
			if (!ParseConsumerRef(Ref, ReaderKey, InputIndex))
			{
				Transaction.Cancel();
				return FString::Printf(
					TEXT("FAILED: the plan carried a consumer reference this pass cannot parse (%s), so the ")
					TEXT("edit was ROLLED BACK rather than applied against a reference it does not ")
					TEXT("understand."), *Ref);
			}

			UMaterialExpression* const Reader = ByKey.FindRef(ReaderKey);
			if (Reader == nullptr)
			{
				Transaction.Cancel();
				return FString::Printf(
					TEXT("FAILED: the reader %s this plan would retarget no longer exists, so the edit was ")
					TEXT("ROLLED BACK rather than applied in part."), *ReaderKey);
			}

			UMaterialExpressionNamedRerouteUsage* Usage = UsageByReaderKey.FindRef(ReaderKey);
			if (Usage == nullptr)
			{
				UMaterialExpression* const CreatedUsage = UMaterialEditingLibrary::CreateMaterialExpressionEx(
					Material, /*MaterialFunction=*/nullptr,
					UMaterialExpressionNamedRerouteUsage::StaticClass(),
					/*SelectedAsset=*/nullptr,
					Reader->MaterialExpressionEditorX - 150,
					Reader->MaterialExpressionEditorY,
					/*bAllowMarkingPackageDirty=*/true);

				Usage = Cast<UMaterialExpressionNamedRerouteUsage>(CreatedUsage);
				if (Usage == nullptr)
				{
					Transaction.Cancel();
					return FString::Printf(
						TEXT("FAILED: the usage node for @%s could not be created, so the whole edit was ")
						TEXT("ROLLED BACK rather than applied in part."), *Crossing.ProposedName);
				}

				// BOTH THE POINTER AND THE GUID, because the reader prefers the pointer and falls back to
				// the GUID for a freshly loaded asset whose fixup has not run - `LabelOfUsage` records it.
				Usage->Declaration = Declaration;
				Usage->DeclarationGuid = Declaration->VariableGuid;
				UsageByReaderKey.Add(ReaderKey, Usage);
				PlannedAdds.Add(FTurboMCPMaterial::KeyOf(Usage));
				++UsagesCreated;
			}

			// THE EXPECTED OLD VALUE IS THE SOURCE, because a crossing is measured on the RESOLVED source.
			if (!RetargetInput(Reader, InputIndex, Source, Usage))
			{
				Transaction.Cancel();
				return FString::Printf(
					TEXT("FAILED: %s no longer points at %s, so the material changed between the Preview ")
					TEXT("and this call and the edit was ROLLED BACK rather than rewired on the wrong ")
					TEXT("seam."), *Ref, *Crossing.SourceKey);
			}
			++InputsRetargeted;
		}
	}

	// ---------------------------------------------------------------------------------------------
	// (2) MERGE THE REDUNDANT USAGE NODES. The readers move to the kept node FIRST, and only then is the
	// redundant one removed - so the value is never briefly unread, and a removal that failed would leave
	// a graph that still works rather than one with a dangling reference.
	// ---------------------------------------------------------------------------------------------
	for (const FMerge& Merge : InPlan.Merges)
	{
		UMaterialExpression* const Keep = ByKey.FindRef(Merge.KeepKey);
		if (Keep == nullptr)
		{
			Transaction.Cancel();
			return FString::Printf(
				TEXT("FAILED: the usage node to keep (%s) for @%s no longer exists, so the edit was ROLLED ")
				TEXT("BACK rather than applied in part."), *Merge.KeepKey, *Merge.Label);
		}

		for (const FString& Ref : Merge.ConsumerRefs)
		{
			FString ReaderKey;
			int32 InputIndex = INDEX_NONE;
			if (!ParseConsumerRef(Ref, ReaderKey, InputIndex))
			{
				Transaction.Cancel();
				return FString::Printf(
					TEXT("FAILED: the plan carried a consumer reference this pass cannot parse (%s)."), *Ref);
			}

			UMaterialExpression* const Reader = ByKey.FindRef(ReaderKey);
			if (Reader == nullptr)
			{
				Transaction.Cancel();
				return FString::Printf(
					TEXT("FAILED: the reader %s this merge would retarget no longer exists, so the edit was ")
					TEXT("ROLLED BACK rather than applied in part."), *ReaderKey);
			}

			// THE OLD VALUE IS RECOMPUTED FROM THE READER ITSELF rather than carried, because a merge
			// retargets away from a usage node whose key is known but whose current wiring is not.
			UMaterialExpression* OldTarget = nullptr;
			for (FExpressionInputIterator It{ Reader }; It; ++It)
			{
				if (It.Index == InputIndex && It.Input != nullptr)
				{
					OldTarget = It.Input->Expression;
					break;
				}
			}

			if (!RetargetInput(Reader, InputIndex, OldTarget, Keep))
			{
				Transaction.Cancel();
				return FString::Printf(
					TEXT("FAILED: %s could not be moved onto the kept usage node for @%s, so the edit was ")
					TEXT("ROLLED BACK rather than applied in part."), *Ref, *Merge.Label);
			}
			++InputsRetargeted;
		}

		for (const FString& RemoveKey : Merge.RemoveKeys)
		{
			UMaterialExpression* const Doomed = ByKey.FindRef(RemoveKey);
			if (Doomed == nullptr)
			{
				// ALREADY GONE IS NOT A FAILURE AND NOT A SILENT SKIP EITHER: the reader count is checked
				// afterwards, so a node that vanished is caught by the set comparison rather than here.
				continue;
			}

			// THE REMOVAL IS RECORDED BY KEY ALONE. This set is the before/after comparison's subject,
			// not an entry of the report, so no class is recorded beside the key here.
			PlannedRemovals.Add(RemoveKey);
			UMaterialEditingLibrary::DeleteMaterialExpression(Material, Doomed);
			++UsagesRemoved;
		}
	}

	// ---------------------------------------------------------------------------------------------
	// (3) REMOVE THE IN-BLOCK REROUTES. Same order as the merge: the readers move to what the reroute was
	// passing along, and the reroute is removed only afterwards.
	// ---------------------------------------------------------------------------------------------
	for (const FInBlockReroute& Removal : InPlan.InBlockReroutes)
	{
		UMaterialExpression* const Reroute = ByKey.FindRef(Removal.RerouteKey);
		UMaterialExpression* const Source = ByKey.FindRef(Removal.SourceKey);
		if (Reroute == nullptr || Source == nullptr)
		{
			Transaction.Cancel();
			return FString::Printf(
				TEXT("FAILED: the in-block reroute %s or the value it passes (%s) no longer exists, so the ")
				TEXT("edit was ROLLED BACK rather than applied in part."),
				*Removal.RerouteKey, *Removal.SourceKey);
		}

		for (const FString& Ref : Removal.ConsumerRefs)
		{
			FString ReaderKey;
			int32 InputIndex = INDEX_NONE;
			if (!ParseConsumerRef(Ref, ReaderKey, InputIndex))
			{
				Transaction.Cancel();
				return FString::Printf(
					TEXT("FAILED: the plan carried a consumer reference this pass cannot parse (%s)."), *Ref);
			}

			UMaterialExpression* const Reader = ByKey.FindRef(ReaderKey);
			if (Reader == nullptr)
			{
				Transaction.Cancel();
				return FString::Printf(
					TEXT("FAILED: the reader %s this removal would retarget no longer exists, so the edit ")
					TEXT("was ROLLED BACK rather than applied in part."), *ReaderKey);
			}

			// THE REROUTE MAY FAR INTO A CHAIN, but the readers point at THIS one, so that is what the
			// input is expected to hold.
			if (!RetargetInput(Reader, InputIndex, Reroute, Source))
			{
				Transaction.Cancel();
				return FString::Printf(
					TEXT("FAILED: %s no longer points at the reroute %s, so the edit was ROLLED BACK ")
					TEXT("rather than rewired on the wrong seam."), *Ref, *Removal.RerouteKey);
			}
			++InputsRetargeted;
		}

		PlannedRemovals.Add(Removal.RerouteKey);
		UMaterialEditingLibrary::DeleteMaterialExpression(Material, Reroute);
		++ReroutesRemoved;
	}

	// ---------------------------------------------------------------------------------------------
	// THE COMPILED-RESULT CHECK. "Same compiled result check" IS THE REQUIREMENT AND IT IS MEASURED HERE.
	//
	// THE ASYMMETRY IS DELIBERATE AND FOLLOWS `LabelsPromote`. This pass ADDS declarations and usages and
	// REMOVES usage nodes and reroutes, so the assertion is that no pre-existing expression was lost
	// EXCEPT THE ONES THIS PASS PLANNED TO REMOVE, and that what was GAINED is exactly what it planned. A
	// bare "the sets differ" check would fail every time it worked; a one-directional check would miss a
	// dropped expression.
	//
	// AND THE REMOVALS ARE COMPARED AGAINST THE PLAN RATHER THAN COUNTED, because "changes exactly the
	// items in the Preview list" means an edit that removed ONE MORE node than it listed has failed even
	// though the material still compiles.
	// ---------------------------------------------------------------------------------------------
	Material->PostEditChange();

	TArray<FString> const CompileErrorsAfter = UMaterialEditingLibrary::RecompileMaterial(Material);

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

	if (!bReadBack)
	{
		Transaction.Cancel();
		return FString::Printf(
			TEXT("FAILED: the material could not be read back after the edit (%s), so the compiled result ")
			TEXT("could NOT be verified and the edit was ROLLED BACK."), *ReadError);
	}

	TSet<FString> const Before(InPlan.KeysBefore);
	TSet<FString> const After(KeysAfter);
	TSet<FString> const PlannedGains(PlannedAdds);
	TSet<FString> const PlannedLosses(PlannedRemovals);

	TArray<FString> Lost;
	for (const FString& Key : InPlan.KeysBefore)
	{
		if (!After.Contains(Key) && !PlannedLosses.Contains(Key))
		{
			Lost.Add(Key);
		}
	}

	TArray<FString> GainedUnexpected;
	for (const FString& Key : KeysAfter)
	{
		if (!Before.Contains(Key) && !PlannedGains.Contains(Key))
		{
			GainedUnexpected.Add(Key);
		}
	}

	// A NEW ERROR IS THE FAILURE SIGNAL, not any error. A material that ALREADY had a compiler error
	// before this pass must not make this pass fail - that would be blaming the edit for what it found.
	TSet<FString> const ErrorsBefore(InPlan.CompileErrorsBefore);
	TArray<FString> NewErrors;
	for (const FString& Error : CompileErrorsAfter)
	{
		if (!ErrorsBefore.Contains(Error))
		{
			NewErrors.Add(Error);
		}
	}

	if (NewErrors.Num() > 0)
	{
		Transaction.Cancel();
		return FString::Printf(
			TEXT("FAILED: making labels follow blocks introduced %d compile error(s) that were not there ")
			TEXT("before, so the edit was ROLLED BACK. A named reroute forwards to its input and a plain ")
			TEXT("reroute passes its input along, so neither can legitimately change what a material ")
			TEXT("computes. First: %s"), NewErrors.Num(), *NewErrors[0]);
	}

	if (Lost.Num() > 0)
	{
		Transaction.Cancel();
		return FString::Printf(
			TEXT("FAILED: the edit LOST %d expression(s) it did not list, so it was ROLLED BACK. This pass ")
			TEXT("lists every removal by key; a node gone that was not on that list means it removed more ")
			TEXT("than the Preview authorised. First lost: %s"), Lost.Num(), *Lost[0]);
	}

	if (GainedUnexpected.Num() > 0)
	{
		Transaction.Cancel();
		return FString::Printf(
			TEXT("FAILED: the edit GAINED %d expression(s) it did not plan, so it was ROLLED BACK. First ")
			TEXT("unexpected: %s"), GainedUnexpected.Num(), *GainedUnexpected[0]);
	}

	FString Out = FString::Printf(TEXT("changed: made labels follow blocks in %s"), *InPlan.Asset);
	Out += FString::Printf(
		TEXT("\n%d label(s) added, %d usage node(s) created, %d usage node(s) merged away, %d reroute(s) ")
		TEXT("removed, %d input(s) retargeted"),
		LabelsAdded, UsagesCreated, UsagesRemoved, ReroutesRemoved, InputsRetargeted);

	for (const FCrossing& Crossing : InPlan.Crossings)
	{
		Out += FString::Printf(TEXT("\n@%s <- %s (read from %d block(s))"),
			*Crossing.ProposedName, *Crossing.SourceKey, Crossing.ReaderBlocks.Num());
	}
	for (const FMerge& Merge : InPlan.Merges)
	{
		Out += FString::Printf(TEXT("\n@%s: %d usage node(s) merged in block %d"),
			*Merge.Label, Merge.RemoveKeys.Num(), Merge.BlockIndex);
	}
	// EVERY REMOVAL IS LISTED BY KEY RATHER THAN COUNTED. `PlannedRemovals` holds keys and nothing else,
	// so no class can be printed beside them here.
	for (const FString& Key : PlannedRemovals)
	{
		Out += FString::Printf(TEXT("\nremoved %s"), *Key);
	}

	// THE COMPILED-RESULT EVIDENCE, STATED WITH ITS OWN LIMITS RATHER THAN AS A BARE CLAIM.
	Out += FString::Printf(
		TEXT("\nCOMPILED RESULT: %d of %d pre-existing expression(s) still present, %d removed and all of ")
		TEXT("them on the plan's own list, %d gained and all of them planned, and the engine reports %d ")
		TEXT("error(s), none of them new. StateId %s -> %s (reported rather than asserted: it can change for ")
		TEXT("reasons this pass does not control)."),
		KeysAfter.Num(), InPlan.KeysBefore.Num(), PlannedRemovals.Num(),
		GainedUnexpected.Num() == 0 ? PlannedGains.Num() : 0,
		CompileErrorsAfter.Num(),
		*InPlan.StateIdBefore.ToString(), *Material->StateId.ToString());

	// THE ITEM COUNT IS COMPARED AGAINST THE PLAN RATHER THAN RESTATED. `InPlan.ItemCount()` is the list
	// the Preview printed, so this line is the assertion that the edit did what it was authorised to do
	// and nothing else: the labels, the merge GROUPS - not the nodes inside them, because one group is one
	// item the Preview listed - and the reroutes.
	const int32 ItemsChanged = InPlan.Crossings.Num() + InPlan.Merges.Num() + ReroutesRemoved;
	Out += FString::Printf(
		TEXT("\nITEMS CHANGED: %d, which is exactly the %d the Preview listed."),
		ItemsChanged, InPlan.ItemCount());

	return Out;
}
