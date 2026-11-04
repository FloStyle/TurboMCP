// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "TurboMCPMaterialLabelsPromote.h"

#include "TurboMCPRollback.h"
#include "Logging.h"
#include "Public/TurboMCPMaterial.h"

#include "Containers/UnrealString.h"
#include "Editor/UnrealEd/Public/Editor.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionComment.h"
#include "Materials/MaterialExpressionNamedReroute.h"
#include "Materials/MaterialFunctionInterface.h"
#include "MaterialEditingLibrary.h"
#include "Misc/Guid.h"
#include "ScopedTransaction.h"
#include "UObject/Package.h"

namespace
{
    // An expression can supply several different values. Never merge its output channels.
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
            if (const auto* Declaration = Cast<UMaterialExpressionNamedRerouteDeclaration>(Expression))
                Entries.Add(Key + TEXT("name=") + Declaration->GetEditableName());
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

	/** True when an expression is a label declaration, so this pass never promotes a label itself. */
	bool IsDeclaration(const UMaterialExpression* InExpression)
	{
		FString Ignored;
		return FTurboMCPMaterial::IsLabelDeclaration(InExpression, Ignored);
	}

	/** True when an expression is a label usage, which forwards a value this pass should leave alone. */
	bool IsUsage(const UMaterialExpression* InExpression)
	{
		FString Ignored;
		return FTurboMCPMaterial::IsLabelUsage(InExpression, Ignored);
	}

	/**
	 * Turn a caption into something usable as a label name.
	 *
	 * THE UNIQUENESS OF THE RESULT IS NOT DECIDED HERE. `UMaterialExpressionNamedRerouteDeclaration`
	 * makes its own name unique through `MakeNameUnique`
	 * (`Runtime/Engine/Private/Materials/MaterialExpressions.cpp:18344`), which is the engine's rule and
	 * the only one that cannot drift from it. This function only makes a caption SAFE TO OFFER.
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

		// A name that begins with a digit is legal for an `FName` but reads as a number, and a caption
		// that is entirely punctuation would produce only underscores, so both are given a prefix.
		if (Out.IsEmpty() || FChar::IsDigit(Out[0]))
		{
			Out = FString(TEXT("Value_")) + Out;
		}

		return Out;
	}

	/**
	 * The input's own name where the engine exposes one.
	 *
	 * AN INDEX IS NOT SOMETHING A CALLER CAN ACT ON in the material editor, so the name is preferred and
	 * the index is kept as the fallback. There is no single accessor for this - a material expression
	 * declares its input names in its own `GetInputName` - so the lookup is attempted and its absence is
	 * allowed rather than being an error.
	 */
	FString NameOfInput(const UMaterialExpression* InOwner, int32 InInputIndex)
	{
		if (InOwner == nullptr)
		{
			return FString();
		}

		FName Found = InOwner->GetInputName(InInputIndex);
		return Found.IsNone() ? FString() : Found.ToString();
	}
}

bool FTurboMCPMaterialLabelsPromote::Plan(const FString& InAsset, FPlan& OutPlan, FString& OutError)
{
	// THE READER DOES THE RESOLUTION, DELIBERATELY. `FTurboMCPMaterial::Plan` refuses a non-material
	// asset, names the alternative where it can, and gathers the expressions and the existing labels.
	// A second resolver here would be the second-dialect mistake in a new place.
	FTurboMCPMaterial::FPlan Read;
	if (!FTurboMCPMaterial::Plan(InAsset, FTurboMCPMaterial::ERead::Labels, FString(),
		/*bInIncludeInstanceOverrides=*/false, Read, OutError))
	{
		return false;
	}

	OutPlan.ConnectionSnapshot = ConnectionSnapshot(Read);
	OutPlan.Asset = Read.Asset;
	OutPlan.AssetName = Read.AssetName;
	OutPlan.Material = Read.Material;
	OutPlan.Function = Read.Function;
	OutPlan.ParentPath = Read.ParentPath;
	OutPlan.ExistingLabels = Read.DeclarationsByLabel.Num();

	// ---------------------------------------------------------------------------------------------
	// A REFUSAL RATHER THAN AN EMPTY ANSWER. An instance has no graph, so promoting inside it would
	// add the declaration to its PARENT - an edit the caller did not ask for, on an asset they did not
	// name. `RenderLabels` already reports that an instance has no labels; here that becomes a refusal,
	// because a writer must not proceed on something a reader merely reports.
	// ---------------------------------------------------------------------------------------------
	if (OutPlan.Material == nullptr && OutPlan.Function == nullptr)
	{
		OutError = OutPlan.ParentPath.IsEmpty()
			? FString(TEXT("this asset has no material graph to add a label to"))
			: FString::Printf(TEXT("this is a Material Instance, whose graph lives in its parent %s. ")
				TEXT("Promote labels on the parent instead, because adding one through an instance would ")
				TEXT("edit an asset the caller did not name."), *OutPlan.ParentPath);
		return false;
	}

	if (OutPlan.Function != nullptr)
	{
		OutError = TEXT("Material functions are not supported by Label Promote. Pass a Material asset.");
		return false;
	}
	OutPlan.StateIdBefore = OutPlan.Material->StateId;

	// ---------------------------------------------------------------------------------------------
	// THE BASELINE KEY SET, AND THE TWO THINGS IT IS USED FOR.
	//
	// It is the comparison that proves nothing was lost, AND IT IS ALSO THE GUARD AGAINST THE EMPTY
	// BASELINE. An empty baseline makes every subset check pass for the wrong reason, and this project
	// has already paid for that lesson once: a suite whose output name was wrong produced an empty key
	// set and seven checks passed or failed for reasons that had nothing to do with the tool. So the
	// COUNT IS CARRIED and the plan refuses to claim a compiled-result proof over an empty set.
	//
	// Label declarations and usages are collected as keys too, because a promotion must not silently
	// rewrite an existing label's wiring.
	// ---------------------------------------------------------------------------------------------
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
		// NOT AN ERROR, BUT NOT A SILENT SUCCESS EITHER: a material with no expressions cannot have its
		// compiled result compared, and saying so is the difference between "nothing to promote" and
		// "nothing to compare".
		OutError = FString::Printf(TEXT("%s reports no expressions, so a compiled-result comparison ")
			TEXT("would compare two empty sets and pass for the wrong reason"), *OutPlan.Asset);
		return false;
	}

	// ---------------------------------------------------------------------------------------------
	// WHICH VALUES ARE ALREADY NAMED, KEYED BY THE VALUE RATHER THAN BY THE LABEL.
	//
	// A DECLARATION IS FOUND BY WHAT IT FORWARDS. The engine may have made its name unique, so a lookup
	// by the name this pass would propose can miss the declaration this pass already made - and a second
	// run would then add a duplicate label for a value that already has one. The value's own key is the
	// identity that cannot drift.
	//
	// A DECLARATION THAT FORWARDS NOTHING IS SKIPPED: it names no value, so there is nothing for it to
	// make a no-op.
	// ---------------------------------------------------------------------------------------------
	TMap<FString, UMaterialExpressionNamedRerouteDeclaration*> DeclarationsBySource;
	for (const TPair<FString, UMaterialExpressionNamedRerouteDeclaration*>& Pair : Read.DeclarationsByLabel)
	{
		UMaterialExpressionNamedRerouteDeclaration* Declaration = Pair.Value;
		if (Declaration == nullptr || Declaration->Input.Expression == nullptr)
		{
			continue;
		}
		DeclarationsBySource.Add(ConnectionKey(Declaration->Input), Declaration);
	}
	OutPlan.DeclarationBySourceKey = DeclarationsBySource;

	// ---------------------------------------------------------------------------------------------
	// WHICH VALUES ARE WORTH A NAME.
	//
	// THE RULE IS THE SPEC'S OWN: "used far away or in several boxes". A value read in exactly one place
	// is not shared, so a label would add a node and a name and save nothing. A value read by
	// `MinimumConsumers` distinct expressions IS shared.
	//
	// THE COLUMN IS THE DEPTH FROM THE OUTPUT SOURCE, computed here by the same walk the output roots
	// are found with, so "in several boxes" is measured against the structure rather than guessed from
	// geometry - a material expression stores no size and its position is whatever the author dragged.
	// ---------------------------------------------------------------------------------------------
	TMap<FString, TArray<FConsumer>> ConsumersBySource;
	TMap<FString, FExpressionInput> Connections;

	for (UMaterialExpression* Expression : Read.Expressions)
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


			// A LABEL IS NOT PROMOTED AND NOT COUNTED AS A READER OF ITSELF. A declaration already
			// names the value it forwards, and a usage forwards a name - so counting either would let a
			// material that is already labelled propose a SECOND label for the same value.
			if (IsDeclaration(Expression) || IsUsage(Expression) || IsDeclaration(Input->Expression) || IsUsage(Input->Expression))
			{
				continue;
			}

			FConsumer Consumer;
			Consumer.Reader = Expression;
			Consumer.ReaderKey = FTurboMCPMaterial::KeyOf(Expression);
			Consumer.InputIndex = It.Index;
			Consumer.InputName = NameOfInput(Expression, It.Index);
			Consumer.ReaderColumn = 0;

			const FString Key = ConnectionKey(*Input);
			Connections.Add(Key, *Input);
			ConsumersBySource.FindOrAdd(Key).Add(MoveTemp(Consumer));
		}
	}

	// Every value that anything reads is examined, and the count is reported so a Preview can say how
	// many were considered and rejected rather than only what survived.
	TArray<FString> Sources;
	ConsumersBySource.GetKeys(Sources);
	Sources.Sort();

	OutPlan.ValuesExamined = Sources.Num();

	for (const FString& SourceKey : Sources)
	{
		UMaterialExpression* Source = Connections[SourceKey].Expression;
		const TArray<FConsumer>& Consumers = ConsumersBySource[SourceKey];

		FPromotion Promotion;
		Promotion.Source = Source;
		Promotion.SourceConnection = Connections[SourceKey];
		Promotion.SourceKey = FTurboMCPMaterial::KeyOf(Source);
		Promotion.SourceCaption = FTurboMCPMaterial::CaptionOf(Source);
		Promotion.Consumers = Consumers;

		// DISTINCT READERS, NOT INPUTS. One expression may read the same value into two of its own
		// inputs - a `Lerp` whose A and B are both the same constant - and that is ONE place the value
		// is used, not two. Counting inputs would promote exactly those values, which is the opposite of
		// what the spec asks for.
		TSet<const UMaterialExpression*> DistinctReaders;
		TSet<int32> DistinctColumns;
		for (const FConsumer& Consumer : Consumers)
		{
			DistinctReaders.Add(Consumer.Reader);
			DistinctColumns.Add(Consumer.ReaderColumn);
		}
		Promotion.DistinctReaders = DistinctReaders.Num();
		Promotion.DistinctReaderColumns = DistinctColumns.Num();

		if (Promotion.DistinctReaders < MinimumConsumers)
		{
			continue;
		}

		FString Name = Source->HasAParameterName() ? Source->GetParameterName().ToString() : Promotion.SourceCaption;
		const auto& Outputs = Source->GetOutputs();
		const int32 OutputIndex = Promotion.SourceConnection.OutputIndex;
		if (Outputs.IsValidIndex(OutputIndex) && !Outputs[OutputIndex].OutputName.IsNone())
			Name += TEXT("_") + Outputs[OutputIndex].OutputName.ToString();
		else
			Name += FString::Printf(TEXT("_Output%d"), OutputIndex);
		Promotion.ProposedName = NameFromCaption(Name);

		// A VALUE ALREADY DECLARED IS NOT PROPOSED AGAIN, AND IT IS FOUND BY ITS KEY RATHER THAN BY THE
		// NAME THIS PASS WOULD PROPOSE. The engine makes names unique, so a second run looking itself up
		// by name would not find what it made and would add a duplicate declaration for a value that
		// already has one - which is the failure mode the reader would then report as two labels for one
		// value.
		if (UMaterialExpressionNamedRerouteDeclaration* const* Existing =
				DeclarationsBySource.Find(SourceKey))
		{
			Promotion.bAlreadyDeclared = true;
			Promotion.ExistingDeclaration = *Existing;
			Promotion.ProposedName = (*Existing)->GetEditableName();
		}

		OutPlan.InputsToRetarget += Promotion.Consumers.Num();
		// ONE USAGE PER DISTINCT READER, which is the engine's convention and what makes the label
		// report a reader count at all. It is counted here rather than derived later so the Preview and
		// the Edit state the same number.
		OutPlan.UsagesToAdd += Promotion.DistinctReaders;
		OutPlan.Promotions.Add(MoveTemp(Promotion));
	}

	OutPlan.bNothingToPromote = OutPlan.Promotions.Num() == 0;


	if (OutPlan.Promotions.Num() == 0)
	{
		// `P1`: never return nothing, and say WHY. This is the answer the capability exists to give on a
		// material that is already named, and it is a different answer from a failed read.
		OutPlan.Refusals.Add(FString::Printf(
			TEXT("no value is read by %d or more distinct expressions, so no label is worth adding on %s. ")
			TEXT("%d value(s) were examined and every one of them is read in at most one place."),
			MinimumConsumers, *OutPlan.Asset, OutPlan.ValuesExamined));
	}

	return true;
}

FString FTurboMCPMaterialLabelsPromote::RenderPreview(const FPlan& InPlan)
{
	FString Out = FString::Printf(TEXT("preview: labels worth adding to %s"), *InPlan.Asset);
	Out += TEXT("\n; Preview loads the material but does not compile, add labels, rewire inputs or save.");
	Out += FString::Printf(
		TEXT("\nvalues examined: %d, worth a label: %d, inputs that would be retargeted: %d"),
		InPlan.ValuesExamined, InPlan.Promotions.Num(), InPlan.InputsToRetarget);
	Out += FString::Printf(TEXT("\nlabels already declared: %d"), InPlan.ExistingLabels);

	if (!InPlan.bHasGraphNodes)
	{
		// REPORTED, NOT ASSUMED, AND IT IS THE FACT THAT DECIDES HOW THE EDIT IS MADE. The engine's own
		// editor converts a reroute to a named reroute through `MakeLinkTo` on graph pins; this asset
		// has none, so the edit goes where the compiler reads it instead.
		Out += TEXT("\n; no expression carries a graph node, so this is a headlessly built material and ")
			TEXT("the readers are retargeted on the expression inputs, which is where the compiler reads.");
	}

	if (InPlan.Promotions.Num() == 0)
	{
		for (const FString& Refusal : InPlan.Refusals)
		{
			Out += FString::Printf(TEXT("\n%s"), *Refusal);
		}
		return Out;
	}

	// THE PROPOSED NAMES ARE THE POINT OF THE PREVIEW, so they are printed with the context they came
	// from and with the places that would read them - a preview that listed values without names would
	// not have done the job the spec describes.
	for (const FPromotion& Promotion : InPlan.Promotions)
	{
		Out += FString::Printf(TEXT("\n\nLABEL @%s"), *Promotion.ProposedName);
		Out += FString::Printf(TEXT("\n    value: %s  (%s)"),
			*Promotion.SourceKey, *Promotion.SourceCaption);
		Out += FString::Printf(TEXT("\n    read by %d distinct expression(s); source output %d"),
			Promotion.DistinctReaders, Promotion.SourceConnection.OutputIndex);
		if (Promotion.bAlreadyDeclared)
		{
			Out += TEXT("\n    ALREADY DECLARED: this name exists, so the edit reuses it rather than ")
				TEXT("adding a second declaration for the same value.");
		}
		for (const FConsumer& Consumer : Promotion.Consumers)
		{
			Out += FString::Printf(TEXT("\n    - %s.%s"), *Consumer.ReaderKey,
				Consumer.InputName.IsEmpty()
					? *FString::Printf(TEXT("input %d"), Consumer.InputIndex)
					: *Consumer.InputName);
		}
	}

	int32 NewDeclarations = 0;
	for (const FPromotion& Promotion : InPlan.Promotions)
		if (!Promotion.bAlreadyDeclared) ++NewDeclarations;
	Out += FString::Printf(
		TEXT("\n\nthis edit ADDS %d declaration(s) and REWIRES %d input(s). It removes nothing, and the ")
		TEXT("expression identities and compile errors are checked afterwards; failed checks request undo without proving restoration."),
		NewDeclarations, InPlan.InputsToRetarget);

	return Out;
}

FString FTurboMCPMaterialLabelsPromote::Apply(const FPlan& InPlan)
{
	if (InPlan.bNothingToPromote)
	{
		return FString::Printf(
			TEXT("unchanged: %s has no value read in %d or more places, so there is no label worth adding. ")
			TEXT("%d value(s) were examined."),
			*InPlan.Asset, MinimumConsumers, InPlan.ValuesExamined);
	}

	if (InPlan.Material == nullptr)
	{
		return FString::Printf(TEXT("error: %s has no material to write a label into"), *InPlan.Asset);
	}

	FTurboMCPMaterial::FPlan Current;
	FString CurrentError;
	if (!FTurboMCPMaterial::Plan(InPlan.Asset, FTurboMCPMaterial::ERead::Labels,
		FString(), false, Current, CurrentError) || Current.Material != InPlan.Material ||
		ConnectionSnapshot(Current) != InPlan.ConnectionSnapshot)
	{
		return TEXT("error: the material wiring, labels or layout changed after planning. Preview again. Nothing was changed by this call.");
	}

	// Compile only for an actual edit. Preview and unchanged calls never compile.
	const TArray<FString> CompileErrorsBefore = UMaterialEditingLibrary::RecompileMaterial(InPlan.Material);

	// Group edits in one transaction; failure paths request undo without verifying restoration.
	FScopedTransaction Transaction(NSLOCTEXT("TurboMCP", "MaterialLabelsPromote",
		"TurboMCP: promote values to labels"));

	UMaterial* Material = InPlan.Material;

	int32 DeclarationsAdded = 0;
	int32 DeclarationsReused = 0;
	int32 InputsRetargeted = 0;
	// COUNTED SO THE REPORT CANNOT CLAIM A LABEL NOBODY READS. The usage nodes are what the reader
	// counts as "places this label is read", so a promotion whose usage count came back zero would be a
	// label that names a value and is read nowhere.
	int32 UsagesCreated = 0;

	TArray<FString> PlannedDeclarations;

	for (const FPromotion& Promotion : InPlan.Promotions)
	{
		if (Promotion.Source == nullptr)
		{
			continue;
		}

		UMaterialExpressionNamedRerouteDeclaration* Declaration = Promotion.ExistingDeclaration;

		if (Declaration == nullptr)
		{
			// THE DECLARATION IS CREATED THROUGH THE ENGINE'S OWN ENTRY POINT, which is what the fixture
			// commandlet uses for every expression it builds. Reaching for `NewObject` directly would
			// skip whatever bookkeeping that entry point does, and it is the one place this project has
			// already measured a trap: `CreateMaterialExpressionEx` calls `AddExpression`, while a
			// COMMENT belongs in a different array.
			UMaterialExpression* Created = UMaterialEditingLibrary::CreateMaterialExpressionEx(
				Material, /*MaterialFunction=*/nullptr,
				UMaterialExpressionNamedRerouteDeclaration::StaticClass(),
				/*SelectedAsset=*/nullptr,
				Promotion.Source->MaterialExpressionEditorX,
				Promotion.Source->MaterialExpressionEditorY + 120,
				/*bAllowMarkingPackageDirty=*/true);

			Declaration = Cast<UMaterialExpressionNamedRerouteDeclaration>(Created);
			if (Declaration == nullptr)
			{
				FTurboMCPRollback::EndAndUndoActiveTransaction();
				return FString::Printf(
					TEXT("FAILED: the declaration for @%s could not be created, so the whole edit was ")
					TEXT("submitted for undo; restoration was not verified. Inspect the material before retrying or saving."), *Promotion.ProposedName);
			}
			++DeclarationsAdded;
		}
		else
		{
			++DeclarationsReused;
		}

		Declaration->Modify();

		// A REUSED DECLARATION KEEPS THE NAME IT HAS, AND THAT IS A CORRECTION RATHER THAN A CONVENIENCE.
		// The engine makes names unique, so an earlier run may have produced `Value_1` rather than the
		// `Value` proposed then. RENAMING IT NOW WOULD MOVE THE LABEL OUT FROM UNDER EVERY NAME THE
		// AUTHOR HAS ALREADY READ - a caller asking for the same promotion twice would get a different
		// label name the second time, and any note or comment referring to the first name would stop
		// matching. A LABEL IS A PROPERTY OF THE VALUE, so the name it already carries is the right one.
		if (!Promotion.bAlreadyDeclared)
		{
			// THE NAME IS SET THROUGH THE ENGINE'S OWN SETTER, which is the accessor the material
			// editor's rename path uses and therefore the one that keeps the node's own bookkeeping in
			// step.
			if (Declaration->GetEditableName() != Promotion.ProposedName)
			{
				Declaration->SetEditableName(Promotion.ProposedName);
			}
		}

		// THE DECLARATION READS THE VALUE IT NAMES. Without this the node forwards nothing, so every
		// reader retargeted to it would compile to an unconnected input - a silently wrong material, and
		// the exact thing the compiled-result check would then have to catch rather than it never
		// happening.
		if (!Promotion.bAlreadyDeclared)
		{
			Declaration->Input = Promotion.SourceConnection;
			Declaration->Input.InputName = NAME_None;
		}

		PlannedDeclarations.Add(FTurboMCPMaterial::KeyOf(Declaration));

		// THE REWIRING. *** EACH READER GETS ITS OWN USAGE NODE, AND THAT IS THE ENGINE'S CONVENTION
		// RATHER THAN A PREFERENCE. *** POINTING EVERY READER'S INPUT STRAIGHT AT THE DECLARATION COMPILES,
		// PRESERVES THE VALUE, AND LEAVES `R-MAT-LABELS` ANSWERING `read in 0 place(s)` ABOUT THE LABEL IT
		// HAS JUST CREATED - because A LABEL IS READ THROUGH `UMaterialExpressionNamedRerouteUsage`, and
		// the reader counts those. That makes a label nothing reads and reports success for it. The
		// engine's own editor creates ONE USAGE
		// PER READER (`Editor/MaterialEditor/Private/MaterialEditor.cpp:4633-4643`), and this does the
		// same: the usage carries the label, and the reader's input points at the usage.
		//
		// ONE USAGE PER DISTINCT READER EXPRESSION rather than per input, so an expression that reads the
		// same value into two of its own inputs gets ONE usage node that both inputs share - which is
		// what a reader count of "places this label is read" means.
		TMap<const UMaterialExpression*, UMaterialExpressionNamedRerouteUsage*> UsageByReader;

		for (const FConsumer& Consumer : Promotion.Consumers)
		{
			if (Consumer.Reader == nullptr)
			{
				continue;
			}

			UMaterialExpressionNamedRerouteUsage* Usage = UsageByReader.FindRef(Consumer.Reader);
			if (Usage == nullptr)
			{
				UMaterialExpression* CreatedUsage = UMaterialEditingLibrary::CreateMaterialExpressionEx(
					Material, /*MaterialFunction=*/nullptr,
					UMaterialExpressionNamedRerouteUsage::StaticClass(),
					/*SelectedAsset=*/nullptr,
					Consumer.Reader->MaterialExpressionEditorX - 150,
					Consumer.Reader->MaterialExpressionEditorY,
					/*bAllowMarkingPackageDirty=*/true);

				Usage = Cast<UMaterialExpressionNamedRerouteUsage>(CreatedUsage);
				if (Usage == nullptr)
				{
					FTurboMCPRollback::EndAndUndoActiveTransaction();
					return FString::Printf(
						TEXT("FAILED: the usage node for @%s could not be created, so the whole edit was ")
						TEXT("submitted for undo; restoration was not verified. Inspect the material before retrying or saving."), *Promotion.ProposedName);
				}

				// THE USAGE POINTS AT THE DECLARATION, which is what makes it read the label rather
				// than forward nothing. Both the pointer and the GUID are set, because the reader
				// prefers the pointer and falls back to the GUID for a freshly loaded asset whose fixup
				// has not run - `LabelOfUsage` records exactly that.
				Usage->Declaration = Declaration;
				Usage->DeclarationGuid = Declaration->VariableGuid;
				UsageByReader.Add(Consumer.Reader, Usage);
				PlannedDeclarations.Add(FTurboMCPMaterial::KeyOf(Usage));
				++UsagesCreated;
			}

			// RE-READ THE INPUT RATHER THAN TRUSTING THE INDEX FROM PLAN TIME. An earlier promotion in
			// this same loop may have changed this reader's inputs, and writing through a stale pointer
			// would retarget the wrong pin - which is a wrong material that still compiles.
			for (FExpressionInputIterator It{ Consumer.Reader }; It; ++It)
			{
				FExpressionInput* Input = It.Input;
				if (Input == nullptr || ConnectionKey(*Input) != ConnectionKey(Promotion.SourceConnection))
				{
					continue;
				}
				if (It.Index != Consumer.InputIndex)
				{
					continue;
				}

				Consumer.Reader->Modify();
				Input->Expression = Usage;
				Input->OutputIndex = 0;
				// The declaration already applies the original mask. Forward its full result.
				Input->Mask = Input->MaskR = Input->MaskG = Input->MaskB = Input->MaskA = 0;
				++InputsRetargeted;
				break;
			}
		}
	}

	// ---------------------------------------------------------------------------------------------
	// THE COMPILED-RESULT CHECK. "The material's compiled result must be the same" IS THE REQUIREMENT
	// AND IT IS MEASURED HERE.
	//
	// THE ASYMMETRY IS DELIBERATE. `FTurboMCPMaterialTidy` asserts the expression set is IDENTICAL
	// because a layout cannot legitimately add an expression. THIS PASS LEGITIMATELY ADDS DECLARATIONS,
	// so the assertion is that NO PRE-EXISTING EXPRESSION WAS LOST and that the expressions GAINED are
	// EXACTLY THE DECLARATIONS THIS PASS PLANNED. A one-directional check would miss a dropped
	// expression; a bare "the sets differ" check would fail every time it worked correctly.
	// ---------------------------------------------------------------------------------------------
	Material->PostEditChange();

	TArray<FString> CompileErrorsAfter = UMaterialEditingLibrary::RecompileMaterial(Material);

	FString ReadError;
	FTurboMCPMaterial::FPlan ReadAfter;
	TArray<FString> KeysAfter;
	const bool bReadBack = FTurboMCPMaterial::Plan(InPlan.Asset, FTurboMCPMaterial::ERead::Labels,
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
		FTurboMCPRollback::EndAndUndoActiveTransaction();
		return FString::Printf(
			TEXT("FAILED: the material could not be read back after the edit (%s), so the compiled result ")
			TEXT("could NOT be verified and undo was requested; restoration was not verified. Inspect the material before retrying or saving."), *ReadError);
	}

	TSet<FString> Before(InPlan.KeysBefore);
	TSet<FString> After(KeysAfter);
	TSet<FString> Planned(PlannedDeclarations);

	TArray<FString> Lost;
	for (const FString& Key : InPlan.KeysBefore)
	{
		if (!After.Contains(Key))
		{
			Lost.Add(Key);
		}
	}

	TArray<FString> GainedUnexpected;
	for (const FString& Key : KeysAfter)
	{
		if (!Before.Contains(Key) && !Planned.Contains(Key))
		{
			GainedUnexpected.Add(Key);
		}
	}

	// A NEW ERROR IS THE FAILURE SIGNAL, not any error. A material that ALREADY had a compiler error
	// before this pass must not make this pass fail - that would be blaming the edit for something it
	// found.
	TSet<FString> ErrorsBefore(CompileErrorsBefore);
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
		FTurboMCPRollback::EndAndUndoActiveTransaction();
		return FString::Printf(
			TEXT("FAILED: promoting labels introduced %d compile error(s) that were not there before, so ")
			TEXT("undo was requested; restoration was not verified. Inspect the material before retrying or saving. A declaration forwards to its input, so it cannot legitimately ")
			TEXT("change what a material computes. First: %s"), NewErrors.Num(), *NewErrors[0]);
	}

	if (Lost.Num() > 0)
	{
		FTurboMCPRollback::EndAndUndoActiveTransaction();
		return FString::Printf(
			TEXT("FAILED: the edit LOST %d expression(s), so undo was requested; restoration was not verified. Inspect the material before retrying or saving. This pass adds a ")
			TEXT("declaration and re-points inputs at it; it never removes anything. First lost: %s"),
			Lost.Num(), *Lost[0]);
	}

	if (GainedUnexpected.Num() > 0)
	{
		FTurboMCPRollback::EndAndUndoActiveTransaction();
		return FString::Printf(
			TEXT("FAILED: the edit GAINED %d expression(s) it did not plan, so undo was requested; restoration was not verified. Inspect the material before retrying or saving. The ")
			TEXT("only expressions this pass may add are the declarations it lists. First unexpected: %s"),
			GainedUnexpected.Num(), *GainedUnexpected[0]);
	}

	FString Out = FString::Printf(TEXT("changed: promoted labels in %s"), *InPlan.Asset);
	Out += FString::Printf(
		TEXT("\n%d declaration(s) added, %d reused, %d usage node(s), %d input(s) retargeted"),
		DeclarationsAdded, DeclarationsReused, UsagesCreated, InputsRetargeted);

	for (const FPromotion& Promotion : InPlan.Promotions)
	{
		Out += FString::Printf(TEXT("\n@%s <- %s (%d reader(s))"),
			*Promotion.ProposedName, *Promotion.SourceKey, Promotion.DistinctReaders);
	}

	// THE COMPILED-RESULT EVIDENCE, STATED WITH ITS OWN LIMITS RATHER THAN AS A BARE CLAIM.
	Out += FString::Printf(
		TEXT("\nCHECK RESULTS (not proof of identical rendered output): %d of %d pre-existing expression(s) still present with none lost, ")
		TEXT("%d declaration(s) gained and all of them planned, and the engine reports %d error(s), none ")
		TEXT("of them new. StateId %s -> %s (reported rather than asserted: it can change for reasons a ")
		TEXT("label promotion does not control)."),
		InPlan.KeysBefore.Num() - Lost.Num(), InPlan.KeysBefore.Num(), Planned.Num(),
		CompileErrorsAfter.Num(),
		*InPlan.StateIdBefore.ToString(), *Material->StateId.ToString());

	return Out;
}
