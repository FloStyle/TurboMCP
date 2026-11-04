// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "TurboMCPStruct.h"
#include "Editor.h"
#include "TurboMCPTypeParser.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "BlueprintEditorLibrary.h"
#include "Containers/UnrealString.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "K2Node_StructOperation.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "Engine/DataTable.h"
#include "Kismet2/StructureEditorUtils.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "ScopedTransaction.h"
#include "StructUtils/UserDefinedStruct.h"
#include "UObject/Class.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UObjectIterator.h"
#include "UserDefinedStructure/UserDefinedStructEditorData.h"

namespace
{
    // Read requested options back because engine setters can reject a value or return
    // false for an already-matching value. Their return code alone is not the result.
    void VerifyFieldOptions(UUserDefinedStruct* Struct, const FGuid& Guid,
        const FTurboMCPStruct::FField& Field, TArray<FString>& Failures)
    {
        const FStructVariableDescription* Observed = FStructureEditorUtils::GetVarDescByGuid(Struct, Guid);
        if (!Observed)
        {
            Failures.Add(Field.Name + TEXT(" (field missing during option verification)"));
            return;
        }
        if (!Field.Default.IsEmpty() && Observed->DefaultValue != Field.Default)
            Failures.Add(Field.Name + TEXT(" (requested default was not applied)"));
        if (!Field.Tooltip.IsEmpty() && Observed->ToolTip != Field.Tooltip)
            Failures.Add(Field.Name + TEXT(" (requested tooltip was not applied)"));
        if (!Field.Category.IsEmpty() && Observed->Category != FName(*Field.Category))
            Failures.Add(Field.Name + TEXT(" (requested category was not applied)"));
    }

	/** Trims, so every comparison below is against a trimmed string. */
	FString Trim(const FString& In)
	{
		return In.TrimStartAndEnd();
	}

	/**
	 * Split on a separator, ignoring separators nested inside brackets.
	 *
	 * A `Map<String, Int>` carries a comma at depth 1, and splitting there would turn one field into two broken
	 * ones. Depth is counted rather than pattern-matched, so any nesting works. This is the same rule
	 * `TurboMCPVarsAdd.cpp:31-56` applies to the same class of input, restated here rather than shared, because the
	 * original sits in an anonymous namespace and cannot be reached from this file.
	 */
	void SplitTopLevel(const FString& In, TCHAR InSeparator, TArray<FString>& Out)
	{
		Out.Reset();
		int32 Depth = 0;
		int32 Start = 0;
		for (int32 Index = 0; Index < In.Len(); ++Index)
		{
			const TCHAR Character = In[Index];
			if (Character == TEXT('<') || Character == TEXT('(')
				|| Character == TEXT('[') || Character == TEXT('{'))
			{
				++Depth;
			}
			else if (Character == TEXT('>') || Character == TEXT(')')
				|| Character == TEXT(']') || Character == TEXT('}'))
			{
				--Depth;
			}
			else if (Character == InSeparator && Depth == 0)
			{
				Out.Add(In.Mid(Start, Index - Start));
				Start = Index + 1;
			}
		}
		Out.Add(In.Mid(Start));
	}

	/** True when every angle bracket balances and none closes early. */
	bool BracketsBalance(const FString& In, FString& OutWhy)
	{
		int32 Depth = 0;
		for (const TCHAR Character : In)
		{
			if (Character == TEXT('<'))
			{
				++Depth;
			}
			else if (Character == TEXT('>'))
			{
				--Depth;
				if (Depth < 0)
				{
					OutWhy = TEXT("a '>' closes before any '<' opens");
					return false;
				}
			}
		}
		if (Depth != 0)
		{
			OutWhy = FString::Printf(TEXT("%d '<' is never closed"), Depth);
			return false;
		}
		return true;
	}

	/** True when the string is usable as a display name for a struct field. */
	bool IsValidFieldName(const FString& InName, FString& OutWhy)
	{
		if (InName.IsEmpty())
		{
			OutWhy = TEXT("the name is empty");
			return false;
		}
		// Unreal generates <base>_<uint32>_<32-digit GUID>; our collision
		// helper may append three more digits to the base before that.
		constexpr int32 MaxFieldNameLength = NAME_SIZE - 1 - (1 + 10 + 1 + 32) - 3;
		if (InName.Len() > MaxFieldNameLength)
		{
			OutWhy = FString::Printf(TEXT("the name exceeds %d characters, including room for Unreal's generated suffix"), MaxFieldNameLength);
			return false;
		}
		if (!FChar::IsAlpha(InName[0]) && InName[0] != TEXT('_'))
		{
			OutWhy = TEXT("it does not start with a letter or an underscore");
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
		return true;
	}

	/**
	 * The basic type names a caller may write, against the PIN CATEGORY constant the engine's own
	 * factory wants.
	 *
	 * *** THE ENGINE'S FACTORY TAKES `PC_*` CATEGORY NAMES, NOT THE WORDS A PERSON WRITES, AND IT
	 * SILENTLY DEFAULTING TO `int` IS THE BUG THIS TABLE EXISTS TO AVOID. *** Read from
	 * `BlueprintEditorLibrary.cpp:2105-2129`:
	 *
	 *     const TSet<FName> PrimitiveTypes = { PC_Boolean, PC_Byte, PC_Int, PC_Int64, PC_Real,
	 *                                          PC_Name, PC_String, PC_Text };
	 *     if(!PrimitiveTypes.Contains(TypeName))
	 *     {
	 *         UE_LOGF(..., "Primitive type: %ls not recognized, defaulting to int", ...);
	 *         TypeName = UEdGraphSchema_K2::PC_Int;
	 *     }
	 *
	 * SO `GetBasicTypeByName("float")`, `("double")` AND `("real")` ALL PRODUCE AN `int` - the words
	 * are not category names - and the function's own doc comment confirms it returns `int` for an
	 * invalid type. MEASURED: `Tools/test-struct.py` asked for `Alpha: double` and the tool reported
	 * `compiled type 'int32' ... asked for 'int'`, because `double` had been mapped to the category
	 * `int` two steps earlier by the engine, silently.
	 *
	 * `float` and `real` BOTH mean `PC_Real`, which is what the engine's own `PC_Real` branch gives a
	 * `PC_Double` sub-category (`:2124-2127`) - so they are one type here exactly as they are one type
	 * in the editor's menu.
	 */
	const TMap<FString, FName>& BasicTypeAliases()
	{
		static const TMap<FString, FName> Names = {
			{ TEXT("bool"),   UEdGraphSchema_K2::PC_Boolean },
			{ TEXT("byte"),   UEdGraphSchema_K2::PC_Byte },
			{ TEXT("int"),    UEdGraphSchema_K2::PC_Int },
			{ TEXT("int64"),  UEdGraphSchema_K2::PC_Int64 },
			{ TEXT("float"),  UEdGraphSchema_K2::PC_Real },
			{ TEXT("real"),   UEdGraphSchema_K2::PC_Real },
			{ TEXT("double"), UEdGraphSchema_K2::PC_Real },
			{ TEXT("name"),   UEdGraphSchema_K2::PC_Name },
			{ TEXT("string"), UEdGraphSchema_K2::PC_String },
			{ TEXT("text"),   UEdGraphSchema_K2::PC_Text },
		};
		return Names;
	}

	/**
	 * True when the category the factory produced is the one that was asked for.
	 *
	 * THIS IS THE GUARD THE SILENT DEFAULT NEEDS. `GetBasicTypeByName` cannot report failure, so a
	 * category that comes back different from the one requested means the engine substituted `int` for
	 * a name it did not know - and handing that back as the caller's type would create a field of the
	 * WRONG TYPE while reporting success. Refusing is the only honest answer.
	 */
	bool CategoryMatchesRequest(const FName& InRequested, const FName& InProduced)
	{
		if (InProduced == InRequested)
		{
			return true;
		}
		// The engine's `PC_Real` branch is the one case where the general helper is expected to
		// answer with the integer category instead, so both are accepted for a real request.
		if (InRequested == UEdGraphSchema_K2::PC_Real
			&& InProduced == UEdGraphSchema_K2::PC_Int)
		{
			return true;
		}
		return false;
	}

	/** The struct names the engine's own type table carries, and their object paths. */
	const TMap<FString, FString>& StructTypeAliases()
	{
		static const TMap<FString, FString> Names = {
			{ TEXT("vector"),      TEXT("/Script/CoreUObject.Vector") },
			{ TEXT("rotator"),     TEXT("/Script/CoreUObject.Rotator") },
			{ TEXT("transform"),   TEXT("/Script/CoreUObject.Transform") },
			{ TEXT("vector2d"),    TEXT("/Script/CoreUObject.Vector2D") },
			{ TEXT("linearcolor"), TEXT("/Script/CoreUObject.LinearColor") },
			{ TEXT("color"),       TEXT("/Script/CoreUObject.Color") },
		};
		return Names;
	}

	/** Sorted, so an error message's list does not change order between runs. */
	FString AcceptedTypeList()
	{
		TArray<FString> Basic;
		BasicTypeAliases().GetKeys(Basic);
		Basic.Sort();

		TArray<FString> Structs;
		StructTypeAliases().GetKeys(Structs);
		Structs.Sort();

		return FString::Printf(
			TEXT("Basic types: %s. Struct names: %s. An enum or a struct asset is named by its path."),
			*FString::Join(Basic, TEXT(", ")), *FString::Join(Structs, TEXT(", ")));
	}

	/** Which container wrapper a type text carries, if any. */
	enum class EContainer : uint8
	{
		None,
		Array,
		Set,
		Map,
	};

	/**
	 * Turn one type text into a pin type.
	 *
	 * The container is stripped FIRST and the inner type resolved, because `Array<Vector>` has to become an array OF
	 * vector rather than an array of whatever the inner resolution happened to return.
	 */
	bool ResolveType(const FString& InType, FEdGraphPinType& OutPinType, FString& OutWhy)
	{
		const FString Text = Trim(InType);
		if (Text.IsEmpty())
		{
			OutWhy = TEXT("no type given");
			return false;
		}

		FString BracketWhy;
		if (!BracketsBalance(Text, BracketWhy))
		{
			OutWhy = FString::Printf(TEXT("the type '%s' is not balanced: %s"), *Text, *BracketWhy);
			return false;
		}

		EContainer Container = EContainer::None;
		FString Inner = Text;
		FString MapValue;

		struct FWrapper
		{
			const TCHAR* Prefix;
			EContainer Kind;
		};
		static const FWrapper Wrappers[] = {
			{ TEXT("Array<"), EContainer::Array },
			{ TEXT("Set<"),   EContainer::Set },
			{ TEXT("Map<"),   EContainer::Map },
		};

		for (const FWrapper& Wrapper : Wrappers)
		{
			if (!Text.StartsWith(Wrapper.Prefix, ESearchCase::IgnoreCase))
			{
				continue;
			}
			if (!Text.EndsWith(TEXT(">")))
			{
				OutWhy = FString::Printf(
					TEXT("the type '%s' opens '%s' but does not end with '>'"), *Text, Wrapper.Prefix);
				return false;
			}

			Container = Wrapper.Kind;

			const int32 PrefixLength = FCString::Strlen(Wrapper.Prefix);
			Inner = Text.Mid(PrefixLength, Text.Len() - PrefixLength - 1);

			if (Container == EContainer::Map)
			{
				TArray<FString> Sides;
				SplitTopLevel(Inner, TEXT(','), Sides);
				if (Sides.Num() != 2)
				{
					OutWhy = FString::Printf(
						TEXT("a Map needs exactly two types, 'Map<Key, Value>', and '%s' has %d"),
						*Text, Sides.Num());
					return false;
				}
				Inner = Trim(Sides[0]);
				MapValue = Trim(Sides[1]);
			}
			else
			{
				Inner = Trim(Inner);
			}
			break;
		}

		FEdGraphPinType Resolved;
		const FString Lower = Inner.ToLower();

		if (const FName* Alias = BasicTypeAliases().Find(Lower))
		{
			Resolved = UBlueprintEditorLibrary::GetBasicTypeByName(*Alias);
			// `GetBasicTypeByName` CANNOT REPORT FAILURE - it substitutes and returns - so the answer
			// is checked against what was asked for. See `CategoryMatchesRequest`.
			if (!CategoryMatchesRequest(*Alias, Resolved.PinCategory))
			{
				OutWhy = FString::Printf(
					TEXT("the engine substituted the type '%s' for '%s'. That should not happen for an ")
					TEXT("accepted basic type and means this tool's type table is wrong, so nothing was ")
					TEXT("created rather than a field of the wrong type."),
					*Resolved.PinCategory.ToString(), *Inner);
				return false;
			}
		}
		else if (const FString* StructPath = StructTypeAliases().Find(Lower))
		{
			UScriptStruct* Struct = FindObject<UScriptStruct>(nullptr, **StructPath);
			if (Struct == nullptr)
			{
				OutWhy = FString::Printf(
					TEXT("the engine struct '%s' could not be found, which is an engine problem and not an ")
					TEXT("input problem"), **StructPath);
				return false;
			}
			Resolved = UBlueprintEditorLibrary::GetStructType(Struct);
		}
		else
		{
			// An enum or a struct ASSET, named by its path.
			UObject* TypeObject = LoadObject<UObject>(nullptr, *Inner);
			if (TypeObject == nullptr)
			{
				// A bare name with no path is the common mistake, so the accepted forms are listed rather than the
				// failure left as a refused load.
				OutWhy = FString::Printf(TEXT("'%s' is not a type. %s"), *Inner, *AcceptedTypeList());
				return false;
			}
			if (UScriptStruct* AsStruct = Cast<UScriptStruct>(TypeObject))
			{
				Resolved = UBlueprintEditorLibrary::GetStructType(AsStruct);
			}
			else if (UEnum* AsEnum = Cast<UEnum>(TypeObject))
			{
				// THERE IS NO `GetEnumType`. The engine's library carries basic, struct, class-reference and
				// object-reference helpers and nothing for an enum, so the pin type is assembled directly: the
				// category is the byte category and the enum itself is the sub-category object, which is the shape
				// `EdGraphSchema_K2` reads back. Same rule as `TurboMCPVarsAdd.cpp:589-598`.
				Resolved.PinCategory = UEdGraphSchema_K2::PC_Byte;
				Resolved.PinSubCategoryObject = AsEnum;
			}
			else
			{
				OutWhy = FString::Printf(
					TEXT("%s is a %s, and a struct field can be an enum or a struct but not that."),
					*Inner, *TypeObject->GetClass()->GetName());
				return false;
			}
		}

		// The container wrappers are the engine's own, and replacing the inner type is what makes `Array<Vector>` an
		// array OF vector. `GetMapType` also carries the value type into the terminal, which setting
		// `ContainerType` alone does not do.
		switch (Container)
		{
		case EContainer::Array:
			Resolved = UBlueprintEditorLibrary::GetArrayType(Resolved);
			break;
		case EContainer::Set:
			Resolved = UBlueprintEditorLibrary::GetSetType(Resolved);
			break;
		case EContainer::Map:
		{
			FEdGraphPinType ValueType;
			FString ValueWhy;
			if (!ResolveType(MapValue, ValueType, ValueWhy))
			{
				OutWhy = FString::Printf(TEXT("the Map's value type is wrong: %s"), *ValueWhy);
				return false;
			}
			Resolved = UBlueprintEditorLibrary::GetMapType(Resolved, ValueType);
			break;
		}
		default:
			break;
		}

		OutPinType = Resolved;
		return true;
	}

	/** The GUID of a field by its display name, or an invalid GUID when it is gone. */
	FGuid GuidOfField(const UUserDefinedStruct* InStruct, const FString& InName)
	{
		if (InStruct == nullptr || InStruct->EditorData == nullptr)
		{
			return FGuid();
		}
		for (const FStructVariableDescription& Description : FStructureEditorUtils::GetVarDesc(InStruct))
		{
			if (Description.FriendlyName == InName)
			{
				return Description.VarGuid;
			}
		}
		return FGuid();
	}

	/**
	 * True when a field's COMPILED type is what a type text asks for.
	 *
	 * *** THIS EXISTS BECAUSE `ChangeVariableType` RETURNS `false` FOR A NO-OP, WHICH IS NOT A
	 * FAILURE. *** `StructureEditorUtils.cpp:413-426`:
	 *
	 *     const bool bChangedType = (VarDesc->ToPinType() != NewType);
	 *     if (bChangedType) { ... return true; }
	 *     ...
	 *     return false;
	 *
	 * So asking for the type a field ALREADY has returns `false` - indistinguishable, at the call
	 * site, from the `false` that means `CanHaveAMemberVariableOfType` refused the type
	 * (`:404-408`). MEASURED: `Tools/test-struct.py` changing `Alpha` from `int` to `double` was
	 * reported as "the struct changed between the plan and this call" while the change had in fact
	 * been applied - the tool blamed the caller for the engine's "nothing to do" answer.
	 *
	 * The ambiguity is resolved by ASKING THE FIELD, not by reading the return value: the compiled
	 * property is re-read and its C++ type compared to the property type the requested text would
	 * produce. `float` and `real` name one type, so `RealType` is the requested C++ name where the
	 * engine has a fixed spelling.
	 */
	bool TypeMatches(const UUserDefinedStruct* InStruct, const FString& InFieldName,
		const FString& InTypeText)
	{
		const FProperty* Property = FStructureEditorUtils::GetPropertyByFriendlyName(InStruct, InFieldName);
		if (Property == nullptr)
		{
			return false;
		}

		const FString Current = Property->GetCPPType();
		const FString Lower = Trim(InTypeText).ToLower();

		// The engine has one property type behind two names (`float` and `real`), so the requested
		// text is mapped to the property C++ name before any comparison.
		static const TMap<FString, FString> RequestedCppNames = {
			{ TEXT("float"), TEXT("double") },
			{ TEXT("real"),  TEXT("double") },
		};

		if (const FString* CppName = RequestedCppNames.Find(Lower))
		{
			return Current.Equals(*CppName, ESearchCase::IgnoreCase);
		}

		// Every other accepted spelling is either already the C++ name (`double`, `int64`,
		// `FName`, `FString`, `FVector`, ...) or differs only in case (`int` -> `int32`,
		// `bool` -> `bool`), so a case-insensitive containment of one in the other is the test.
		// It is deliberately NOT an equality: `int` must match `int32`.
		return Current.Equals(Lower, ESearchCase::IgnoreCase)
			|| Current.ToLower().Contains(Lower)
			|| Lower.Contains(Current.ToLower());
	}

	/**
	 * A display name this struct does not already carry, derived from the wanted one.
	 *
	 * `FStructureEditorUtils::RenameVariable` REFUSES A NAME THAT IS TAKEN, and
	 * `CreateUserDefinedStruct` has already taken `MemberVar_N` for the boolean field it adds itself
	 * (`StructureEditorUtils.cpp:59`, generated through `:251-257`). A single `AddVariable` can therefore
	 * consume every `MemberVar_<n>` a caller might want, and the rename would silently fail.
	 *
	 * So the wanted name is tried FIRST and a suffix is added only when the engine says no, which is the
	 * engine's own rule asked one name at a time rather than a rule restated here. `IsUniqueVariableFriendlyName`
	 * is the same predicate `RenameVariable` checks (`:354`).
	 */
	FString MakeUniqueFriendlyName(const UUserDefinedStruct* InStruct, const FString& InWanted)
	{
		if (FStructureEditorUtils::IsUniqueVariableFriendlyName(InStruct, InWanted))
		{
			return InWanted;
		}
		for (int32 Suffix = 2; Suffix < 1000; ++Suffix)
		{
			const FString Candidate = FString::Printf(TEXT("%s%d"), *InWanted, Suffix);
			if (FStructureEditorUtils::IsUniqueVariableFriendlyName(InStruct, Candidate))
			{
				return Candidate;
			}
		}
		return FString();
	}

	/**
	 * Split a comma-separated list, ATTACHING each `key=value` OPTION to the item before it.
	 *
	 * *** THE COMMA INSIDE AN OPTION IS THE SAME CHARACTER THAT SEPARATES TWO ITEMS, AND THIS
	 * FUNCTION IS THE ONE PLACE THAT RESOLVES THAT. ***
	 *
	 * `move:Gamma, above=Alpha` and `Good: int, default=1` are each ONE item with ONE option, but a
	 * plain `SplitTopLevel` turns each into two. Writing the join twice - once in the field parser and
	 * once in the change parser - is what ships the SAME defect in two opposite directions: a parser
	 * that splits first reads `move:Gamma` alone ("the reorder of 'Gamma' does not say where to put
	 * it"), and one that attaches first reads `Good: int, default=1` alone ("'default=1' has no ':'").
	 * One rule, one place, so neither direction is available.
	 *
	 * THE RULE, and it is deliberately the simplest one that works: an item with NO ':' and SOME '=' is
	 * an OPTION of the item before it, whatever its key is called; anything else starts a new item.
	 *
	 * Attaching a key the verb does not accept - `sideways=Beta`, `banana=3` - is INTENTIONAL rather
	 * than a hole: it lets the item's own option loop refuse it, which answers "'sideways' is not an
	 * option of a reorder. The options are above= and below=" instead of a message about the wrong item.
	 *
	 * The one shape this reads differently from a person is a field written `Vector amount=3`, where
	 * the OPTION is written before the '='. That is refused rather than guessed at - the key
	 * `Vector amount` is not an option - so the failure is loud and the fix is `Vector: amount=3`.
	 */
	void SplitItemsWithOptions(const FString& In, TArray<FString>& Out)
	{
		TArray<FString> Parts;
		SplitTopLevel(In, TEXT(','), Parts);

		Out.Reset();
		for (const FString& Raw : Parts)
		{
			const FString Item = Trim(Raw);
			if (Item.IsEmpty())
			{
				continue;
			}

			const bool bIsOption = !Item.Contains(TEXT(":")) && Item.Contains(TEXT("="));
			if (bIsOption && Out.Num() > 0)
			{
				Out.Last() += TEXT(", ") + Item;
			}
			else
			{
				Out.Add(Item);
			}
		}
	}
}

void FTurboMCPStruct::ReadFields(const UUserDefinedStruct* InStruct, TArray<FField>& OutFields)
{
	OutFields.Reset();
	if (InStruct == nullptr || InStruct->EditorData == nullptr)
	{
		return;
	}

	const UUserDefinedStructEditorData* Data =
		CastChecked<const UUserDefinedStructEditorData>(InStruct->EditorData);

	for (const FStructVariableDescription& Description : Data->VariablesDescriptions)
	{
		FField Field;
		Field.Name = Description.FriendlyName;
		Field.Default = Description.DefaultValue;
		Field.Tooltip = Description.ToolTip;
		Field.Category = Description.Category.ToString();

		// THE TYPE IS TAKEN FROM THE COMPILED PROPERTY, NOT FROM THE DESCRIPTION'S PIN TYPE, because the property is
		// what every reader and the engine itself will see. A description that has not been compiled yet has no
		// property, and the pin type is then not invented: the field says it is uncompiled.
		const FProperty* Property = FStructureEditorUtils::GetPropertyByFriendlyName(InStruct, Field.Name);
		Field.TypeText = Property != nullptr ? Property->GetCPPType() : FString(TEXT("(uncompiled)"));

		OutFields.Add(MoveTemp(Field));
	}
}

void FTurboMCPStruct::FindReaders(const UUserDefinedStruct* InStruct,
	TArray<FReader>& OutReaders)
{
	OutReaders.Reset();
	if (InStruct == nullptr)
	{
		return;
	}

	// Readers are loaded assets, not a complete registry-wide dependency scan.
	// Count member declarations and graph nodes carrying the struct (including
	// make/break operations and map-value pin types), then list each asset once.
	for (TObjectIterator<UObject> It; It; ++It)
	{
		UObject* Object = *It;
		if (Object == nullptr || Object->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject))
		{
			continue;
		}

		if (const UBlueprint* Blueprint = Cast<UBlueprint>(Object))
		{
			int32 Uses = 0;
			for (const FBPVariableDescription& Variable : Blueprint->NewVariables)
			{
				if (Variable.VarType.PinSubCategoryObject == InStruct
					|| Variable.VarType.PinValueType.TerminalSubCategoryObject == InStruct)
				{
					++Uses;
				}
			}
			TArray<UEdGraph*> Graphs;
			Blueprint->GetAllGraphs(Graphs);
			TSet<const UEdGraphNode*> SeenNodes;
			for (const UEdGraph* Graph : Graphs)
			{
				if (Graph == nullptr) { continue; }
				for (const UEdGraphNode* Node : Graph->Nodes)
				{
					if (Node == nullptr || SeenNodes.Contains(Node)) { continue; }
					SeenNodes.Add(Node);
					const UK2Node_StructOperation* Operation = Cast<UK2Node_StructOperation>(Node);
					bool bUsesStruct = Operation != nullptr && Operation->StructType == InStruct;
					for (const UEdGraphPin* Pin : Node->Pins)
					{
						if (Pin != nullptr && (Pin->PinType.PinSubCategoryObject == InStruct
							|| Pin->PinType.PinValueType.TerminalSubCategoryObject == InStruct))
						{
							bUsesStruct = true;
							break;
						}
					}
					if (bUsesStruct) { ++Uses; }
				}
			}
			if (Uses > 0)
			{
				FReader Reader;
				Reader.Path = Object->GetPathName();
				Reader.Class = Object->GetClass()->GetName();
				Reader.Uses = Uses;
				OutReaders.Add(MoveTemp(Reader));
			}
			continue;
		}

		if (const UDataTable* Table = Cast<UDataTable>(Object))
		{
			if (Table->GetRowStruct() == InStruct)
			{
				FReader Reader;
				Reader.Path = Object->GetPathName();
				Reader.Class = Object->GetClass()->GetName();
				Reader.Uses = 1;
				OutReaders.Add(MoveTemp(Reader));
			}
		}
	}

	// Sorted, so a refusal does not report the same set in a different order between runs. A message whose list
	// moves is a message nobody can compare, which is the reason `TurboMCPVarsAdd.cpp:565-567` sorts its lists too.
	OutReaders.Sort([](const FReader& A, const FReader& B)
	{
		return A.Path < B.Path;
	});
}

bool FTurboMCPStruct::ParseFields(const FString& InFields, TArray<FField>& OutFields, FString& OutError)
{
	OutError.Empty();
	OutFields.Reset();

	TArray<FString> Parts;
	SplitItemsWithOptions(InFields, Parts);

	TSet<FString> Seen;
	for (const FString& Raw : Parts)
	{
		const FString Item = Trim(Raw);
		if (Item.IsEmpty())
		{
			continue;
		}

		// `Name: Type` first. A missing colon is refused by name rather than guessed at, because `Vector` on its own
		// is either a name with no type or a type with no name and this tool must not pick one.
		FString Name;
		FString Rest;
		if (!Item.Split(TEXT(":"), &Name, &Rest))
		{
			OutError = FString::Printf(
				TEXT("'%s' has no ':'. A field is 'Name: Type', followed by any of 'default=', ")
				TEXT("'tooltip=' and 'category='."), *Item);
			return false;
		}
		Name = Trim(Name);
		Rest = Trim(Rest);

		FString NameWhy;
		if (!IsValidFieldName(Name, NameWhy))
		{
			OutError = FString::Printf(TEXT("'%s' cannot be a field name: %s."), *Name, *NameWhy);
			return false;
		}

		if (Seen.Contains(Name.ToLower()))
		{
			// Two fields with one name is a struct no caller can address, so it is refused rather than collapsed.
			OutError = FString::Printf(
				TEXT("'%s' is given twice. Each field must have its own name."), *Name);
			return false;
		}
		Seen.Add(Name.ToLower());

		FField Field;
		Field.Name = Name;

		// The type is everything up to the first option. The options are named, so one can never be mistaken for
		// part of the type, and an unknown one is refused rather than silently made part of the type text.
		TArray<FString> Segments;
		SplitTopLevel(Rest, TEXT(','), Segments);
		Field.TypeText = Trim(Segments.Num() > 0 ? Segments[0] : FString());

		TSet<FString> SeenOptions;
		for (int32 Index = 1; Index < Segments.Num(); ++Index)
		{
			const FString Segment = Trim(Segments[Index]);
			if (Segment.IsEmpty())
			{
				continue;
			}

			FString Key;
			FString Value;
			if (!Segment.Split(TEXT("="), &Key, &Value))
			{
				OutError = FString::Printf(
					TEXT("'%s' on field '%s' is not 'option=value'. The options are default, tooltip ")
					TEXT("and category."), *Segment, *Name);
				return false;
			}

			const FString LowerKey = Trim(Key).ToLower();
			const FString OptionValue = Trim(Value);
			if (SeenOptions.Contains(LowerKey))
			{
				OutError = FString::Printf(TEXT("field '%s' repeats option '%s'; each option may be supplied once."), *Name, *Trim(Key));
				return false;
			}
			SeenOptions.Add(LowerKey);

			if (LowerKey == TEXT("default"))
			{
				Field.Default = OptionValue;
			}
			else if (LowerKey == TEXT("tooltip"))
			{
				Field.Tooltip = OptionValue;
			}
			else if (LowerKey == TEXT("category"))
			{
				// Apply stores this as FName; refuse before any struct mutation.
				if (OptionValue.Len() >= NAME_SIZE)
				{
					OutError = FString::Printf(TEXT("category on field '%s' exceeds the maximum name length of %d characters."), *Name, NAME_SIZE - 1);
					return false;
				}
				Field.Category = OptionValue;
			}
			else
			{
				// REFUSED RATHER THAN IGNORED. An ignored option is a caller who believes it set something it did
				// not, which is the same class of harm as a silently skipped node.
				OutError = FString::Printf(
					TEXT("'%s' is not a field option on '%s'. The options are default=, tooltip= and ")
					TEXT("category=."), *Trim(Key), *Name);
				return false;
			}
		}

		if (Field.TypeText.IsEmpty())
		{
			OutError = FString::Printf(TEXT("field '%s' has no type after the ':'."), *Name);
			return false;
		}

		// THE TYPE IS RESOLVED WHILE PLANNING, so the Preview refuses the same input the Edit will. A preview that
		// prints a plan the edit will not carry out is worse than no preview (`D-02`).
		FEdGraphPinType PinType;
		FString TypeWhy;
		if (!FTurboMCPTypeParser::Resolve(Field.TypeText, PinType, TypeWhy))
		{
			OutError = FString::Printf(
				TEXT("field '%s' has type '%s', which cannot be used: %s."),
				*Name, *Field.TypeText, *TypeWhy);
			return false;
		}

		OutFields.Add(MoveTemp(Field));
	}

	if (OutFields.Num() == 0)
	{
		OutError = TEXT("no fields given. Pass them as 'Name: Type', separated by commas, for example ")
			TEXT("Weight: float, Label: name.");
		return false;
	}

	return true;
}

bool FTurboMCPStruct::PlanNew(const FString& InPath, const FString& InFields,
	FPlanNew& OutPlan, FString& OutError)
{
	OutError.Empty();

	const FString Path = Trim(InPath);
	if (Path.IsEmpty())
	{
		OutError = TEXT("no path given. Pass a package path, for example /Game/Tests/MyStruct.");
		return false;
	}
	if (!Path.StartsWith(TEXT("/")))
	{
		OutError = FString::Printf(
			TEXT("'%s' does not start with '/'. A package path is absolute, for example ")
			TEXT("/Game/Tests/MyStruct."), *Path);
		return false;
	}

	FText PathReason;
	if (Path.Len() >= NAME_SIZE || !FPackageName::IsValidLongPackageName(Path, false, &PathReason))
	{
		OutError = Path.Len() >= NAME_SIZE
			? TEXT("package path exceeds Unreal's name length limit; use a shorter package path.")
			: FString::Printf(TEXT("invalid package path: %s"), *PathReason.ToString());
		return false;
	}
	OutPlan.PackagePath = Path;
	OutPlan.AssetName = FPaths::GetCleanFilename(Path);
	if (OutPlan.AssetName.IsEmpty())
	{
		OutError = FString::Printf(TEXT("'%s' has no asset name in it."), *Path);
		return false;
	}

	return ParseFields(InFields, OutPlan.Fields, OutError);
}

FString FTurboMCPStruct::RenderPreviewNew(const FPlanNew& InPlan)
{
	FString Out = FString::Printf(TEXT("preview: struct %s"), *InPlan.PackagePath);
	Out += FString::Printf(TEXT("\nfields: %d"), InPlan.Fields.Num());

	for (const FField& Field : InPlan.Fields)
	{
		Out += FString::Printf(TEXT("\n  %s: %s"), *Field.Name, *Field.TypeText);
		if (!Field.Default.IsEmpty())
		{
			Out += FString::Printf(TEXT("  default=%s"), *Field.Default);
		}
		if (!Field.Tooltip.IsEmpty())
		{
			Out += FString::Printf(TEXT("  tooltip=%s"), *Field.Tooltip);
		}
		if (!Field.Category.IsEmpty())
		{
			Out += FString::Printf(TEXT("  category=%s"), *Field.Category);
		}
	}

	// Whether the asset already exists is the fact that decides whether the Edit will be refused, so the preview
	// states it rather than leaving it to be discovered.
	const FString ObjectPath = FString::Printf(TEXT("%s.%s"), *InPlan.PackagePath, *InPlan.AssetName);
	const bool bExists = FindObject<UObject>(nullptr, *ObjectPath) != nullptr
		|| FPackageName::DoesPackageExist(InPlan.PackagePath);

	if (bExists)
	{
		Out += TEXT("\nthis asset already exists: the edit will REFUSE rather than overwrite it. Pick ")
			TEXT("another package path.");
	}
	else
	{
		Out += TEXT("\nthis asset does not exist yet; creation and field application can still fail.");
	}

	Out += TEXT("\n; the struct is created through the editor's own ")
		TEXT("FStructureEditorUtils::CreateUserDefinedStruct, so it stays editable in the struct editor ")
		TEXT("rather than being a hand-built asset the editor cannot open.");
	return Out;
}

FString FTurboMCPStruct::ApplyNew(const FPlanNew& InPlan)
{
    // Struct editor operations compile and notify dependents; do not nest them in another edit.
    if (!IsInGameThread() || !GEditor || GEditor->IsTransactionActive() || GEditor->IsPlaySessionInProgress())
        return TEXT("unchanged: struct editing requires an idle editor outside PIE and active transactions; save=not_requested");
	const FString ObjectPath = FString::Printf(TEXT("%s.%s"), *InPlan.PackagePath, *InPlan.AssetName);

	// REFUSED RATHER THAN OVERWRITTEN. A name someone else chose is not this tool's to replace, which is what `P8`
	// means by never leaving the user worse off.
	if (FindObject<UObject>(nullptr, *ObjectPath) != nullptr
		|| FPackageName::DoesPackageExist(InPlan.PackagePath))
	{
		return FString::Printf(
			TEXT("unchanged: %s already exists, so it was not touched. Choose another package path."), *InPlan.PackagePath);
	}

	UPackage* Package = CreatePackage(*InPlan.PackagePath);
	if (Package == nullptr)
	{
		return FString::Printf(
			TEXT("error: could not create the package %s."), *InPlan.PackagePath);
	}

	// ONE TRANSACTION, so the work is one undo step (`P4`).
	const FScopedTransaction Transaction(NSLOCTEXT("TurboMCP", "StructNew", "Create User Struct"));

	// *** THE ENGINE'S OWN CREATOR, NOT A HAND-BUILT `UUserDefinedStruct`, AND THIS ONE CALL IS THE WHOLE
	// MEASUREMENT. *** `CreateUserDefinedStruct` installs the `UUserDefinedStructEditorData` that `AddVariable`
	// reaches for (`StructureEditorUtils.cpp:49-50`), and the recorded commandlet crash was from skipping exactly
	// this call by using `NewObject<UUserDefinedStruct>` directly.
	UUserDefinedStruct* Struct = FStructureEditorUtils::CreateUserDefinedStruct(
		Package, FName(*InPlan.AssetName), RF_Public | RF_Standalone);

	if (Struct == nullptr)
	{
		// It returns null when `UserDefinedStructEnabled()` is false, which is a config switch. Naming the switch
		// beats reporting a null pointer the caller cannot act on.
		return TEXT("error: the engine's struct creator returned nothing. That happens when ")
			TEXT("[/Script/UnrealEd.UserDefinedStruct] bUseUserDefinedStructure is false in the engine ")
			TEXT("config (FStructureEditorUtils::UserDefinedStructEnabled, StructureEditorUtils.cpp:544).");
	}

	// *** THE CREATOR ADDS ONE BOOLEAN FIELD OF ITS OWN, AND TWO ENGINE FACTS DECIDE THE ORDER OF WHAT
	// FOLLOWS. READING BACK WITHOUT ALLOWING FOR THEM GIVES FIVE FIELDS ALL CALLED `MemberVar_N` - THE BOOL
	// PLUS THE FOUR ASKED FOR, AND NOT ONE OF THEM NAMED. ***
	//
	//   1. `RemoveVariable` REFUSES TO EMPTY A STRUCT: `StructureEditorUtils.cpp:326-327` reads
	//      `const bool bAllowToMakeEmpty = false; if (bAllowToMakeEmpty || (OldNum > 1))`. So the creator's
	//      boolean CANNOT be removed while it is the only field - it has to go AFTER the caller's fields
	//      exist, when `OldNum > 1` holds.
	//   2. `AddVariable` ALSO CANNOT NAME A FIELD: it calls `FMemberVariableNameHelper::Generate(Struct,
	//      FString(), Guid, &DisplayName)` with an EMPTY name base (`:304`), and `Generate` turns an empty
	//      base into the literal `MemberVar` (`:251-253`). The display name is set by `RenameVariable`
	//      (`:347-373`), which is what the struct editor itself calls when a person types a name.
	//
	// SO THE ORDER IS: add every field, then REMOVE the creator's boolean, then RENAME. Renaming is done
	// last because a rename generates a fresh unique `VarName` (`:364`) and each rename is easier to make
	// unique once the final field set is known.
	const FGuid CreatorFieldGuid = [Struct]()
	{
		const TArray<FStructVariableDescription>& Existing = FStructureEditorUtils::GetVarDesc(Struct);
		return Existing.Num() > 0 ? Existing[0].VarGuid : FGuid();
	}();

	int32 Added = 0;
	TArray<FString> Failed;
	for (const FField& Field : InPlan.Fields)
	{
		FEdGraphPinType PinType;
		FString TypeWhy;
		if (!FTurboMCPTypeParser::Resolve(Field.TypeText, PinType, TypeWhy))
		{
			// UNREACHABLE WHILE `PlanNew` IS THE ONLY ENTRY POINT, because every type was resolved there. It is kept
			// because a resolution that succeeded while planning and fails while applying is a fact the caller must
			// be told, not a reason to carry on quietly.
			Failed.Add(Field.Name);
			continue;
		}

		if (!FStructureEditorUtils::AddVariable(Struct, PinType))
		{
			Failed.Add(Field.Name);
			continue;
		}
		++Added;
	}

	if (Failed.Num() > 0)
	{
		// REPORTED AS PARTIAL RATHER THAN DESCRIBED AS A SUCCESS. A caller told "3 of 4" can fix it; a caller told
		// "created" cannot.
		return FString::Printf(
			TEXT("partial: %s was created but %d of %d field(s) could not be added: %s. ")
			TEXT("An unsaved partial struct remains; inspect it before retrying or using EditTurboMCPStructFields; no rollback was attempted."),
			*InPlan.AssetName, Failed.Num(), InPlan.Fields.Num(),
			*FString::Join(Failed, TEXT(", ")));
	}

	// The creator's own boolean goes now, while `OldNum > 1` holds and the removal is therefore permitted.
	// Its return value is CHECKED rather than ignored: a field the caller did not ask for must not be left
	// in the asset, so a refusal is a partial result the caller is told about rather than a silent extra.
	bool bCreatorFieldRemoved = false;
	if (CreatorFieldGuid.IsValid())
	{
		bCreatorFieldRemoved = FStructureEditorUtils::RemoveVariable(Struct, CreatorFieldGuid);
	}

	// THE NAMES, ONE RENAME PER FIELD, IN THE ORDER THE CALLER GAVE THEM. Each rename re-resolves the field
	// by the GUID the add returned, so a rename can never land on a different field.
	//
	// `AddVariable` APPENDS, so the field the caller listed first is the field the first add created - but the
	// rename is done by matching the field's POSITION IN THE REQUEST to the position it was appended at,
	// which is read from the editor data rather than assumed.
	TArray<FString> GivenNames;
	{
		const TArray<FStructVariableDescription>& Descriptions =
			FStructureEditorUtils::GetVarDesc(Struct);

		// The fields this call added are the LAST `InPlan.Fields.Num()` entries, because `AddVariable` appends
		// and anything the creator left is either first or already removed.
		const int32 FirstOwnIndex = Descriptions.Num() - InPlan.Fields.Num();
		for (int32 Index = 0; Index < InPlan.Fields.Num(); ++Index)
		{
			const int32 DescIndex = FirstOwnIndex + Index;
			if (!Descriptions.IsValidIndex(DescIndex))
			{
				GivenNames.Add(FString());
				continue;
			}

			const FGuid Guid = Descriptions[DescIndex].VarGuid;
			const FString Unique = MakeUniqueFriendlyName(Struct, InPlan.Fields[Index].Name);
			GivenNames.Add(Unique);

			if (!Unique.IsEmpty())
			{
				FStructureEditorUtils::RenameVariable(Struct, Guid, Unique);
			}
		}
	}

	// The default, tooltip and category live on the description rather than on the add call, so they are set after,
	// by re-resolving each field's own NAME to the GUID the editor data now holds. Re-resolving rather than indexing
	// is the same `D-45` rule the field editor follows below.
	for (int32 Index = 0; Index < InPlan.Fields.Num(); ++Index)
	{
		const FField& Field = InPlan.Fields[Index];
		const FString& ActualName = GivenNames.IsValidIndex(Index) ? GivenNames[Index] : Field.Name;
		const FGuid Guid = GuidOfField(Struct, ActualName);
		if (!Guid.IsValid())
		{
			continue;
		}

		if (!Field.Default.IsEmpty())
		{
			FStructureEditorUtils::ChangeVariableDefaultValue(Struct, Guid, Field.Default);
		}
		if (!Field.Tooltip.IsEmpty())
		{
			FStructureEditorUtils::ChangeVariableTooltip(Struct, Guid, Field.Tooltip);
		}
		if (!Field.Category.IsEmpty())
		{
			// `Category` is a plain `FName` on the description with no setter, so it is written directly. It is the
			// member the details panel reads, so writing it is the same edit a person makes.
			if (FStructVariableDescription* Mutable = FStructureEditorUtils::GetVarDescByGuid(Struct, Guid))
			{
				Mutable->Category = FName(*Field.Category);
			}
		}
	}

	// The struct is compiled so its properties exist: every reader, `R-TYPE-FIND` included, sees the fields only
	// after this. Without it the asset exists with a field list that has no reflected properties behind it.
	FStructureEditorUtils::CompileStructure(Struct);

	// The asset registry has to learn about the new asset, or it is invisible to `R-ASSET-SEARCH` and to every
	// path-based lookup until the editor restarts.
	FAssetRegistryModule::AssetCreated(Struct);
	Package->MarkPackageDirty();

	// THE REPORT IS TAKEN FROM THE STRUCT, NOT FROM THE LOOP. `Added` counts what the calls returned; the read back
	// counts what the struct actually holds. Those are two different measurements and the second is the evidence,
	// which is why both are printed.
	TArray<FField> ReadBack;
	ReadFields(Struct, ReadBack);

    TArray<FString> OptionFailures;
    for (int32 Index = 0; Index < InPlan.Fields.Num(); ++Index)
    {
        const FString& ActualName = GivenNames.IsValidIndex(Index) ? GivenNames[Index] : InPlan.Fields[Index].Name;
        VerifyFieldOptions(Struct, GuidOfField(Struct, ActualName), InPlan.Fields[Index], OptionFailures);
    }
    FString Out = OptionFailures.IsEmpty()
        ? FString::Printf(TEXT("changed: created struct %s"), *InPlan.PackagePath)
        : FString::Printf(TEXT("partial: created struct %s, but requested settings were not applied: %s; inspect the unsaved asset before retrying; no rollback was attempted"), *InPlan.PackagePath, *FString::Join(OptionFailures, TEXT(", ")));
	Out += TEXT("\nVerification: observed fields are listed below; requested nonempty defaults, tooltips and categories were read back; dependent assets were not verified.");
	Out += FString::Printf(TEXT("\nfields asked for: %d"), InPlan.Fields.Num());
	Out += FString::Printf(TEXT("\nfields added: %d"), Added);
	if (!bCreatorFieldRemoved)
	{
		// SAID PLAINLY RATHER THAN LEFT TO BE DISCOVERED. The engine adds this field itself and refuses to
		// remove the last one, so if it is still here the caller is looking at a field they did not ask for.
		Out += TEXT("\nwarning: the engine's own boolean field could not be removed, so it is still in ")
			TEXT("the struct; the reason was not verified. Inspect the extra field before saving.");
	}
	Out += FString::Printf(TEXT("\nfields read back: %d"), ReadBack.Num());
	for (const FField& Field : ReadBack)
	{
		Out += FString::Printf(TEXT("\n  %s: %s"), *Field.Name, *Field.TypeText);
	}
	Out += TEXT("\nThe asset is loaded but not saved. Inspect the result before calling EditTurboMCPSave; ")
		TEXT("unsaved changes will be lost when the editor closes.");

	return Out;
}

bool FTurboMCPStruct::PlanFieldEdits(const FString& InStruct, const FString& InChanges,
	FPlanFields& OutPlan, FString& OutError)
{
	OutError.Empty();

	const FString Wanted = Trim(InStruct);
	if (Wanted.IsEmpty())
	{
		OutError = TEXT("no struct given. Pass a user struct asset, for example ")
			TEXT("/Game/Tests/MyStruct.");
		return false;
	}

	// The struct is a user struct or this tool cannot edit it: `FStructureEditorUtils` addresses members through
	// `UUserDefinedStructEditorData`, which a native `USTRUCT` does not have. That is refused BY NAME, with the
	// reason, rather than left to crash inside the engine.
	UObject* Loaded = LoadObject<UObject>(nullptr, *Wanted);
	if (Loaded == nullptr)
	{
		OutError = FString::Printf(
			TEXT("'%s' could not be loaded. Pass the path of a user struct asset, for example ")
			TEXT("/Game/Tests/MyStruct."), *Wanted);
		return false;
	}

	UUserDefinedStruct* Struct = Cast<UUserDefinedStruct>(Loaded);
	if (Struct == nullptr)
	{
		// THIS IS THE `UUserDefinedStruct` LIMIT, STATED WHERE IT ACTUALLY APPLIES. `TurboMCPKitRow.h` records
		// that a NATIVE `USTRUCT` is project-owned by construction, which is what a DataTable's `RowStruct` must
		// be - and a native struct is C++ source, so no tool can add a field to it at runtime. The two facts are
		// different facts and this refusal names the right one.
		OutError = FString::Printf(
			TEXT("%s is a %s, not a user-defined struct. Only a UUserDefinedStruct has the editor data ")
			TEXT("FStructureEditorUtils edits; a native USTRUCT is compiled C++ source and no runtime tool ")
			TEXT("can add a field to it."),
			*Loaded->GetPathName(), *Loaded->GetClass()->GetName());
		return false;
	}

	OutPlan.ObjectPath = Struct->GetPathName();
	ReadFields(Struct, OutPlan.CurrentFields);

	// The changes, split on top-level commas WITH EACH `key=value` OPTION ATTACHED TO THE CHANGE IT
	// BELONGS TO. Each change is `Name: Type` for an add or a change, `remove:Name` to remove, or
	// `move:Name` with `above=` or `below=` to reorder.
	//
	// THE ATTACHMENT RULE LIVES IN `SplitItemsWithOptions` AND IS SHARED WITH THE FIELD PARSER.
	// Writing it twice ships the SAME defect in two opposite directions: splitting first reads
	// `move:Gamma, above=Alpha` as two changes ("the reorder of 'Gamma' does not say where to put
	// it"), and attaching first reads `Good: int, default=1` as two fields ("'default=1' has no ':'").
	// One rule, one place.
	TArray<FString> Joined;
	SplitItemsWithOptions(InChanges, Joined);

	bool bSawAnything = false;
	for (const FString& Raw : Joined)
	{
		const FString Item = Trim(Raw);
		if (Item.IsEmpty())
		{
			continue;
		}
		bSawAnything = true;

		FFieldChange Change;

		const int32 Colon = Item.Find(TEXT(":"));
		const FString Head = Colon == INDEX_NONE ? Item : Trim(Item.Left(Colon));
		const FString Tail = Colon == INDEX_NONE ? FString() : Trim(Item.Mid(Colon + 1));

		const FString HeadLower = Head.ToLower();

		if (HeadLower == TEXT("remove") || HeadLower == TEXT("delete"))
		{
			// The field name is the tail, up to any option.
			TArray<FString> Segments;
			SplitTopLevel(Tail, TEXT(','), Segments);
			const FString FieldName = Trim(Segments.Num() > 0 ? Segments[0] : FString());
			if (FieldName.IsEmpty())
			{
				OutError = TEXT("a removal needs a field name: 'remove:FieldName'.");
				return false;
			}

			for (int32 Index = 1; Index < Segments.Num(); ++Index)
			{
				const FString Segment = Trim(Segments[Index]);
				if (Segment.IsEmpty())
				{
					continue;
				}
				FString Key;
				FString Value;
				if (!Segment.Split(TEXT("="), &Key, &Value) || Trim(Key).ToLower() != TEXT("force"))
				{
					OutError = FString::Printf(
						TEXT("'%s' is not an option of a removal. The only option is force=<count>."),
						*Segment);
					return false;
				}
				const FString CountText = Trim(Value);
				int32 ParsedCount = 0;
				bool bValidCount = !CountText.IsEmpty();
				for (TCHAR Digit : CountText)
				{
					if (Digit < TEXT('0') || Digit > TEXT('9')
						|| ParsedCount > (MAX_int32 - (Digit - TEXT('0'))) / 10)
					{
						bValidCount = false;
						break;
					}
					ParsedCount = ParsedCount * 10 + (Digit - TEXT('0'));
				}
				if (!bValidCount)
				{
					OutError = TEXT("force must be an integer count from 0 to 2147483647 matching the loaded readers reported by preview.");
					return false;
				}
				Change.ForceCount = ParsedCount;
			}

			Change.Op = EFieldOp::Remove;
			Change.Field.Name = FieldName;
		}
		else if (HeadLower == TEXT("move"))
		{
			TArray<FString> Segments;
			SplitTopLevel(Tail, TEXT(','), Segments);
			Change.Field.Name = Trim(Segments.Num() > 0 ? Segments[0] : FString());
			if (Change.Field.Name.IsEmpty())
			{
				OutError = TEXT("a reorder needs a field name: 'move:FieldName, above=OtherField'.");
				return false;
			}

			bool bSawAnchor = false;
			for (int32 Index = 1; Index < Segments.Num(); ++Index)
			{
				const FString Segment = Trim(Segments[Index]);
				if (Segment.IsEmpty())
				{
					continue;
				}
				FString Key;
				FString Value;
				if (!Segment.Split(TEXT("="), &Key, &Value))
				{
					OutError = FString::Printf(
						TEXT("'%s' is not an option of a reorder. The options are above=<field> and ")
						TEXT("below=<field>."), *Segment);
					return false;
				}
				const FString LowerKey = Trim(Key).ToLower();
				if (LowerKey != TEXT("above") && LowerKey != TEXT("below"))
				{
					OutError = FString::Printf(
						TEXT("'%s' is not an option of a reorder. The options are above= and below=."),
						*Trim(Key));
					return false;
				}
				if (bSawAnchor)
				{
					// Both would be two contradictory instructions, so it is refused rather than the second
					// silently winning.
					OutError = TEXT("a reorder can name above= or below=, not both.");
					return false;
				}
				bSawAnchor = true;
				Change.Anchor = Trim(Value);
				Change.bAbove = LowerKey == TEXT("above");
			}

			if (!bSawAnchor || Change.Anchor.IsEmpty())
			{
				OutError = FString::Printf(
					TEXT("the reorder of '%s' does not say where to put it. Add above=<field> or ")
					TEXT("below=<field>."), *Change.Field.Name);
				return false;
			}

			Change.Op = EFieldOp::Reorder;
		}
		else
		{
			// An add or a change, in the `Name: Type` form the creator takes. The same parser is used, so the two
			// tools cannot disagree about what a field description means.
			TArray<FField> Parsed;
			FString ParseError;
			if (!ParseFields(Item, Parsed, ParseError) || Parsed.Num() != 1)
			{
				OutError = ParseError.IsEmpty()
					? FString::Printf(TEXT("'%s' does not describe exactly one field."), *Item)
					: ParseError;
				return false;
			}

			Change.Field = Parsed[0];

			// An ADD when the name is new, a CHANGE when it is not. The caller writes one syntax for both, and which
			// one it is follows from the struct rather than from the caller's guess - which is also what makes the
			// preview and the edit agree about it.
			const bool bExists = OutPlan.CurrentFields.ContainsByPredicate(
				[&Change](const FField& Existing)
				{
					return Existing.Name == Change.Field.Name;
				});
			Change.Op = bExists ? EFieldOp::Change : EFieldOp::Add;
		}

		OutPlan.Changes.Add(MoveTemp(Change));
	}

	if (!bSawAnything)
	{
		OutError = TEXT("no changes given. Pass 'Name: Type' to add or change a field, ")
			TEXT("'move:Name, above=Other' to reorder, or 'remove:Name' to remove.");
		return false;
	}

	// EVERY CHECK IS HERE RATHER THAN IN `Apply`, so the Preview and the Edit refuse the same input.
	TSet<FString> Touched;
	for (const FFieldChange& Change : OutPlan.Changes)
	{
		const FString& Name = Change.Field.Name;

		if (Change.Op != EFieldOp::Add)
		{
			const bool bExistsNow = OutPlan.CurrentFields.ContainsByPredicate(
				[&Name](const FField& Existing)
				{
					return Existing.Name == Name;
				});
			if (!bExistsNow)
			{
				// A target that is not there is refused BY NAME rather than skipped, because skipping it would make
				// the edit differ from the preview that authorised it (`D-45`).
				TArray<FString> Real;
				for (const FField& Existing : OutPlan.CurrentFields)
				{
					Real.Add(Existing.Name);
				}
				OutError = FString::Printf(
					TEXT("'%s' is not a field of %s, so it cannot be %s. The fields it has are: %s."),
					*Name, *OutPlan.ObjectPath,
					Change.Op == EFieldOp::Remove ? TEXT("removed")
						: (Change.Op == EFieldOp::Reorder ? TEXT("moved") : TEXT("changed")),
					*FString::Join(Real, TEXT(", ")));
				return false;
			}
		}
		else
		{
			const bool bExistsNow = OutPlan.CurrentFields.ContainsByPredicate(
				[&Name](const FField& Existing)
				{
					return Existing.Name == Name;
				});
			if (bExistsNow)
			{
				OutError = FString::Printf(
					TEXT("'%s' is already a field of %s, so it cannot be added. Use its name with a new ")
					TEXT("type to change it."), *Name, *OutPlan.ObjectPath);
				return false;
			}
		}

		if (Change.Op == EFieldOp::Reorder)
		{
			if (Change.Anchor == Name)
			{
				OutError = FString::Printf(
					TEXT("'%s' cannot be moved relative to itself."), *Name);
				return false;
			}
			const bool bAnchorExists = OutPlan.CurrentFields.ContainsByPredicate(
				[&Change](const FField& Existing)
				{
					return Existing.Name == Change.Anchor;
				});
			if (!bAnchorExists)
			{
				TArray<FString> Real;
				for (const FField& Existing : OutPlan.CurrentFields)
				{
					Real.Add(Existing.Name);
				}
				OutError = FString::Printf(
					TEXT("'%s' is not a field of %s, so it cannot be the place '%s' is moved to. The ")
					TEXT("fields it has are: %s."),
					*Change.Anchor, *OutPlan.ObjectPath, *Name, *FString::Join(Real, TEXT(", ")));
				return false;
			}
		}

		if (Touched.Contains(Name))
		{
			OutError = FString::Printf(
				TEXT("'%s' is changed twice in one call. Each field may be changed once."), *Name);
			return false;
		}
		Touched.Add(Name);
	}

	// THE LOADED READERS OF EVERY REMOVAL, MEASURED HERE SO THE PREVIEW AND THE EDIT SEE ONE NUMBER.
	//
	// *** A REMOVAL IS ONLY REFUSED FOR A COUNT THE CALLER GOT WRONG, AND `force` IS ONLY REQUIRED WHEN
	// THERE IS SOMETHING TO AUTHORISE. *** The spec's words are "refused unless `force` names that
	// count". When a field has NO loaded readers there is nothing to authorise, so NOT passing `force`
	// is accepted and the removal proceeds. REQUIRING `force=0` HERE WOULD REFUSE A PERFECTLY SAFE
	// REMOVAL AND MAKE THE DANGEROUS CASE AND THE HARMLESS ONE LOOK ALIKE.
	//
	// The rule is therefore: a caller who named a count must have named the RIGHT one, and a caller who
	// named none must have nothing to authorise. Both halves are checked, and neither is a guess.
	//
	// A caller that names the WRONG count is refused with the real one, so the guard cannot be satisfied
	// by guessing. `force=0` on a field with readers is exactly as wrong as no force at all.
	for (const FFieldChange& Change : OutPlan.Changes)
	{
		if (Change.Op != EFieldOp::Remove)
		{
			continue;
		}

		TArray<FReader> Readers;
		FindReaders(Struct, Readers);

		if (Readers.Num() > 0)
		{
			OutPlan.ReadersByRemovedField.Add(Change.Field.Name, Readers);
		}

		const bool bNamedACount = Change.ForceCount != INDEX_NONE;
		const bool bCountIsRight = bNamedACount && Change.ForceCount == Readers.Num();

		// Accepted when the count is right, OR when nothing needed authorising and no count was claimed.
		if (bCountIsRight || (!bNamedACount && Readers.Num() == 0))
		{
			continue;
		}

		FString Listed;
		for (const FReader& Reader : Readers)
		{
			Listed += FString::Printf(TEXT("\n    %s (%s), %d member(s) of this struct"),
				*Reader.Path, *Reader.Class, Reader.Uses);
		}
		if (Listed.IsEmpty())
		{
			Listed = TEXT("\n    (none)");
		}

		// THE `force` VALUE IS NAMED IN THE REFUSAL, because the caller's next call is the one that works.
		// The count is printed as the count the caller must pass and is parsed, never matched as a sentence.
		OutError = FString::Printf(
			TEXT("removing '%s' from %s may affect %d loaded dependent asset(s), and force=%s does not name ")
			TEXT("that count:%s\n  This loaded-asset scan does not prove breakage; dependent compilation has not been run. Preview again with force=%d to proceed."),
			*Change.Field.Name, *OutPlan.ObjectPath, Readers.Num(),
			bNamedACount ? *FString::FromInt(Change.ForceCount) : TEXT("(not given)"),
			*Listed, Readers.Num());
		return false;
	}

	return true;
}

FString FTurboMCPStruct::RenderPreviewFields(const FPlanFields& InPlan)
{
	FString Out = FString::Printf(TEXT("preview: struct %s"), *InPlan.ObjectPath);
	Out += FString::Printf(TEXT("\nfields now: %d"), InPlan.CurrentFields.Num());
	for (const FField& Field : InPlan.CurrentFields)
	{
		Out += FString::Printf(TEXT("\n  %s: %s"), *Field.Name, *Field.TypeText);
	}

	Out += FString::Printf(TEXT("\nchanges: %d"), InPlan.Changes.Num());
	for (const FFieldChange& Change : InPlan.Changes)
	{
		switch (Change.Op)
		{
		case EFieldOp::Add:
			Out += FString::Printf(TEXT("\n  add    %s: %s"), *Change.Field.Name, *Change.Field.TypeText);
			break;
		case EFieldOp::Change:
			Out += FString::Printf(TEXT("\n  change %s: -> %s"), *Change.Field.Name, *Change.Field.TypeText);
			break;
		case EFieldOp::Reorder:
			Out += FString::Printf(TEXT("\n  move   %s %s %s"), *Change.Field.Name,
				Change.bAbove ? TEXT("above") : TEXT("below"), *Change.Anchor);
			break;
		case EFieldOp::Remove:
		{
			Out += FString::Printf(TEXT("\n  remove %s"), *Change.Field.Name);

			// D-45: A REMOVAL IS LISTED BY KEY WITH ITS CLASS AND TITLE, NEVER AS A BARE COUNT. The readers are
			// printed for every removal, including a removal that will be refused, because the list is what tells a
			// caller whether to pass force and what that force is authorising.
			if (const TArray<FReader>* Readers = InPlan.ReadersByRemovedField.Find(Change.Field.Name))
			{
				Out += FString::Printf(TEXT("  (%d loaded reader(s))"), Readers->Num());
				for (const FReader& Reader : *Readers)
				{
					Out += FString::Printf(TEXT("\n           %s (%s), %d member(s) of this struct"),
						*Reader.Path, *Reader.Class, Reader.Uses);
				}
			}
			else
			{
				Out += TEXT("  (0 loaded readers)");
			}
			break;
		}
		default:
			break;
		}
	}

	Out += TEXT("\nThe edit checks field names again; later failures may leave earlier changes applied. ")
		TEXT("This preview does not reserve the struct or prove dependent assets will compile.");
	return Out;
}

FString FTurboMCPStruct::ApplyFields(const FPlanFields& InPlan)
{
    // Struct editor operations compile and notify dependents; do not nest them in another edit.
    if (!IsInGameThread() || !GEditor || GEditor->IsTransactionActive() || GEditor->IsPlaySessionInProgress())
        return TEXT("unchanged: struct editing requires an idle editor outside PIE and active transactions; save=not_requested");
	// THE TARGET IS RE-RESOLVED HERE, NOT TRUSTED FROM THE PLAN. `D-45` requires it and the reason is that a plan is
	// made and applied in two calls, so the object can be gone in between.
	UObject* Loaded = LoadObject<UObject>(nullptr, *InPlan.ObjectPath);
	UUserDefinedStruct* Struct = Cast<UUserDefinedStruct>(Loaded);
	if (Struct == nullptr)
	{
		return FString::Printf(
			TEXT("error: %s is no longer a user-defined struct. It was re-resolved at apply time rather ")
			TEXT("than trusted from the plan, so nothing was changed."), *InPlan.ObjectPath);
	}

	const FScopedTransaction Transaction(NSLOCTEXT("TurboMCP", "StructFields", "Edit User Struct Fields"));

	int32 Added = 0;
	int32 Changed = 0;
	int32 Moved = 0;
	int32 Removed = 0;
	TArray<FString> Vanished;

	for (const FFieldChange& Change : InPlan.Changes)
	{
		const FString& Name = Change.Field.Name;

		if (Change.Op == EFieldOp::Add)
		{
			FEdGraphPinType PinType;
			FString TypeWhy;
			if (!ResolveType(Change.Field.TypeText, PinType, TypeWhy))
			{
				Vanished.Add(Name);
				continue;
			}
			if (!FStructureEditorUtils::AddVariable(Struct, PinType))
			{
				Vanished.Add(Name);
				continue;
			}

			// THE FIELD IS NAMED STRAIGHT AFTER THE ADD, BECAUSE `AddVariable` CANNOT NAME IT.
			// `StructureEditorUtils.cpp:304` passes an EMPTY name base to
			// `FMemberVariableNameHelper::Generate`, and `Generate` turns an empty base into the literal
			// `MemberVar` (`:251-253`). The rename is what the struct editor itself does (`:347-373`), and
			// without it the field would come back as `MemberVar_N` and no later operation could address it
			// by the name the caller used.
			//
			// The GUID is taken from the LAST description, because `AddVariable` APPENDS: the field that was
			// just added is the one at the end, and reading it there is what makes the rename land on this
			// field rather than on whichever one happened to match a name.
			const TArray<FStructVariableDescription>& AfterAdd =
				FStructureEditorUtils::GetVarDesc(Struct);
			if (AfterAdd.Num() > 0)
			{
				const FGuid AddedGuid = AfterAdd.Last().VarGuid;
				// *** THE NAME IS FREE BY CONSTRUCTION, SO THIS RETURNS IT UNCHANGED. *** The plan refuses an
				// Add whose name the struct already carries (`:1315-1321`), and
				// `IsUniqueVariableFriendlyName` tests the SAME thing with the SAME case-sensitive match on
				// `FriendlyName` (`StructureEditorUtils.cpp:496`). So there is nothing to suffix here, and
				// `RenameVariable` - which refuses only a name that is taken (`:352-355`) - accepts the name.
				// THE RENAME IS STILL NECESSARY, because `AddVariable` cannot name a field at all; see above.
				const FString Unique = MakeUniqueFriendlyName(Struct, Name);
				if (Unique.IsEmpty()
					|| !FStructureEditorUtils::RenameVariable(Struct, AddedGuid, Unique))
				{
					// *** DEFENSIVE, AND UNREACHABLE FROM A NAME THE STRUCT ALREADY CARRIES. *** Reaching here
					// means those two predicates disagreed, or that every suffix up to 999 was taken - and the
					// second requires the name to be taken, which the plan's refusal already rules out.
					//
					// AND THE FIELD IS NOT REMOVED: `AddVariable` has already created it, so it stays in the
					// struct under the engine's generated `MemberVar_N`. `Vanished.Add(Name)` is therefore a
					// statement about the CALLER'S REQUEST rather than about the asset, which is why it is
					// reported rather than counted as added - a field the caller cannot address must not pass
					// as one it can.
					Vanished.Add(Name);
					continue;
				}
			}
			++Added;

			// The options are set straight after the rename, because they live on the description and the name is
			// now the key to it. THAT NAME IS `Name`: `Unique` is `Name` on this path, which is what makes this
			// lookup correct rather than lucky.
			const FGuid Guid = GuidOfField(Struct, Name);
			if (Guid.IsValid())
			{
				if (!Change.Field.Default.IsEmpty())
				{
					FStructureEditorUtils::ChangeVariableDefaultValue(Struct, Guid, Change.Field.Default);
				}
				if (!Change.Field.Tooltip.IsEmpty())
				{
					FStructureEditorUtils::ChangeVariableTooltip(Struct, Guid, Change.Field.Tooltip);
				}
				if (!Change.Field.Category.IsEmpty())
				{
					if (FStructVariableDescription* Mutable =
							FStructureEditorUtils::GetVarDescByGuid(Struct, Guid))
					{
						Mutable->Category = FName(*Change.Field.Category);
					}
				}
			}
			continue;
		}

		// EVERY OTHER OPERATION NEEDS THE FIELD, AND EVERY ONE OF THEM RE-RESOLVES IT BY NAME. A field that has gone
		// is REFUSED BY NAME rather than skipped, because silently skipping it would make this edit differ from the
		// preview that authorised it.
		const FGuid Guid = GuidOfField(Struct, Name);
		if (!Guid.IsValid())
		{
			Vanished.Add(Name);
			continue;
		}

		switch (Change.Op)
		{
		case EFieldOp::Change:
		{
			FEdGraphPinType PinType;
			FString TypeWhy;
			if (!ResolveType(Change.Field.TypeText, PinType, TypeWhy))
			{
				Vanished.Add(Name);
				continue;
			}

			// THE RETURN VALUE IS NOT THE TEST, BECAUSE IT CANNOT BE. `ChangeVariableType`
			// returns `false` BOTH when the engine refuses the type AND when the field already
			// has it (`StructureEditorUtils.cpp:413-426`), and those two need opposite answers.
			// So the field is ASKED instead: a type that now compiles to what was requested is a
			// success whether or not the call had work to do.
			const bool bCallSucceeded = FStructureEditorUtils::ChangeVariableType(Struct, Guid, PinType);
			if (!bCallSucceeded && !TypeMatches(Struct, Name, Change.Field.TypeText))
			{
				// THE ENGINE'S OWN WORDS ARE CARRIED OUT, because "the type was not changed" is not
				// an answer a caller can act on. `CanHaveAMemberVariableOfType` is the predicate
				// `ChangeVariableType` checks first (`:404`) and it fills a message naming the
				// struct and the property it objected to.
				FString EngineWhy;
				FStructureEditorUtils::CanHaveAMemberVariableOfType(Struct, PinType, &EngineWhy);
				// THE FIELD'S ACTUAL COMPILED TYPE IS CARRIED OUT, because "the type was not changed"
				// is not something a caller can act on, and the engine's own message is empty for the
				// "nothing to do" answer it gives a change to the type a field already has.
				const FProperty* Current = FStructureEditorUtils::GetPropertyByFriendlyName(Struct, Name);
				Vanished.Add(FString::Printf(
					TEXT("%s (it compiles to '%s' and '%s' was asked for%s%s)"),
					*Name,
					Current != nullptr ? *Current->GetCPPType() : TEXT("(no property)"),
					*Change.Field.TypeText,
					EngineWhy.IsEmpty() ? TEXT("") : TEXT("; the engine said: "),
					*EngineWhy));
				continue;
			}
			++Changed;

			if (!Change.Field.Default.IsEmpty())
			{
				FStructureEditorUtils::ChangeVariableDefaultValue(Struct, Guid, Change.Field.Default);
			}
			if (!Change.Field.Tooltip.IsEmpty())
			{
				FStructureEditorUtils::ChangeVariableTooltip(Struct, Guid, Change.Field.Tooltip);
			}
			if (!Change.Field.Category.IsEmpty())
			{
				if (FStructVariableDescription* Mutable =
						FStructureEditorUtils::GetVarDescByGuid(Struct, Guid))
				{
					Mutable->Category = FName(*Change.Field.Category);
				}
			}
			break;
		}
		case EFieldOp::Reorder:
		{
			const FGuid AnchorGuid = GuidOfField(Struct, Change.Anchor);
			if (!AnchorGuid.IsValid())
			{
				Vanished.Add(Change.Anchor);
				continue;
			}
			const FStructureEditorUtils::EMovePosition Position = Change.bAbove
				? FStructureEditorUtils::PositionAbove
				: FStructureEditorUtils::PositionBelow;
			if (!FStructureEditorUtils::MoveVariable(Struct, Guid, AnchorGuid, Position))
			{
				Vanished.Add(Name);
				continue;
			}
			++Moved;
			break;
		}
		case EFieldOp::Remove:
			if (!FStructureEditorUtils::RemoveVariable(Struct, Guid))
			{
				Vanished.Add(Name);
				continue;
			}
			++Removed;
			break;
		default:
			break;
		}
	}

	if (Vanished.Num() > 0)
	{
		// A field this pass could not re-resolve stops the report from claiming the change set landed. The counts of
		// what DID land are printed, so the caller knows the exact state rather than being told "failed".
		//
		// THE FIELD LIST AS THIS CODE SEES IT IS PRINTED TOO, because "could not be re-resolved" on a name that
		// plainly exists is the one failure a caller cannot act on without it. It is the difference between "the
		// struct changed under you" and "this tool is looking at the struct wrongly", and those need different
		// fixes.
		TArray<FField> Seen;
		ReadFields(Struct, Seen);
		TArray<FString> SeenNames;
		for (const FField& Field : Seen)
		{
			SeenNames.Add(Field.Name);
		}

		return FString::Printf(
			TEXT("partial: these field operations could not be completed: %s. ")
			TEXT("Observed fields: %s. Completed operation counts: %d added, %d changed, %d moved, %d removed. ")
			TEXT("Other partial changes may remain; no rollback was attempted. Inspect before retrying or saving."),
			*FString::Join(Vanished, TEXT(", ")),
			SeenNames.Num() > 0 ? *FString::Join(SeenNames, TEXT(", ")) : TEXT("(none)"),
			Added, Changed, Moved, Removed);
	}

    // Field changes may have succeeded even when an option setter rejected a value.
    for (const FFieldChange& Change : InPlan.Changes)
        if (Change.Op == EFieldOp::Add || Change.Op == EFieldOp::Change)
            VerifyFieldOptions(Struct, GuidOfField(Struct, Change.Field.Name), Change.Field, Vanished);
    if (!Vanished.IsEmpty())
        return FString::Printf(TEXT("partial: requested settings were not applied: %s; other field changes may remain; inspect the unsaved struct before retrying or saving; no rollback was attempted"), *FString::Join(Vanished, TEXT(", ")));

	// Compile the final field list; individual editor operations may also compile.
	FStructureEditorUtils::CompileStructure(Struct);
	Struct->MarkPackageDirty();

	// THE READ BACK IS THE EVIDENCE, AND IT IS TAKEN FROM THE STRUCT RATHER THAN FROM THE COUNTERS ABOVE.
	TArray<FField> ReadBack;
	ReadFields(Struct, ReadBack);

	FString Out = FString::Printf(TEXT("changed: struct %s"), *InPlan.ObjectPath);
	Out += TEXT("\nVerification: operation counts and observed fields follow; requested nonempty defaults, tooltips and categories were read back; dependent assets were not verified.");
	Out += FString::Printf(TEXT("\nadded: %d"), Added);
	Out += FString::Printf(TEXT("\nchanged: %d"), Changed);
	Out += FString::Printf(TEXT("\nmoved: %d"), Moved);
	Out += FString::Printf(TEXT("\nremoved: %d"), Removed);
	Out += FString::Printf(TEXT("\nfields now: %d"), ReadBack.Num());
	for (const FField& Field : ReadBack)
	{
		Out += FString::Printf(TEXT("\n  %s: %s"), *Field.Name, *Field.TypeText);
	}
	Out += TEXT("\nThe asset is loaded but not saved. Inspect the result before calling EditTurboMCPSave; ")
		TEXT("unsaved changes will be lost when the editor closes.");

	return Out;
}
