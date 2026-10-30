// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "TurboMCPVarsAdd.h"
#include "KismetCompilerMisc.h"
#include "Kismet2/CompilerResultsLog.h"
#include "Serialization/ArchiveUObject.h"
#include "Misc/OutputDevice.h"
#include "Internationalization/Regex.h"
#include "UObject/UnrealType.h"
#include "Kismet2/Kismet2NameValidators.h"
#include "TurboMCPTypeParser.h"

#include "BlueprintEditorLibrary.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "Editor.h"
#include "Engine/MemberReference.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "UObject/Class.h"
#include "UObject/Object.h"
#include "UObject/UObjectGlobals.h"

#include "TurboMCPAssetResolver.h"

namespace
{
	/**
	 * Splits on a separator, ignoring separators nested inside `<>` or `()`.
	 *
	 * This is the whole reason the splitter is not `ParseIntoArray`: a `Map<String, Int>`
	 * carries a comma at depth 1, and splitting there would turn one variable into two
	 * broken ones. Depth is counted rather than pattern-matched, so any nesting works.
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

	/** True when every bracket in the string is balanced and none closes early. */
	bool BracketsBalance(const FString& In, FString& OutWhy)
	{
		int32 Depth = 0;
		for (const TCHAR Character : In)
		{
			if (Character == TEXT('<')) { ++Depth; }
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

	FString Trim(const FString& In)
	{
		return In.TrimStartAndEnd();
	}

	/** The basic type names a Blueprint variable can have. */
	const TSet<FString>& BasicTypeNames()
	{
		static const TSet<FString> Names = {
			TEXT("bool"), TEXT("int"), TEXT("int64"), TEXT("byte"), TEXT("real"),
			TEXT("float"), TEXT("double"), TEXT("name"), TEXT("string"), TEXT("text")
		};
		return Names;
	}

	/** The struct names the engine's own type table carries. */
	const TMap<FString, FString>& StructTypeNames()
	{
		static const TMap<FString, FString> Names = {
			{ TEXT("vector"),      TEXT("/Script/CoreUObject.Vector") },
			{ TEXT("rotator"),     TEXT("/Script/CoreUObject.Rotator") },
			{ TEXT("transform"),   TEXT("/Script/CoreUObject.Transform") },
			{ TEXT("vector2d"),    TEXT("/Script/CoreUObject.Vector2D") },
			{ TEXT("linearcolor"), TEXT("/Script/CoreUObject.LinearColor") },
			{ TEXT("color"),       TEXT("/Script/CoreUObject.Color") }
		};
		return Names;
	}

	/**
	 * True when the name is a type a variable can have, and why not when it is not.
	 *
	 * WHY THIS IS A SEPARATE STEP, CALLED BY THE PLAN
	 * -----------------------------------------------
	 * CHECKING THE TYPE IN `Apply` WOULD LET THE PREVIEW PRINT A PLAN FOR `X: banana` AND LEAVE ONLY
	 * THE EDIT TO REFUSE IT. A preview that shows a plan the edit will not carry out is worse than no
	 * preview, which is exactly what `D-02` exists to prevent. So the type is resolved while planning,
	 * and both tools refuse the same input.
	 */
    FString ReferenceTypeSyntax(const FString& Type)
    {
        const FString Trimmed=Type.TrimStartAndEnd();
        // A generated class path denotes the class of an object reference, matching
        // the editor object-variable workflow. Use class<path> for a class reference.
        if(Trimmed.StartsWith(TEXT("/")) && Trimmed.EndsWith(TEXT("_C"),ESearchCase::IgnoreCase))
            return TEXT("object<")+Trimmed+TEXT(">");
        return Trimmed;
    }
	bool ResolveTypeName(const FString& InType, FString& OutWhy)
	{
		const FString ReferenceType=ReferenceTypeSyntax(InType);
        const FString Lower = ReferenceType.ToLower();

		// Typed object and class references share the full field syntax with structs and function pins.
		if (Lower.StartsWith(TEXT("object<")) || Lower.StartsWith(TEXT("class<"))
			|| Lower.StartsWith(TEXT("softobject<")) || Lower.StartsWith(TEXT("softclass<")))
		{
			FEdGraphPinType Ignored;
			return FTurboMCPTypeParser::Resolve(ReferenceType, Ignored, OutWhy);
		}

		if (BasicTypeNames().Contains(Lower) || StructTypeNames().Contains(Lower))
		{
			return true;
		}

		// Not a basic or a named struct, so it has to be an enum or a struct ASSET, named by
		// any path form the rest of the plugin accepts (`P5`).
		FTurboMCPAssetResolver::FResolved Resolved;
		const FString Error = FTurboMCPAssetResolver::Resolve(InType, Resolved);
		if (!Error.IsEmpty())
		{
			TArray<FString> BasicList = BasicTypeNames().Array();
			BasicList.Sort();
			TArray<FString> StructKeys;
			StructTypeNames().GetKeys(StructKeys);
			StructKeys.Sort();

			OutWhy = FString::Printf(
				TEXT("'%s' is not a type. Basic types: %s. Struct names: %s. An enum or a ")
				TEXT("struct asset is named by its path."),
				*InType, *FString::Join(BasicList, TEXT(", ")),
				*FString::Join(StructKeys, TEXT(", ")));
			return false;
		}

		UObject* Loaded = LoadObject<UObject>(nullptr, *Resolved.ObjectPath);
		if (Cast<UEnum>(Loaded) != nullptr || Cast<UScriptStruct>(Loaded) != nullptr)
		{
			return true;
		}

		OutWhy = FString::Printf(
			TEXT("%s is a %s, and a variable can be an enum or a struct but not that."),
			*Resolved.ObjectPath,
			Loaded != nullptr ? *Loaded->GetClass()->GetName() : TEXT("missing object"));
		return false;
	}
    struct FDefaultImportMessages : FOutputDevice
    {
        FString Text;
        void Serialize(const TCHAR* Message, ELogVerbosity::Type, const FName&) override
        {
            if (!Text.IsEmpty()) Text += TEXT("; ");
            Text += Message;
        }
    };

    // Build an unlinked property in transient storage, then import the default into a
    // temporary value. No target Blueprint or generated class is modified by validation.
    FString ValidateVariableDefault(const FEdGraphPinType& Type, const FString& Default)
    {
        if (Default.IsEmpty()) return FString();
        UScriptStruct* Scope = NewObject<UScriptStruct>(GetTransientPackage());
        FCompilerResultsLog Messages;
        TUniquePtr<FProperty> Property(FKismetCompilerUtilities::CreatePropertyOnScope(
            Scope, TEXT("Value"), Type, nullptr, CPF_None, GetDefault<UEdGraphSchema_K2>(), Messages));
        if (!Property || Messages.NumErrors > 0)
            return TEXT("the declared type could not be represented as a variable property.");
        // Integer import accepts globally registered enum names. Numeric variables
        // require numeric text so an unrelated enum cannot silently choose their value.
        if (const FNumericProperty* Numeric = CastField<FNumericProperty>(Property.Get()))
        {
            if (!CastField<FByteProperty>(Property.Get()) || !CastField<FByteProperty>(Property.Get())->Enum)
            {
                const FString Pattern = Numeric->IsInteger()
                    ? TEXT("^[+-]?(?:[0-9]+|0[xX][0-9a-fA-F]+)$")
                    : TEXT("^[+-]?(?:[0-9]+(?:\\.[0-9]*)?|\\.[0-9]+)(?:[eE][+-]?[0-9]+)?[fF]?$");
                FRegexMatcher Matcher(FRegexPattern(Pattern), Default);
                if (!Matcher.FindNext()) return TEXT("provide a numeric literal for this numeric variable, not an enum name or malformed number.");
            }
        }
        FArchiveUObject Archive;
        Property->LinkWithoutChangingOffset(Archive);
        FDefaultConstructedPropertyElement Temporary(Property.Get());
        FDefaultImportMessages ImportMessages;
        const TCHAR* End = Property->ImportText_Direct(*Default, Temporary.GetObjAddress(), nullptr, PPF_None, &ImportMessages);
        if (!End || !FString(End).TrimStartAndEnd().IsEmpty() || !ImportMessages.Text.IsEmpty())
            return TEXT("provide complete Unreal property text with no trailing characters. ") + ImportMessages.Text;
        return FString();
    }

    FString ResolveVariableTypes(const FTurboMCPVarsAdd::FPlan& InPlan, TArray<FEdGraphPinType>& ResolvedTypes)
    {
	const int32 Added = 0;
	ResolvedTypes.Reset();
	for (const FTurboMCPVarsAdd::FVar& Variable : InPlan.Variables)
	{
		// THE TYPE RESOLVER IS THE ENGINE'S OWN, AND IT CANNOT REPORT FAILURE.
		//
		// `UBlueprintEditorLibrary::GetBasicTypeByName` says in its own comment that it
		// "returns 'int' type if invalid type is provided", so a caller that trusted it
		// would silently add an int where a `banana` was asked for. That is why the name is
		// checked against the accepted list FIRST, and the resolver is only reached for a
		// name that is known to be one of them.
		FEdGraphPinType PinType;
		const FString ReferenceType=ReferenceTypeSyntax(Variable.InnerType);
        const FString Lower = ReferenceType.ToLower();

		const TSet<FString>& BasicNames = BasicTypeNames();
		const TMap<FString, FString>& StructNames = StructTypeNames();

		if (Lower.StartsWith(TEXT("object<")) || Lower.StartsWith(TEXT("class<"))
			|| Lower.StartsWith(TEXT("softobject<")) || Lower.StartsWith(TEXT("softclass<")))
		{
			FString TypeWhy;
			if (!FTurboMCPTypeParser::Resolve(ReferenceType, PinType, TypeWhy))
			{
				return FString::Printf(TEXT("error: '%s' has invalid type '%s': %s. %d variable(s) before it were added."),
					*Variable.Name, *Variable.InnerType, *TypeWhy, Added);
			}
		}
		else if (BasicNames.Contains(Lower))
		{
			// `float` is the name a caller writes; the engine's menu calls it `real`.
			PinType = UBlueprintEditorLibrary::GetBasicTypeByName(
				FName(*(Lower == TEXT("float") ? TEXT("real") : Lower)));
		}
		else if (StructNames.Contains(Lower))
		{
			UScriptStruct* Struct = FindObject<UScriptStruct>(
				nullptr, *StructNames[Lower]);
			if (Struct == nullptr)
			{
				return FString::Printf(
					TEXT("error: the struct for '%s' could not be found in this engine, ")
					TEXT("so the list stopped there. %d variable(s) before it were added."),
					*Variable.InnerType, Added);
			}
			PinType = UBlueprintEditorLibrary::GetStructType(Struct);
		}
		else
		{
			// A path to an enum or a struct asset, which is how a caller names one the
			// engine's short table does not carry. The asset is resolved with the same
			// resolver every other tool uses, so a short name behaves the same way here.
			FTurboMCPAssetResolver::FResolved TypeResolved;
			const FString TypeError =
				FTurboMCPAssetResolver::Resolve(Variable.InnerType, TypeResolved);

			if (!TypeError.IsEmpty())
			{
				// The accepted names are listed so a caller corrects itself in one step
				// rather than guessing (`P7`). Both lists are SORTED: a TSet and a TMap do
				// not iterate in a caller's order, and an error message whose list changes
				// order between runs is a message nobody can compare.
				TArray<FString> BasicList = BasicNames.Array();
				BasicList.Sort();

				TArray<FString> StructKeys;
				StructNames.GetKeys(StructKeys);
				StructKeys.Sort();

				return FString::Printf(
					TEXT("error: '%s' is not a type. Accepted basic types: %s. Accepted ")
					TEXT("struct names: %s. For an enum or a struct asset, pass its path. ")
					TEXT("%d variable(s) before it were added."),
					*Variable.InnerType, *FString::Join(BasicList, TEXT(", ")),
					*FString::Join(StructKeys, TEXT(", ")),
					Added);
			}

			UObject* TypeObject = LoadObject<UObject>(nullptr, *TypeResolved.ObjectPath);
			if (UScriptStruct* Struct = Cast<UScriptStruct>(TypeObject))
			{
				PinType = UBlueprintEditorLibrary::GetStructType(Struct);
			}
			else if (UEnum* Enum = Cast<UEnum>(TypeObject))
			{
				// THERE IS NO `GetEnumType`. The engine's library carries basic, struct,
				// class-reference and object-reference helpers and nothing for an enum, so
				// an enum variable's pin type is assembled directly: its category is the
				// byte category and the enum itself is the sub-category object, which is
				// the shape `EdGraphSchema_K2` reads back.
				PinType.PinCategory = UEdGraphSchema_K2::PC_Byte;
				PinType.PinSubCategoryObject = Enum;
			}
			else
			{
				return FString::Printf(
					TEXT("error: %s is a %s, and a variable can be an enum or a struct ")
					TEXT("but not that. %d variable(s) before it were added."),
					*TypeResolved.ObjectPath,
					TypeObject != nullptr ? *TypeObject->GetClass()->GetName()
						: TEXT("missing object"),
					Added);
			}
		}

		// THE CONTAINER WRAPPERS ARE THE ENGINE'S OWN, and replacing the inner type is what
		// makes `Array<Vector>` an array OF vector rather than an array of whatever the
		// inner call happened to return. `GetMapType` also carries the value type into the
		// terminal, which setting `ContainerType` alone does not do.
		if (Variable.Container != FTurboMCPVarsAdd::FVar::EContainer::None)
		{
            if (Variable.Container != FTurboMCPVarsAdd::FVar::EContainer::Array
                && !FBlueprintEditorUtils::HasGetTypeHash(PinType))
                return FString::Printf(TEXT("'%s' requires a hashable set element or map key type."), *Variable.Name);
			if (Variable.Container == FTurboMCPVarsAdd::FVar::EContainer::Array)
			{
				PinType = UBlueprintEditorLibrary::GetArrayType(PinType);
			}
			else if (Variable.Container == FTurboMCPVarsAdd::FVar::EContainer::Set)
			{
				PinType = UBlueprintEditorLibrary::GetSetType(PinType);
			}
			else
			{
				const FString ValueLower = Variable.ValueType.ToLower();
				if (!BasicNames.Contains(ValueLower))
				{
					return FString::Printf(
						TEXT("error: '%s' is not a usable map value type; this tool accepts ")
						TEXT("a basic type there. %d variable(s) before it were added."),
						*Variable.ValueType, Added);
				}

				const FEdGraphPinType ValueType = UBlueprintEditorLibrary::GetBasicTypeByName(
					FName(*(ValueLower == TEXT("float") ? TEXT("real") : ValueLower)));
				PinType = UBlueprintEditorLibrary::GetMapType(PinType, ValueType);
			}
		}

        const FString DefaultError = ValidateVariableDefault(PinType, Variable.Default);
        if (!DefaultError.IsEmpty())
            return FString::Printf(TEXT("'%s' has an invalid default: %s"), *Variable.Name, *DefaultError);
        ResolvedTypes.Add(PinType);
    }

        return FString();
    }

}

bool FTurboMCPVarsAdd::Plan(const FString& InAsset, const FString& InVariables,
	FPlan& OutPlan, FString& OutError)
{
	OutError.Empty();
	OutPlan = FPlan();
	OutPlan.Asset = Trim(InAsset);

	if (OutPlan.Asset.IsEmpty())
	{
		OutError = TEXT("no asset given. Pass a Blueprint in any path form, for example ")
			TEXT("BP_NodeTest.");
		return false;
	}
	if (Trim(InVariables).IsEmpty())
	{
		OutError = TEXT("no variables given. Pass them as a comma-separated list, for ")
			TEXT("example \"Health: float default=100, bIsDead: bool default=false\".");
		return false;
	}

	// Each variable is a `Name: Type` head followed by optional `key=value` fields. Fields
	// are separated by spaces at depth 0, so a type with a space in it must be inside
	// brackets, which is what `<...>` is for.
	TArray<FString> Items;
	SplitTopLevel(InVariables, TEXT(','), Items);

	TSet<FName> Seen;
	for (const FString& RawItem : Items)
	{
		const FString Item = Trim(RawItem);
		if (Item.IsEmpty())
		{
			continue;
		}

		FVar Variable;

		// The name is everything up to the first colon at depth 0.
		FString Head;
		FString Rest;
		if (!Item.Split(TEXT(":"), &Head, &Rest))
		{
			OutError = FString::Printf(
				TEXT("'%s' has no colon, so it does not say what type it is. Pass ")
				TEXT("\"Name: Type\"."), *Item);
			return false;
		}

		Variable.Name = Trim(Head);
		if (Variable.Name.IsEmpty())
		{
			OutError = FString::Printf(TEXT("'%s' has no name before the colon."), *Item);
			return false;
		}

		// Is the name usable? The engine would refuse it later, which is too late: the
		// other variables in the list would already be added.
		{
			FString Why;
			bool bNameOk = !Variable.Name.IsEmpty();
			// Blueprint names are FNames, not C++ identifiers: a leading digit is legal.
			if (Variable.Name.Len() > FKismetNameValidator::GetMaximumNameLength())
			{
				Why = TEXT("it exceeds the Blueprint name length limit");
				bNameOk = false;
			}
			for (const TCHAR Character : Variable.Name)
			{
				if (!FChar::IsAlnum(Character) && Character != TEXT('_'))
				{
					Why = FString::Printf(
						TEXT("'%c' is not a letter, a digit or an underscore"), Character);
					bNameOk = false;
					break;
				}
			}
			if (!bNameOk)
			{
				OutError = FString::Printf(
					TEXT("'%s' cannot be a variable name: %s."), *Variable.Name, *Why);
				return false;
			}
		}

		if (Seen.Contains(FName(*Variable.Name)))
		{
			OutError = FString::Printf(
				TEXT("'%s' is given twice. Each variable needs its own name."),
				*Variable.Name);
			return false;
		}
		Seen.Add(FName(*Variable.Name));

		// The fields, split on spaces at depth 0. The first token is the type; the rest are
		// `key=value` pairs.
		TArray<FString> Tokens;
		SplitTopLevel(Trim(Rest), TEXT(' '), Tokens);

		bool bTypeSeen = false;
		TSet<FString> SeenOptions;
		for (const FString& RawToken : Tokens)
		{
			const FString Token = Trim(RawToken);
			if (Token.IsEmpty())
			{
				continue;
			}

			FString Key;
			FString Value;
			const bool bIsPair = Token.Split(TEXT("="), &Key, &Value);

			if (!bIsPair)
			{
				if (bTypeSeen)
				{
					// A second bare token would be ambiguous: it could be a type with a
					// space or a mistake. Refusing is better than guessing which.
					OutError = FString::Printf(
						TEXT("'%s' has a bare word '%s' after its type. A field must be ")
						TEXT("key=value, and a type with a space goes in brackets."),
						*Variable.Name, *Token);
					return false;
				}
				Variable.TypeText = Token;
				bTypeSeen = true;
				continue;
			}

			Key = Trim(Key).ToLower();
			Value = Trim(Value);
			const FString OptionKey = Key == TEXT("value") ? TEXT("default") : Key;
			if (SeenOptions.Contains(OptionKey))
			{
				OutError = FString::Printf(TEXT("'%s' repeats option '%s'; default and value are aliases and cannot both be supplied."), *Variable.Name, *Key);
				return false;
			}
			SeenOptions.Add(OptionKey);
			if (Key == TEXT("private") || Key == TEXT("readonly") || Key == TEXT("instanceeditable") || Key == TEXT("exposeonspawn"))
			{
				const FString LowerValue = Value.ToLower();
				const bool bTrueValue = LowerValue == TEXT("true") || LowerValue == TEXT("1") || LowerValue == TEXT("yes") || LowerValue == TEXT("on");
				const bool bFalseValue = LowerValue == TEXT("false") || LowerValue == TEXT("0") || LowerValue == TEXT("no") || LowerValue == TEXT("off");
				if (!bTrueValue && !bFalseValue)
				{
					OutError = FString::Printf(TEXT("'%s' option '%s' requires true/false, 1/0, yes/no or on/off; got '%s'."), *Variable.Name, *Key, *Value);
					return false;
				}
				Value = bTrueValue ? TEXT("true") : TEXT("false");
			}


			if (Key == TEXT("default") || Key == TEXT("value"))
			{
				Variable.Default = Value;
			}
			else if (Key == TEXT("category"))
			{
				Variable.Category = Value;
			}
			else if (Key == TEXT("tooltip"))
			{
				// Tooltip follows the same grouped-token grammar as other values.
				const int32 At = Token.Find(TEXT("="));
				Variable.Tooltip = At >= 0 ? Trim(Token.Mid(At + 1)) : Value;
			}
			else if (Key == TEXT("private"))    { Variable.bPrivate = Value.ToBool(); }
			else if (Key == TEXT("readonly"))   { Variable.bReadOnly = Value.ToBool(); }
			else if (Key == TEXT("instanceeditable"))
			{
				Variable.bInstanceEditable = Value.ToBool();
			}
			else if (Key == TEXT("exposeonspawn"))
			{
				Variable.bExposeOnSpawn = Value.ToBool();
			}
			else
			{
				// An unknown key is refused rather than ignored, because a caller who wrote
				// `readonly=true` and got a settable variable has been misled.
				OutError = FString::Printf(
					TEXT("'%s' has an unknown field '%s'. Accepted: default, category, ")
					TEXT("tooltip, private, readonly, instanceEditable, exposeOnSpawn."),
					*Variable.Name, *Key);
				return false;
			}
		}

		if (!bTypeSeen || Variable.TypeText.IsEmpty())
		{
			OutError = FString::Printf(
				TEXT("'%s' has no type. Pass \"%s: float\"."),
				*Variable.Name, *Variable.Name);
			return false;
		}

		// Containers, recognised by their brackets. A bracket that never closes is refused
		// here, where the message can name the variable, rather than producing a truncated
		// type that silently means something else.
		FString Why;
		if (!BracketsBalance(Variable.TypeText, Why))
		{
			OutError = FString::Printf(
				TEXT("'%s' has an unbalanced type '%s': %s."),
				*Variable.Name, *Variable.TypeText, *Why);
			return false;
		}

		const FString LowerType = Variable.TypeText.ToLower();
		auto Inside = [&Variable](const FString& Prefix) -> FString
		{
			return Trim(Variable.TypeText.Mid(
				Prefix.Len(), Variable.TypeText.Len() - Prefix.Len() - 1));
		};

		if (LowerType.StartsWith(TEXT("array<")))
		{
			Variable.Container = FVar::EContainer::Array;
			Variable.InnerType = Inside(TEXT("array<"));
		}
		else if (LowerType.StartsWith(TEXT("set<")))
		{
			Variable.Container = FVar::EContainer::Set;
			Variable.InnerType = Inside(TEXT("set<"));
		}
		else if (LowerType.StartsWith(TEXT("map<")))
		{
			Variable.Container = FVar::EContainer::Map;
			const FString Inner = Inside(TEXT("map<"));
			TArray<FString> Halves;
			SplitTopLevel(Inner, TEXT(','), Halves);
			if (Halves.Num() != 2)
			{
				OutError = FString::Printf(
					TEXT("'%s' has a map type with %d part(s); a map needs a key type and ")
					TEXT("a value type, as Map<String, Int>."),
					*Variable.Name, Halves.Num());
				return false;
			}
			Variable.InnerType = Trim(Halves[0]);
			Variable.ValueType = Trim(Halves[1]);
		}
		else
		{
			Variable.Container = FVar::EContainer::None;
			Variable.InnerType = Variable.TypeText;
		}

		// THE TYPE IS RESOLVED WHILE PLANNING, so the Preview refuses the same input the
		// Edit would. Checking it only in `Apply` let the preview print a plan for
		// `X: banana`, which is the disagreement `D-02` exists to prevent.
		{
			FString TypeWhy;
			if (!ResolveTypeName(Variable.InnerType, TypeWhy))
			{
				OutError = FString::Printf(TEXT("'%s': %s"), *Variable.Name, *TypeWhy);
				return false;
			}

			// A map's value type is checked too, for the same reason.
			if (Variable.Container == FVar::EContainer::Map)
			{
				const FString ValueLower = Variable.ValueType.ToLower();
				if (!BasicTypeNames().Contains(ValueLower))
				{
					OutError = FString::Printf(
						TEXT("'%s': '%s' is not a usable map value type; this tool accepts ")
						TEXT("a basic type there."),
						*Variable.Name, *Variable.ValueType);
					return false;
				}
			}
		}

		OutPlan.Variables.Add(MoveTemp(Variable));
	}

	if (OutPlan.Variables.Num() == 0)
	{
		OutError = TEXT("the list parsed to no variables.");
		return false;
	}

    TArray<FEdGraphPinType> ResolvedTypes;
    OutError = ResolveVariableTypes(OutPlan, ResolvedTypes);
    if (!OutError.IsEmpty()) { OutPlan.Variables.Reset(); return false; }
    return true;
}

FString FTurboMCPVarsAdd::RenderPreview(const FPlan& InPlan)
{
	FString Out = FString::Printf(TEXT("preview: add %d variable(s) to %s"),
		InPlan.Variables.Num(), *InPlan.Asset);

	for (const FVar& Variable : InPlan.Variables)
	{
		Out += FString::Printf(TEXT("\n  %s : %s"),
			*Variable.Name, *Variable.TypeText);

		TArray<FString> Facts;
		if (!Variable.Default.IsEmpty())
		{
			Facts.Add(FString::Printf(TEXT("default %s"), *Variable.Default));
		}
		if (!Variable.Category.IsEmpty())
		{
			Facts.Add(FString::Printf(TEXT("category %s"), *Variable.Category));
		}

		TArray<FString> FlagWords;
		if (Variable.bPrivate)          { FlagWords.Add(TEXT("private")); }
		if (Variable.bReadOnly)         { FlagWords.Add(TEXT("read only")); }
		if (Variable.bInstanceEditable) { FlagWords.Add(TEXT("instance editable")); }
		if (Variable.bExposeOnSpawn)    { FlagWords.Add(TEXT("expose on spawn")); }
		if (FlagWords.Num() > 0)
		{
			Facts.Add(FString::Join(FlagWords, TEXT(", ")));
		}
		if (!Variable.Tooltip.IsEmpty())
		{
			Facts.Add(FString::Printf(TEXT("tooltip \"%s\""), *Variable.Tooltip));
		}

		if (Facts.Num() > 0)
		{
			Out += FString::Printf(TEXT("\n      %s"), *FString::Join(Facts, TEXT("; ")));
		}
	}

	Out += TEXT("\n; Preview validates declaration syntax and referenced types, not the target Blueprint or existing names. Defaults are checked by importing Unreal property text into temporary storage.");
	Out += TEXT("\n; Edit checks names before adding variables, but an editor failure can leave a partial list. No automatic rollback is performed.");
	Out += TEXT("\n; Edit may regenerate the skeleton repeatedly and run editor callbacks; it does not perform a full compile or save.");
	return Out;
}

FString FTurboMCPVarsAdd::Apply(const FPlan& InPlan)
{
    // Stock AddMemberVariable performs structural compilation; never put it inside rollback scope.
    if (!IsInGameThread() || !GEditor || GEditor->IsTransactionActive() || GEditor->IsPlaySessionInProgress())
        return TEXT("unchanged: variable creation requires an idle editor outside a transaction; structural compilation cannot be rolled back safely");
    if (InPlan.Variables.IsEmpty()) return TEXT("unchanged: no variables requested");

	FTurboMCPAssetResolver::FResolved Resolved;
	const FString ResolveError = FTurboMCPAssetResolver::Resolve(InPlan.Asset, Resolved);
	if (!ResolveError.IsEmpty())
	{
		return ResolveError;
	}

	UBlueprint* Blueprint = Cast<UBlueprint>(LoadObject<UObject>(nullptr, *Resolved.ObjectPath));
	if (Blueprint == nullptr)
	{
		return FString::Printf(
			TEXT("error: %s is not a Blueprint, so it cannot hold variables."),
			*Resolved.ObjectPath);
	}

	// EVERY NAME IS CHECKED BEFORE ANYTHING IS ADDED. A list that added three of five
	// variables and then refused would leave the Blueprint in a state the caller did not
	// ask for, which is what `P8` forbids.
	TArray<FString> Clashes;
    TSet<FName> RequestedNames;
    FKismetNameValidator NameValidator(Blueprint);
	for (const FVar& Variable : InPlan.Variables)
	{
		const EValidatorResult NameResult = NameValidator.IsValid(Variable.Name);
        if (NameResult != EValidatorResult::Ok)
        {
            Clashes.Add(Variable.Name + TEXT(" (") + INameValidatorInterface::GetErrorString(Variable.Name, NameResult) + TEXT(")"));
            continue;
        }
        const FName Name(*Variable.Name);
        if (Name.IsNone() || RequestedNames.Contains(Name))
        { Clashes.Add(Variable.Name + TEXT(" (empty or duplicate name)")); continue; }
        RequestedNames.Add(Name);
		if (FBlueprintEditorUtils::FindMemberVariableGuidByName(Blueprint, Name).IsValid())
		{
			Clashes.Add(FString::Printf(TEXT("%s (already a variable)"), *Variable.Name));
		}
		else if (FindFProperty<FProperty>(
			Blueprint->ParentClass, Name) != nullptr)
		{
			Clashes.Add(FString::Printf(TEXT("%s (already on the parent class)"),
				*Variable.Name));
		}
	}

	if (Clashes.Num() > 0)
	{
		return FString::Printf(
			TEXT("unchanged: %d name(s) are invalid or already used, so nothing was added: %s"),
			Clashes.Num(), *FString::Join(Clashes, TEXT(", ")));
	}

    int32 Added = 0;
    TArray<FEdGraphPinType> ResolvedTypes;
    const FString TypeError = ResolveVariableTypes(InPlan, ResolvedTypes);
    if (!TypeError.IsEmpty()) return TEXT("error: ") + TypeError + TEXT(" No variables were added.");

    // Type resolution above is read-only. Do not partially apply a list with an invalid later type.
    for (int32 Index = 0; Index < InPlan.Variables.Num(); ++Index)
    {
        const FVar& Variable = InPlan.Variables[Index];
        if (!FBlueprintEditorUtils::AddMemberVariable(Blueprint, FName(*Variable.Name), ResolvedTypes[Index], Variable.Default))
            return FString::Printf(TEXT("%s: engine refused variable '%s'; added=%d; rollback=not_attempted; structural compilation may have run; inspect before retrying"),
                Added ? TEXT("partial") : TEXT("unchanged"), *Variable.Name, Added);
        ++Added;
    }

	// The flags and the category live on the description rather than on the add call, so
	// they are set after, by finding the entry the add created.
	for (const FVar& Variable : InPlan.Variables)
	{
		const FName Name(*Variable.Name);
		FBPVariableDescription* Description = Blueprint->NewVariables.FindByPredicate(
			[&Name](const FBPVariableDescription& Candidate)
			{
				return Candidate.VarName == Name;
			});

		if (Description == nullptr)
		{
			return FString::Printf(TEXT("partial: added %d variable(s), but metadata for %s could not be applied because its description is missing. Inspect the Blueprint before retrying; changes are not saved."), Added, *Variable.Name);
		}

		if (!Variable.Category.IsEmpty())
		{
			Description->Category = FText::FromString(Variable.Category);
		}


		if (Variable.bReadOnly)
		{
			// The engine's own toggling of this flag is at BlueprintEditorUtils.cpp:3838,
			// which is the read-only checkbox in the details panel.
			Description->PropertyFlags |= CPF_BlueprintReadOnly;
		}
		else
		{
			Description->PropertyFlags &= ~CPF_BlueprintReadOnly;
		}

		// A Blueprint variable is visible to Blueprint code unless it says otherwise; the
		// flag is set here so the value does not depend on what the add left behind.
		Description->PropertyFlags |= CPF_BlueprintVisible;

		// `CPF_DisableEditOnInstance` is what the details panel shows as "Instance
		// Editable" being OFF.
		if (Variable.bInstanceEditable)
		{
			Description->PropertyFlags &= ~CPF_DisableEditOnInstance;
		}
		else
		{
			Description->PropertyFlags |= CPF_DisableEditOnInstance;
		}

		if (Variable.bExposeOnSpawn)
		{
			Description->PropertyFlags |= CPF_ExposeOnSpawn;
		}

		// Blueprint private access uses metadata, not the Details-panel editability flag.
		if (Variable.bPrivate)
		{
			FBlueprintEditorUtils::SetBlueprintVariableMetaData(
				Blueprint, Name, nullptr, FBlueprintMetadata::MD_Private, TEXT("true"));
		}
		else
		{
			FBlueprintEditorUtils::RemoveBlueprintVariableMetaData(
				Blueprint, Name, nullptr, FBlueprintMetadata::MD_Private);
		}

		if (!Variable.Tooltip.IsEmpty())
		{
			// The scope argument is NULL, which is what the engine's own
			// BlueprintEditorLibrary.cpp:1421 passes for a Blueprint-level variable.
			FBlueprintEditorUtils::SetBlueprintVariableMetaData(
				Blueprint, Name, /*InLocalVarScope=*/nullptr,
				FName(TEXT("tooltip")),
				Variable.Tooltip);
		}
	}

	// Publish flag and metadata changes; each earlier add may also regenerate the skeleton.
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

	// The read-back. A tool that reported what it added without checking would be reporting
	// its own intention, which this project has already learned is not evidence.
	int32 Confirmed = 0;
	TArray<FString> Missing;
	for (const FVar& Variable : InPlan.Variables)
	{
		if (FBlueprintEditorUtils::FindMemberVariableGuidByName(
				Blueprint, FName(*Variable.Name)).IsValid())
		{
			++Confirmed;
		}
		else
		{
			Missing.Add(Variable.Name);
		}
	}

	FString Out = FString::Printf(TEXT("%s: added %d variable(s) to %s"),
		Missing.IsEmpty() ? TEXT("changed") : TEXT("partial"), Added, *Resolved.ObjectPath);
	Out += FString::Printf(TEXT("\nread back from the Blueprint: %d of %d"),
		Confirmed, InPlan.Variables.Num());

	if (Missing.Num() > 0)
	{
		Out += FString::Printf(TEXT("\nNOT found after the add: %s"),
			*FString::Join(Missing, TEXT(", ")));
	}

	Out += TEXT("\n; Changes are in memory and are not saved. Read-back checks names only, not defaults, flags or full compilation.");
	Out += TEXT("\n; Review the variables, then use EditTurboMCPCompile and EditTurboMCPSave as needed. No automatic rollback or dedicated undo transaction is provided.");
	return Out;
}
