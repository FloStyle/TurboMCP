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
#include "Materials/MaterialParameters.h"
#include "Materials/Material.h"
#include "StaticParameterSet.h"
#include "UObject/UObjectGlobals.h"

#include "TurboMCPAssetResolver.h"

namespace
{
    FString ParameterIdentity(const FMaterialParameterInfo& Info, const FString& Type)
    {
        return FString::Printf(TEXT("%s|%d|%d|%s"), *Info.Name.ToString().ToLower(),
            static_cast<int32>(Info.Association), Info.Index, *Type.ToLower().Replace(TEXT(" "), TEXT("")));
    }

	/** Stable, human-readable parameter association. */
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

	/** Format each metadata value using its declared type. */
	FString ValueOf(const FMaterialParameterMetadata& InMetadata)
	{
		switch (InMetadata.Value.Type)
		{
		case EMaterialParameterType::Scalar:
			return FString::Printf(TEXT("%g"), InMetadata.Value.AsScalar());

		case EMaterialParameterType::Vector:
		{
			// Vector metadata stores four float components; DoubleVector uses AsVector4d.
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

	/** The parameter types enumerated at each ancestor. See CoveredTypes. */
	const TArray<FString>& CoveredTypeNames()
	{
		static const TArray<FString> Names = { TEXT("scalar"), TEXT("vector"), TEXT("double vector"), TEXT("texture"), TEXT("static switch") };
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
			TEXT("INSTANCE; read a Material with ReadTurboMCPMaterialLoad."), *Resolved.ObjectPath, *Loaded->GetClass()->GetName());
		return false;
	}

	UMaterialInstance* Instance = Cast<UMaterialInstance>(Interface);
	if (Instance == nullptr)
	{
		// A base material has no instance overrides; direct the caller to the graph reader.
		OutError = FString::Printf(TEXT("error: %s is a %s, which is NOT a material instance. Every parameter of a ")
			TEXT("base material is editable on the material itself, so the inherited-versus-overridden distinction ")
			TEXT("this tool exists to make does not apply. Use ReadTurboMCPMaterialLoad for the graph."),
			*Resolved.ObjectPath, *Loaded->GetClass()->GetName());
		return false;
	}

	OutResult.Asset = Resolved.ObjectPath;

	// Follow parent instances to the base material, retaining nearest-first order.
	TArray<UMaterialInterface*> Levels;
	{
		UMaterialInterface* Link = Interface;
		TSet<const UMaterialInterface*> Guard;
		while (Link != nullptr && !Guard.Contains(Link))
		{
			// Guard against malformed cyclic parent chains.
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

	// Read local override arrays rather than resolved values, which can come from a parent.
	TSet<FString> OverriddenNames;

	auto AddOverride = [&OutResult, &OverriddenNames](const FMaterialParameterInfo& Info, const FString& InType, const FString& InValue)
	{
		FParameter Parameter;
		Parameter.Name = Info.Name.ToString();
		Parameter.Association = AssociationName(Info.Association);
		Parameter.Index = Info.Index;
		Parameter.Identity = ParameterIdentity(Info, InType);
		Parameter.Type = InType;
		Parameter.Value = InValue;
		OutResult.Overridden.Add(Parameter);
		OverriddenNames.Add(Parameter.Identity);
	};

	for (const FScalarParameterValue& Value : Instance->ScalarParameterValues)
	{
		AddOverride(Value.ParameterInfo, TEXT("scalar"),
			FString::Printf(TEXT("%g"), Value.ParameterValue));
	}
	for (const FVectorParameterValue& Value : Instance->VectorParameterValues)
	{
		const FLinearColor Colour = Value.ParameterValue;
		AddOverride(Value.ParameterInfo, TEXT("vector"),
			FString::Printf(TEXT("(%g, %g, %g, %g)"), Colour.R, Colour.G, Colour.B, Colour.A));
	}
	for (const FDoubleVectorParameterValue& Value : Instance->DoubleVectorParameterValues)
	{
		AddOverride(Value.ParameterInfo, TEXT("double vector"),
			FString::Printf(TEXT("(%g, %g, %g, %g)"),
				Value.ParameterValue.X, Value.ParameterValue.Y, Value.ParameterValue.Z, Value.ParameterValue.W));
	}
	for (const FTextureParameterValue& Value : Instance->TextureParameterValues)
	{
		AddOverride(Value.ParameterInfo, TEXT("texture"),
			Value.ParameterValue != nullptr ? Value.ParameterValue->GetPathName() : TEXT("(none)"));
	}

	// Read inherited values nearest-first; preserve type, association and layer index.
	const FStaticParameterSet StaticParameters = static_cast<const UMaterialInstance*>(Instance)->GetStaticParameters();
    for (const FStaticSwitchParameter& Value : StaticParameters.StaticSwitchParameters)
    {
        if (Value.bOverride) AddOverride(Value.ParameterInfo, TEXT("static switch"), Value.Value ? TEXT("true") : TEXT("false"));
    }
    TSet<FString> DeclaredBySomeAncestor;

	for (int32 LevelIndex = 1; LevelIndex < Levels.Num(); ++LevelIndex)
	{
		UMaterialInterface* Level = Levels[LevelIndex];
		const FString LevelPath = Level->GetPathName();

		TArray<TPair<FMaterialParameterInfo, FMaterialParameterMetadata>> Parameters;
		for (const EMaterialParameterType Type : { EMaterialParameterType::Scalar, EMaterialParameterType::Vector,
			EMaterialParameterType::DoubleVector, EMaterialParameterType::Texture, EMaterialParameterType::StaticSwitch })
		{

			// Keep each type separate: identical names and layer indices can belong to different types.
			TMap<FMaterialParameterInfo, FMaterialParameterMetadata> ForType;
			Level->GetAllParametersOfType(Type, ForType);
			for (const TPair<FMaterialParameterInfo, FMaterialParameterMetadata>& Pair : ForType)
			{
				Parameters.Emplace(Pair.Key, Pair.Value);
			}
		}

		for (const TPair<FMaterialParameterInfo, FMaterialParameterMetadata>& Pair : Parameters)
		{
			const FString Name = Pair.Key.Name.ToString();

			const FString Identity = ParameterIdentity(Pair.Key, TypeOf(Pair.Value));
            if (DeclaredBySomeAncestor.Contains(Identity)) continue;
            DeclaredBySomeAncestor.Add(Identity);

            if (OverriddenNames.Contains(Identity))
			{
				continue;
			}

			FParameter Parameter;
			Parameter.Name = Name;
			Parameter.Association = AssociationName(Pair.Key.Association);
			Parameter.Index = Pair.Key.Index;
			Parameter.Identity = Identity;
			Parameter.Type = TypeOf(Pair.Value);
			Parameter.Value = ValueOf(Pair.Value);
			Parameter.DeclaredAt = LevelPath;
			OutResult.Inherited.Add(Parameter);
		}
	}

	// Report local overrides that no ancestor declares, including renamed parameters.
	for (const FParameter& Parameter : OutResult.Overridden)
	{
		if (!DeclaredBySomeAncestor.Contains(Parameter.Identity))
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
		// Name each relationship relative to the requested instance.
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
		Out += FString::Printf(TEXT("\n%s = %s  [%s, %s, index=%d]"),
			*Parameter.Name, *Parameter.Value, *Parameter.Type, *Parameter.Association, Parameter.Index);
	}

	Out += FString::Printf(TEXT("\n\n; INHERITED - declared by a parent, NOT overridden here (%d)"), InResult.Inherited.Num());
	Out += TEXT("\n; Values below are inherited from a parent. To change one of");
	Out += TEXT("\n; these, set an override on this instance; changing its parent can affect other instances.");
	if (InResult.Inherited.Num() == 0)
	{
		Out += TEXT("\nnone - every parameter the chain declares is overridden here");
	}
	for (const FParameter& Parameter : InResult.Inherited)
	{
		// Keep inheritance explicit when a caller reads a single output line.
		Out += FString::Printf(TEXT("\n%s = %s  [%s, %s, index=%d, inherited, resolved from %s]"),
			*Parameter.Name, *Parameter.Value, *Parameter.Type, *Parameter.Association, Parameter.Index, *Parameter.DeclaredAt);
	}

	Out += FString::Printf(TEXT("\n\n; ORPHANED - set here, declared by no parent (%d)"), InResult.Orphaned.Num());
	if (InResult.Orphaned.Num() == 0)
	{
		Out += TEXT("\nnone");
	}
	for (const FParameter& Parameter : InResult.Orphaned)
	{
		Out += FString::Printf(TEXT("\n%s = %s  [%s, %s, index=%d]"),
			*Parameter.Name, *Parameter.Value, *Parameter.Type, *Parameter.Association, Parameter.Index);
	}

	// State the inspected ancestor count and supported parameter types.
	const TArray<FString>& Types = FTurboMCPMaterialInstance::CoveredTypes();
	Out += FString::Printf(TEXT("\n\n; looked at %d level(s) in the chain, each for: %s"),
		FMath::Max(0, InResult.Chain.Num() - 1), *FString::Join(Types, TEXT(", ")));
	Out += TEXT("\n; (the levels looked at are the ANCESTORS - self declares nothing to inherit)");

	return Out;
}
