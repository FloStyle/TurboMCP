// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "TurboMCPMaterial.h"

#include "Containers/UnrealString.h"
#include "MaterialEditingLibrary.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionComment.h"
#include "Materials/MaterialExpressionFunctionOutput.h"
#include "Materials/MaterialExpressionNamedReroute.h"
#include "Materials/MaterialExpressionParameter.h"
#include "Materials/MaterialExpressionReroute.h"
#include "Materials/MaterialExpressionRerouteBase.h"
#include "Materials/MaterialFunction.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialParameters.h"
#include "SceneTypes.h"
#include "Misc/Guid.h"
#include "StaticParameterSet.h"
#include "UObject/Class.h"
#include "UObject/Object.h"
#include "UObject/UnrealType.h"
#include "UObject/UObjectGlobals.h"

#include "TurboMCPAssetResolver.h"
#include "TurboMCPNodeId.h"
#include "TurboMCPTokenEstimate.h"

namespace
{
	/**
	 * The registered struct for a named engine input type, or null if it is not registered.
	 *
	 * WHY A RUNTIME LOOKUP RATHER THAN `StaticStruct()`. `StaticStruct()` does not exist for
	 * `FExpressionInput`: it is declared in `MaterialExpressionIO.h:25` as a plain `struct`, and its
	 * `TStructOpsTypeTraits` specialisation sets only `WithSerializer`, so it is a CPPSTRUCT with no
	 * generated accessor. `FColorMaterialInput`, `FScalarMaterialInput` and `FVectorMaterialInput` are
	 * declared in the SAME header, so they have no accessor either.
	 *
	 * The `Z_Construct_UScriptStruct_...` functions do exist for these types and ARE exported -
	 * `MaterialExpression.gen.cpp:23` declares `Z_Construct_UScriptStruct_FExpressionInput` with
	 * `ENGINE_API` - but they live under `Intermediate`, which is not on any public include path, so
	 * reaching them means repeating the declaration by hand. THAT ROUTE IS RECORDED AND NOT USED: it
	 * would tie this file to the exact engine build's generated symbol names, and a name lookup
	 * cannot.
	 *
	 * The structs are registered at runtime regardless - `Material.gen.cpp:566` builds
	 * `FColorMaterialInput` with `FMaterialInput` as its super, and `MaterialExpression.gen.cpp:136`
	 * registers `FExpressionInput` itself - so both are found by name.
	 *
	 * Cached, because the test that calls this runs once per property of every expression in a
	 * material and the lookup is a global object search.
	 */
	UScriptStruct* EngineStruct(const TCHAR* InPath)
	{
		static TMap<FString, UScriptStruct*> Cache;
		const FString Key = InPath;
		if (UScriptStruct** Found = Cache.Find(Key))
		{
			return *Found;
		}
		UScriptStruct* Resolved = FindObject<UScriptStruct>(nullptr, InPath);
		Cache.Add(Key, Resolved);
		return Resolved;
	}

	/**
	 * True when a struct property holds an expression input, at either level.
	 *
	 * TWO ROOTS ARE NEEDED, AND THE GENERATED CODE IS WHY.
	 *
	 * `Engine/Intermediate/.../UHT/Material.gen.cpp` registers `FMaterialInput` from the `#if !CPP`
	 * filler struct in `Material.h` - its eight `Expression`/`OutputIndex`/`InputName`/`Mask*` fields
	 * are what the UHT_STATICS block declares - with a SuperFunc of `nullptr`
	 * (`Material.gen.cpp:474-478`). Each of `FColorMaterialInput`, `FScalarMaterialInput`,
	 * `FVectorMaterialInput`, `FVector2MaterialInput`, `FShadingModelMaterialInput` and
	 * `FSubstrateMaterialInput` then registers with `Z_Construct_UScriptStruct_FMaterialInput` as its
	 * super. So a material OUTPUT's reflected chain is `<input> -> FMaterialInput -> (none)`.
	 *
	 * `FExpressionInput` ITSELF ALSO HAS A NULL SUPER (`MaterialExpression.gen.cpp:136-140`), and it
	 * is what a struct property on an expression uses - `UPROPERTY() FExpressionInput Input;`.
	 *
	 * SO NO SINGLE ROOT COVERS BOTH. `FMaterialInput` does not reach `FExpressionInput`, because the
	 * template base the compiler sees is not the base UHT recorded, and `FExpressionInput` obviously
	 * cannot reach its own derived types. A test against `FExpressionInput` alone matches NOTHING for
	 * a material's outputs - which would make `R-MAT-LOAD` report zero outputs while looking like it
	 * had checked - and a test against `FMaterialInput` alone matches nothing for an expression's own
	 * inputs. Both are tried, which is why this list has five names rather than one.
	 *
	 * The derived names are listed too, because a build that registers them with a chain this file has
	 * not seen still gets the right answer. A null lookup answers `false` rather than crashing, so a
	 * build where none resolves degrades to "no input constants printed" instead of taking the editor
	 * down.
	 */
	bool IsExpressionOrMaterialInputStruct(const FStructProperty* InProperty)
	{
		if (InProperty == nullptr || InProperty->Struct == nullptr)
		{
			return false;
		}
		static const TCHAR* const Roots[] = {
			TEXT("/Script/Engine.MaterialInput"),
			TEXT("/Script/Engine.ExpressionInput"),
			TEXT("/Script/Engine.ColorMaterialInput"),
			TEXT("/Script/Engine.ScalarMaterialInput"),
			TEXT("/Script/Engine.VectorMaterialInput"),
		};
		for (const TCHAR* RootPath : Roots)
		{
			if (const UScriptStruct* Root = EngineStruct(RootPath))
			{
				if (InProperty->Struct->IsChildOf(Root)
					|| InProperty->Struct == Root)
				{
					return true;
				}
			}
		}
		return false;
	}

	/** How deep a tree is printed before the rest becomes a reference. */
	constexpr int32 MaxDepth = 24;

	/** How many expressions are printed before the rest is summarised. */
	constexpr int32 MaxExpressions = 400;

	/** Hex digits in a node key. `04_FORMATS.md` 3.1. */
	constexpr int32 KeyDigits = 6;

	/** A quoted S-expression string, so a caption with a bracket cannot break the form. */
	FString Quoted(const FString& In)
	{
		FString Escaped = In;
		Escaped.ReplaceInline(TEXT("\\"), TEXT("\\\\"));
		Escaped.ReplaceInline(TEXT("\""), TEXT("\\\""));
		Escaped.ReplaceInline(TEXT("\n"), TEXT("\\n"));
		Escaped.ReplaceInline(TEXT("\r"), TEXT(""));
		return FString::Printf(TEXT("\"%s\""), *Escaped);
	}

	/** True when the two names match, case-insensitively. */
	bool NameMatches(const FString& InA, const FString& InB)
	{
		return InA.Equals(InB, ESearchCase::IgnoreCase);
	}

	/**
	 * The name of a parameter association.
	 *
	 * `StaticEnum<T>()` DOES NOT EXIST for this enum and was checked rather than assumed:
	 * `EMaterialParameterAssociation` is declared `enum EMaterialParameterAssociation : int` in
	 * `MaterialParameters.h:25`, a plain integer enum rather than a `UENUM` with generated reflection
	 * helpers, so the engine's own name lookup is not available for it. The values are read from that
	 * declaration - `LayerParameter`, `BlendParameter`, `GlobalParameter`, in that order - and the
	 * NAME is a fixed map rather than a switch, so a value this does not know prints its number
	 * instead of being silently called something it is not.
	 */
	FString AssociationName(const TEnumAsByte<EMaterialParameterAssociation>& InAssociation)
	{
		switch (static_cast<EMaterialParameterAssociation>(InAssociation.GetValue()))
		{
		case EMaterialParameterAssociation::LayerParameter:  return TEXT("layer");
		case EMaterialParameterAssociation::BlendParameter:  return TEXT("blend");
		case EMaterialParameterAssociation::GlobalParameter: return TEXT("global");
		default: break;
		}
		return FString::Printf(TEXT("(%d)"), InAssociation.GetValue());
	}

} // namespace

FString FTurboMCPMaterial::KeyOf(const UMaterialExpression* InExpression)
{
	if (InExpression == nullptr)
	{
		return FString();
	}

	// `04_FORMATS.md` 3.2: an expression's ID uses its GUID.
	if (InExpression->MaterialExpressionGuid.IsValid())
	{
		return TurboMCPNodeId::MakeKey(InExpression->MaterialExpressionGuid, KeyDigits);
	}

	// THE FALLBACK IS STABLE AND LABELLED, WHICH IS WHAT 3.2 ASKS FOR. It is derived from the
	// expression's class and its editor position, both of which are properties of the expression
	// rather than of this call, so two readers of one material agree. It is NOT a generated GUID: a
	// Read must never write, and 3.2 says explicitly that a Preview uses a stable fallback instead.
	const FString Seed = FString::Printf(TEXT("%s@%d,%d"),
		*InExpression->GetClass()->GetName(),
		InExpression->MaterialExpressionEditorX, InExpression->MaterialExpressionEditorY);
	return FString::Printf(TEXT("noguid-%s"),
		*TurboMCPNodeId::MakeKey(FGuid(GetTypeHash(Seed), GetTypeHash(Seed) >> 8,
			GetTypeHash(Seed) >> 16, GetTypeHash(Seed) >> 24), KeyDigits));
}

FString FTurboMCPMaterial::IdOf(const UMaterialExpression* InExpression,
	const FString& InAssetName, const FString& InGraphName)
{
	// THE ID IS COMPOSED HERE RATHER THAN THROUGH `TurboMCPNodeId::Make`, AND THAT IS DELIBERATE.
	//
	// MEASURED COMPILE ERROR: `Make` takes an `FGuid` as its third argument
	// (`TurboMCPNodeId.h:72-74`, `const FGuid& InNodeGuid`), and passing the KEY STRING that `KeyOf`
	// produced was `C2664: cannot convert argument 3 from FString to const FGuid&`.
	//
	// Turning the key back into a guid is not possible, and would be wrong even if it were: `KeyOf`
	// has a documented fallback that produces a key of the form `noguid-<hex>` for an expression with
	// no valid guid (`04_FORMATS.md` 3.2 asks for a stable labelled fallback, because a Read must
	// never write). `Make` always formats its third part as `MakeKey(InNodeGuid)`, so it can never
	// emit that `noguid-` prefix and would silently DROP the distinction between a real guid and the
	// fallback. The two parts are therefore composed here, which is the same `A:G:K` shape
	// `TurboMCPNodeId.cpp:72` produces.
	if (InAssetName.IsEmpty() || InGraphName.IsEmpty() || InExpression == nullptr)
	{
		// `Make` returns an empty ID for a partial input so the caller must refuse on it
		// (`TurboMCPNodeId.cpp:41-46`). The same contract is kept here.
		return FString();
	}

	// `3.1`: the asset's own name, reduced from an object path exactly as `Make` reduces it.
	FString AssetPart = InAssetName;
	{
		int32 LastDot = INDEX_NONE;
		if (AssetPart.FindLastChar(TEXT('.'), LastDot))
		{
			AssetPart = AssetPart.Mid(LastDot + 1);
		}
		int32 LastSlash = INDEX_NONE;
		if (AssetPart.FindLastChar(TEXT('/'), LastSlash))
		{
			AssetPart = AssetPart.Mid(LastSlash + 1);
		}
	}

	// `3.1` quotes the graph name when it has spaces, so the ID reads back unambiguously.
	const FString GraphPart = InGraphName.Contains(TEXT(" "))
		? FString::Printf(TEXT("\"%s\""), *InGraphName)
		: InGraphName;

	return FString::Printf(TEXT("%s:%s:%s"), *AssetPart, *GraphPart,
		*KeyOf(InExpression));
}

bool FTurboMCPMaterial::IsPlainReroute(const UMaterialExpression* InExpression)
{
	// A PLAIN reroute only. `UMaterialExpressionReroute` is the unlabelled one; the named base class
	// is deliberately NOT included, because `2.5` says a named reroute has its own meaning and prints
	// as a label rather than being looked through.
	return Cast<UMaterialExpressionReroute>(InExpression) != nullptr;
}

UMaterialExpression* FTurboMCPMaterial::ResolveThroughReroutes(const UMaterialExpression* InExpression)
{
	// A bound on the walk: a malformed material could point a reroute at itself, and an unbounded
	// loop in a Read tool would hang the editor rather than return a wrong answer.
	TSet<const UMaterialExpression*> Seen;
	UMaterialExpression* Current = const_cast<UMaterialExpression*>(InExpression);
	int32 Steps = 0;

	while (Current != nullptr && Steps < 64)
	{
		if (Seen.Contains(Current))
		{
			return Current;
		}
		Seen.Add(Current);

		UMaterialExpressionReroute* Reroute = Cast<UMaterialExpressionReroute>(Current);
		if (Reroute == nullptr)
		{
			return Current;
		}

		// `GetRerouteInput` IS PROTECTED AND CANNOT BE CALLED, and that was a compile error
		// (`C2248: cannot access protected member declared in class UMaterialExpressionReroute`).
		// Read from the headers: `MaterialExpressionRerouteBase.h:48-53` declares it under
		// `protected:`, and `MaterialExpressionReroute.h:36-38` is its override, also protected.
		//
		// The PUBLIC field is used instead. `MaterialExpressionReroute.h:10-12` declares
		// `UPROPERTY() FExpressionInput Input;` under `public:`, which is the link itself and exactly
		// what this walk needs. The comment that used to sit here claimed the accessor was "preferred
		// over reading the `Input` field directly, because a derived reroute may answer it
		// differently" - that reasoning is fine but the call does not compile, and a non-compiling
		// preference is worth nothing.
		//
		// For a PLAIN reroute - and `IsPlainReroute` is checked above - `Input` IS the answer, so
		// nothing is lost. A DERIVED reroute takes a different branch earlier, so its override never
		// mattered to this walk.
		if (Reroute->Input.Expression == nullptr)
		{
			return Current;
		}
		Current = Reroute->Input.Expression;
		++Steps;
	}
	return Current;
}

bool FTurboMCPMaterial::IsLabelDeclaration(const UMaterialExpression* InExpression, FString& OutLabel)
{
	// A declaration with NO name is still a declaration, and `2.5` says a value that already has a
	// label is referred to only by its label. An unnamed one gets a name derived from its key so the
	// text stays a single connected picture rather than falling back to node IDs.
	if (const UMaterialExpressionNamedRerouteDeclaration* Declaration =
			Cast<UMaterialExpressionNamedRerouteDeclaration>(InExpression))
	{
		OutLabel = Declaration->Name.IsNone()
			? FString::Printf(TEXT("unnamed_%s"), *KeyOf(InExpression))
			: Declaration->Name.ToString();
		return true;
	}
	return false;
}

FString FTurboMCPMaterial::LabelOfUsage(const UMaterialExpressionNamedRerouteUsage* InUsage)
{
	if (InUsage == nullptr)
	{
		return FString();
	}

	// THE DECLARATION POINTER IS THE PREFERRED ROUTE, because it is what the editor maintains when a
	// label is renamed. `DeclarationGuid` is the fallback: the pointer can be null on a freshly
	// loaded asset whose fixup has not run, and the GUID survives that.
	if (InUsage->Declaration != nullptr && !InUsage->Declaration->Name.IsNone())
	{
		return InUsage->Declaration->Name.ToString();
	}

	// THE GUID IS RESOLVED BY SCANNING, which is what the engine's own `FindDeclarationInMaterial`
	// does. There is no public accessor for it (`MaterialExpressionNamedReroute.h:31` declares it
	// `protected`), so the scan is the available route rather than a shortcut.
	if (InUsage->DeclarationGuid.IsValid())
	{
		if (const UMaterial* Material = Cast<UMaterial>(InUsage->GetOuter()))
		{
			for (UMaterialExpression* Candidate :
					Material->GetExpressionCollection().Expressions)
			{
				if (const UMaterialExpressionNamedRerouteDeclaration* Declaration =
						Cast<UMaterialExpressionNamedRerouteDeclaration>(Candidate))
				{
					if (Declaration->VariableGuid == InUsage->DeclarationGuid
						&& !Declaration->Name.IsNone())
					{
						return Declaration->Name.ToString();
					}
				}
			}
		}
	}

	// A usage whose declaration cannot be found is REPORTED as such rather than printed as a made-up
	// label, which would look like a working reference to a label that does not exist.
	return FString::Printf(TEXT("dangling_%s"), *KeyOf(InUsage));
}

bool FTurboMCPMaterial::IsLabelUsage(const UMaterialExpression* InExpression, FString& OutLabel)
{
	if (const UMaterialExpressionNamedRerouteUsage* Usage =
			Cast<UMaterialExpressionNamedRerouteUsage>(InExpression))
	{
		OutLabel = LabelOfUsage(Usage);
		return true;
	}
	return false;
}

FString FTurboMCPMaterial::CaptionOf(const UMaterialExpression* InExpression)
{
	if (InExpression == nullptr)
	{
		return FString();
	}

	// `GetCaption` is the editor's own accessor and is overridden per expression class, so it is what
	// the material editor draws. Reading it beats deriving a name from the class, which would print
	// `MaterialExpressionConstant3Vector` where the editor prints the value.
	TArray<FString> Captions;
	InExpression->GetCaption(Captions);
	if (Captions.Num() > 0)
	{
		return FString::Join(Captions, TEXT(" "));
	}

	// `2.1.12`: an expression that cannot be captioned is a comment line, not a silent gap.
	return InExpression->GetClass()->GetName();
}

FString FTurboMCPMaterial::ConstantForInput(const UMaterialExpression* InOwner,
	const FString& InInputName)
{
	if (InOwner == nullptr)
	{
		return FString();
	}

	// MOST EXPRESSIONS KEEP AN UNLINKED INPUT CONSTANT IN A SEPARATE PROPERTY, AND NOT LOOKING FOR
	// IT MADE EVERY SUCH INPUT READ AS UNSET.
	//
	// MEASURED, ON THE MATERIAL FIXTURE, which is why the fixture exists. `UMaterialExpressionAdd`
	// stores its B constant in `float ConstB`, a SIBLING of `FExpressionInput B` carrying
	// `meta = (OverridingInputProperty = "B")`. The scan below walks the INPUT STRUCT fields, so it
	// never sees `ConstB`, and `R-MAT-LOAD` answered `B "B unset"` for an input whose constant was
	// 0.375.
	//
	// `2.5` requires the constant an expression keeps to be printed, and this is the shape MOST
	// expressions use - `Add`, `Multiply`, `Divide`, `Subtract` and `Power` all declare
	// `ConstA`/`ConstB` this way. The old scan only ever found a constant for a class that put its
	// value INSIDE the input struct, so the requirement was met for a minority and the text looked
	// right.
	//
	// THE RELATIONSHIP COMES FROM THE ENGINE OWN METADATA rather than from a list of class names:
	// the property that overrides an input SAYS which input it overrides. That is the same reason
	// the reflected scan below exists at all.
	static FName OverridingInputPropertyMetaData("OverridingInputProperty");
	for (TFieldIterator<FProperty> OverrideIt(InOwner->GetClass(),
			EFieldIteratorFlags::IncludeSuper, EFieldIteratorFlags::ExcludeDeprecated);
		OverrideIt; ++OverrideIt)
	{
		// NO NULL CHECK ON THE ITERATOR'S VALUE, AND ADDING ONE IS A BUILD ERROR. `TFieldIterator`
		// never yields null - it walks until the iteration flag says stop - so a guard compiles to
		// unreachable code, and this project builds with warnings as errors:
		//
		//     TurboMCPMaterial.cpp(418): error C4702: unreachable code
		//     TurboMCPMaterial.cpp(423): error C4702: unreachable code
		//
		// It was removed rather than suppressed with a pragma. A check that cannot be false is noise
		// that hides the checks that can.
		FProperty* Override = *OverrideIt;
		if (!Override->HasMetaData(OverridingInputPropertyMetaData))
		{
			continue;
		}
		const FString Overrides = Override->GetMetaData(OverridingInputPropertyMetaData);
		if (Overrides.IsEmpty() || !NameMatches(Overrides, InInputName))
		{
			continue;
		}

		const void* OverrideValue = Override->ContainerPtrToValuePtr<void>(InOwner);
		if (const FFloatProperty* FloatProperty = CastField<FFloatProperty>(Override))
		{
			return FString::Printf(TEXT("%g"), FloatProperty->GetPropertyValue(OverrideValue));
		}
		if (const FDoubleProperty* DoubleProperty = CastField<FDoubleProperty>(Override))
		{
			return FString::Printf(TEXT("%g"), DoubleProperty->GetPropertyValue(OverrideValue));
		}
		if (const FIntProperty* IntProperty = CastField<FIntProperty>(Override))
		{
			return FString::Printf(TEXT("%d"), IntProperty->GetPropertyValue(OverrideValue));
		}
		if (const FBoolProperty* BoolProperty = CastField<FBoolProperty>(Override))
		{
			return BoolProperty->GetPropertyValue(OverrideValue) ? TEXT("true") : TEXT("false");
		}
	}

	// READ BY REFLECTION, WHICH `2.5` REQUIRES BY NAME: "Read those constants by reflection, not from
	// a hard coded list of expression classes." A list would silently miss every class added after it
	// was written, and the miss would look like an input that keeps no constant.
	//
	// The owner's fields are scanned for one whose name matches the input, and whose type derives from
	// `FExpressionInput`. A match is descended into and every reflected field inside it is printed
	// except the link itself, which is what "the constant the expression keeps for that input" means.
	for (TFieldIterator<FProperty> PropertyIt(InOwner->GetClass()); PropertyIt; ++PropertyIt)
	{
		FProperty* Property = *PropertyIt;
		if (Property == nullptr || !NameMatches(Property->GetName(), InInputName))
		{
			continue;
		}

		FStructProperty* StructProperty = CastField<FStructProperty>(Property);
		if (!IsExpressionOrMaterialInputStruct(StructProperty))
		{
			continue;
		}

		const void* Value = StructProperty->ContainerPtrToValuePtr<void>(InOwner);
		TArray<FString> Parts;

		for (TFieldIterator<FProperty> InnerIt(StructProperty->Struct); InnerIt; ++InnerIt)
		{
			FProperty* Inner = *InnerIt;
			if (Inner == nullptr)
			{
				continue;
			}

			// The link itself is not a constant, and the masks are the editor's own bookkeeping
			// rather than a value the expression keeps. Neither belongs in the text.
			const FString InnerName = Inner->GetName();
			if (InnerName == TEXT("Expression") || InnerName == TEXT("OutputIndex")
				|| InnerName == TEXT("InputName") || InnerName.StartsWith(TEXT("Mask")))
			{
				continue;
			}

			const void* InnerValue = Inner->ContainerPtrToValuePtr<void>(Value);

			if (const FFloatProperty* FloatProperty = CastField<FFloatProperty>(Inner))
			{
				Parts.Add(FString::Printf(TEXT("%s %g"), *InnerName,
					FloatProperty->GetPropertyValue(InnerValue)));
			}
			else if (const FDoubleProperty* DoubleProperty = CastField<FDoubleProperty>(Inner))
			{
				Parts.Add(FString::Printf(TEXT("%s %g"), *InnerName,
					DoubleProperty->GetPropertyValue(InnerValue)));
			}
			else if (const FIntProperty* IntProperty = CastField<FIntProperty>(Inner))
			{
				Parts.Add(FString::Printf(TEXT("%s %d"), *InnerName,
					IntProperty->GetPropertyValue(InnerValue)));
			}
			else if (const FBoolProperty* BoolProperty = CastField<FBoolProperty>(Inner))
			{
				Parts.Add(FString::Printf(TEXT("%s %s"), *InnerName,
					BoolProperty->GetPropertyValue(InnerValue) ? TEXT("true") : TEXT("false")));
			}
			else if (const FStructProperty* ColourProperty = CastField<FStructProperty>(Inner))
			{
				// A COLOUR IS EXPORTED THROUGH THE ENGINE'S OWN TEXT FORM, which `2.3` requires for a
				// struct value and which saves this file from knowing every struct shape.
				//
				// THE CALL TAKES `TNotNull<const void*>`, NOT A `const void*`
				// (`UnrealType.h:587`, read rather than assumed), so the pointer is passed by
				// ADDRESS. Passing `InnerValue` directly does not compile, which is the compiler
				// catching a signature this file would otherwise have guessed wrong.
				FString Exported;
				ColourProperty->ExportTextItem_Direct(Exported, &InnerValue, nullptr, nullptr,
					PPF_None);
				if (!Exported.IsEmpty())
				{
					Parts.Add(FString::Printf(TEXT("%s %s"), *InnerName, *Quoted(Exported)));
				}
			}
		}

		return FString::Join(Parts, TEXT(" "));
	}

	return FString();
}

TArray<FString> FTurboMCPMaterial::AllOutputProperties(FPlan& InPlan)
{
	TArray<FString> Out;

	// THE OUTPUT LIST COMES FROM THE ENGINE'S OWN PROPERTY ENUM, WHICH IS THE ONLY ROUTE THAT WORKS.
	//
	// This started as a reflection walk over `UMaterialEditorOnlyData`'s fields, testing each
	// `FStructProperty` with `IsChildOf` against the input struct. THAT CANNOT WORK, and the proof is
	// in the generated code rather than in an opinion:
	//
	//   `Engine/Intermediate/.../UHT/Material.gen.cpp:378-478` registers `FMaterialInput` from the
	//   `#if !CPP` filler struct in `Material.h`, whose reflected properties are the eight
	//   `Expression`/`OutputIndex`/`InputName`/`Mask*` fields, and gives it a SuperFunc of `nullptr`.
	//   `FColorMaterialInput` and its five siblings register with `Z_Construct_UScriptStruct_FMaterialInput`
	//   as their SuperFunc, so the reflected chain is `FColorMaterialInput -> FMaterialInput -> (none)`.
	//   It NEVER reaches `FExpressionInput`, because the template base the compiler sees is not the
	//   base UHT recorded. So an `IsChildOf(FExpressionInput)` test matches nothing at all, and the
	//   material would have reported zero outputs while looking like it had checked.
	//
	//   Worse, the symbol that would fix it - `Z_Construct_UScriptStruct_FMaterialInput` - is declared
	//   in NO generated header; it exists only inside `Material.gen.cpp`. Checked across the whole
	//   engine tree. A plugin cannot call it.
	//
	// So the enumeration uses `EMaterialProperty` (`SceneTypes.h:147`) with the engine's public
	// `UMaterial::GetExpressionInputForProperty` (`Material.h:1890`, `WITH_EDITOR`), whose documented
	// contract is exactly "a pointer to the expression input of the property specified, or NULL if an
	// invalid property was requested". Iterating the enum asks the engine about EVERY property rather
	// than about a list this file maintains, so a property added by a future engine version is found
	// without this file being touched - which was the original goal, reached by a route that works.
	if (InPlan.Material != nullptr)
	{
		if (const UEnum* PropertyEnum = StaticEnum<EMaterialProperty>())
		{
			for (int32 Index = 0; Index < PropertyEnum->NumEnums() - 1; ++Index)
			{
				// `NumEnums() - 1` because UHT appends a `_MAX` sentinel, which is not a property and
				// which `GetExpressionInputForProperty` would reject anyway.
				const int64 Value = PropertyEnum->GetValueByIndex(Index);
				const EMaterialProperty Property = static_cast<EMaterialProperty>(Value);
				if (InPlan.Material->GetExpressionInputForProperty(Property) != nullptr)
				{
					Out.Add(PropertyEnum->GetNameStringByIndex(Index));
				}
			}
		}
	}

	// A MATERIAL FUNCTION HAS OUTPUTS TOO, AND ITS OUTPUTS ARE NODES RATHER THAN MATERIAL PROPERTIES.
	//
	// MEASURED, ON THE MATERIAL FIXTURE, which is why the fixture exists. A function whose only output
	// node was wired to a labelled value answered:
	//
	//     connected outputs: 0 of 0
	//     4 expression(s) reach no output, so they do not affect what the material looks like:
	//       3a5922 MaterialExpressionConstant3Vector 0.1,0.9,0.1
	//       50bc37 MaterialExpressionNamedRerouteDeclaration FunctionTint
	//       d6c6a7 MaterialExpressionNamedRerouteUsage FunctionTint
	//       5603ad MaterialExpressionFunctionOutput Output Result
	//
	// EVERY expression was reported unreached, including the output node itself. `EMaterialProperty`
	// describes a MATERIAL's inputs - BaseColor, Emissive, Roughness - and a function has none of
	// them, so the loop above added nothing and the function branch of these readers printed a
	// document that described no outputs at all. The branch ran, and everything it said was wrong in
	// the same direction, which is the failure mode a count-based check would have accepted.
	//
	// A function's outputs are `UMaterialExpressionFunctionOutput` nodes: the `OutputName` is the name
	// a caller sees and the `A` input is the value returned. They are gathered the same way the
	// material's are - by asking the graph - so the two branches cannot disagree about what an output
	// is called.
	if (InPlan.Function != nullptr)
	{
		for (UMaterialExpression* Expression : InPlan.Expressions)
		{
			const UMaterialExpressionFunctionOutput* FunctionOutput =
				Cast<UMaterialExpressionFunctionOutput>(Expression);
			if (FunctionOutput == nullptr)
			{
				continue;
			}
			const FString Name = FunctionOutput->OutputName.IsNone()
				? FString::Printf(TEXT("Output_%s"), *KeyOf(Expression))
				: FunctionOutput->OutputName.ToString();
			Out.AddUnique(Name);
		}
	}

	// SORTED, so two reads of one material always print the outputs in one order. `EMaterialProperty`
	// happens to be declared in a stable order, but nothing promises that, and a reader that reorders
	// its own output between runs makes a diff useless.
	Out.Sort();
	return Out;
}

UMaterialExpression* FTurboMCPMaterial::OutputSource(FPlan& InPlan, const FString& InProperty)
{
	// A MATERIAL FUNCTION IS RESOLVED FIRST, because a plan carries either a material or a function
	// and never both. Its output is the `FunctionOutput` node whose `OutputName` was asked for, and
	// the value is that node's `A` input - the same "one output, one source" shape the material
	// branch below has, reached a different way because a function has no `EMaterialProperty`.
	if (InPlan.Function != nullptr)
	{
		if (InProperty.IsEmpty())
		{
			return nullptr;
		}
		for (UMaterialExpression* Expression : InPlan.Expressions)
		{
			const UMaterialExpressionFunctionOutput* FunctionOutput =
				Cast<UMaterialExpressionFunctionOutput>(Expression);
			if (FunctionOutput == nullptr)
			{
				continue;
			}
			const FString Name = FunctionOutput->OutputName.IsNone()
				? FString::Printf(TEXT("Output_%s"), *KeyOf(Expression))
				: FunctionOutput->OutputName.ToString();
			if (!Name.Equals(InProperty, ESearchCase::IgnoreCase))
			{
				continue;
			}
			// A plain reroute feeding the output is looked through, for the same reason the material
			// branch does it: `2.5` says plain reroutes are invisible.
			return ResolveThroughReroutes(FunctionOutput->A.Expression);
		}
		return nullptr;
	}

	if (InPlan.Material == nullptr || InProperty.IsEmpty())
	{
		return nullptr;
	}

	const UEnum* PropertyEnum = StaticEnum<EMaterialProperty>();
	if (PropertyEnum == nullptr)
	{
		return nullptr;
	}

	// BY NAME, against the same enum the list came from, so a name that `AllOutputProperties`
	// produced always resolves and a name that it did not is always refused. Matching against the
	// enum rather than against a field name keeps these two functions from drifting.
	for (int32 Index = 0; Index < PropertyEnum->NumEnums() - 1; ++Index)
	{
		if (!PropertyEnum->GetNameStringByIndex(Index).Equals(InProperty, ESearchCase::IgnoreCase))
		{
			continue;
		}

		const EMaterialProperty Property =
			static_cast<EMaterialProperty>(PropertyEnum->GetValueByIndex(Index));
		if (FExpressionInput* Input = InPlan.Material->GetExpressionInputForProperty(Property))
		{
			// A plain reroute feeding an output is looked through, because `2.5` says plain reroutes
			// are invisible. The OUTPUT is what the reader asked about, so resolving here keeps the
			// caller from having to know the rule.
			return ResolveThroughReroutes(Input->Expression);
		}

		// The property is real but unconnected. Answering null is the honest result, and it is what
		// makes `R-MAT-TRACE` able to say "that output is not connected" rather than printing nothing.
		return nullptr;
	}

	return nullptr;
}

TArray<FString> FTurboMCPMaterial::ConnectedOutputs(FPlan& InPlan)
{
	TArray<FString> Out;
	for (const FString& Property : AllOutputProperties(InPlan))
	{
		if (OutputSource(InPlan, Property) != nullptr)
		{
			Out.Add(Property);
		}
	}
	return Out;
}

bool FTurboMCPMaterial::Plan(const FString& InAsset, ERead InKind,
	const FString& InOutputProperty, bool bInIncludeInstanceOverrides,
	FPlan& OutPlan, FString& OutError)
{
	OutError.Empty();
	OutPlan = FPlan();
	OutPlan.Asset = InAsset.TrimStartAndEnd();
	OutPlan.OutputProperty = InOutputProperty.TrimStartAndEnd();
	OutPlan.bIncludeInstanceOverrides = bInIncludeInstanceOverrides;

	if (OutPlan.Asset.IsEmpty())
	{
		OutError = TEXT("no asset given. Pass a Material, a Material Instance or a Material ")
			TEXT("Function in any path form, for example M_Ground.");
		return false;
	}

	FTurboMCPAssetResolver::FResolved Resolved;
	const FString ResolveError = FTurboMCPAssetResolver::Resolve(OutPlan.Asset, Resolved);
	if (!ResolveError.IsEmpty())
	{
		OutError = ResolveError;
		return false;
	}

	UObject* Loaded = LoadObject<UObject>(nullptr, *Resolved.ObjectPath);
	if (Loaded == nullptr)
	{
		OutError = FString::Printf(
			TEXT("%s resolved but could not be loaded."), *Resolved.ObjectPath);
		return false;
	}

	OutPlan.Asset = Resolved.ObjectPath;
	OutPlan.AssetName = Resolved.AssetName;

	// A MATERIAL INSTANCE IS REFUSED WITH THE PATH OF ITS PARENT, which `03_TOOLS.md` requires by
	// name. The refusal is here rather than later because only this function has the parent, and a
	// refusal that does not name the way forward leaves the caller stuck (`P7`).
	if (UMaterialInstanceConstant* Instance = Cast<UMaterialInstanceConstant>(Loaded))
	{
		OutPlan.Instance = Instance;
		OutPlan.ParentPath = Instance->Parent != nullptr
			? Instance->Parent->GetPathName() : FString();

		if (!bInIncludeInstanceOverrides)
		{
			OutError = FString::Printf(
				TEXT("%s is a Material Instance, not a Material: it has no expression graph of its ")
				TEXT("own, only overrides of its parent. Read the parent's graph instead, at %s, ")
				TEXT("or pass includeInstanceOverrides=true to list this instance's overrides."),
				*Resolved.ObjectPath,
				OutPlan.ParentPath.IsEmpty() ? TEXT("(its parent is not set)")
					: *OutPlan.ParentPath);
			return false;
		}
		// The overrides were asked for, so the plan carries the instance and the reader prints the
		// short table. No graph is gathered, because an instance has none.
		return true;
	}

	if (UMaterial* Material = Cast<UMaterial>(Loaded))
	{
		OutPlan.Material = Material;
		OutPlan.GraphName = TEXT("Material");

		for (const TObjectPtr<UMaterialExpression>& Expression :
				Material->GetExpressionCollection().Expressions)
		{
			if (Expression != nullptr)
			{
				OutPlan.Expressions.Add(Expression);
			}
		}
		for (const TObjectPtr<UMaterialExpressionComment>& Comment :
				Material->GetExpressionCollection().EditorComments)
		{
			if (Comment != nullptr)
			{
				OutPlan.Comments.Add(Comment);
			}
		}
	}
	else if (UMaterialFunctionInterface* Function = Cast<UMaterialFunctionInterface>(Loaded))
	{
		OutPlan.Function = Function;
		// A material FUNCTION IS its own graph, so its own name is the graph name rather than the
		// word `Material`. That keeps a node ID from a function distinguishable from one in a
		// material, which matters because the keys are GUIDs and can be compared.
		OutPlan.GraphName = Resolved.AssetName;

		for (const TObjectPtr<UMaterialExpression>& Expression : Function->GetExpressions())
		{
			if (Expression == nullptr)
			{
				continue;
			}
			// A material function keeps its comment boxes in the same array as its expressions -
			// `UMaterialExpressionComment` derives from `UMaterialExpression` - so they are sorted
			// out here rather than being printed as if they computed a value.
			if (UMaterialExpressionComment* Comment = Cast<UMaterialExpressionComment>(Expression))
			{
				OutPlan.Comments.Add(Comment);
			}
			else
			{
				OutPlan.Expressions.Add(Expression);
			}
		}
	}
	else
	{
		// REFUSED BY NAME, WITH WHAT WAS FOUND INSTEAD. Pointing a material reader at a Blueprint or
		// an enum must say so; an empty document would read as "this material has nothing in it",
		// which is a different and much worse answer.
		OutError = FString::Printf(
			TEXT("%s is a %s, and these readers take a Material, a Material Instance or a Material ")
			TEXT("Function. For a Blueprint use R-BP-LOAD; for a widget use the widget tree reader."),
			*Resolved.ObjectPath, *Loaded->GetClass()->GetName());
		return false;
	}

	// The labels, gathered once so every reader agrees about which ones exist and how many readers
	// each has. `2.5` makes the label the primary name for a value, so getting this wrong makes every
	// reference in the output wrong.
	for (UMaterialExpression* Expression : OutPlan.Expressions)
	{
		FString Label;
		if (IsLabelDeclaration(Expression, Label))
		{
			// A DUPLICATE LABEL IS KEPT AS THE FIRST DECLARATION and the reader count still counts
			// every usage, so the count is right even when the declaration is ambiguous. Reporting
			// the ambiguity is `R-MAT-LABELS`'s job rather than a silent pick.
			OutPlan.DeclarationsByLabel.Add(Label,
				Cast<UMaterialExpressionNamedRerouteDeclaration>(Expression));
		}
	}
	for (UMaterialExpression* Expression : OutPlan.Expressions)
	{
		FString Label;
		if (IsLabelUsage(Expression, Label))
		{
			int32& Count = OutPlan.ReaderCountByLabel.FindOrAdd(Label);
			++Count;
		}
	}

	if (InKind == ERead::Trace)
	{
		if (OutPlan.OutputProperty.IsEmpty())
		{
			OutError = TEXT("no output property given. Pass one of the material's connected ")
				TEXT("outputs; this material has none listed yet.");
			return false;
		}

		const TArray<FString> All = AllOutputProperties(OutPlan);
		const TArray<FString> Connected = ConnectedOutputs(OutPlan);

		if (OutputSource(OutPlan, OutPlan.OutputProperty) == nullptr)
		{
			// `03_TOOLS.md`: "An output property that is not connected: list the connected ones."
			// The list is the whole answer here, because a caller who named the wrong property needs
			// the right ones rather than a restatement of the mistake.
			OutError = FString::Printf(
				TEXT("'%s' is not a connected output of %s. Its connected outputs are: %s. It has ")
				TEXT("%d output propert%s in total, and the rest are unconnected."),
				*OutPlan.OutputProperty, *OutPlan.Asset,
				Connected.Num() > 0 ? *FString::Join(Connected, TEXT(", ")) : TEXT("(none)"),
				All.Num(), All.Num() == 1 ? TEXT("y") : TEXT("ies"));
			return false;
		}
	}

	return true;
}

FString FTurboMCPMaterial::RenderExpression(const UMaterialExpression* InExpression,
	FRenderState& InOutState, int32 InDepth, int32 InTokenBudget)
{
	if (InExpression == nullptr)
	{
		return TEXT("(none)");
	}

	// A LABEL IS REFERRED TO BY ITS LABEL AND NOTHING ELSE. `2.5`: "A value that already has a
	// label is referred to only by its label." So a declaration prints its label, and a second
	// reference to the same declaration prints only the label again - never `$nodeid` beside it,
	// which is the specific mistake the spec names.
	FString Label;
	if (IsLabelDeclaration(InExpression, Label))
	{
		if (InOutState.DefinedLabels.Contains(Label))
		{
			return FString::Printf(TEXT("@%s"), *Label);
		}
		InOutState.DefinedLabels.Add(Label);
	}

	// A USAGE always prints its label, because the declaration is what carries the value. A usage
	// whose declaration is missing prints the dangling marker `LabelOfUsage` produced, so a broken
	// reference is visible rather than looking like a working one.
	if (IsLabelUsage(InExpression, Label))
	{
		return FString::Printf(TEXT("@%s"), *Label);
	}

	// A SHARED VALUE IS DEFINED ONCE. This mirrors `RenderPinValue` in `TurboMCPGraphText.cpp`, which
	// prints `$<key>` for a value with more than one consumer so a diamond does not become a tree.
	if (InOutState.Defined.Contains(InExpression))
	{
		return FString::Printf(TEXT("$%s"),
			*KeyOf(InExpression));
	}

	// A CYCLE WOULD BE WALKED FOREVER. A material graph is meant to be a DAG, but a malformed one can
	// point a value at itself, and a Read that hangs is worse than a Read that says so.
	if (InOutState.InProgress.Contains(InExpression))
	{
		InOutState.Notes.Add(FString::Printf(
			TEXT("; error: %s is part of a cycle, so its inputs are not printed"),
			*KeyOf(InExpression)));
		return FString::Printf(TEXT("(cycle %s)"), *KeyOf(InExpression));
	}

	if (InDepth > MaxDepth)
	{
		InOutState.Notes.Add(FString::Printf(
			TEXT("; error: %s is deeper than %d levels, so its inputs are not printed"),
			*KeyOf(InExpression), MaxDepth));
		return FString::Printf(TEXT("(deep %s)"), *KeyOf(InExpression));
	}

	if (InOutState.Defined.Num() + InOutState.DefinedLabels.Num() > MaxExpressions)
	{
		// `P6`: a cap, and the closing block names the call that returns the rest in full.
		return FString::Printf(TEXT("(capped %s)"), *KeyOf(InExpression));
	}

	InOutState.InProgress.Add(InExpression);

	const FString Caption = CaptionOf(InExpression);

	// The head carries the expression's own class, so the text says what the node IS. The identifier
	// rides in a comment on the same line, which is the shape `RenderNode` uses for a Blueprint node.
	FString Out = FString::Printf(TEXT("(%s ; %s"),
		*InExpression->GetClass()->GetName(), *KeyOf(InExpression));

	if (!Caption.IsEmpty())
	{
		Out += FString::Printf(TEXT("\n    ; caption %s"), *Caption);
	}

	// `2.5`: "Every input of every expression is printed: the linked value, or the constant the
	// expression keeps for that input when it is not linked."
	//
	// `GetInput` and `GetInputName` are the per-index accessors, which is what
	// `FExpressionInputIterator` is built on (`MaterialExpression.h:684-718`) and what the engine
	// deprecation notice for `GetInputsView` points at since 5.5. Using them rather than the
	// deprecated view keeps this off a removal path.
	for (int32 Index = 0; ; ++Index)
	{
		FExpressionInput* Input = const_cast<UMaterialExpression*>(InExpression)->GetInput(Index);
		if (Input == nullptr)
		{
			break;
		}

		const FName InputName = InExpression->GetInputName(Index);
		const FString NameText = InputName.IsNone()
			? FString::Printf(TEXT("input%d"), Index)
			: InputName.ToString();

		const FString Value = RenderInput(InExpression, Index, *Input, InOutState,
			InDepth + 1, InTokenBudget);

		if (Value.IsEmpty())
		{
			// `2.1.8`: an input left at its default is not printed, and `2.5` says the constant is
			// printed when the input keeps one. Empty means neither a link nor a constant, so there
			// is nothing to say. *** NOTHING IS COUNTED HERE: this loop has no cap, and the counter
			// that used to be incremented on this path and on the printing path was never read. ***
			continue;
		}

		Out += FString::Printf(TEXT("\n    %s %s"), *NameText, *Value);
	}

	InOutState.InProgress.Remove(InExpression);
	InOutState.Defined.Add(InExpression);

	Out += TEXT(")");
	return Out;
}

FString FTurboMCPMaterial::RenderInput(const UMaterialExpression* InOwner, int32 InInputIndex,
	const FExpressionInput& InInput, FRenderState& InOutState, int32 InDepth,
	int32 InTokenBudget)
{
	// The input's own name, used both for the text and to find the constant by reflection. Both use
	// the SAME source, so a printed name and a looked-up constant can never disagree.
	const FName InputName = InOwner->GetInputName(InInputIndex);
	const FString NameText = InputName.IsNone()
		? FString::Printf(TEXT("input%d"), InInputIndex)
		: InputName.ToString();

	if (InInput.Expression != nullptr)
	{
		// A PLAIN REROUTE IS INVISIBLE (`2.5`), so the link is followed to the value it really
		// carries. A NAMED reroute is not followed: it prints as its label, which is the point of it.
		UMaterialExpression* Real = ResolveThroughReroutes(InInput.Expression);

		FString Label;
		if (Real != nullptr && IsLabelUsage(Real, Label))
		{
			return FString::Printf(TEXT("@%s"), *Label);
		}
		if (Real != nullptr && IsLabelDeclaration(Real, Label))
		{
			if (InOutState.DefinedLabels.Contains(Label))
			{
				return FString::Printf(TEXT("@%s"), *Label);
			}
			// The declaration is being reached for the first time, so it is defined here rather than
			// referred to. `2.5` says a named reroute gets one definition.
			return RenderExpression(Real, InOutState, InDepth, InTokenBudget);
		}

		return RenderExpression(Real, InOutState, InDepth, InTokenBudget);
	}

	// NOT LINKED: the constant the expression keeps. `2.5` requires this to come from reflection.
	const FString Constant = ConstantForInput(InOwner, NameText);
	if (!Constant.IsEmpty())
	{
		return Constant;
	}

	// No link and no constant. `2.5` says every input is printed, so an input that keeps nothing is
	// still NAMED - as the text form's own marker, not as silence, because silence would read as "no
	// such input" rather than "this input keeps no value".
	return FString::Printf(TEXT("\"%s unset\""), *NameText);
}

void FTurboMCPMaterial::MarkReached(const UMaterialExpression* InExpression,
	FRenderState& InOutState)
{
	// Depth-first over the inputs, marking everything an output depends on. This is what makes the
	// unreached flag exact instead of a guess: an expression nothing marks cannot be reached from any
	// connected output, so it is either dead or a parameter waiting to be used.
	TSet<const UMaterialExpression*> Seen;
	TArray<const UMaterialExpression*> Stack;
	if (InExpression != nullptr)
	{
		Stack.Add(InExpression);
	}

	int32 Steps = 0;
	while (Stack.Num() > 0 && Steps < MaxExpressions * 4)
	{
		++Steps;
		const UMaterialExpression* Current = Stack.Pop();
		if (Current == nullptr || Seen.Contains(Current))
		{
			continue;
		}
		Seen.Add(Current);
		InOutState.Reached.Add(Current);

		// A FUNCTION OUTPUT IS AN EXPRESSION AND REACHES ITSELF.
		//
		// MEASURED, ON THE MATERIAL FIXTURE. A function whose output node was correctly wired
		// answered:
		//
		//     connected outputs: 1 of 1
		//     (Result
		//         @FunctionTint)
		//     1 expression(s) reach no output, so they do not affect what the material looks like:
		//       f877ef MaterialExpressionFunctionOutput Output Result
		//
		// THE OUTPUT NODE WAS NAMED AS REACHING NO OUTPUT, WHILE BEING THE OUTPUT. A function output
		// is a NODE in its expression list, unlike a material output which is a property of the
		// material and never appears in that list - so the material branch could not lose its output
		// this way and the function branch could.
		//
		// `RenderTree` marks each output SOURCE, which is the value feeding the node. For a material
		// that source is the whole story; for a function there is one more node between the source
		// and the output, and it is the one that makes the value leave. Marking it where the walk
		// starts covers every caller without each having to remember.

		for (FExpressionInputIterator It(const_cast<UMaterialExpression*>(Current)); It; ++It)
		{
			if (It.Input != nullptr && It.Input->Expression != nullptr)
			{
				// A plain reroute is transparent, so marking through it keeps the reach set the same
				// shape as the printed tree.
				Stack.Add(ResolveThroughReroutes(It.Input->Expression));
			}
		}

		// A LABEL USAGE IS A LINK TOO, AND NOT FOLLOWING IT MADE THE UNREACHED REPORT WRONG.
		//
		// MEASURED, ON THE MATERIAL FIXTURE, which is why the fixture exists. `VerticalMix` is
		// declared, read by two usages, and its declaration is reached from the BaseColor output
		// through one of them. The report still listed EIGHT expressions as reaching no output,
		// including `InnerProduct` - the shared value the fixture exists to have read twice:
		//
		//     8 expression(s) reach no output, so they do not affect what the material looks like:
		//       afb219 MaterialExpressionScalarParameter Param (0.75) 'LazyScalar'
		//       f8907d MaterialExpressionConstant3Vector 0.2,0.4,0.6
		//       ecaedc MaterialExpressionMultiply Multiply
		//       ...
		//
		// A named reroute's LINK IS ITS DECLARATION POINTER, not an `FExpressionInput`, so the input
		// iterator above can never see it. The declaration is what carries the value, so marking it -
		// and then its own inputs, which the loop then does - is what makes the reach set match the
		// printed tree.
		//
		// A finding that fires on every labelled value is worth no more than one that never fires: a
		// reader who sees eight unreached expressions learns nothing about which one is a mistake.
		// `Declaration` is preferred over `DeclarationGuid` for the same reason `LabelOfUsage`
		// prefers it - it is what the editor maintains, and the GUID is the reload fallback.
		if (const UMaterialExpressionNamedRerouteUsage* Usage =
				Cast<UMaterialExpressionNamedRerouteUsage>(Current))
		{
			if (Usage->Declaration != nullptr)
			{
				Stack.Add(Usage->Declaration);
			}
		}
	}
}

FString FTurboMCPMaterial::BudgetNote(const FString& InText, int32 InTokenBudget,
	const FString& InFullCall)
{
	const int32 Estimate = TurboMCPTokenEstimate(InText);
	FString Out = FString::Printf(TEXT("\n; %d characters, ~%d tokens (estimate)"),
		InText.Len(), Estimate);

	if (InTokenBudget > 0 && Estimate > InTokenBudget)
	{
		// `04_FORMATS.md` budget step 4: "Stop, and add a closing block that lists each reduced item
		// and the exact call that returns it in full." The reduction itself happened while rendering,
		// where a subtree could still be replaced by a reference; this is the note that says so.
		Out += FString::Printf(
			TEXT("\n; REDUCED: this is over the %d-token budget, so it is at ~%d. Deeper subtrees ")
			TEXT("were replaced by references rather than cut, because a form must never be cut in ")
			TEXT("the middle. The full text comes from %s without a token budget."),
			InTokenBudget, Estimate, *InFullCall);
	}

	// `P1`: a Read that produced nothing says WHY rather than being blank.
	if (InText.TrimStartAndEnd().IsEmpty())
	{
		Out += TEXT("\n; nothing was printed. That is a bug in this reader rather than an empty ")
			TEXT("material: every material has at least its own asset line.");
	}

	Out += TEXT("\n; the token estimate is an estimate, per 04_FORMATS.md section 5.");
	return Out;
}

FString FTurboMCPMaterial::RenderLabels(const FPlan& InPlan)
{
	const int32 Declarations = InPlan.DeclarationsByLabel.Num();
	const int32 Readers = InPlan.ReaderCountByLabel.Num();

	FString Out = FString::Printf(TEXT("read: named reroute labels of %s"),
		*InPlan.Asset);
	Out += FString::Printf(TEXT("\nlabels: %d declared, %d distinct name(s) read"),
		Declarations, Readers);

	// THE INSTANCE CASE IS THE SAME TABLE THE OTHER READERS PRINT, so a caller who asked for
	// overrides gets one shape rather than four.
	if (InPlan.Instance != nullptr)
	{
		Out += FString::Printf(TEXT("\n; this is a Material Instance, so it has no graph and no ")
			TEXT("labels. Its parent is %s."),
			InPlan.ParentPath.IsEmpty() ? TEXT("(not set)") : *InPlan.ParentPath);
		return Out;
	}

	if (Declarations == 0 && Readers == 0)
	{
		// `P1`: never return nothing, and say WHY. A material with no labels is a different answer
		// from a failed read, and the difference matters because the whole point of calling this
		// first is to learn whether the material is one connected picture.
		Out += TEXT("\nthis material declares no named reroute labels, and nothing reads one. Every ")
			TEXT("value is reached by a direct wire, so the graph is as many islands as it has ")
			TEXT("shared values. P-MAT-LABEL-PROMOTE proposes labels worth adding.");
		return Out;
	}

	// Declarations first, each with where it is and how many places read it.
	TArray<FString> Labels;
	InPlan.DeclarationsByLabel.GetKeys(Labels);
	Labels.Sort();

	TArray<FString> Unread;
	for (const FString& Label : Labels)
	{
		const UMaterialExpressionNamedRerouteDeclaration* Declaration =
			InPlan.DeclarationsByLabel[Label];
		const int32* Count = InPlan.ReaderCountByLabel.Find(Label);
		const int32 ReaderCount = Count != nullptr ? *Count : 0;

		Out += FString::Printf(TEXT("\n\n@%s"), *Label);
		Out += FString::Printf(TEXT("\n    declared at %s"),
			*IdOf(Declaration, InPlan.AssetName, InPlan.GraphName));
		Out += FString::Printf(TEXT("\n    at (%d, %d)"),
			Declaration != nullptr ? Declaration->MaterialExpressionEditorX : 0,
			Declaration != nullptr ? Declaration->MaterialExpressionEditorY : 0);
		Out += FString::Printf(TEXT("\n    read in %d place(s)"), ReaderCount);

		// WHAT THE DECLARATION FORWARDS, NAMED BY CLASS.
		//
		// *** THIS LINE EXISTS BECAUSE A BREAK TEST EXPOSED A BLIND SPOT, AND THE BLIND SPOT WAS IN THE
		// TEST RATHER THAN IN THE TOOL. *** `P/E-MAT-LABEL-PROMOTE` was deliberately broken so that each
		// declaration forwarded NOTHING - `Input.Expression` left null - and the suite that tests it
		// STILL PASSED, because `read in N place(s)` counts USAGE NODES, and a usage pointing at a
		// declaration that forwards nothing is still a usage. The material would have compiled to an
		// unconnected input and NO READER WOULD HAVE SAID SO.
		//
		// SO THE READER NOW NAMES THE FORWARDING SOURCE'S CLASS. It is the ONE fact that separates a
		// label that carries a value from a label that carries nothing, and it is readable without
		// compiling the material. A caller who sees `forwards NOTHING` is looking at a broken label, and
		// A READER THAT CANNOT DISTINGUISH THOSE TWO STATES IS NOT WITNESSING THE THING THAT MATTERS.
		if (Declaration != nullptr && Declaration->Input.Expression != nullptr)
		{
			Out += FString::Printf(TEXT("\n    forwards %s"),
				*Declaration->Input.Expression->GetClass()->GetName());
		}
		else
		{
			Out += TEXT("\n    forwards NOTHING: this declaration has no source expression, so every ")
				TEXT("usage of it reads an unconnected input.");
		}

		if (ReaderCount == 0)
		{
			// `03_TOOLS.md`: "the ones nothing reads." Named as such rather than omitted, because an
			// unread label is the finding this reader exists to produce.
			Out += TEXT("\n    UNREAD: nothing reads this label. It is either a leftover or a value ")
				TEXT("that was meant to be used.");
			Unread.Add(Label);
		}
	}

	// A USAGE WHOSE DECLARATION IS MISSING is a broken reference and is reported separately, because
	// it is not "a label nothing reads" - it is a reader pointing at nothing.
	TArray<FString> Dangling;
	for (const TPair<FString, int32>& Pair : InPlan.ReaderCountByLabel)
	{
		if (!InPlan.DeclarationsByLabel.Contains(Pair.Key))
		{
			Dangling.Add(Pair.Key);
		}
	}
	if (Dangling.Num() > 0)
	{
		Dangling.Sort();
		Out += FString::Printf(
			TEXT("\n\n%d name(s) are READ but never DECLARED in this material: %s"),
			Dangling.Num(), *FString::Join(Dangling, TEXT(", ")));
		Out += TEXT("\n; a usage whose declaration is gone is a broken reference, not a label ")
			TEXT("with no readers.");
	}

	if (Unread.Num() > 0)
	{
		Out += FString::Printf(TEXT("\n\n%d label(s) nothing reads: %s"),
			Unread.Num(), *FString::Join(Unread, TEXT(", ")));
	}

	Out += FString::Printf(TEXT("\n; %d expression(s) in the graph, %d comment box(es)"),
		InPlan.Expressions.Num(), InPlan.Comments.Num());
	Out += TEXT("\n; call this first on any material: a label is the only thing that keeps a ")
		TEXT("material as one connected picture rather than islands.");
	Out += BudgetNote(Out, 0, FString());
	return Out;
}

FString FTurboMCPMaterial::RenderTree(FPlan& InPlan, bool bTraceOnly, int32 InTokenBudget)
{
	FString Out = bTraceOnly
		? FString::Printf(TEXT("read: %s, output %s"), *InPlan.Asset, *InPlan.OutputProperty)
		: FString::Printf(TEXT("read: %s"), *InPlan.Asset);

	// THE INSTANCE TABLE, when overrides were asked for. `03_TOOLS.md`: "which are then listed as a
	// short table." The table names the parameter, its association and its value, and it carries the
	// parent's path because an override is meaningless without knowing what it overrides.
	if (InPlan.Instance != nullptr)
	{
		Out += FString::Printf(TEXT("\n; this is a Material Instance. Its parent is %s."),
			InPlan.ParentPath.IsEmpty() ? TEXT("(not set)") : *InPlan.ParentPath);

		const int32 Scalars = InPlan.Instance->ScalarParameterValues.Num();
		const int32 Vectors = InPlan.Instance->VectorParameterValues.Num();
		const int32 Textures = InPlan.Instance->TextureParameterValues.Num();
		const int32 StaticSwitches = InPlan.Instance->GetStaticParameters().StaticSwitchParameters.Num();

		Out += FString::Printf(
			TEXT("\noverrides: %d scalar, %d vector, %d texture, %d static switch"),
			Scalars, Vectors, Textures, StaticSwitches);

		if (Scalars + Vectors + Textures + StaticSwitches == 0)
		{
			Out += TEXT("\nthis instance overrides nothing, so it shows exactly what its parent ")
				TEXT("shows. Read the parent for the graph.");
			return Out;
		}

		Out += TEXT("\n\n; name association value");
		for (const FScalarParameterValue& Value : InPlan.Instance->ScalarParameterValues)
		{
			Out += FString::Printf(TEXT("\n%s %s %g"),
				*Value.ParameterInfo.Name.ToString(),
				*AssociationName(Value.ParameterInfo.Association),
				Value.ParameterValue);
		}
		for (const FVectorParameterValue& Value : InPlan.Instance->VectorParameterValues)
		{
			const FLinearColor Colour = Value.ParameterValue;
			Out += FString::Printf(TEXT("\n%s %s (%g, %g, %g, %g)"),
				*Value.ParameterInfo.Name.ToString(),
				*AssociationName(Value.ParameterInfo.Association),
				Colour.R, Colour.G, Colour.B, Colour.A);
		}
		for (const FTextureParameterValue& Value : InPlan.Instance->TextureParameterValues)
		{
			Out += FString::Printf(TEXT("\n%s %s %s"),
				*Value.ParameterInfo.Name.ToString(),
				*AssociationName(Value.ParameterInfo.Association),
				Value.ParameterValue != nullptr
					? *Value.ParameterValue->GetPathName() : TEXT("(none)"));
		}
		return Out;
	}

	// PARAMETERS COME FIRST, which `03_TOOLS.md` requires: "parameters with defaults and groups, then
	// every connected output property as an expression tree."
	//
	// A parameter expression IS the parameter, so they are found by class rather than from a separate
	// list. `UMaterialExpressionParameter` is the base every parameter derives from
	// (`MaterialExpressionParameter.h:22`) and it carries the name and the group.
	TArray<UMaterialExpression*> Parameters;
	for (UMaterialExpression* Expression : InPlan.Expressions)
	{
		if (Expression != nullptr
			&& Expression->GetClass()->IsChildOf(UMaterialExpressionParameter::StaticClass()))
		{
			Parameters.Add(Expression);
		}
	}

	Out += FString::Printf(TEXT("\nparameters: %d"), Parameters.Num());
	if (Parameters.Num() > 0)
	{
		TArray<FString> ParameterLines;
		for (UMaterialExpression* Expression : Parameters)
		{
			const UMaterialExpressionParameter* Parameter =
				Cast<UMaterialExpressionParameter>(Expression);
			const FName Name = Parameter != nullptr ? Parameter->ParameterName : NAME_None;
			const FName Group = Parameter != nullptr ? Parameter->Group : NAME_None;

			ParameterLines.Add(FString::Printf(TEXT("\n%s %s%s ; %s"),
				Name.IsNone() ? TEXT("(unnamed)") : *Name.ToString(),
				*Expression->GetClass()->GetName(),
				Group.IsNone() ? TEXT("") : *FString::Printf(TEXT(" group=%s"), *Group.ToString()),
				*KeyOf(Expression)));
		}
		ParameterLines.Sort();
		for (const FString& Line : ParameterLines)
		{
			Out += Line;
		}
	}

	// THE OUTPUTS, one top-level form each, which `2.5` requires: "Outputs as top level forms, one per
	// connected output property."
	//
	// `FRenderState` HAS NO `Plan` FIELD. Every renderer takes the plan as its own argument, so a field
	// holding it would be assigned here and read nowhere. The header records the same absence.
	FRenderState State;

	TArray<FString> Properties = bTraceOnly
		? TArray<FString>{ InPlan.OutputProperty }
		: ConnectedOutputs(InPlan);

	Out += FString::Printf(TEXT("\nconnected outputs: %d of %d"),
		bTraceOnly ? 1 : Properties.Num(), AllOutputProperties(InPlan).Num());

	for (const FString& Property : Properties)
	{
		UMaterialExpression* Source = OutputSource(InPlan, Property);
		if (Source == nullptr)
		{
			continue;
		}
		MarkReached(Source, State);
	}

	for (const FString& Property : Properties)
	{
		UMaterialExpression* Source = OutputSource(InPlan, Property);
		Out += FString::Printf(TEXT("\n\n(%s"), *Property);
		if (Source == nullptr)
		{
			// Only reachable at Trace, where the plan already refused an unconnected property. Kept
			// because a plan is made and printed in one call but the guard costs nothing and a silent
			// gap is what `2.1.12` forbids.
			Out += TEXT("\n    ; error: this output is not connected");
			Out += TEXT(")");
			continue;
		}

		// A LABELLED OUTPUT PRINTS ITS LABEL AND STOPS. `2.5` says a value that has a label is
		// referred to only by its label, and the label's own definition appears in the labels section
		// or where it is first reached. Printing the subtree here as well would be the second way of
		// naming one value that the rule exists to prevent.
		FString Label;
		if (IsLabelDeclaration(Source, Label) || IsLabelUsage(Source, Label))
		{
			Out += FString::Printf(TEXT("\n    @%s"), *Label);
		}
		else
		{
			Out += FString::Printf(TEXT("\n    %s"),
				*RenderExpression(Source, State, 0, InTokenBudget));
		}
		Out += TEXT(")");
	}

	// THE LABELS SECTION, when the tree reached any label. `2.5`: "Named reroute labels as `@Label`,
	// defined once, in a labels section."
	const int32 DefinedLabels = State.DefinedLabels.Num();
	if (DefinedLabels > 0)
	{
		Out += FString::Printf(TEXT("\n\n; %d label(s) are defined inside the trees above"),
			DefinedLabels);
	}

	// UNREACHED EXPRESSIONS AND PARAMETERS, which `03_TOOLS.md` requires: "Parameters and expressions
	// that reach no output are flagged."
	//
	// REACHABILITY IS COMPUTED FROM THE OUTPUTS, not from "has a consumer", and the two differ: a
	// value that feeds only another unreached value has a consumer and still reaches nothing. The
	// first version of this used `HasAnyConsumer` and would have missed exactly that case.
	TArray<FString> Unreached;
	for (UMaterialExpression* Expression : InPlan.Expressions)
	{
		// A FUNCTION OUTPUT NODE IS AN OUTPUT, SO IT CANNOT "REACH NO OUTPUT" - AND IT WAS LISTED
		// AS ONE.
		//
		// MEASURED, ON THE MATERIAL FIXTURE. A function whose output was correctly wired, and whose
		// value resolved to the right label, still answered:
		//
		//     connected outputs: 1 of 1
		//     (Result
		//         @FunctionTint)
		//     1 expression(s) reach no output, so they do not affect what the material looks like:
		//       f877ef MaterialExpressionFunctionOutput Output Result
		//
		// THE OUTPUT NODE WAS NAMED AS REACHING NO OUTPUT WHILE BEING THE OUTPUT. It could only
		// happen on the function branch: a function output is an expression IN the function
		// expression list, while a material output is a property of the material and never appears
		// in that list. So the material branch can never lose its output this way and the function
		// branch could.
		//
		// EXCLUDED BY ITS NATURE RATHER THAN BY MARKING IT REACHED, which is the honest rule: the
		// node is a DESTINATION a value leaves through, not a value that feeds another. Saying it
		// reaches no output is not a finding, it is a contradiction. The same reasoning applies to a
		// function output whose `A` is unconnected - it is an empty output, which is a different
		// report and not this one.
		if (Cast<UMaterialExpressionFunctionOutput>(Expression) != nullptr)
		{
			continue;
		}

		if (Expression != nullptr && !State.Reached.Contains(Expression))
		{
			Unreached.Add(FString::Printf(TEXT("\n  %s %s %s"),
				*KeyOf(Expression),
				*Expression->GetClass()->GetName(),
				*CaptionOf(Expression)));
		}
	}

	if (Unreached.Num() > 0)
	{
		Out += FString::Printf(
			TEXT("\n\n%d expression(s) reach no output, so they do not affect what the material ")
			TEXT("looks like:"), Unreached.Num());
		for (const FString& Line : Unreached)
		{
			Out += Line;
		}
		Out += TEXT("\n; a parameter in this list is a parameter nothing uses. That is a finding, ")
			TEXT("not an omission: it is the one thing a material reader can say that the editor ")
			TEXT("cannot show at a glance.");
	}

	// `2.1.12`: anything that could not be printed is a comment line, never a silent gap.
	for (const FString& Note : State.Notes)
	{
		Out += TEXT("\n") + Note;
	}

	if (bTraceOnly)
	{
		Out += FString::Printf(
			TEXT("\n\n; only %s is printed, and only the labels and shared values inside its tree. ")
			TEXT("R-MAT-LOAD prints every output of this material."), *InPlan.OutputProperty);
	}

	Out += BudgetNote(Out, InTokenBudget,
		bTraceOnly ? TEXT("R-MAT-TRACE") : TEXT("R-MAT-LOAD"));
	return Out;
}

int32 FTurboMCPMaterial::BlockIndexOf(const FPlan& InPlan, const UMaterialExpression* InExpression)
{
	if (InExpression == nullptr)
	{
		return INDEX_NONE;
	}

	// THE SMALLEST CONTAINING BOX WINS, so a box inside a box owns its own members rather than both
	// claiming them. That is the innermost-wins rule this project reports for Blueprint comment nesting,
	// and a different rule here would make the two tools disagree about one picture.
	int32 BestIndex = INDEX_NONE;
	int64 BestArea = MAX_int64;

	for (int32 Index = 0; Index < InPlan.Comments.Num(); ++Index)
	{
		const UMaterialExpressionComment* Comment = InPlan.Comments[Index];
		if (Comment == nullptr)
		{
			continue;
		}

		const int32 Left = Comment->MaterialExpressionEditorX;
		const int32 Top = Comment->MaterialExpressionEditorY;
		const int32 Right = Left + Comment->SizeX;
		const int32 Bottom = Top + Comment->SizeY;

		if (InExpression->MaterialExpressionEditorX >= Left
			&& InExpression->MaterialExpressionEditorX <= Right
			&& InExpression->MaterialExpressionEditorY >= Top
			&& InExpression->MaterialExpressionEditorY <= Bottom)
		{
			// `int64` RATHER THAN `int32`, because a large box's area overflows `int32` and a negative
			// area would then win the comparison - a bug that would only appear on a big material and
			// would look like the wrong box owning a node.
			const int64 Area = static_cast<int64>(Right - Left) * static_cast<int64>(Bottom - Top);
			if (Area < BestArea)
			{
				BestArea = Area;
				BestIndex = Index;
			}
		}
	}

	return BestIndex;
}

FString FTurboMCPMaterial::RenderBlocks(FPlan& InPlan, int32 InTokenBudget)
{
	FString Out = FString::Printf(TEXT("read: %s by block of logic"), *InPlan.Asset);

	if (InPlan.Instance != nullptr)
	{
		Out += FString::Printf(TEXT("\n; this is a Material Instance, so it has no blocks. Its ")
			TEXT("parent is %s."),
			InPlan.ParentPath.IsEmpty() ? TEXT("(not set)") : *InPlan.ParentPath);
		return Out;
	}

	Out += FString::Printf(TEXT("\nblocks: %d comment box(es), %d expression(s)"),
		InPlan.Comments.Num(), InPlan.Expressions.Num());

	if (InPlan.Comments.Num() == 0)
	{
		// `P1`. A material with no boxes has one block, which is itself the finding.
		Out += TEXT("\nthis material has no comment box, so it is one block. R-MAT-LOAD prints it ")
			TEXT("as a whole; P-COMMENT adds boxes to a Blueprint, and a material box is added in ")
			TEXT("the material editor for now.");
		return Out;
	}

	// MEMBERSHIP IS GEOMETRIC AND IS COMPUTED BY `BlockIndexOf`, WHICH IS PUBLIC AND SHARED.
	//
	// A material comment box holds no list of members; the editor decides membership when it lays the
	// graph out, exactly as `SGraphNodeComment` does for a Blueprint. This project MEASURED that the
	// Blueprint side's cache is filled from RENDERED widgets and is therefore empty headlessly - see the
	// long note in `TurboMCPComment.cpp` - and a material box has no equivalent accessor at all.
	//
	// THIS GEOMETRY IS SHARED RATHER THAN DUPLICATED: `P/E-MAT-BLOCKS` must decide which values CROSS a
	// block boundary, so it needs exactly this answer - and two copies of a geometric rule drift the
	// moment either is adjusted. A writer that disagreed with this reader about which box holds what
	// would move values between blocks this reader does not believe in.
	struct FBox
	{
		UMaterialExpressionComment* Comment = nullptr;
		int32 Left = 0;
		int32 Top = 0;
		int32 Right = 0;
		int32 Bottom = 0;
		TArray<UMaterialExpression*> Members;
	};

	TArray<FBox> Boxes;
	for (UMaterialExpressionComment* Comment : InPlan.Comments)
	{
		if (Comment == nullptr)
		{
			continue;
		}
		FBox Box;
		Box.Comment = Comment;
		Box.Left = Comment->MaterialExpressionEditorX;
		Box.Top = Comment->MaterialExpressionEditorY;
		Box.Right = Comment->MaterialExpressionEditorX + Comment->SizeX;
		Box.Bottom = Comment->MaterialExpressionEditorY + Comment->SizeY;
		Boxes.Add(MoveTemp(Box));
	}

	// Which box an expression belongs to: the SMALLEST one that contains it, so a box inside a box
	// owns its own members rather than both claiming them. That is the same "innermost wins" rule the
	// comment reader reports for Blueprint nesting, and using a different one here would make the two
	// tools disagree about the same picture.
	auto BoxFor = [&InPlan](const UMaterialExpression* Expression, int32& OutIndex) -> bool
	{
		OutIndex = FTurboMCPMaterial::BlockIndexOf(InPlan, Expression);
		return OutIndex != INDEX_NONE;
	};

	for (UMaterialExpression* Expression : InPlan.Expressions)
	{
		if (Expression == nullptr)
		{
			continue;
		}
		int32 Index = INDEX_NONE;
		if (BoxFor(Expression, Index))
		{
			Boxes[Index].Members.Add(Expression);
		}
	}

	// The findings, gathered per box. Each one carries node IDs, which `03_TOOLS.md` requires.
	TArray<FString> CrossBoxWires;
	TArray<FString> InBoxLabels;
	TArray<FString> DuplicateUsages;

	for (int32 Index = 0; Index < Boxes.Num(); ++Index)
	{
		FBox& Box = Boxes[Index];
		const FString BoxName = Box.Comment->Text.IsEmpty()
			? FString::Printf(TEXT("(untitled %s)"), *KeyOf(Box.Comment))
			: Box.Comment->Text;

		Out += FString::Printf(TEXT("\n\n(%s ; %s ; %dx%d at %d,%d"),
			*Quoted(BoxName), *KeyOf(Box.Comment),
			Box.Right - Box.Left, Box.Bottom - Box.Top, Box.Left, Box.Top);

		if (Box.Members.Num() == 0)
		{
			// An empty box is named plainly. It is a block with no logic, which is a finding rather
			// than an error: the box is probably a leftover.
			Out += TEXT("\n    ; this box holds no expression. It is a label with nothing to label.");
			Out += TEXT(")");
			continue;
		}

		// ITS INPUTS: labels or raw wires that come in from outside. A label arriving is clean; a raw
		// wire crossing a box boundary is the finding `03_TOOLS.md` names.
		TArray<FString> Inputs;
		TArray<FString> Outputs;

		TSet<UMaterialExpression*> MemberSet;
		for (UMaterialExpression* Member : Box.Members)
		{
			MemberSet.Add(Member);
		}

		for (UMaterialExpression* Member : Box.Members)
		{
			for (FExpressionInputIterator It(Member); It; ++It)
			{
				UMaterialExpression* Source = It.Input != nullptr
					? ResolveThroughReroutes(It.Input->Expression) : nullptr;
				if (Source == nullptr || MemberSet.Contains(Source))
				{
					continue;
				}

				FString Label;
				if (IsLabelUsage(Source, Label) || IsLabelDeclaration(Source, Label))
				{
					Inputs.Add(FString::Printf(TEXT("@%s"), *Label));
				}
				else
				{
					Inputs.Add(FString::Printf(TEXT("(raw %s %s)"),
						*KeyOf(Source), *CaptionOf(Source)));
					CrossBoxWires.Add(FString::Printf(
						TEXT("  %s -> %s : %s crosses into %s without a label"),
						*KeyOf(Source), *KeyOf(Member), *CaptionOf(Source), *BoxName));
				}
			}
		}

		// ITS OUTPUTS: values that leave the box, however they leave.
		for (UMaterialExpression* Member : Box.Members)
		{
			for (UMaterialExpression* Other : InPlan.Expressions)
			{
				if (Other == nullptr || MemberSet.Contains(Other))
				{
					continue;
				}
				for (FExpressionInputIterator It(Other); It; ++It)
				{
					// `It.Input` IS A POINTER, so the member is reached with `->`. Read from
					// `Engine/Public/Materials/MaterialExpression.h`: the iterator holds
					// `FExpressionInput* Input;`. The dot form compiles nowhere, and this was the
					// `error C2228: left of '.Expression' must have class/struct/union` the build
					// reported - at a line number one off, which is why it read as a puzzle. The loop
					// 40 lines above already had it right.
					UMaterialExpression* Source = It.Input != nullptr
						? ResolveThroughReroutes(It.Input->Expression) : nullptr;
					if (Source == Member)
					{
						FString Label;
						const bool bLabelled = IsLabelDeclaration(Member, Label);
						Outputs.Add(bLabelled
							? FString::Printf(TEXT("@%s"), *Label)
							: FString::Printf(TEXT("(raw %s %s)"),
								*KeyOf(Member), *CaptionOf(Member)));
						break;
					}
				}
			}
		}

		Inputs.Sort();
		Outputs.Sort();
		Out += FString::Printf(TEXT("\n    %d expression(s)"), Box.Members.Num());
		Out += FString::Printf(TEXT("\n    inputs: %s"),
			Inputs.Num() > 0 ? *FString::Join(Inputs, TEXT(", ")) : TEXT("(none)"));
		Out += FString::Printf(TEXT("\n    outputs: %s"),
			Outputs.Num() > 0 ? *FString::Join(Outputs, TEXT(", ")) : TEXT("(none)"));

		for (UMaterialExpression* Member : Box.Members)
		{
			Out += FString::Printf(TEXT("\n    %s %s %s"),
				*KeyOf(Member), *Member->GetClass()->GetName(), *CaptionOf(Member));
		}

		// FINDING: a label READ inside its own box. `03_TOOLS.md` names this one specifically - a
		// label whose declaration and readers are in one box buys nothing, because the box already
		// says they belong together.
		TSet<UMaterialExpression*> DeclarationInBox;
		for (UMaterialExpression* Member : Box.Members)
		{
			FString Label;
			if (IsLabelDeclaration(Member, Label))
			{
				DeclarationInBox.Add(Member);
			}
		}
		TMap<FString, int32> UsagesByName;
		for (UMaterialExpression* Member : Box.Members)
		{
			FString Label;
			if (!IsLabelUsage(Member, Label))
			{
				continue;
			}
			// NOT CONST. `FindOrAdd` answers a reference TO THE STORED VALUE, and the line below
			// increments it, so a `const int32&` here was `C3892: cannot assign to a variable that
			// is const`. The build caught it; nothing else would have, because the reference is only
			// ever wrong at the moment someone writes through it.
			int32& Count = UsagesByName.FindOrAdd(Label);
			++Count;

			const int32* Declared = InPlan.ReaderCountByLabel.Find(Label);

			// THE DECLARATION'S OWN BOX IS COMPARED WITH THIS BOX, so a label whose declaration and
			// readers share a box is found. `DeclarationBox` IS A LOCAL BY VALUE: satisfying the
			// out-parameter with a heap `int32` would leak it on every usage, in a Read tool.
			int32 DeclarationBox = INDEX_NONE;
			const bool bHasDeclaration = InPlan.DeclarationsByLabel.Contains(Label);
			if (bHasDeclaration)
			{
				BoxFor(InPlan.DeclarationsByLabel[Label], DeclarationBox);
			}
			const bool bDeclaredInThisBox = bHasDeclaration && DeclarationBox == Index;

			if (Declared != nullptr && bDeclaredInThisBox)
			{
				InBoxLabels.Add(FString::Printf(
					TEXT("  @%s is declared AND read inside %s, at %s"),
					*Label, *BoxName, *KeyOf(Member)));
			}
		}

		// FINDING: two usage nodes of one label in one box, which `03_TOOLS.md` names. Two usages of
		// one label inside one box is one value named twice in one place.
		for (const TPair<FString, int32>& Pair : UsagesByName)
		{
			if (Pair.Value > 1)
			{
				DuplicateUsages.Add(FString::Printf(
					TEXT("  @%s has %d usage node(s) inside %s"),
					*Pair.Key, Pair.Value, *BoxName));
			}
		}

		Out += TEXT(")");
	}

	// The expressions that fell in no box are named, because a block view that silently dropped them
	// would look complete while missing part of the material.
	int32 Unboxed = 0;
	for (UMaterialExpression* Expression : InPlan.Expressions)
	{
		int32 Index = INDEX_NONE;
		if (Expression != nullptr && !BoxFor(Expression, Index))
		{
			++Unboxed;
		}
	}
	if (Unboxed > 0)
	{
		Out += FString::Printf(
			TEXT("\n\n%d expression(s) are inside no comment box. R-MAT-LOAD prints them as part ")
			TEXT("of the whole material."), Unboxed);
	}

	// THE FINDINGS SECTION, which is the reason this reader exists: it says what is wrong with the
	// material AS BLOCKS rather than what the blocks contain.
	const bool bAnyFinding =
		CrossBoxWires.Num() > 0 || InBoxLabels.Num() > 0 || DuplicateUsages.Num() > 0;

	if (!bAnyFinding)
	{
		Out += TEXT("\n\nfindings: none. No value crosses a box without a label, no label is read ")
			TEXT("inside its own box, and no label has two usage nodes in one box.");
	}
	else
	{
		Out += FString::Printf(TEXT("\n\nfindings: %d"),
			CrossBoxWires.Num() + InBoxLabels.Num() + DuplicateUsages.Num());

		if (CrossBoxWires.Num() > 0)
		{
			Out += FString::Printf(TEXT("\n%d value(s) cross between boxes without a label:"),
				CrossBoxWires.Num());
			for (const FString& Line : CrossBoxWires)
			{
				Out += TEXT("\n") + Line;
			}
			Out += TEXT("\n; a wire crossing a box boundary is what a label is for. ")
				TEXT("P-MAT-LABEL-PROMOTE proposes the labels.");
		}
		if (InBoxLabels.Num() > 0)
		{
			Out += FString::Printf(TEXT("\n%d label(s) are read inside their own box:"),
				InBoxLabels.Num());
			for (const FString& Line : InBoxLabels)
			{
				Out += TEXT("\n") + Line;
			}
		}
		if (DuplicateUsages.Num() > 0)
		{
			Out += FString::Printf(
				TEXT("\n%d label(s) have two usage nodes in one box:"), DuplicateUsages.Num());
			for (const FString& Line : DuplicateUsages)
			{
				Out += TEXT("\n") + Line;
			}
		}
	}

	Out += BudgetNote(Out, InTokenBudget, TEXT("R-MAT-BLOCKS"));
	return Out;
}
