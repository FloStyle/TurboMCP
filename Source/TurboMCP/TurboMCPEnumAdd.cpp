// Copyright FloStyle. All Rights Reserved.
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "TurboMCPEnumAdd.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Containers/UnrealString.h"
#include "Engine/UserDefinedEnum.h"
#include "Kismet2/EnumEditorUtils.h"
#include "Misc/Paths.h"
#include "UObject/Class.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "UObject/UnrealType.h"
#include "UObject/UObjectGlobals.h"

namespace
{
	/** True when the string is usable as an enumerator name. */
	bool IsValidEnumeratorName(const FString& InName, FString& OutWhy)
	{
		if (InName.IsEmpty())
		{
			OutWhy = TEXT("it is empty");
			return false;
		}
		if (!FChar::IsAlpha(InName[0]) && InName[0] != TEXT('_'))
		{
			OutWhy = TEXT("it does not start with a letter or underscore");
			return false;
		}
		for (const TCHAR Character : InName)
		{
			if (!FChar::IsAlnum(Character) && Character != TEXT('_'))
			{
				OutWhy = FString::Printf(
					TEXT("'%c' is not a letter, a digit or an underscore"), Character);
				return false;
			}
		}
		if (InName.Equals(TEXT("_MAX"), ESearchCase::IgnoreCase))
		{
			// Every user-defined enum carries a `_MAX` entry the editor owns.
			OutWhy = TEXT("_MAX is reserved: every user-defined enum carries one");
			return false;
		}
		return true;
	}
}

bool FTurboMCPEnumAdd::Plan(const FString& InPath, const FString& InValues,
	FPlan& OutPlan, FString& OutError)
{
	OutError.Empty();

	const FString Path = InPath.TrimStartAndEnd();
	if (Path.IsEmpty())
	{
		OutError = TEXT("no path given. Pass a package path, for example ")
			TEXT("/Game/Tests/MyEnum.");
		return false;
	}
	if (!Path.StartsWith(TEXT("/")))
	{
		OutError = FString::Printf(
			TEXT("'%s' does not start with '/'. A package path is absolute, for example ")
			TEXT("/Game/Tests/MyEnum."), *Path);
		return false;
	}

	OutPlan.PackagePath = Path;
	OutPlan.AssetName = FPaths::GetCleanFilename(Path);
	if (OutPlan.AssetName.IsEmpty())
	{
		OutError = FString::Printf(TEXT("'%s' has no asset name in it."), *Path);
		return false;
	}

	// The values, as `Name` or `Name=Display Name`, separated by commas or pipes. The same
	// separators the rest of the plugin accepts, so a caller does not learn a second set.
	TArray<FString> Parts;
	InValues.TrimStartAndEnd().ParseIntoArray(Parts, TEXT(","), /*CullEmpty=*/true);
	if (Parts.Num() == 0)
	{
		InValues.TrimStartAndEnd().ParseIntoArray(Parts, TEXT("|"), /*CullEmpty=*/true);
	}

	if (Parts.Num() == 0)
	{
		// An enum with no values cannot be written back to, so it is refused rather than
		// created empty and left for the caller to discover.
		OutError = TEXT("no values given. Pass them as a comma-separated list, for example ")
			TEXT("Red,Green=Dark Green,Blue.");
		return false;
	}

	TSet<FString> Seen;
	for (const FString& Raw : Parts)
	{
		FString Name = Raw.TrimStartAndEnd();
		FString DisplayName;

		FString Left;
		FString Right;
		if (Name.Split(TEXT("="), &Left, &Right))
		{
			Name = Left.TrimStartAndEnd();
			DisplayName = Right.TrimStartAndEnd();
		}

		FString Why;
		if (!IsValidEnumeratorName(Name, Why))
		{
			OutError = FString::Printf(
				TEXT("'%s' cannot be an enumerator name: %s."), *Name, *Why);
			return false;
		}

		if (Seen.Contains(Name))
		{
			// A duplicate would produce an enum the engine cannot look up reliably, so it
			// is refused by name rather than silently collapsed.
			OutError = FString::Printf(
				TEXT("'%s' is given twice. Each value must have its own name."), *Name);
			return false;
		}
		Seen.Add(Name);

		FValue Value;
		Value.Name = Name;
		Value.DisplayName = DisplayName;
		OutPlan.Values.Add(Value);
	}

	return true;
}

FString FTurboMCPEnumAdd::RenderPreview(const FPlan& InPlan)
{
	FString Out = FString::Printf(TEXT("preview: enum %s"), *InPlan.PackagePath);
	Out += FString::Printf(TEXT("\nvalues: %d"), InPlan.Values.Num());

	for (const FValue& Value : InPlan.Values)
	{
		if (Value.DisplayName.IsEmpty())
		{
			Out += FString::Printf(TEXT("\n  %s"), *Value.Name);
		}
		else
		{
			// The display name is printed only when it differs from the name, because a
			// line that repeats the name says nothing.
			Out += FString::Printf(TEXT("\n  %s  shown as \"%s\""),
				*Value.Name, *Value.DisplayName);
		}
	}

	// Whether the asset already exists is the fact that decides whether the Edit will be
	// refused, so the preview states it rather than leaving it to be discovered.
	const FString ObjectPath = FString::Printf(TEXT("%s.%s"),
		*InPlan.PackagePath, *InPlan.AssetName);
	const bool bExists = FindObject<UObject>(nullptr, *ObjectPath) != nullptr
		|| FPackageName::DoesPackageExist(InPlan.PackagePath);

	if (bExists)
	{
		Out += FString::Printf(
			TEXT("\nthis asset already exists: the edit will REFUSE rather than overwrite ")
			TEXT("it. Pick another path, or delete the asset first."));
	}
	else
	{
		Out += TEXT("\nthis asset does not exist yet, so the edit will create it.");
	}

	Out += TEXT("\n; the enum is created through the editor's own ")
		+ FString(TEXT("FEnumEditorUtils, so it stays editable in the enum editor."));
	return Out;
}

FString FTurboMCPEnumAdd::Apply(const FPlan& InPlan)
{
	const FString ObjectPath = FString::Printf(TEXT("%s.%s"),
		*InPlan.PackagePath, *InPlan.AssetName);

	// REFUSED RATHER THAN OVERWRITTEN. A name someone else chose is not this tool's to
	// replace, which is what `P8` means by never leaving the user worse off.
	if (FindObject<UObject>(nullptr, *ObjectPath) != nullptr
		|| FPackageName::DoesPackageExist(InPlan.PackagePath))
	{
		return FString::Printf(
			TEXT("unchanged: %s already exists, so it was not touched. Pick another path, ")
			TEXT("or delete the asset first."), *InPlan.PackagePath);
	}

	UPackage* Package = CreatePackage(*InPlan.PackagePath);
	if (Package == nullptr)
	{
		return FString::Printf(
			TEXT("error: could not create the package %s."), *InPlan.PackagePath);
	}

	// The engine's own creator, not a hand-built UUserDefinedEnum: it maintains the
	// display-name map and the `_MAX` entry the editor relies on.
	UEnum* Enum = FEnumEditorUtils::CreateUserDefinedEnum(
		Package, FName(*InPlan.AssetName), RF_Public | RF_Standalone);

	if (Enum == nullptr)
	{
		return FString::Printf(
			TEXT("error: the engine's enum creator refused to make %s."), *InPlan.AssetName);
	}

	UUserDefinedEnum* UserEnum = Cast<UUserDefinedEnum>(Enum);
	if (UserEnum == nullptr)
	{
		return FString::Printf(
			TEXT("error: %s was created but is not a user-defined enum, so it cannot be ")
			TEXT("edited in the enum editor."), *InPlan.AssetName);
	}

	// One enumerator per value, through the engine's utility so the internal `_MAX` entry
	// stays in the right place.
	for (int32 Index = 0; Index < InPlan.Values.Num(); ++Index)
	{
		FEnumEditorUtils::AddNewEnumeratorForUserDefinedEnum(UserEnum);
	}

	// THE NAMES ARE SET THROUGH `UEnum::SetEnums`, WHICH IS THE ENGINE'S OWN API.
	//
	// The first version of this tried to write the private `Names` array by reflection and
	// could not: `UEnum::Names` is a private UNION of a `const FName*` used by compiled-in
	// enums and a `TArray` used by user-defined ones, and it carries no `UPROPERTY`, so
	// `FindFProperty` does not see it. The tool detected that and said so rather than
	// writing a wrong name, which is how the mistake was caught.
	//
	// `SetEnums` is what `FEnumEditorUtils` itself calls, so this is the same path the enum
	// editor takes. It rebuilds the name list in one step, which is also why it is safer
	// than poking entries: there is no half-renamed state to leave behind.
	//
	// The names carry the enum's own prefix, `<EnumName>::<Value>`, because
	// `GenerateFullEnumName` is what the engine would produce and a bare name would not
	// resolve through `GetIndexByName`.
	TArray<TPair<FName, int64>> NewNames;
	for (int32 Index = 0; Index < InPlan.Values.Num(); ++Index)
	{
		const FString FullName = UserEnum->GenerateFullEnumName(
			*InPlan.Values[Index].Name);
		NewNames.Emplace(FName(*FullName), Index);
	}

	const bool bNamesSet = UserEnum->SetEnums(
		NewNames,
		UserEnum->GetCppForm(),
		UserEnum->GetUnderlyingType(),
		// `EEnumFlags` is a GLOBAL enum in `UObject/ObjectMacros.h`, not a member of
		// `UEnum`; `UEnum::EAddMaxKeyIfMissing` and `UEnum::EUnderlyingType` are members.
		EEnumFlags::None,
		UEnum::EAddMaxKeyIfMissing::Yes);

	if (!bNamesSet)
	{
		// `SetEnums` returns false when a `_MAX` entry already exists and is not last, which
		// means the enum is in a state the caller cannot use. Saying so beats reporting a
		// success whose names may not have landed.
		return FString::Printf(
			TEXT("error: %s was created but the engine refused the value names, because ")
			TEXT("its _MAX entry is not in a state the engine accepts. The asset exists ")
			TEXT("and is editable; set the values in the enum editor."), *InPlan.AssetName);
	}

	int32 Renamed = NewNames.Num();

	// The display names, through the engine's own setter, which validates them.
	int32 ShownAs = 0;
	for (int32 Index = 0; Index < InPlan.Values.Num(); ++Index)
	{
		if (InPlan.Values[Index].DisplayName.IsEmpty())
		{
			continue;
		}
		if (FEnumEditorUtils::SetEnumeratorDisplayName(UserEnum, Index,
				FText::FromString(InPlan.Values[Index].DisplayName)))
		{
			++ShownAs;
		}
	}

	FEnumEditorUtils::EnsureAllDisplayNamesExist(UserEnum);

	// The asset registry has to learn about the new asset, and the package has to be saved,
	// or the enum dies with the editor exactly as the first test assets did.
	FAssetRegistryModule::AssetCreated(UserEnum);
	Package->MarkPackageDirty();

	FString Out = FString::Printf(TEXT("changed: created enum %s"), *InPlan.PackagePath);
	Out += FString::Printf(TEXT("\nvalues: %d"), InPlan.Values.Num());
	Out += FString::Printf(TEXT("\nnames set: %d"), Renamed);
	if (ShownAs > 0)
	{
		Out += FString::Printf(TEXT("\ndisplay names set: %d"), ShownAs);
	}
	Out += TEXT("\n; the asset is loaded but NOT saved. Call E-SAVE to write it to disk, ");
	Out += TEXT("because an unsaved asset dies with the editor.");

	return Out;
}
