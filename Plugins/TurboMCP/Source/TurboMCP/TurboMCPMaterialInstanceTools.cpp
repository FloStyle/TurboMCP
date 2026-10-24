// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0

#include "Tools/TurboMCPToolsets.h"

#include "Materials/Material.h"
#include "Materials/MaterialExpressionTextureSampleParameter.h"
#include "Materials/MaterialInstance.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Materials/MaterialParameters.h"
#include "Engine/Texture.h"
#include "UObject/UObjectGlobals.h"

#include "TurboMCPAssetResolver.h"
#include "TurboMCPMaterialInstance.h"
#include "TurboMCPToolError.h"

FString UTurboMCPReadToolset::ReadTurboMCPMaterialInstance(const FString& Asset)
{
	if (Asset.TrimStartAndEnd().IsEmpty())
	{
		return TEXT("error: no asset given. Pass a material instance, for example MI_TurboMCPFixture.");
	}

	FTurboMCPMaterialInstance::FResult Result;
	FString Error;
	if (!FTurboMCPMaterialInstance::Read(Asset, Result, Error))
	{
		return Error;
	}

	return FTurboMCPMaterialInstance::Render(Result);
}

namespace
{
	struct FTextureOverride
	{
		FMaterialParameterInfo Parameter;
		FString Name;
		UTexture* Texture = nullptr;
	};

	bool IsSamplerCompatible(const EMaterialSamplerType SamplerType, const UTexture* Texture)
	{
		if (Texture == nullptr)
		{
			return false;
		}
		// Color samplers read sRGB textures; linear-color samplers read linear textures. Other sampler
		// kinds have engine-specific constraints and are left to the expression's own validation.
		if (SamplerType == SAMPLERTYPE_Color)
		{
			return Texture->SRGB;
		}
		if (SamplerType == SAMPLERTYPE_LinearColor)
		{
			return !Texture->SRGB;
		}
		return true;
	}

	UMaterialExpressionTextureSampleParameter* FindTextureParameter(UMaterialInstanceConstant* Instance,
		const FName ParameterName)
	{
		for (UMaterialInterface* Link = Instance != nullptr ? Instance->Parent : nullptr;
			Link != nullptr; Link = Cast<UMaterialInstance>(Link) != nullptr
				? Cast<UMaterialInstance>(Link)->Parent : nullptr)
		{
			const UMaterial* Material = Cast<UMaterial>(Link);
			if (Material == nullptr)
			{
				continue;
			}
			for (UMaterialExpression* Expression : Material->GetExpressionCollection().Expressions)
			{
				UMaterialExpressionTextureSampleParameter* TextureParameter =
					Cast<UMaterialExpressionTextureSampleParameter>(Expression);
				if (TextureParameter != nullptr && TextureParameter->ParameterName == ParameterName)
				{
					return TextureParameter;
				}
			}
		}
		return nullptr;
	}
}

FString UTurboMCPEditToolset::EditTurboMCPMaterialInstanceSet(const FString& Asset,
	const TArray<FString>& Overrides)
{
	if (Asset.TrimStartAndEnd().IsEmpty())
	{
		return TEXT("error: no material instance given.");
	}
	if (Overrides.Num() == 0)
	{
		return TEXT("error: no overrides given. Pass entries in the form Parameter = /Path/Texture.Asset.");
	}

	FTurboMCPAssetResolver::FResolved Resolved;
	const FString ResolveError = FTurboMCPAssetResolver::Resolve(Asset, Resolved);
	if (!ResolveError.IsEmpty())
	{
		return FTurboMCPToolError::AsError(ResolveError);
	}
	UMaterialInstanceConstant* Instance = LoadObject<UMaterialInstanceConstant>(nullptr,
		*Resolved.ObjectPath);
	if (Instance == nullptr)
	{
		return FString::Printf(TEXT("error: %s is not a Material Instance Constant."), *Resolved.ObjectPath);
	}

	TMap<FMaterialParameterInfo, FMaterialParameterMetadata> Declared;
	Instance->GetAllParametersOfType(EMaterialParameterType::Texture, Declared);
	TArray<FString> DeclaredNames;
	for (const TPair<FMaterialParameterInfo, FMaterialParameterMetadata>& Pair : Declared)
	{
		DeclaredNames.Add(Pair.Key.Name.ToString());
	}
	DeclaredNames.Sort();

	TArray<FTextureOverride> Planned;
	TSet<FName> RequestedNames;
	for (const FString& Entry : Overrides)
	{
		FString Name;
		FString TextureAsset;
		if (!Entry.Split(TEXT("="), &Name, &TextureAsset))
		{
			return FString::Printf(TEXT("error: '%s' is not Parameter = texture."), *Entry);
		}
		Name = Name.TrimStartAndEnd();
		TextureAsset = TextureAsset.TrimStartAndEnd();
        if (RequestedNames.Contains(FName(*Name)))
            return FString::Printf(TEXT("error: texture parameter '%s' is requested more than once; send one value per parameter; nothing was written."), *Name);
        RequestedNames.Add(FName(*Name));
		const TPair<FMaterialParameterInfo, FMaterialParameterMetadata>* DeclaredPair = nullptr;
		for (const TPair<FMaterialParameterInfo, FMaterialParameterMetadata>& Pair : Declared)
		{
			if (Pair.Key.Name.ToString().Equals(Name, ESearchCase::IgnoreCase))
			{
                if (DeclaredPair != nullptr)
                    return FString::Printf(TEXT("error: texture parameter '%s' is ambiguous across layers or blends; this tool accepts only unique parameter names; nothing was written."), *Name);
                DeclaredPair = &Pair;
			}
		}
		if (DeclaredPair == nullptr)
		{
			return FString::Printf(TEXT("error: texture parameter '%s' is not declared by %s. Known texture parameters: %s."),
				*Name, *Resolved.ObjectPath,
				DeclaredNames.Num() > 0 ? *FString::Join(DeclaredNames, TEXT(", ")) : TEXT("(none)"));
		}

		FTurboMCPAssetResolver::FResolved TextureResolved;
		const FString TextureResolveError = FTurboMCPAssetResolver::Resolve(TextureAsset, TextureResolved);
		if (!TextureResolveError.IsEmpty())
		{
			return FTurboMCPToolError::AsError(TextureResolveError);
		}
		UTexture* Texture = LoadObject<UTexture>(nullptr, *TextureResolved.ObjectPath);
		if (Texture == nullptr)
		{
			return FString::Printf(TEXT("error: %s is not a texture."), *TextureResolved.ObjectPath);
		}

		UMaterialExpressionTextureSampleParameter* Expression = FindTextureParameter(Instance,
			DeclaredPair->Key.Name);
		if (Expression == nullptr)
		{
			return FString::Printf(TEXT("error: texture parameter '%s' has no editable texture-sampler expression."),
				*DeclaredPair->Key.Name.ToString());
		}
		if (!IsSamplerCompatible(Expression->SamplerType, Texture))
		{
			return FString::Printf(TEXT("error: texture '%s' is incompatible with parameter '%s': sampler %s requires an %s texture."),
				*Texture->GetPathName(), *DeclaredPair->Key.Name.ToString(),
				*UEnum::GetValueAsString(Expression->SamplerType),
				Expression->SamplerType == SAMPLERTYPE_LinearColor ? TEXT("linear (sRGB disabled)") : TEXT("sRGB"));
		}

		FTextureOverride& Override = Planned.AddDefaulted_GetRef();
		Override.Parameter = DeclaredPair->Key;
		Override.Name = DeclaredPair->Key.Name.ToString();
		Override.Texture = Texture;
	}

	for (const FTextureOverride& Override : Planned)
	{
		Instance->SetTextureParameterValueEditorOnly(Override.Parameter, Override.Texture);
	}
	Instance->PostEditChange();
    for (const FTextureOverride& Override : Planned)
    {
        UTexture* Actual = nullptr;
        if (!Instance->GetTextureParameterValue(Override.Parameter, Actual) || Actual != Override.Texture)
            return FString::Printf(TEXT("partial: texture override '%s' did not read back as requested; changes may remain; no rollback was attempted; inspect the instance before saving or retrying."), *Override.Name);
    }

	FTurboMCPMaterialInstance::FResult Result;
	FString ReadError;
	if (!FTurboMCPMaterialInstance::Read(Asset, Result, ReadError))
	{
		return FString::Printf(TEXT("partial: overrides were written but the instance report failed: %s; inspect before saving or retrying; no rollback was attempted."), *ReadError);
	}
	return FString::Printf(TEXT("changed: %d texture override(s); effective texture values verified; rendered appearance not verified; save=not_requested\n%s"), Planned.Num(),
		*FTurboMCPMaterialInstance::Render(Result));
}
