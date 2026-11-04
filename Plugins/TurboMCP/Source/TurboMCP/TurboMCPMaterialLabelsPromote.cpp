
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

		TSet<const UMaterialExpression*> DistinctReaders;
		for (const FConsumer& Consumer : Consumers)
		{
			DistinctReaders.Add(Consumer.Reader);
		}
		Promotion.DistinctReaders = DistinctReaders.Num();

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

    // Reserve conservative node envelopes before mutation. Existing positions never change.
    struct FOccupied { double X, Y, W, H; };
    TArray<FOccupied> Occupied;
    for (UMaterialExpression* Expression : Read.Expressions)
        if (Expression) Occupied.Add({double(Expression->MaterialExpressionEditorX), double(Expression->MaterialExpressionEditorY),
            double(FMath::Max(240, FTurboMCPMaterial::CaptionOf(Expression).Len() * 9 + 60)), 220.0});
    Occupied.Add({double(Read.Material->EditorX), double(Read.Material->EditorY), 260.0, 800.0});
    auto Reserve = [&](double X, double Y, double Width, FIntPoint& Result) -> bool
    {
        for (int32 Attempt = 0; Attempt < 4096; ++Attempt, Y += 128.0)
        {
            if (X < MIN_int32 || X > MAX_int32 - Width || Y < MIN_int32 || Y > MAX_int32 - 128.0) return false;
            bool bOverlap = false;
            for (const FOccupied& Box : Occupied)
                if (X < Box.X + Box.W + 24 && X + Width + 24 > Box.X && Y < Box.Y + Box.H + 24 && Y + 124 > Box.Y)
                { bOverlap = true; break; }
            if (!bOverlap)
            {
                Result = FIntPoint(int32(X), int32(Y));
                Occupied.Add({X, Y, Width, 100.0});
                return true;
            }
        }
        return false;
    };
    for (FPromotion& Promotion : OutPlan.Promotions)
    {
        const double Width = FMath::Max(180, Promotion.ProposedName.Len() * 9 + 60);
        if (!Promotion.bAlreadyDeclared && !Reserve(Promotion.Source->MaterialExpressionEditorX,
            double(Promotion.Source->MaterialExpressionEditorY) + 244, Width, Promotion.DeclarationPosition))
        { OutError = TEXT("No safe space found for a new label. Reorganize the material and preview again. Nothing changed."); return false; }
        for (const FConsumer& Consumer : Promotion.Consumers)
        {
            if (Promotion.UsagePositions.Contains(Consumer.ReaderKey)) continue;
            FIntPoint Position;
            if (!Reserve(double(Consumer.Reader->MaterialExpressionEditorX) - Width - 48,
                Consumer.Reader->MaterialExpressionEditorY, Width, Position))
            { OutError = TEXT("No safe space found for a label usage. Reorganize the material and preview again. Nothing changed."); return false; }
            Promotion.UsagePositions.Add(Consumer.ReaderKey, Position);
        }
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
			UMaterialExpression* Created = UMaterialEditingLibrary::CreateMaterialExpressionEx(
				Material, /*MaterialFunction=*/nullptr,
				UMaterialExpressionNamedRerouteDeclaration::StaticClass(),
				/*SelectedAsset=*/nullptr,
				Promotion.DeclarationPosition.X,
				Promotion.DeclarationPosition.Y,
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

		if (!Promotion.bAlreadyDeclared)
		{
			Declaration->Input = Promotion.SourceConnection;
			Declaration->Input.InputName = NAME_None;
		}

		if (!Promotion.bAlreadyDeclared) PlannedDeclarations.Add(FTurboMCPMaterial::KeyOf(Declaration));

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
					Promotion.UsagePositions[Consumer.ReaderKey].X,
					Promotion.UsagePositions[Consumer.ReaderKey].Y,
					/*bAllowMarkingPackageDirty=*/true);

				Usage = Cast<UMaterialExpressionNamedRerouteUsage>(CreatedUsage);
				if (Usage == nullptr)
				{
					FTurboMCPRollback::EndAndUndoActiveTransaction();
					return FString::Printf(
						TEXT("FAILED: the usage node for @%s could not be created, so the whole edit was ")
						TEXT("submitted for undo; restoration was not verified. Inspect the material before retrying or saving."), *Promotion.ProposedName);
				}

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

    for (const FString& Key : Planned)
    {
        if (!After.Contains(Key))
        {
            FTurboMCPRollback::EndAndUndoActiveTransaction();
            return TEXT("FAILED: a planned label node is missing after the edit. Undo was requested; restoration was not verified. Inspect before retrying or saving.");
        }
    }
    if (InputsRetargeted != InPlan.InputsToRetarget || UsagesCreated != InPlan.UsagesToAdd)
    {
        FTurboMCPRollback::EndAndUndoActiveTransaction();
        return TEXT("FAILED: not all planned label connections were created. Undo was requested; restoration was not verified. Inspect before retrying or saving.");
    }

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
		TEXT("%d label node(s) gained and all of them planned, and the engine reports %d error(s), none ")
		TEXT("of them new. StateId %s -> %s (reported rather than asserted: it can change for reasons a ")
		TEXT("label promotion does not control)."),
		InPlan.KeysBefore.Num() - Lost.Num(), InPlan.KeysBefore.Num(), Planned.Num(),
		CompileErrorsAfter.Num(),
		*InPlan.StateIdBefore.ToString(), *Material->StateId.ToString());

	return Out;
}
