// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0

#include "TurboMCPTypeParser.h"

#include "BlueprintEditorLibrary.h"
#include "EdGraphSchema_K2.h"
#include "UObject/Class.h"
#include "UObject/UObjectGlobals.h"

namespace
{
	FString Trim(const FString& In) { return In.TrimStartAndEnd(); }

	bool BracketsBalance(const FString& In, FString& OutWhy)
	{
		int32 Depth = 0;
		for (const TCHAR Character : In)
		{
			if (Character == TEXT('<')) { ++Depth; }
			else if (Character == TEXT('>') && --Depth < 0)
			{
				OutWhy = TEXT("a '>' closes before any '<' opens");
				return false;
			}
		}
		if (Depth != 0) { OutWhy = FString::Printf(TEXT("%d '<' is never closed"), Depth); return false; }
		return true;
	}

	void SplitTopLevel(const FString& In, TArray<FString>& Out)
	{
		Out.Reset(); int32 Depth = 0; int32 Start = 0;
		for (int32 Index = 0; Index < In.Len(); ++Index)
		{
			if (In[Index] == TEXT('<')) { ++Depth; }
			else if (In[Index] == TEXT('>')) { --Depth; }
			else if (In[Index] == TEXT(',') && Depth == 0) { Out.Add(In.Mid(Start, Index - Start)); Start = Index + 1; }
		}
		Out.Add(In.Mid(Start));
	}

	const TMap<FString, FName>& BasicTypes()
	{
		static const TMap<FString, FName> Types = {
			{ TEXT("bool"), UEdGraphSchema_K2::PC_Boolean }, { TEXT("byte"), UEdGraphSchema_K2::PC_Byte },
			{ TEXT("int"), UEdGraphSchema_K2::PC_Int }, { TEXT("int64"), UEdGraphSchema_K2::PC_Int64 },
			{ TEXT("float"), UEdGraphSchema_K2::PC_Real }, { TEXT("real"), UEdGraphSchema_K2::PC_Real },
			{ TEXT("double"), UEdGraphSchema_K2::PC_Real }, { TEXT("name"), UEdGraphSchema_K2::PC_Name },
			{ TEXT("string"), UEdGraphSchema_K2::PC_String }, { TEXT("text"), UEdGraphSchema_K2::PC_Text }};
		return Types;
	}
	const TMap<FString, FString>& StructTypes()
	{
		static const TMap<FString, FString> Types = {
			{ TEXT("vector"), TEXT("/Script/CoreUObject.Vector") }, { TEXT("rotator"), TEXT("/Script/CoreUObject.Rotator") },
			{ TEXT("transform"), TEXT("/Script/CoreUObject.Transform") }, { TEXT("vector2d"), TEXT("/Script/CoreUObject.Vector2D") },
			{ TEXT("linearcolor"), TEXT("/Script/CoreUObject.LinearColor") }, { TEXT("color"), TEXT("/Script/CoreUObject.Color") }};
		return Types;
	}

	bool ResolveClass(const FString& InName, UClass*& OutClass, FString& OutWhy)
	{
		OutClass = LoadObject<UClass>(nullptr, *InName);
		if (OutClass == nullptr)
		{
			OutWhy = FString::Printf(TEXT("'%s' is not a known class. Pass a loadable class object path, for example /Script/Engine.Actor."), *InName);
			return false;
		}
		return true;
	}

	bool ResolveBare(const FString& InText, FEdGraphPinType& Out, FString& OutWhy)
	{
		const FString Lower = InText.ToLower();
		if (const FName* Basic = BasicTypes().Find(Lower))
		{
			Out = UBlueprintEditorLibrary::GetBasicTypeByName(*Basic);
			return true;
		}
		if (const FString* StructPath = StructTypes().Find(Lower))
		{
			if (UScriptStruct* Struct = FindObject<UScriptStruct>(nullptr, **StructPath)) { Out = UBlueprintEditorLibrary::GetStructType(Struct); return true; }
			OutWhy = FString::Printf(TEXT("the engine struct '%s' could not be found"), **StructPath); return false;
		}
		UObject* Object = LoadObject<UObject>(nullptr, *InText);
		if (UScriptStruct* Struct = Cast<UScriptStruct>(Object)) { Out = UBlueprintEditorLibrary::GetStructType(Struct); return true; }
		if (UEnum* Enum = Cast<UEnum>(Object)) { Out.PinCategory = UEdGraphSchema_K2::PC_Byte; Out.PinSubCategoryObject = Enum; return true; }
		OutWhy = Object == nullptr
			? FString::Printf(TEXT("'%s' is not a type or a loadable class path"), *InText)
			: FString::Printf(TEXT("%s is a %s, and is not a struct or enum"), *InText, *Object->GetClass()->GetName());
		return false;
	}
}

bool FTurboMCPTypeParser::Resolve(const FString& InType, FEdGraphPinType& OutPinType, FString& OutWhy)
{
	const FString Text = Trim(InType);
	if (Text.IsEmpty()) { OutWhy = TEXT("no type given"); return false; }
	if (!BracketsBalance(Text, OutWhy)) { OutWhy = FString::Printf(TEXT("the type '%s' is not balanced: %s"), *Text, *OutWhy); return false; }

	const FString Lower = Text.ToLower();
	struct FContainer { const TCHAR* Prefix; EPinContainerType Kind; };
	static const FContainer Containers[] = { { TEXT("array<"), EPinContainerType::Array }, { TEXT("set<"), EPinContainerType::Set }, { TEXT("map<"), EPinContainerType::Map } };
	for (const FContainer& Container : Containers)
	{
		const FString Prefix(Container.Prefix);
		if (!Lower.StartsWith(Prefix)) { continue; }
		if (!Text.EndsWith(TEXT(">"))) { OutWhy = FString::Printf(TEXT("the type '%s' does not end with '>'"), *Text); return false; }
		const FString Contents = Text.Mid(Prefix.Len(), Text.Len() - Prefix.Len() - 1);
		TArray<FString> Parts; SplitTopLevel(Contents, Parts);
		if ((Container.Kind == EPinContainerType::Map && Parts.Num() != 2) || (Container.Kind != EPinContainerType::Map && Parts.Num() != 1))
		{ OutWhy = FString::Printf(TEXT("'%s' has the wrong number of contained types"), *Text); return false; }
		FEdGraphPinType Key; if (!Resolve(Trim(Parts[0]), Key, OutWhy)) { return false; }
		if (Container.Kind == EPinContainerType::Array) { OutPinType = UBlueprintEditorLibrary::GetArrayType(Key); return true; }
		if (Container.Kind == EPinContainerType::Set) { OutPinType = UBlueprintEditorLibrary::GetSetType(Key); return true; }
		FEdGraphPinType Value; if (!Resolve(Trim(Parts[1]), Value, OutWhy)) { return false; }
		OutPinType = UBlueprintEditorLibrary::GetMapType(Key, Value); return true;
	}

	struct FReference { const TCHAR* Prefix; const FName Category; };
	static const FReference References[] = { { TEXT("object<"), UEdGraphSchema_K2::PC_Object }, { TEXT("class<"), UEdGraphSchema_K2::PC_Class }, { TEXT("softobject<"), UEdGraphSchema_K2::PC_SoftObject }, { TEXT("softclass<"), UEdGraphSchema_K2::PC_SoftClass } };
	for (const FReference& Reference : References)
	{
		const FString Prefix(Reference.Prefix);
		if (!Lower.StartsWith(Prefix)) { continue; }
		if (!Text.EndsWith(TEXT(">"))) { OutWhy = FString::Printf(TEXT("the typed reference '%s' does not end with '>'"), *Text); return false; }
		const FString ClassName = Trim(Text.Mid(Prefix.Len(), Text.Len() - Prefix.Len() - 1));
		if (ClassName.IsEmpty()) { OutWhy = FString::Printf(TEXT("'%s' names no class"), *Text); return false; }
		UClass* Class = nullptr; if (!ResolveClass(ClassName, Class, OutWhy)) { return false; }
		OutPinType.PinCategory = Reference.Category;
		OutPinType.PinSubCategoryObject = Class;
		return true;
	}
	return ResolveBare(Text, OutPinType, OutWhy);
}
