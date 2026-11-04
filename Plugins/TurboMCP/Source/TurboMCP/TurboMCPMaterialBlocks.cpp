// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "TurboMCPMaterialBlocks.h"

#include "TurboMCPRollback.h"
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

		UMaterialExpression* Source = nullptr;
		FExpressionInput SourceConnection;
		FExpressionInput DirectConnection;
	};

    // Follow plain reroutes as the compiler does, retaining the complete final input.
    // A cycle has no usable source and must not become a cleanup candidate.
    FExpressionInput TracePlainInput(FExpressionInput Input)
    {
        TSet<UMaterialExpression*> Visited;
        while (auto* Reroute = Cast<UMaterialExpressionReroute>(Input.Expression))
        {
            if (Visited.Contains(Reroute)) return FExpressionInput();
            Visited.Add(Reroute);
            Input = Reroute->Input;
        }
        return Input;
    }

    FString ConnectionKey(const FExpressionInput& Input)
    {
        return FString::Printf(TEXT("%s:%d:%d:%d:%d:%d:%d"),
            *FTurboMCPMaterial::KeyOf(Input.Expression), Input.OutputIndex,
            Input.Mask, Input.MaskR, Input.MaskG, Input.MaskB, Input.MaskA);
    }

    FString ConnectionSnapshot(FTurboMCPMaterial::FPlan& Read)
    {
        TArray<FString> Entries;
        for (UMaterialExpression* Expression : Read.Expressions)
        {
            if (!Expression) continue;
            const FString Key = FTurboMCPMaterial::KeyOf(Expression);
            Entries.Add(FString::Printf(TEXT("%s:%s:%d:%d"), *Key,
                *Expression->GetClass()->GetName(), Expression->MaterialExpressionEditorX,
                Expression->MaterialExpressionEditorY));
            for (FExpressionInputIterator It{ Expression }; It; ++It)
                if (It.Input) Entries.Add(FString::Printf(TEXT("%s#%d=%s"), *Key, It.Index, *ConnectionKey(*It.Input)));
            if (const auto* Usage = Cast<UMaterialExpressionNamedRerouteUsage>(Expression))
                Entries.Add(Key + TEXT("@") + Usage->DeclarationGuid.ToString());
        }
        if (Read.Material)
        {
            for (int32 Index = 0; Index < MP_MAX; ++Index)
                if (FExpressionInput* Input = Read.Material->GetExpressionInputForProperty(static_cast<EMaterialProperty>(Index)))
                    Entries.Add(FString::Printf(TEXT("output%d=%s"), Index, *ConnectionKey(*Input)));
            for (const UMaterialExpressionComment* Box : Read.Material->GetExpressionCollection().EditorComments)
                if (Box) Entries.Add(FString::Printf(TEXT("box:%d:%d:%d:%d:%s"),
                    Box->MaterialExpressionEditorX, Box->MaterialExpressionEditorY, Box->SizeX, Box->SizeY, *Box->Text));
        }
        Entries.Sort();
        return FString::Join(Entries, TEXT("\n"));
    }

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
				Link.DirectConnection = *Input;
				Link.SourceConnection = TracePlainInput(*Input);
				Link.Source = Link.SourceConnection.Expression;
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

			OutUsages.FindOrAdd(Label).Add(FTurboMCPMaterial::KeyOf(Usage), Usage);
		}
	}
}

bool FTurboMCPMaterialBlocks::Plan(const FString& InAsset, FPlan& OutPlan, FString& OutError)
{
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
	OutPlan.ConnectionSnapshot = ConnectionSnapshot(Read);

    if (OutPlan.Function != nullptr)
    {
        OutError = TEXT("Material Blocks cleanup supports materials, not material functions. Use ReadTurboMCPMaterialBlocks to inspect this function; no changes were made.");
        return false;
    }

	if (OutPlan.Material == nullptr && OutPlan.Function == nullptr)
	{
		OutError = OutPlan.ParentPath.IsEmpty()
			? FString(TEXT("this asset has no material graph to arrange into blocks"))
			: FString::Printf(TEXT("this is a Material Instance, whose graph lives in its parent %s. ")
				TEXT("Change blocks on the parent instead, because editing through an instance would change ")
				TEXT("an asset the caller did not name."), *OutPlan.ParentPath);
		return false;
	}

	// Planning only inspects the loaded graph. Compilation belongs to Apply, not preview.

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

	TSet<FString> DeclaredSourceKeys;
	for (const TPair<FString, UMaterialExpressionNamedRerouteDeclaration*>& Pair : Read.DeclarationsByLabel)
	{
		const UMaterialExpressionNamedRerouteDeclaration* Declaration = Pair.Value;
		if (Declaration != nullptr && Declaration->Input.Expression != nullptr)
		{
			DeclaredSourceKeys.Add(ConnectionKey(TracePlainInput(Declaration->Input)));
		}
	}

	TMap<FString, TArray<const FLink*>> LinksBySource;
	for (const FLink& Link : Links)
	{
		if (Link.Source == nullptr)
		{
			continue;
		}

		if (IsDeclaration(Link.Reader) || IsUsage(Link.Reader))
		{
			continue;
		}

		LinksBySource.FindOrAdd(ConnectionKey(Link.SourceConnection)).Add(&Link);
	}

    TArray<FString> Sources;
    LinksBySource.GetKeys(Sources);
    Sources.Sort();
    for (const FString& ValueKey : Sources)
    {
        if (DeclaredSourceKeys.Contains(ValueKey)) continue;
        const TArray<const FLink*>& SourceLinks = LinksBySource[ValueKey];
        const UMaterialExpression* Source = SourceLinks[0]->Source;
        const FString SourceKey = FTurboMCPMaterial::KeyOf(Source);

		TSet<int32> Blocks;
		for (const FLink* Link : SourceLinks)
		{
			const int32 Block = BlockOf(Read, Link->Reader);
			if (Block != INDEX_NONE)
			{
				Blocks.Add(Block);
			}
		}

		if (Blocks.Num() < 2)
		{
			continue;
		}

		FCrossing Crossing;
		Crossing.Source = const_cast<UMaterialExpression*>(Source);
		Crossing.SourceKey = SourceKey;
		Crossing.SourceConnection = SourceLinks[0]->SourceConnection;
		Crossing.SourceCaption = FTurboMCPMaterial::CaptionOf(Source);
		Crossing.ProposedName = NameFromCaption(Source->HasAParameterName() ? Source->GetParameterName().ToString() : Crossing.SourceCaption);
        const auto& Outputs = Crossing.Source->GetOutputs();
        if (Outputs.IsValidIndex(Crossing.SourceConnection.OutputIndex))
        {
            const FName OutputName = Outputs[Crossing.SourceConnection.OutputIndex].OutputName;
            Crossing.ProposedName += TEXT("_") + (OutputName.IsNone()
                ? FString::FromInt(Crossing.SourceConnection.OutputIndex) : NameFromCaption(OutputName.ToString()));
        }

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
			Crossing.ExpectedConsumers.Add(ConsumerRef(Link->ReaderKey, Link->InputIndex), Link->DirectConnection);
		}

		OutPlan.Crossings.Add(MoveTemp(Crossing));
	}

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

            for (const FString& Output : FTurboMCPMaterial::ConnectedOutputs(Read))
            {
                if (Removed.Contains(FTurboMCPMaterial::KeyOf(
                    FTurboMCPMaterial::OutputSource(Read, Output, false))))
                    Merge.OutputProperties.Add(Output);
            }
			OutPlan.Merges.Add(MoveTemp(Merge));
		}
	}

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

    TSet<UMaterialExpression*> MaterialOutputSources;
    for (const FString& Output : FTurboMCPMaterial::ConnectedOutputs(Read))
        MaterialOutputSources.Add(FTurboMCPMaterial::OutputSource(Read, Output, false));

	for (UMaterialExpression* Reroute : Reroutes)
	{
        // Material outputs are outside comment blocks. Keep their routing nodes: removing
        // one would disconnect an output that is not in the in-block consumer list.
        if (MaterialOutputSources.Contains(Reroute)) continue;

		const TArray<const FLink*>* Readers = ReadersByDirect.Find(Reroute);
		if (Readers == nullptr || Readers->Num() == 0)
		{
			continue;
		}

		const int32 RerouteBlock = BlockOf(Read, Reroute);
		if (RerouteBlock == INDEX_NONE)
		{
			continue;
		}

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
		Removal.RerouteClass = Reroute->GetClass()->GetName();
		Removal.Source = Source;
		Removal.SourceConnection = TracePlainInput(AsReroute->Input);
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
		// material that already follows the block rules, and it is a different answer from a failed read.
		OutPlan.Refusals.Add(FString::Printf(
			TEXT("%s already follows the block rules across its %d block(s): no value crosses a block without a ")
			TEXT("label, no label is read through two usage nodes in one block, and no plain reroute stands ")
			TEXT("for a value inside a single block. %d expression(s) were examined, %d of them in no block."),
			*OutPlan.Asset, OutPlan.BlockCount, OutPlan.KeysBefore.Num(), OutPlan.UnboxedExpressions));
	}

	return true;
}

FString FTurboMCPMaterialBlocks::RenderPreview(const FPlan& InPlan)
{
	FString Out = FString::Printf(TEXT("preview: blocks in %s"), *InPlan.Asset);
	Out += TEXT("\n; Preview recompiles materials for baseline errors and may run editor callbacks; it does not apply graph changes or save.");
	Out += FString::Printf(TEXT("\nblocks: %d comment box(es), %d expression(s), %d of them in no block"),
		InPlan.BlockCount, InPlan.KeysBefore.Num(), InPlan.UnboxedExpressions);
	Out += FString::Printf(TEXT("\nitems the edit would change: %d"), InPlan.ItemCount());

	if (!InPlan.bHasGraphNodes)
	{
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
		Out += TEXT("\n\nLABELS TO ADD - a value read from more than one block, to route through ");
		Out += TEXT("a named reroute:");
		for (const FCrossing& Crossing : InPlan.Crossings)
		{
			Out += FString::Printf(TEXT("\n  + @%s  <- %s output %d mask %d:%d%d%d%d (%s)"),
				*Crossing.ProposedName, *Crossing.SourceKey, Crossing.SourceConnection.OutputIndex,
                Crossing.SourceConnection.Mask, Crossing.SourceConnection.MaskR, Crossing.SourceConnection.MaskG,
                Crossing.SourceConnection.MaskB, Crossing.SourceConnection.MaskA, *Crossing.SourceCaption);
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
		Out += TEXT("block:");
		for (const FMerge& Merge : InPlan.Merges)
		{
			Out += FString::Printf(TEXT("\n  = @%s in block %d: keep %s, remove %d node(s)"),
				*Merge.Label, Merge.BlockIndex, *Merge.KeepKey, Merge.RemoveKeys.Num());
            for (const FString& Output : Merge.OutputProperties)
                Out += FString::Printf(TEXT("\n      retarget material output %s"), *Output);
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
		Out += TEXT("block:");
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
		TEXT("the listed graph elements; expression identities and compile errors are checked afterwards. ")
		TEXT("Failure requests undo; full restoration is not verified."),
		InPlan.Crossings.Num(), InPlan.Merges.Num(), InPlan.InBlockReroutes.Num());

	return Out;
}

namespace
{
	
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

	struct FRollbackMeasurement
	{
		FRollbackMeasurement(const FTurboMCPMaterialBlocks::FPlan& InPlan, FString* InOutNote,
			bool bInMaterialRegistered, const bool& bInRolledBack)
			: Plan(InPlan), OutNote(InOutNote),
			  bMaterialRegistered(bInMaterialRegistered), bRolledBack(bInRolledBack)
		{
		}

		~FRollbackMeasurement()
		{
			// NOTHING TO DO ON A SUCCESSFUL PASS: the material is then MEANT to hold the new state.
			if (OutNote == nullptr || !bRolledBack)
			{
				return;
			}

			FTurboMCPRollback::EndAndUndoActiveTransaction();

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
					TEXT("\nROLLBACK IDENTITY CHECK: the material holds the %d expression(s) the plan read. ")
					TEXT("Links, values, layout and other state were not verified; inspect before saving or retrying."), Plan.KeysBefore.Num());
				return;
			}

			int32 RawLength = 0;
			int32 RawLive = 0;
			int32 RawHoldsMissing = 0;
			if (Plan.Material != nullptr)
			{
				const auto& RawExpressions = Plan.Material->GetExpressionCollection().Expressions;
				RawLength = RawExpressions.Num();
				for (const UMaterialExpression* Expression : RawExpressions)
				{
					if (Expression == nullptr)
					{
						continue;
					}
					++RawLive;
					if (Missing.Num() > 0 && FTurboMCPMaterial::KeyOf(Expression) == Missing[0])
					{
						++RawHoldsMissing;
					}
				}
			}

			*OutNote = FString::Printf(
				TEXT("\n*** THE ROLLBACK DID NOT RESTORE THE MATERIAL: %d of the %d expression(s) the plan ")
				TEXT("read are gone, and %d are present that it did not read. THIS IS A DEFECT IN THIS TOOL ")
				TEXT("AND NOT A CLEAN ROLLBACK. First named: %s")
				TEXT("\n    THE RAW COLLECTION IS %d ENTRIES LONG, OF WHICH %d ARE LIVE, AND IT %s THE FIRST ")
				TEXT("MISSING KEY. %s")
				TEXT("\n    AND THE MATERIAL'S OWN `Modify()` ANSWERED %s BEFORE ANY MUTATION, so the ")
				TEXT("collection was %sTHE TRANSACTION'S BUFFER."),
				Missing.Num(), Plan.KeysBefore.Num(), Extra.Num(),
				Missing.Num() > 0 ? *Missing[0] : *Extra[0],
				RawLength, RawLive, RawHoldsMissing > 0 ? TEXT("holds") : TEXT("does not hold"),
				RawHoldsMissing > 0
					? TEXT("So the array came back WITH the node, and something is hiding it from the reader - look for state that is not a serialized property.")
					: (RawLength > RawLive
						? TEXT("So the array came back and the slot is EMPTY: the node was removed from the collection AND destroyed, and a destroyed object is not something a property snapshot can put back.")
						: TEXT("So the array itself did not come back: the removal was never in the transaction's buffer.")),
				bMaterialRegistered ? TEXT("YES") : TEXT("NO"),
				bMaterialRegistered ? TEXT("") : TEXT("NEVER IN "));
		}

		const FTurboMCPMaterialBlocks::FPlan& Plan;
		FString* OutNote;
		bool bMaterialRegistered;
		/** SET BY THE FAILING PATH BEFORE IT RETURNS. The transaction is no longer the signal: nothing cancels
		 * it any more, because `Cancel()` is what made the "rollback" a no-op. */
		const bool& bRolledBack;
	};
}

FString FTurboMCPMaterialBlocks::Apply(const FPlan& InPlan, FString& OutRollbackNote)
{
	OutRollbackNote.Empty();

	bool bOutRolledBack = false;

    FTurboMCPMaterial::FPlan Current;
    FString CurrentError;
    if (!FTurboMCPMaterial::Plan(InPlan.Asset, FTurboMCPMaterial::ERead::Blocks, FString(), false, Current, CurrentError))
        return FString::Printf(TEXT("error: cannot recheck the material before editing: %s"), *CurrentError);
    if (ConnectionSnapshot(Current) != InPlan.ConnectionSnapshot)
        return TEXT("error: material connections or layout changed after planning. Nothing was edited. Request a new preview.");

	if (InPlan.bNothingToChange)
	{
		return FString::Printf(
			TEXT("unchanged: %s already follows the block rules across its %d block(s), so there is nothing to change. ")
			TEXT("%d expression(s) were examined."),
			*InPlan.Asset, InPlan.BlockCount, InPlan.KeysBefore.Num());
	}

	if (InPlan.Material == nullptr)
	{
		return FString::Printf(TEXT("error: %s has no material to change blocks in"), *InPlan.Asset);
	}

	// Capture compile diagnostics only for an edit that has work to apply.
	// Compilation can invoke editor callbacks; preview and no-op edits must avoid it.
	const FGuid StateIdBefore = InPlan.Material->StateId;
	const TArray<FString> CompileErrorsBefore =
		UMaterialEditingLibrary::RecompileMaterial(InPlan.Material);

	// Group graph changes in one transaction; rollback checks expression identities only.
	FScopedTransaction Transaction(NSLOCTEXT("TurboMCP", "MaterialBlocks",
		"TurboMCP: make labels follow blocks"));

	UMaterial* const Material = InPlan.Material;

	const bool bMaterialRegistered = Material->Modify();

	FRollbackMeasurement Rollback(InPlan, &OutRollbackNote, bMaterialRegistered, bOutRolledBack);

	// THE MATERIAL IS RE-READ BEFORE ANYTHING IS TOUCHED, which is what makes this an edit of the CURRENT
	// graph rather than of the one the plan was built from.
	FTurboMCPMaterial::FPlan Read;
	FString ReadError;
	if (!FTurboMCPMaterial::Plan(InPlan.Asset, FTurboMCPMaterial::ERead::Blocks, FString(),
		/*bInIncludeInstanceOverrides=*/false, Read, ReadError))
	{
		bOutRolledBack = true;
		return FString::Printf(
			TEXT("FAILED: the material could not be re-read at apply time (%s), so the edit was NOT ")
			TEXT("applied rather than applied against a stale plan."), *ReadError);
	}

	TMap<FString, UMaterialExpression*> ByKey;
	IndexByKey(Read, ByKey);

    // Reserve space for added nodes without moving existing expressions. Headless
    // materials have no measured Slate geometry, so use conservative envelopes.
    struct FOccupiedNode { int32 X, Y, Width, Height; };
    TArray<FOccupiedNode> Occupied;
    for (const UMaterialExpression* Expression : Read.Expressions)
        if (Expression) Occupied.Add({Expression->MaterialExpressionEditorX,
            Expression->MaterialExpressionEditorY, 260, 200});
    auto ReservePosition = [&](int32 X, int32 Y, int32 Width) -> FIntPoint
    {
        bool bCollision;
        do
        {
            bCollision = false;
            for (const FOccupiedNode& Node : Occupied)
            {
                if (X < Node.X + Node.Width + 24 && X + Width + 24 > Node.X &&
                    Y < Node.Y + Node.Height + 24 && Y + 100 + 24 > Node.Y)
                {
                    Y = Node.Y + Node.Height + 24;
                    bCollision = true;
                    break;
                }
            }
        } while (bCollision);
        Occupied.Add({X, Y, Width, 100});
        return FIntPoint(X, Y);
    };

	int32 LabelsAdded = 0;
	int32 UsagesCreated = 0;
	int32 UsagesRemoved = 0;
	int32 ReroutesRemoved = 0;
	int32 InputsRetargeted = 0;

	TArray<FString> PlannedAdds;
	TArray<FString> PlannedRemovals;

	for (const FCrossing& Crossing : InPlan.Crossings)
	{
		UMaterialExpression* const Source = ByKey.FindRef(Crossing.SourceKey);
		if (Source == nullptr)
		{
			bOutRolledBack = true;
			return FString::Printf(
				TEXT("FAILED: the value %s this plan would name no longer exists, so the edit was submitted for undo. Inspect the rollback check below before retrying or saving. ")
				TEXT(""), *Crossing.SourceKey);
		}

        const int32 LabelWidth = FMath::Max(180, Crossing.ProposedName.Len() * 8 + 48);
        const FIntPoint DeclarationPosition = ReservePosition(Source->MaterialExpressionEditorX,
            Source->MaterialExpressionEditorY + 240, LabelWidth);
		UMaterialExpression* const Created = UMaterialEditingLibrary::CreateMaterialExpressionEx(
			Material, /*MaterialFunction=*/nullptr,
			UMaterialExpressionNamedRerouteDeclaration::StaticClass(),
			/*SelectedAsset=*/nullptr,
			DeclarationPosition.X,
			DeclarationPosition.Y,
			/*bAllowMarkingPackageDirty=*/true);

		UMaterialExpressionNamedRerouteDeclaration* const Declaration =
			Cast<UMaterialExpressionNamedRerouteDeclaration>(Created);
		if (Declaration == nullptr)
		{
			bOutRolledBack = true;
			return FString::Printf(
				TEXT("FAILED: the declaration for @%s could not be created, so the whole edit was submitted for undo. Inspect the rollback check below before retrying or saving. ")
				TEXT(""), *Crossing.ProposedName);
		}
		++LabelsAdded;

		if (Declaration->GetEditableName() != Crossing.ProposedName)
		{
			Declaration->SetEditableName(Crossing.ProposedName);
		}

		// THE DECLARATION READS THE VALUE IT NAMES. Without this the node forwards nothing, so every
		// reader retargeted to it would compile to an unconnected input - a silently wrong material.
		Declaration->Input = Crossing.SourceConnection;
		Declaration->Input.Expression = Source;
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
				bOutRolledBack = true;
				return FString::Printf(
					TEXT("FAILED: the plan carried a consumer reference this pass cannot parse (%s), so the ")
					TEXT("edit was submitted for undo. Inspect the rollback check below before retrying or saving. ")
					TEXT("The reference could not be resolved."), *Ref);
			}

			UMaterialExpression* const Reader = ByKey.FindRef(ReaderKey);
			if (Reader == nullptr)
			{
				bOutRolledBack = true;
				return FString::Printf(
					TEXT("FAILED: the reader %s this plan would retarget no longer exists, so the edit was ")
					TEXT("submitted for undo. Inspect the rollback check below before retrying or saving."), *ReaderKey);
			}

			const int32 ReaderBlock = BlockOf(Read, Reader);
            const FString UsageGroup = ReaderBlock == INDEX_NONE ? ReaderKey : FString::FromInt(ReaderBlock);
            UMaterialExpressionNamedRerouteUsage* Usage = UsageByReaderKey.FindRef(UsageGroup);
			if (Usage == nullptr)
			{
                const FIntPoint UsagePosition = ReservePosition(Reader->MaterialExpressionEditorX - LabelWidth - 40,
                    Reader->MaterialExpressionEditorY, LabelWidth);
				UMaterialExpression* const CreatedUsage = UMaterialEditingLibrary::CreateMaterialExpressionEx(
					Material, /*MaterialFunction=*/nullptr,
					UMaterialExpressionNamedRerouteUsage::StaticClass(),
					/*SelectedAsset=*/nullptr,
					UsagePosition.X,
					UsagePosition.Y,
					/*bAllowMarkingPackageDirty=*/true);

				Usage = Cast<UMaterialExpressionNamedRerouteUsage>(CreatedUsage);
				if (Usage == nullptr)
				{
					bOutRolledBack = true;
					return FString::Printf(
						TEXT("FAILED: the usage node for @%s could not be created, so the whole edit was ")
						TEXT("submitted for undo. Inspect the rollback check below before retrying or saving."), *Crossing.ProposedName);
				}

				// BOTH THE POINTER AND THE GUID, because the reader prefers the pointer and falls back to
				// the GUID for a freshly loaded asset whose fixup has not run - `LabelOfUsage` records it.
				Usage->Declaration = Declaration;
				Usage->DeclarationGuid = Declaration->VariableGuid;
				UsageByReaderKey.Add(UsageGroup, Usage);
				PlannedAdds.Add(FTurboMCPMaterial::KeyOf(Usage));
				++UsagesCreated;
			}

			// THE EXPECTED OLD VALUE IS THE SOURCE, because a crossing is measured on the RESOLVED source.
			if (!RetargetInput(Reader, InputIndex, Crossing.ExpectedConsumers.FindChecked(Ref).Expression, Usage))
			{
				bOutRolledBack = true;
				return FString::Printf(
					TEXT("FAILED: %s no longer points at %s, so the material changed between the Preview ")
					TEXT("and this call and the edit was submitted for undo. Inspect the rollback check below before retrying or saving. ")
					TEXT("The connection changed after planning."), *Ref, *Crossing.SourceKey);
			}
            for (FExpressionInputIterator It{ Reader }; It; ++It)
            {
                if (It.Index == InputIndex && It.Input != nullptr)
                    It.Input->SetMask(0, 0, 0, 0, 0);
            }
			++InputsRetargeted;
		}
	}

	for (const FMerge& Merge : InPlan.Merges)
	{
		UMaterialExpression* const Keep = ByKey.FindRef(Merge.KeepKey);
		if (Keep == nullptr)
		{
			bOutRolledBack = true;
			return FString::Printf(
				TEXT("FAILED: the usage node to keep (%s) for @%s no longer exists, so the edit was submitted for undo. Inspect the rollback check below before retrying or saving. ")
				TEXT(""), *Merge.KeepKey, *Merge.Label);
		}

		for (const FString& Ref : Merge.ConsumerRefs)
		{
			FString ReaderKey;
			int32 InputIndex = INDEX_NONE;
			if (!ParseConsumerRef(Ref, ReaderKey, InputIndex))
			{
				bOutRolledBack = true;
				return FString::Printf(
					TEXT("FAILED: the plan carried a consumer reference this pass cannot parse (%s)."), *Ref);
			}

			UMaterialExpression* const Reader = ByKey.FindRef(ReaderKey);
			if (Reader == nullptr)
			{
				bOutRolledBack = true;
				return FString::Printf(
					TEXT("FAILED: the reader %s this merge would retarget no longer exists, so the edit was ")
					TEXT("submitted for undo. Inspect the rollback check below before retrying or saving."), *ReaderKey);
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
				bOutRolledBack = true;
				return FString::Printf(
					TEXT("FAILED: %s could not be moved onto the kept usage node for @%s, so the edit was ")
					TEXT("submitted for undo. Inspect the rollback check below before retrying or saving."), *Ref, *Merge.Label);
			}
			++InputsRetargeted;
		}

        for (const FString& Output : Merge.OutputProperties)
        {
            const int64 Property = StaticEnum<EMaterialProperty>()->GetValueByNameString(Output);
            FExpressionInput* Input = Material->GetExpressionInputForProperty(static_cast<EMaterialProperty>(Property));
            if (Input == nullptr || !Merge.RemoveKeys.Contains(FTurboMCPMaterial::KeyOf(Input->Expression)))
            {
                bOutRolledBack = true;
                return FString::Printf(TEXT("error: material output %s changed after planning; undo requested. See rollback details."), *Output);
            }
            Input->Expression = Keep;
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

    TSet<FString> RerouteRemovalKeys;
    for (const FInBlockReroute& Item : InPlan.InBlockReroutes)
        RerouteRemovalKeys.Add(Item.RerouteKey);
	for (const FInBlockReroute& Removal : InPlan.InBlockReroutes)
	{
		UMaterialExpression* const Reroute = ByKey.FindRef(Removal.RerouteKey);
		UMaterialExpression* const Source = ByKey.FindRef(Removal.SourceKey);
		if (Reroute == nullptr || Source == nullptr)
		{
			bOutRolledBack = true;
			return FString::Printf(
				TEXT("FAILED: the in-block reroute %s or the value it passes (%s) no longer exists, so the ")
				TEXT("edit was submitted for undo. Inspect the rollback check below before retrying or saving."),
				*Removal.RerouteKey, *Removal.SourceKey);
		}

		for (const FString& Ref : Removal.ConsumerRefs)
		{
			FString ReaderKey;
			int32 InputIndex = INDEX_NONE;
			if (!ParseConsumerRef(Ref, ReaderKey, InputIndex))
			{
				bOutRolledBack = true;
				return FString::Printf(
					TEXT("FAILED: the plan carried a consumer reference this pass cannot parse (%s)."), *Ref);
			}

            // A consumer scheduled for removal needs no rewiring. Its surviving
            // consumers use their own final source snapshots, independent of key order.
            if (RerouteRemovalKeys.Contains(ReaderKey)) continue;
			UMaterialExpression* const Reader = ByKey.FindRef(ReaderKey);
			if (Reader == nullptr)
			{
				bOutRolledBack = true;
				return FString::Printf(
					TEXT("FAILED: the reader %s this removal would retarget no longer exists, so the edit ")
					TEXT("was submitted for undo. Inspect the rollback check below before retrying or saving."), *ReaderKey);
			}

			// THE REROUTE MAY FAR INTO A CHAIN, but the readers point at THIS one, so that is what the
			// input is expected to hold.
			if (!RetargetInput(Reader, InputIndex, Reroute, Source))
			{
				bOutRolledBack = true;
				return FString::Printf(
					TEXT("FAILED: %s no longer points at the reroute %s, so the edit was submitted for undo. Inspect the rollback check below before retrying or saving. ")
					TEXT(""), *Ref, *Removal.RerouteKey);
			}
            for (FExpressionInputIterator It{ Reader }; It; ++It)
            {
                if (It.Index == InputIndex && It.Input != nullptr)
                {
                    const FName InputName = It.Input->InputName;
                    *It.Input = Removal.SourceConnection;
                    It.Input->Expression = Source;
                    It.Input->InputName = InputName;
                }
            }
			++InputsRetargeted;
		}

		PlannedRemovals.Add(Removal.RerouteKey);
		UMaterialEditingLibrary::DeleteMaterialExpression(Material, Reroute);
		++ReroutesRemoved;
	}

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
		bOutRolledBack = true;
		return FString::Printf(
			TEXT("FAILED: the material could not be read back after the edit (%s), so the compiled result ")
			TEXT("could NOT be verified and the edit was submitted for undo; see the rollback check below."), *ReadError);
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
	TSet<FString> const ErrorsBefore(CompileErrorsBefore);
	TArray<FString> NewErrors;
	for (const FString& Error : CompileErrorsAfter)
	{
		if (!ErrorsBefore.Contains(Error))
		{
			NewErrors.Add(Error);
		}
	}

    // Verify both directions: planned additions must exist and planned removals must be absent.
    for (const FString& Key : PlannedGains)
    {
        if (!After.Contains(Key))
        {
            bOutRolledBack = true;
            return FString::Printf(TEXT("error: planned new expression %s is missing. Undo requested; see rollback details."), *Key);
        }
    }
    for (const FString& Key : PlannedLosses)
    {
        if (After.Contains(Key))
        {
            bOutRolledBack = true;
            return FString::Printf(TEXT("error: expression %s was not removed as planned. Undo requested; see rollback details."), *Key);
        }
    }

	if (NewErrors.Num() > 0)
	{
		bOutRolledBack = true;
		return FString::Printf(
			TEXT("FAILED: making labels follow blocks introduced %d compile error(s) that were not there ")
			TEXT("before, so the edit was submitted for undo; see the rollback check below. A named reroute forwards to its input and a plain ")
			TEXT("reroute passes its input along, so neither can legitimately change what a material ")
			TEXT("computes. First: %s"), NewErrors.Num(), *NewErrors[0]);
	}

	if (Lost.Num() > 0)
	{
		bOutRolledBack = true;
		return FString::Printf(
			TEXT("FAILED: the edit LOST %d expression(s) it did not list, so it was submitted for undo; see the rollback check below. This pass ")
			TEXT("lists every removal by key; a node gone that was not on that list means it removed more ")
			TEXT("than the Preview authorised. First lost: %s"), Lost.Num(), *Lost[0]);
	}

	if (GainedUnexpected.Num() > 0)
	{
		bOutRolledBack = true;
		return FString::Printf(
			TEXT("FAILED: the edit GAINED %d expression(s) it did not plan, so it was submitted for undo; see the rollback check below. First ")
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
		TEXT("\nCHECK RESULTS (not proof of identical rendered output): %d of %d pre-existing expression(s) still present, %d removed and all of ")
		TEXT("them on the plan's own list, %d gained and all of them planned, and the engine reports %d ")
		TEXT("error(s), none of them new. StateId %s -> %s (reported rather than asserted: it can change for ")
		TEXT("reasons this pass does not control)."),
		InPlan.KeysBefore.Num() - PlannedRemovals.Num(), InPlan.KeysBefore.Num(), PlannedRemovals.Num(),
		GainedUnexpected.Num() == 0 ? PlannedGains.Num() : 0,
		CompileErrorsAfter.Num(),
		*StateIdBefore.ToString(), *Material->StateId.ToString());

	const int32 ItemsChanged = InPlan.Crossings.Num() + InPlan.Merges.Num() + ReroutesRemoved;
	Out += FString::Printf(
		TEXT("\nITEMS CHANGED: %d, which is exactly the %d the Preview listed."),
		ItemsChanged, InPlan.ItemCount());

	return Out;
}
