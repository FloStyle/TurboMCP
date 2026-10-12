// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "TurboMCPMaterialInstance.h"

#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstance.h"
#include "Materials/MaterialInstanceConstant.h"
// EMaterialParameterType, FMaterialParameterInfo and FMaterialParameterMetadata all live here; the metadata struct
// was at MaterialParameters.h:446 and the enum at :187 WHEN THIS WAS WRITTEN.
#include "Materials/MaterialParameters.h"
#include "Materials/Material.h"
#include "UObject/UObjectGlobals.h"

#include "TurboMCPAssetResolver.h"

namespace
{
	/**
	 * A parameter association, as a word, so the text is stable.
	 *
	 * LOCAL RATHER THAN SHARED: TurboMCPMaterial.cpp has its own AssociationName, IT IS IN AN ANONYMOUS NAMESPACE
	 * THERE, and this workstream is not permitted to edit that file - so making it public is a change for the
	 * orchestration to decide, not one to make by reaching into a file other capabilities own. The function is
	 * eleven lines and its OUTPUT IS ASSERTED BY A SUITE, so a drift between two copies would be a CHECK FAILURE
	 * rather than a silent divergence.
	 */
	FString AssociationName(EMaterialParameterAssociation InAssociation)
	{
		switch (InAssociation)
		{
		case EMaterialParameterAssociation::GlobalParameter: return TEXT("global");
		case EMaterialParameterAssociation::LayerParameter:  return TEXT("layer");
		case EMaterialParameterAssociation::BlendParameter:  return TEXT("blend");
		default:                                            return TEXT("unknown");
		}
	}

	/** A metadata value as text, BY TYPE, because a scalar formatted as a colour would be a lie. */
	FString ValueOf(const FMaterialParameterMetadata& InMetadata)
	{
		switch (InMetadata.Value.Type)
		{
		case EMaterialParameterType::Scalar:
			return FString::Printf(TEXT("%g"), InMetadata.Value.AsScalar());

		case EMaterialParameterType::Vector:
		{
			// AsVector DOES NOT EXIST - measured: MaterialParameters.h:377 declares AsVector4d for DoubleVector
			// only, and the Vector storage is Float[4]. Reading the four floats directly is the honest route rather
			// than inventing an accessor name, which is how nine recalled engine names have been wrong here.
			return FString::Printf(TEXT("(%g, %g, %g, %g)"),
				InMetadata.Value.Float[0], InMetadata.Value.Float[1],
				InMetadata.Value.Float[2], InMetadata.Value.Float[3]);
		}

		case EMaterialParameterType::DoubleVector:
		{
			const FVector4d Value = InMetadata.Value.AsVector4d();
			return FString::Printf(TEXT("(%g, %g, %g, %g)"), Value.X, Value.Y, Value.Z, Value.W);
		}

		case EMaterialParameterType::Texture:
		{
			// AsTextureObject is the declared accessor (MaterialParameters.h:381).
			const UObject* Texture = InMetadata.Value.AsTextureObject();
			return Texture != nullptr ? Texture->GetPathName() : TEXT("(none)");
		}

		case EMaterialParameterType::StaticSwitch:
			return InMetadata.Value.Bool[0] ? TEXT("true") : TEXT("false");

		default:
			return FString::Printf(TEXT("(%s)"), *MaterialParameterTypeToString(InMetadata.Value.Type));
		}
	}

	FString TypeOf(const FMaterialParameterMetadata& InMetadata)
	{
		return MaterialParameterTypeToString(InMetadata.Value.Type);
	}

	/** The four types this reader enumerates on every level. See CoveredTypes. */
	const TArray<FString>& CoveredTypeNames()
	{
		static const TArray<FString> Names = { TEXT("scalar"), TEXT("vector"), TEXT("texture"), TEXT("static switch") };
		return Names;
	}
}

const TArray<FString>& FTurboMCPMaterialInstance::CoveredTypes()
{
	return CoveredTypeNames();
}

bool FTurboMCPMaterialInstance::Read(const FString& InAsset, FResult& OutResult, FString& OutError)
{
	OutResult = FResult();
	OutError.Reset();

	if (InAsset.IsEmpty())
	{
		OutError = TEXT("error: no asset given. Pass a Material Instance Constant, for example MI_TurboMCPFixture.");
		return false;
	}

	FTurboMCPAssetResolver::FResolved Resolved;
	const FString ResolveError = FTurboMCPAssetResolver::Resolve(InAsset, Resolved);
	if (!ResolveError.IsEmpty())
	{
		OutError = ResolveError;
		return false;
	}

	UObject* Loaded = LoadObject<UObject>(nullptr, *Resolved.ObjectPath);
	if (Loaded == nullptr)
	{
		OutError = FString::Printf(TEXT("error: %s resolved but did not load."), *Resolved.ObjectPath);
		return false;
	}

	UMaterialInterface* Interface = Cast<UMaterialInterface>(Loaded);
	if (Interface == nullptr)
	{
		OutError = FString::Printf(TEXT("error: %s is a %s, not a material interface. This tool reads a material ")
			TEXT("INSTANCE; read a Material with R-MAT-LOAD."), *Resolved.ObjectPath, *Loaded->GetClass()->GetName());
		return false;
	}

	UMaterialInstance* Instance = Cast<UMaterialInstance>(Interface);
	if (Instance == nullptr)
	{
		// REFUSED RATHER THAN ANSWERED WITH EVERYTHING INHERITED, AND THE DISTINCTION IS THE POINT OF THE TOOL. A
		// base material would make every parameter inherited, which is technically true and USELESS: an agent
		// asking whether a value is on the asset it holds would be told NO about an asset it CAN edit. D-43 refuses
		// the other direction for the same reason.
		OutError = FString::Printf(TEXT("error: %s is a %s, which is NOT a material instance. Every parameter of a ")
			TEXT("base material is editable on the material itself, so the inherited-versus-overridden distinction ")
			TEXT("this tool exists to make does not apply. Use R-MAT-LOAD for the graph."),
			*Resolved.ObjectPath, *Loaded->GetClass()->GetName());
		return false;
	}

	OutResult.Asset = Resolved.ObjectPath;

	// -----------------------------------------------------------------------------------------------
	// THE CHAIN, NEAREST FIRST, WALKED THROUGH Parent UNTIL A BASE MATERIAL.
	//
	// UMaterialInstance::Parent IS A UMaterialInterface (MaterialInstance.h:647), SO A CHAIN CAN PASS THROUGH
	// ANOTHER INSTANCE AND NOT ONLY A BASE MATERIAL - which is why the loop tests for UMaterialInstance AT EVERY
	// STEP rather than assuming the first parent is the end of it.
	// -----------------------------------------------------------------------------------------------
	TArray<UMaterialInterface*> Levels;
	{
		UMaterialInterface* Link = Interface;
		TSet<const UMaterialInterface*> Guard;
		while (Link != nullptr && !Guard.Contains(Link))
		{
			// A CYCLE IS IMPOSSIBLE IN A WELL-FORMED ASSET AND A LOOP FOREVER IS THE FAILURE MODE IF IT IS NOT.
			Guard.Add(Link);
			Levels.Add(Link);

			FLevel Level;
			Level.Path = Link->GetPathName();
			Level.ClassName = Link->GetClass()->GetName();
			Level.bIsTheAssetAskedFor = (Link == Interface);
			OutResult.Chain.Add(Level);

			UMaterialInstance* AsInstance = Cast<UMaterialInstance>(Link);
			Link = AsInstance != nullptr ? AsInstance->Parent : nullptr;
		}
	}

	// -----------------------------------------------------------------------------------------------
	// EVERY PARAMETER THE INSTANCE OVERRIDES, WITH ITS VALUE.
	//
	// READ FROM THE INSTANCE OWN ARRAYS (MaterialInstance.h:773-785) BECAUSE THAT IS WHERE AN OVERRIDE LIVES, and an
	// override is not a resolved value: GetScalarParameterValue would return a value that CAME FROM THE PARENT when
	// the instance does not set one, WHICH IS EXACTLY THE DISTINCTION THIS TOOL MUST NOT BLUR.
	// -----------------------------------------------------------------------------------------------
	TSet<FString> OverriddenNames;

	auto AddOverride = [&OutResult, &OverriddenNames](const FString& InName, EMaterialParameterAssociation InAssociation, const FString& InType, const FString& InValue)
	{
		FParameter Parameter;
		Parameter.Name = InName;
		Parameter.Association = AssociationName(InAssociation);
		Parameter.Type = InType;
		Parameter.Value = InValue;
		OutResult.Overridden.Add(Parameter);
		OverriddenNames.Add(InName);
	};

	for (const FScalarParameterValue& Value : Instance->ScalarParameterValues)
	{
		AddOverride(Value.ParameterInfo.Name.ToString(), Value.ParameterInfo.Association, TEXT("scalar"),
			FString::Printf(TEXT("%g"), Value.ParameterValue));
	}
	for (const FVectorParameterValue& Value : Instance->VectorParameterValues)
	{
		const FLinearColor Colour = Value.ParameterValue;
		AddOverride(Value.ParameterInfo.Name.ToString(), Value.ParameterInfo.Association, TEXT("vector"),
			FString::Printf(TEXT("(%g, %g, %g, %g)"), Colour.R, Colour.G, Colour.B, Colour.A));
	}
	for (const FDoubleVectorParameterValue& Value : Instance->DoubleVectorParameterValues)
	{
		AddOverride(Value.ParameterInfo.Name.ToString(), Value.ParameterInfo.Association, TEXT("double vector"),
			FString::Printf(TEXT("(%g, %g, %g, %g)"),
				Value.ParameterValue.X, Value.ParameterValue.Y, Value.ParameterValue.Z, Value.ParameterValue.W));
	}
	for (const FTextureParameterValue& Value : Instance->TextureParameterValues)
	{
		AddOverride(Value.ParameterInfo.Name.ToString(), Value.ParameterInfo.Association, TEXT("texture"),
			Value.ParameterValue != nullptr ? Value.ParameterValue->GetPathName() : TEXT("(none)"));
	}

	// -----------------------------------------------------------------------------------------------
	// AND THE INVERSE: EVERY PARAMETER THE CHAIN DECLARES THAT THIS INSTANCE DOES NOT OVERRIDE.
	//
	// THE LIST THE SPEC SAYS MATTERS AND THE ONE NOTHING PRINTED BEFORE. It is built by walking the ancestors FROM
	// THE PARENT UP, so a parameter declared by a NEARER level wins over the same name declared further up - THE
	// ORDER THE ENGINE ITSELF RESOLVES IN, and the order that makes DeclaredAt the answer to "which asset would I
	// have to edit".
	// -----------------------------------------------------------------------------------------------
	TSet<FString> DeclaredBySomeAncestor;

	for (int32 LevelIndex = 1; LevelIndex < Levels.Num(); ++LevelIndex)
	{
		UMaterialInterface* Level = Levels[LevelIndex];
		const FString LevelPath = Level->GetPathName();

		TMap<FMaterialParameterInfo, FMaterialParameterMetadata> Parameters;
		for (const EMaterialParameterType Type : { EMaterialParameterType::Scalar, EMaterialParameterType::Vector,
			EMaterialParameterType::Texture, EMaterialParameterType::StaticSwitch })
		{
			// CLEARED PER TYPE AND MERGED, because the engine own signature takes ONE type at a time
			// (MaterialInterface.h:950) and reusing a single map across four calls would be four chances to forget.
			TMap<FMaterialParameterInfo, FMaterialParameterMetadata> ForType;
			Level->GetAllParametersOfType(Type, ForType);
			for (const TPair<FMaterialParameterInfo, FMaterialParameterMetadata>& Pair : ForType)
			{
				Parameters.Add(Pair.Key, Pair.Value);
			}
		}

		for (const TPair<FMaterialParameterInfo, FMaterialParameterMetadata>& Pair : Parameters)
		{
			const FString Name = Pair.Key.Name.ToString();

			DeclaredBySomeAncestor.Add(Name);

			if (OverriddenNames.Contains(Name))
			{
				continue;
			}

			FParameter Parameter;
			Parameter.Name = Name;
			Parameter.Association = AssociationName(Pair.Key.Association);
			Parameter.Type = TypeOf(Pair.Value);
			Parameter.Value = ValueOf(Pair.Value);
			Parameter.DeclaredAt = LevelPath;
			OutResult.Inherited.Add(Parameter);
		}
	}

	// -------------------------------------------------------------------------------------------
	// ORPHANED: SET ON THIS INSTANCE, DECLARED NOWHERE IN THE CHAIN.
	//
	// REPORTED RATHER THAN DROPPED, per the spec own words: "a renamed parameter leaves one behind". An orphaned
	// override still EXISTS on the instance and still does nothing, so dropping it would HIDE A REAL DEFECT IN THE
	// CALLER ASSET.
	// -------------------------------------------------------------------------------------------
	for (const FParameter& Parameter : OutResult.Overridden)
	{
		if (!DeclaredBySomeAncestor.Contains(Parameter.Name))
		{
			OutResult.Orphaned.Add(Parameter);
		}
	}

	return true;
}

FString FTurboMCPMaterialInstance::Render(const FResult& InResult)
{
	FString Out = FString::Printf(TEXT("read: %s"), *InResult.Asset);

	Out += TEXT("\n\n; the parent chain, nearest first, up to the base material");
	for (int32 Index = 0; Index < InResult.Chain.Num(); ++Index)
	{
		// self AND parent, parent xN, RATHER THAN ORDINALS. The first version printed self and up 1, which is true
		// and says nothing about the RELATION - and the relation is the whole point of a chain: a reader has to
		// know that the second level is the asset it would edit.
		FString Label;
		if (Index == 0)
		{
			Label = TEXT("self");
		}
		else if (Index == 1)
		{
			Label = TEXT("parent");
		}
		else
		{
			Label = FString::Printf(TEXT("parent x%d"), Index);
		}

		const bool bIsBaseMaterial = InResult.Chain[Index].ClassName == TEXT("Material");
		Out += FString::Printf(TEXT("\n%-12s %s  [%s%s]"),
			*Label, *InResult.Chain[Index].Path, *InResult.Chain[Index].ClassName,
			bIsBaseMaterial ? TEXT(" - the base material, where a declaration lives") : TEXT(""));
	}

	Out += FString::Printf(TEXT("\n\n; overridden on THIS instance (%d)"), InResult.Overridden.Num());
	if (InResult.Overridden.Num() == 0)
	{
		Out += TEXT("\nnone - this instance overrides nothing, so it shows what its parent shows");
	}
	for (const FParameter& Parameter : InResult.Overridden)
	{
		Out += FString::Printf(TEXT("\n%s = %s  [%s, %s]"),
			*Parameter.Name, *Parameter.Value, *Parameter.Type, *Parameter.Association);
	}

	Out += FString::Printf(TEXT("\n\n; INHERITED - declared by a parent, NOT overridden here (%d)"), InResult.Inherited.Num());
	Out += TEXT("\n; THE VALUE SHOWN IS THE PARENT, AND CHANGING IT HERE IS NOT POSSIBLE. To change one of");
	Out += TEXT("\n; these, edit the asset named in declared at, or set an override on this instance.");
	if (InResult.Inherited.Num() == 0)
	{
		Out += TEXT("\nnone - every parameter the chain declares is overridden here");
	}
	for (const FParameter& Parameter : InResult.Inherited)
	{
		// THE WORD inherited IS ON EVERY LINE RATHER THAN ONLY IN THE HEADING. Measured reason: the first version
		// printed name = value with no per-line marker, AND A LINE THAT READS name = value IS INDISTINGUISHABLE
		// FROM AN OVERRIDE unless the reader still has the heading in mind. THE SPEC SAYS THIS LIST MATTERS BECAUSE
		// AN AGENT MUST TELL WHETHER A VALUE IS REALLY ON THIS INSTANCE - SO THE DISTINCTION HAS TO SURVIVE BEING
		// READ ONE LINE AT A TIME, which is how an agent reads it.
		Out += FString::Printf(TEXT("\n%s = %s  [%s, %s, inherited, declared at %s]"),
			*Parameter.Name, *Parameter.Value, *Parameter.Type, *Parameter.Association, *Parameter.DeclaredAt);
	}

	Out += FString::Printf(TEXT("\n\n; ORPHANED - set here, declared by no parent (%d)"), InResult.Orphaned.Num());
	if (InResult.Orphaned.Num() == 0)
	{
		Out += TEXT("\nnone");
	}
	for (const FParameter& Parameter : InResult.Orphaned)
	{
		Out += FString::Printf(TEXT("\n%s = %s  [%s, %s]"),
			*Parameter.Name, *Parameter.Value, *Parameter.Type, *Parameter.Association);
	}

	// THE DENOMINATOR, ON THE MODEL R-BP-LINT SETTLED: a count with no denominator cannot be told apart from a
	// reader that looked at nothing. IT NAMES THE LEVELS TOO, because naming four types is the denominator for ONE
	// level and a chain of three multiplies it.
	const TArray<FString>& Types = FTurboMCPMaterialInstance::CoveredTypes();
	Out += FString::Printf(TEXT("\n\n; looked at %d level(s) in the chain, each for: %s"),
		FMath::Max(0, InResult.Chain.Num() - 1), *FString::Join(Types, TEXT(", ")));
	Out += TEXT("\n; (the levels looked at are the ANCESTORS - self declares nothing to inherit)");

	return Out;
}
