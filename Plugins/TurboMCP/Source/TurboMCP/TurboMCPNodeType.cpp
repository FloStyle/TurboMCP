// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "TurboMCPNodeType.h"
#include "TurboMCPGraphTextLex.h"
#include "K2Node_PromotableOperator.h"
#include "TurboMCPBlueprintRead.h"

#include "BlueprintActionDatabase.h"
#include "BlueprintActionFilter.h"
#include "BlueprintNodeSpawner.h"
#include "BlueprintFieldNodeSpawner.h"
#include "K2Node_MakeStruct.h"
#include "K2Node_BreakStruct.h"
#include "BlueprintVariableNodeSpawner.h"
#include "BlueprintDelegateNodeSpawner.h"
#include "K2Node_VariableGet.h"
#include "WidgetBlueprint.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Widget.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "K2Node_VariableSet.h"
#include "Internationalization/Text.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "K2Node.h"
#include "K2Node_Self.h"
#include "K2Node_BaseAsyncTask.h"
#include "K2Node_CallFunction.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "UObject/Class.h"
#include "UObject/UnrealType.h"
#include "UObject/Object.h"
#include "UObject/UObjectGlobals.h"

#include "TurboMCPAssetResolver.h"
#include "TurboMCPNodeId.h"
#include "TurboMCPToolSchema.h"

namespace
{
	/** The engine's own spelling for a pin direction, which its pin tools expect. */
	FString DirectionSpelling(const EEdGraphPinDirection InDirection)
	{
		// The engine's `LexToString(EEdGraphPinDirection)` answers "Input" and "Output"
		// (`EdGraphNode.h:104-119`), which is a DISPLAY string. `07_ENGINE_NOTES.md` section
		// 3.1 records that the pin tools take `EGPD_Input` and `EGPD_Output`, so the two cannot
		// be the same function and this one is written to the spelling the tools accept.
		return InDirection == EGPD_Input ? TEXT("EGPD_Input") : TEXT("EGPD_Output");
	}

	/**
	 * The exact ID the engine's action database publishes for one entry.
	 *
	 * Reproduces the two `Replace(TEXT(" "), TEXT(""))` calls from
	 * `BlueprintGraphEditor.cpp:1832-1834`. Nothing is added to the rule: the point is to match
	 * it, and the caller compares the result against the canonical generator for any node that
	 * already exists, which is the only way to know the two agree.
	 */
	FString IdFromUiSpec(const FBlueprintActionUiSpec& InSpec)
	{
		FString Category = InSpec.Category.ToString();
		FString Title = InSpec.MenuName.ToString();
		Category.ReplaceInline(TEXT(" "), TEXT(""));
		Title.ReplaceInline(TEXT(" "), TEXT(""));
		if (Title.IsEmpty())
		{
			return FString();
		}
		return FString::Printf(TEXT("%s|%s"), *Category, *Title);
	}

	/** The last segment after a `|`, which is the title. */
	FString TitleOf(const FString& InTypeId)
	{
		int32 Pipe = INDEX_NONE;
		if (InTypeId.FindLastChar(TEXT('|'), Pipe))
		{
			return InTypeId.Mid(Pipe + 1);
		}
		return InTypeId;
	}

	/** Everything before the last `|`, which is the category. Empty when there is none. */
	FString CategoryOf(const FString& InTypeId)
	{
		int32 Pipe = INDEX_NONE;
		if (InTypeId.FindLastChar(TEXT('|'), Pipe))
		{
			return InTypeId.Left(Pipe);
		}
		return FString();
	}

	/**
	 * A class name fit to print, from an action owner.
	 *
	 * A function's owner is the class that DECLARES it, which is what `03_TOOLS.md` asks for
	 * when the same name is overloaded on several classes.
	 */
	FString OwnerClassName(const UObject* InOwner)
	{
		if (InOwner == nullptr)
		{
			return FString();
		}
		if (const UFunction* Function = Cast<UFunction>(InOwner))
		{
			const UClass* Owner = Function->GetOwnerClass();
			return Owner != nullptr ? Owner->GetName() : FString();
		}
		if (const UBlueprint* Blueprint = Cast<UBlueprint>(InOwner))
		{
			// A Blueprint's own functions and delegates are keyed by the Blueprint rather than
			// by a UClass, so the Blueprint's own name is the useful answer.
			return Blueprint->GetName();
		}
		if (const UClass* Class = Cast<UClass>(InOwner))
		{
			return Class->GetName();
		}
		return InOwner->GetName();
	}

	/** True when the node class is the engine's async-task base, one of the two latent tests. */
	bool ClassIsLatent(const UClass* InNodeClass)
	{
		// Read from the engine rather than invented: `BlueprintActionFilter.cpp:668` treats a
		// class deriving from `UK2Node_BaseAsyncTask` as latent, and `:674` falls back to the
		// function's `MD_Latent` metadata.
		return InNodeClass != nullptr && InNodeClass->IsChildOf<UK2Node_BaseAsyncTask>();
	}

	/** True when the function carries the engine's latent marker. */
	bool FunctionIsLatent(const UFunction* InFunction)
	{
		return InFunction != nullptr
			&& InFunction->HasMetaData(FBlueprintMetadata::MD_Latent);
	}

	/** One entry the database offers, with everything both tools need about it. */
	struct FEntry
	{
		FString TypeId;
        bool bDesignerGetter = false;
        FString InputScope;
        FName InputName;
        FEdGraphPinType InputType;
        FString LegacyTypeId;
		FString SearchKeywords;
		TArray<FString> Aliases;
		FString OwningClass;
		bool bOwnedByThisBlueprint = false;
        bool bContextVariable = false;
		bool bPure = false;
		bool bLatent = false;
		const UBlueprintNodeSpawner* Spawner = nullptr;
		UObject* Owner = nullptr;
	};

    UEdGraphNode* CreateInputGetter(const FEntry& Entry, UEdGraph* Graph)
    {
        UK2Node_VariableGet* Node=NewObject<UK2Node_VariableGet>(Graph,NAME_None,RF_Transactional);
        if(!Node) return nullptr;
        Node->CreateNewGuid();
        if (Entry.bDesignerGetter) Node->VariableReference.SetSelfMember(Entry.InputName);
        else Node->VariableReference.SetLocalMember(Entry.InputName,Entry.InputScope,FGuid());
        Node->CreatePin(EGPD_Output,Entry.InputType,Entry.InputName);
        Graph->AddNode(Node,false,false);
        return Node;
    }

    bool VerifyVariableIdentity(const FEntry& Entry, UEdGraphNode* Node, FString& Note)
    {
        const UBlueprintVariableNodeSpawner* Spawner = Cast<UBlueprintVariableNodeSpawner>(Entry.Spawner);
        if (!Spawner || (!Entry.TypeId.StartsWith(TEXT("Variable|")) && !Entry.TypeId.StartsWith(TEXT("FunctionInput|")))) return true;
        const FProperty* Expected = Spawner->GetVarProperty();
        const UK2Node_Variable* Variable = Cast<UK2Node_Variable>(Node);
        const FProperty* Actual = Variable ? Variable->GetPropertyForVariableFromSkeleton() : nullptr;
        if (!Actual && Variable) Actual = Variable->GetPropertyForVariable();
        if (Entry.TypeId.StartsWith(TEXT("FunctionInput|")))
        {
            const UFunction* ExpectedFunction=Expected?Cast<UFunction>(Expected->GetOwnerStruct()):nullptr;
            const UFunction* ActualFunction=Actual?Cast<UFunction>(Actual->GetOwnerStruct()):nullptr;
            if(ExpectedFunction && ActualFunction && Variable && Variable->VariableReference.IsLocalScope()
                && Variable->VariableReference.GetMemberScopeName()==ExpectedFunction->GetName()
                && Expected->GetFName()==Actual->GetFName() && ExpectedFunction->GetFName()==ActualFunction->GetFName()) return true;
            Note=TEXT("function input getter did not resolve to its requested scope/member"); return false;
        }
        const UClass* ExpectedOwner = Expected ? Expected->GetOwnerClass() : nullptr;
        const UClass* ActualOwner = Actual ? Actual->GetOwnerClass() : nullptr;
        // Skeleton and generated classes represent the same declaration when their Blueprint agrees.
        const bool SameOwner = ExpectedOwner && ActualOwner && (ExpectedOwner == ActualOwner
            || (ExpectedOwner->ClassGeneratedBy && ExpectedOwner->ClassGeneratedBy == ActualOwner->ClassGeneratedBy));
        if (Expected && Actual && SameOwner && Expected->GetFName() == Actual->GetFName()
            && Variable->VariableReference.GetMemberName() == Expected->GetFName()) return true;
        Note = FString::Printf(TEXT("created variable identity did not match requested '%s'; created node removed before linking (expected owner=%s, actual owner=%s)"),
            *Entry.TypeId, *GetPathNameSafe(ExpectedOwner), *GetPathNameSafe(ActualOwner));
        return false;
    }

	/**
	 * Every entry the action database offers for this graph, after the engine's own filter.
	 *
	 * ONE SCAN, AND THE FILTER IS BUILT ONCE. RE-SCANNING THE WHOLE DATABASE FOR EVERY MATCH TO RECOVER
	 * THE OWNER CLASS IS QUADRATIC over an action registry that holds thousands of entries, and building
	 * an empty `FBlueprintActionFilter` for the `GetUiSpec` call is not the same context the id was
	 * produced under. Both are avoided here by collecting everything in the single pass that already
	 * has it.
	 */
    void AddBlueprintOwnerAliases(FEntry& Entry, const UBlueprint* Owner, const TCHAR* Access, const FString& Name)
    {
        if (!Owner) return;
        Entry.Aliases.AddUnique(FString::Printf(TEXT("Variables|%s|%s%s"), *Owner->GetName(), Access, *Name));
        // Both class objects describe the same declaration; their lifetime/version differs.
        if (Owner->GeneratedClass)
            Entry.Aliases.AddUnique(FString::Printf(TEXT("Variable|%s|%s|%s"), *Owner->GeneratedClass->GetPathName(), Access, *Name));
        if (Owner->SkeletonGeneratedClass)
            Entry.Aliases.AddUnique(FString::Printf(TEXT("Variable|%s|%s|%s"), *Owner->SkeletonGeneratedClass->GetPathName(), Access, *Name));
    }

	void CollectEntries(UBlueprint* InBlueprint, UEdGraph* InGraph, TArray<FEntry>& OutEntries)
	{
		if (InBlueprint == nullptr || InGraph == nullptr)
		{
			return;
		}

		FBlueprintActionFilter Filter;
		Filter.Context.Blueprints.Add(InBlueprint);
		Filter.Context.Graphs.Add(InGraph);

		FBlueprintActionDatabase& Database = FBlueprintActionDatabase::Get();
		const FBlueprintActionDatabase::FActionRegistry& Registry = Database.GetAllActions();

		for (FBlueprintActionDatabase::FActionRegistry::TConstIterator Iterator =
				Registry.CreateConstIterator(); Iterator; ++Iterator)
		{
			UObject* ActionObject = Iterator->Key.ResolveObjectPtr();
			if (ActionObject == nullptr)
			{
				// An entry whose owner is gone is dropped from the database on the next tick, as
				// the engine does. It is skipped because it is not a node type anyone meant.
				continue;
			}

			for (const UBlueprintNodeSpawner* NodeSpawner : Iterator->Value)
			{
				if (NodeSpawner == nullptr)
				{
					continue;
				}

				// THE FILTER TAKES AN LVALUE. `FBlueprintActionFilter::IsFiltered` is declared
				// `bool IsFiltered(FBlueprintActionInfo&)` rather than taking a const reference
				// (`BlueprintActionFilter.h:369`), so the info is built into a named local.
				FBlueprintActionInfo ActionInfo(ActionObject, NodeSpawner);
				if (Filter.IsFiltered(ActionInfo))
				{
					// The reason the filter exists: a node type that cannot exist in THIS graph
					// must not be offered, and a latent node in a function graph is the case
					// `03_TOOLS.md` names.
					continue;
				}

				const FBlueprintActionUiSpec UiSpec =
					NodeSpawner->GetUiSpec(Filter.Context, ActionInfo.GetBindings());
				const FString TypeId = IdFromUiSpec(UiSpec);
				if (TypeId.IsEmpty())
				{
					continue;
				}

				FEntry Entry;
				Entry.TypeId = TypeId;
                Entry.LegacyTypeId = TypeId;
				Entry.SearchKeywords = UiSpec.Keywords.ToString();
                if (ActionInfo.GetNodeClass() == UK2Node_FunctionResult::StaticClass())
                {
                    // The reader uses this stable identity for every Return node. Keep the
                    // stock menu spelling as an alias for clients that discovered it earlier.
                    Entry.TypeId = TEXT("Function|Result");
                    Entry.Aliases.AddUnique(TypeId);
                    Entry.SearchKeywords += TEXT(" return function result early exit");
                }
                if (ActionInfo.GetNodeClass() == UK2Node_PromotableOperator::StaticClass())
                {
                    // Discovery synonyms never become creation IDs. The same
                    // graph-filtered stock spawner remains responsible for creation.
                    const FString Title = UiSpec.MenuName.ToString();
                    if (Title.Equals(TEXT("Add"), ESearchCase::IgnoreCase)) Entry.SearchKeywords += TEXT(" add plus addition");
                    if (Title.Equals(TEXT("Subtract"), ESearchCase::IgnoreCase)) Entry.SearchKeywords += TEXT(" subtract minus subtraction");
                }
				if (const UFunction* Fn = ActionInfo.GetAssociatedFunction())
				{
					// Include reflected names and parameter types for discovery of compact-title nodes.
					Entry.SearchKeywords += TEXT(" ") + Fn->GetName() + TEXT(" ") + UiSpec.MenuName.ToString();
					Entry.Aliases.Add(TEXT("Function|") + Fn->GetPathName());
					Entry.Aliases.Add(Fn->GetPathName());
					for (TFieldIterator<FProperty> P(Fn); P; ++P)
						if (P->HasAnyPropertyFlags(CPF_Parm)) Entry.SearchKeywords += TEXT(" ") + P->GetCPPType();
					// Exact public names for common compact/wildcard library actions; never fuzzy-spawn.
					const FString Path = Fn->GetPathName();
					if (Path == TEXT("/Script/Engine.KismetArrayLibrary:Array_Length")) Entry.Aliases.Add(TEXT("Array Length"));
					if (Path == TEXT("/Script/Engine.KismetStringLibrary:Concat_StrStr")) Entry.Aliases.Add(TEXT("String Append"));
					if (Path.StartsWith(TEXT("/Script/Engine.KismetStringLibrary:BuildString_")))
						Entry.SearchKeywords += TEXT(" build string");
					if (Path == TEXT("/Script/Engine.KismetStringLibrary:Conv_IntPointToString"))
						Entry.SearchKeywords += TEXT(" integer point int point to string");
					for (const FString& Alias : Entry.Aliases) Entry.SearchKeywords += TEXT(" ") + Alias;
				}
                // Ordinary function calls have a locale-independent reflected identity.
                if (ActionInfo.GetNodeClass() == UK2Node_CallFunction::StaticClass())
                    if (const UFunction* Function = ActionInfo.GetAssociatedFunction())
                    {
                        const UClass* Owner = Function->GetOuterUClass();
                        const UBlueprint* Source = Owner ? Cast<UBlueprint>(Owner->ClassGeneratedBy) : nullptr;
                        Entry.TypeId = TEXT("Function|") + (Source
                            ? Source->GetPathName() + TEXT(".") + Function->GetName() : Function->GetPathName());
                    }
                // Match the reader's exact struct identity while keeping the palette
                // spelling as a legacy alias. The filtered stock spawner still decides
                // whether this graph can create the node; no direct construction bypass.
                if (ActionInfo.GetNodeClass() == UK2Node_MakeStruct::StaticClass()
                    || ActionInfo.GetNodeClass() == UK2Node_BreakStruct::StaticClass())
                    if (const auto* FieldSpawner = Cast<UBlueprintFieldNodeSpawner>(NodeSpawner))
                        if (const auto* Struct = Cast<UScriptStruct>(FieldSpawner->GetField().ToUObject()))
                        {
                            Entry.TypeId = FString(ActionInfo.GetNodeClass() == UK2Node_MakeStruct::StaticClass()
                                ? TEXT("Struct|Make|") : TEXT("Struct|Break|")) + Struct->GetPathName();
                            Entry.SearchKeywords += TEXT(" ") + Struct->GetName() + TEXT(" ") + UiSpec.MenuName.ToString();
                        }
                // Dispatcher menu labels can differ from their reflected names.
                // Search both, and expose the declaring class so callers can
                // distinguish a button event from a similarly named group event.
                if (const auto* DelegateSpawner = Cast<UBlueprintDelegateNodeSpawner>(NodeSpawner))
                    if (const FMulticastDelegateProperty* Property = DelegateSpawner->GetDelegateProperty())
                    {
                        Entry.SearchKeywords += TEXT(" ") + Property->GetName() + TEXT(" ") + UiSpec.MenuName.ToString();
                        if (const UClass* OwnerClass = Property->GetOwnerClass())
                        {
                            Entry.OwningClass = OwnerClass->GetPathName();
                            Entry.SearchKeywords += TEXT(" ") + OwnerClass->GetName();
                        }
                    }
				// Member-variable IDs must match the reader, not friendly menu labels.
				if (const UBlueprintVariableNodeSpawner* VariableSpawner = Cast<UBlueprintVariableNodeSpawner>(NodeSpawner))
				{
					const UClass* VariableNodeClass = ActionInfo.GetNodeClass();
					const FProperty* Property = VariableSpawner->GetVarProperty();
					if (Property && (VariableNodeClass == UK2Node_VariableGet::StaticClass()
						|| VariableNodeClass == UK2Node_VariableSet::StaticClass()))
					{
						FString Category = TEXT("Default");
						for (const FBPVariableDescription& Variable : InBlueprint->NewVariables)
							if (Variable.VarName == Property->GetFName())
							{
								const FString* Source = FTextInspector::GetSourceString(Variable.Category);
								const FString Declared = Source ? *Source : Variable.Category.ToString();
								if (!Declared.IsEmpty()) Category = Declared;
								break;
							}
						const TCHAR* Access = VariableNodeClass == UK2Node_VariableSet::StaticClass() ? TEXT("Set") : TEXT("Get");
						Entry.Aliases.Add(FString::Printf(TEXT("Variables|%s|%s%s"), *Category, Access, *Property->GetName()));
                        Entry.SearchKeywords += FString::Printf(TEXT(" variable %s %s"),
                            VariableNodeClass == UK2Node_VariableSet::StaticClass() ? TEXT("set setter") : TEXT("get getter"), *Property->GetName());
                        const UClass* PropertyOwner=Property->GetOwnerClass();
                        const UBlueprint* DeclaringBlueprint=PropertyOwner?Cast<UBlueprint>(PropertyOwner->ClassGeneratedBy):nullptr;
                        Entry.TypeId=FString::Printf(TEXT("Variable|%s|%s|%s"),
                            *(DeclaringBlueprint?DeclaringBlueprint->GetPathName():PropertyOwner?PropertyOwner->GetPathName():Property->GetOwnerStruct()->GetPathName()),Access,*Property->GetName());
                        Entry.bContextVariable=DeclaringBlueprint==InBlueprint || (PropertyOwner && InBlueprint->ParentClass && InBlueprint->ParentClass->IsChildOf(PropertyOwner));
                        // Skeleton and generated parent classes are different UClass objects.
                        // Normalize Blueprint ancestry before deciding that a member is foreign.
                        if (DeclaringBlueprint)
                            for (const UClass* Ancestor = InBlueprint->ParentClass; Ancestor; Ancestor = Ancestor->GetSuperClass())
                                if (Ancestor->ClassGeneratedBy == DeclaringBlueprint) { Entry.bContextVariable = true; break; }
                        Entry.OwningClass=PropertyOwner?PropertyOwner->GetPathName():Property->GetOwnerStruct()->GetPathName();
                        Entry.bOwnedByThisBlueprint=DeclaringBlueprint==InBlueprint;
                        AddBlueprintOwnerAliases(Entry, DeclaringBlueprint, Access, Property->GetName());
                        if (PropertyOwner)
                            Entry.Aliases.AddUnique(FString::Printf(TEXT("Variable|%s|%s|%s"), *PropertyOwner->GetPathName(), Access, *Property->GetName()));
					}
				}
				if(Entry.OwningClass.IsEmpty()) Entry.OwningClass = OwnerClassName(ActionObject);
				Entry.bOwnedByThisBlueprint |= Cast<UBlueprint>(ActionObject) == InBlueprint;
				Entry.Spawner = NodeSpawner;
				Entry.Owner = ActionObject;

				const UClass* NodeClass = ActionInfo.GetNodeClass();
				if (NodeClass != nullptr)
				{
                    // Guard the cast: action classes are not necessarily K2 nodes.
                    // Function-call defaults are unconfigured; use the associated
                    // function below instead of inferring its purity from the CDO.
					if (const UObject* DefaultObject = NodeClass->GetDefaultObject())
					{
						if (const UK2Node* DefaultNode = Cast<UK2Node>(DefaultObject))
						{
							Entry.bPure = DefaultNode->IsNodePure();
						}
					}
				}
                if (const UFunction* Function = ActionInfo.GetAssociatedFunction())
                    Entry.bPure = Function->HasAnyFunctionFlags(FUNC_BlueprintPure);
				Entry.bLatent = ClassIsLatent(NodeClass)
					|| FunctionIsLatent(ActionInfo.GetAssociatedFunction());

				OutEntries.Add(MoveTemp(Entry));
			}
		}

        // The live designer tree is authoritative before a new variable reaches the skeleton.
        const UWidgetBlueprint* WidgetBP = Cast<UWidgetBlueprint>(InBlueprint);
        // Mirror Unreal's nearest ancestor with a rooted widget tree, without loading
        // or compiling assets. A child without its own tree uses its parent's tree.
        for (const UClass* Ancestor = InBlueprint->ParentClass;
            WidgetBP && (!WidgetBP->WidgetTree || !WidgetBP->WidgetTree->RootWidget) && Ancestor;
            Ancestor = Ancestor->GetSuperClass())
            if (const UWidgetBlueprint* ParentWidgetBP = Cast<UWidgetBlueprint>(Ancestor->ClassGeneratedBy))
                WidgetBP = ParentWidgetBP;
        if (WidgetBP)
            if (WidgetBP->WidgetTree)
            {
                TArray<UWidget*> Widgets;
                WidgetBP->WidgetTree->GetAllWidgets(Widgets);
                for (UWidget* Widget : Widgets)
                {
                    if (!Widget || !Widget->bIsVariable) continue;
                    const FString Id = FString::Printf(TEXT("Variable|%s|Get|%s"), *WidgetBP->GetPathName(), *Widget->GetName());
                    // A changed designer class can precede skeleton regeneration. Keep its aliases,
                    // but use the live tree's object type instead of a stale reflected property.
                    FEntry Entry;
                    for (const FEntry& Existing : OutEntries)
                        if (Existing.TypeId == Id)
                        {
                            Entry.Aliases.AddUnique(Existing.LegacyTypeId);
                            for (const FString& Alias : Existing.Aliases) Entry.Aliases.AddUnique(Alias);
                        }
                    OutEntries.RemoveAll([&](const FEntry& Existing){ return Existing.TypeId == Id; });
                    Entry.TypeId=Id; Entry.LegacyTypeId=Id; Entry.bDesignerGetter=true;
                    Entry.InputName=Widget->GetFName(); Entry.InputType.PinCategory=UEdGraphSchema_K2::PC_Object;
                    Entry.InputType.PinSubCategoryObject=Widget->GetClass();
                    Entry.OwningClass=WidgetBP->GetPathName(); Entry.bOwnedByThisBlueprint=WidgetBP==InBlueprint;
                    Entry.bContextVariable=true; Entry.bPure=true;
                    Entry.Aliases.Add(TEXT("Variables|Default|Get")+Widget->GetName());
                    AddBlueprintOwnerAliases(Entry, WidgetBP, TEXT("Get"), Widget->GetName());
                    Entry.SearchKeywords=TEXT("widget designer getter ")+Widget->GetName();
                    OutEntries.Add(MoveTemp(Entry));
                }
            }

        // Entry pins are authoritative even before the signature reaches the skeleton class.
        if (InBlueprint->FunctionGraphs.Contains(InGraph))
            for(UEdGraphNode* Node:InGraph->Nodes) if(const UK2Node_FunctionEntry* FunctionEntry=Cast<UK2Node_FunctionEntry>(Node))
                for(const UEdGraphPin* Input:FunctionEntry->Pins)
                {
                    if(!Input || Input->Direction!=EGPD_Output || Input->PinType.PinCategory==UEdGraphSchema_K2::PC_Exec || Input->ParentPin || Input->bOrphanedPin) continue;
                    FEntry Entry;
                    Entry.TypeId=FString::Printf(TEXT("FunctionInput|%s|%s|%s"),*InBlueprint->GetPathName(),*InGraph->GetName(),*Input->PinName.ToString());
                    Entry.LegacyTypeId=Entry.TypeId; Entry.InputScope=InGraph->GetName(); Entry.InputName=Input->PinName; Entry.InputType=Input->PinType;
                    Entry.SearchKeywords=TEXT("function input parameter getter ")+Input->PinName.ToString();
                    Entry.OwningClass=InBlueprint->GetPathName(); Entry.bOwnedByThisBlueprint=true; Entry.bContextVariable=true; Entry.bPure=true;
                    OutEntries.Add(MoveTemp(Entry));
                }

	}
}

TArray<FString> FTurboMCPNodeType::AvailableTypeIds(UBlueprint* InBlueprint, UEdGraph* InGraph)
{
	TArray<FEntry> Entries;
	CollectEntries(InBlueprint, InGraph, Entries);

	TArray<FString> Out;
	Out.Reserve(Entries.Num());
	for (const FEntry& Entry : Entries)
	{
		Out.AddUnique(Entry.TypeId);
        Out.AddUnique(Entry.LegacyTypeId);
        for (const FString& Alias : Entry.Aliases) Out.AddUnique(Alias);
	}
	Out.Sort();
	return Out;
}

FString FTurboMCPNodeType::CanonicalIdOf(const UEdGraphNode* InNode)
{
	return TurboMCPNodeId::GetNodeTypeId(InNode);
}

UEdGraphNode* FTurboMCPNodeType::BuildProbeNode(UBlueprint* InBlueprint, UEdGraph* InGraph,
	const FString& InTypeId, FString& OutNote)
{
	OutNote.Empty();

	if (InBlueprint == nullptr)
	{
		OutNote = TEXT("no Blueprint");
		return nullptr;
	}
	if (InGraph == nullptr)
	{
		OutNote = TEXT("no graph");
		return nullptr;
	}

	// Lookup, check and writers all consume the SAME filtered action list. Keeping the lookup in
	// `CollectEntries` prevents a type ID from being offered by one tool and rejected by another.
	TArray<FEntry> Entries;
	CollectEntries(InBlueprint, InGraph, Entries);
    const bool bHasExactId = Entries.ContainsByPredicate([&](const FEntry& Entry) { return Entry.TypeId == InTypeId; });
    // Unqualified legacy member names prefer this Blueprint's declaration/parents.
    // Exact class-qualified IDs always retain their explicitly requested owner.
    const bool bHasContextVariable=!bHasExactId && Entries.ContainsByPredicate([&](const FEntry& Entry) {
        return Entry.bContextVariable && (Entry.LegacyTypeId==InTypeId || Entry.Aliases.Contains(InTypeId)); });
    if(bHasContextVariable || (!bHasExactId && InTypeId.StartsWith(TEXT("Variables|")))) Entries.RemoveAll([](const FEntry& Entry) { return !Entry.bContextVariable; });
    TSet<FString> MatchingTypes;
    for (const FEntry& Entry : Entries)
        if (Entry.TypeId == InTypeId || (!bHasExactId && (Entry.LegacyTypeId == InTypeId || Entry.Aliases.Contains(InTypeId)))) MatchingTypes.Add(Entry.TypeId);
    if (MatchingTypes.Num() > 1)
    {
        OutNote = TEXT("ambiguous display type ID; use a class-qualified Variable|owner|Get/Set|name or Function|path ID from node-type discovery");
        return nullptr;
    }
	for (const FEntry& Entry : Entries)
	{
		if (Entry.TypeId != InTypeId && (bHasExactId || (Entry.LegacyTypeId != InTypeId && !Entry.Aliases.Contains(InTypeId))))
		{
			continue;
		}

		// A Return node asks its graph for the existing function signature when the
		// stock spawner places it. Keep this scratch graph owned by the Blueprint,
		// as GraphAppend's ownership guard requires, and seed only the existing
		// Return signature in that isolated graph. The stock action was already
		// filtered against the real function graph before Invoke.
		const bool bReturn = Entry.Spawner && Entry.Spawner->NodeClass == UK2Node_FunctionResult::StaticClass();
		UEdGraph* ProbeGraph = NewObject<UEdGraph>(InBlueprint, NAME_None, RF_Transient);
		if (ProbeGraph == nullptr)
		{
			OutNote = TEXT("a transient graph for the probe could not be created");
			return nullptr;
		}
		ProbeGraph->Schema = InGraph->Schema;
        if (bReturn)
        {
            for (UEdGraphNode* Original : InGraph->Nodes)
                if (const UK2Node_FunctionResult* Existing = Cast<UK2Node_FunctionResult>(Original))
                {
                    // The new stock Return node copies the first existing result's
                    // user-defined pins in PostPlacedNewNode. Mirror only that signature
                    // on an isolated seed; copying live pins or links is unnecessary.
                    UK2Node_FunctionResult* Seed = NewObject<UK2Node_FunctionResult>(ProbeGraph);
                    Seed->FunctionReference = Existing->FunctionReference;
                    Seed->bIsEditable = Existing->bIsEditable;
                    for (const TSharedPtr<FUserPinInfo>& Pin : Existing->UserDefinedPins)
                        if (Pin.IsValid()) Seed->UserDefinedPins.Add(MakeShared<FUserPinInfo>(*Pin));
                    ProbeGraph->AddNode(Seed, false, false);
                    break;
                }
        }
        if(Entry.bDesignerGetter || !Entry.InputScope.IsEmpty()) return CreateInputGetter(Entry,ProbeGraph);
		FBlueprintActionInfo ActionInfo(Entry.Owner, Entry.Spawner);
		UEdGraphNode* Spawned = Entry.Spawner->Invoke(ProbeGraph, ActionInfo.GetBindings(),
			FVector2D::ZeroVector);
		if (Spawned == nullptr)
		{
			OutNote = FString::Printf(TEXT("the engine's spawner for '%s' returned no node, so it declined to build one in this context"), *InTypeId);
		}
        if (Spawned && !VerifyVariableIdentity(Entry, Spawned, OutNote))
        {
            Spawned->DestroyNode();
            return nullptr;
        }
		return Spawned;
	}

    OutNote = FString::Printf(TEXT("No supported creation ID in this graph matches '%s'. Use PreviewTurboMCPNodeType to discover a valid ID; nothing was created."), *InTypeId);
    // Guidance only: a familiar word never authorizes a fuzzy creation match.
    // Suggest self only when the filtered stock action actually supports it here.
    if (InTypeId.Contains(TEXT("self"), ESearchCase::IgnoreCase))
    {
        TArray<FString> Suggestions;
        for (const FEntry& Entry : Entries)
            if (Entry.Spawner && Entry.Spawner->NodeClass == UK2Node_Self::StaticClass())
                Suggestions.AddUnique(Entry.TypeId);
        Suggestions.Sort();
        if (!Suggestions.IsEmpty())
            OutNote += TEXT(" Supported self-reference creation ID(s): ") + FString::Join(Suggestions, TEXT(", ")) + TEXT(".");
    }
	return nullptr;
}

FTurboMCPNodeType::ESpawnResult FTurboMCPNodeType::SpawnNodeOfType(
	UBlueprint* InBlueprint, UEdGraph* InGraph, const FString& InTypeId,
	UEdGraphNode*& OutNode, FString& OutNote, UEdGraph* DiscoveryGraph)
{
	OutNode = nullptr;
	OutNote.Empty();

	if (InBlueprint == nullptr)
	{
		OutNote = TEXT("no Blueprint");
		return ESpawnResult::SpawnerDeclined;
	}
	if (InGraph == nullptr)
	{
		OutNote = TEXT("no graph");
		return ESpawnResult::SpawnerDeclined;
	}

	// This is the same resolver used by lookup and probe construction. Replace and patch delegate
	// to graph append, so all five operations consume this one filtered action list.
	TArray<FEntry> Entries;
	CollectEntries(InBlueprint, DiscoveryGraph ? DiscoveryGraph : InGraph, Entries);
    const bool bHasExactId = Entries.ContainsByPredicate([&](const FEntry& Entry) { return Entry.TypeId == InTypeId; });
    // Unqualified legacy member names prefer this Blueprint's declaration/parents.
    // Exact class-qualified IDs always retain their explicitly requested owner.
    const bool bHasContextVariable=!bHasExactId && Entries.ContainsByPredicate([&](const FEntry& Entry) {
        return Entry.bContextVariable && (Entry.LegacyTypeId==InTypeId || Entry.Aliases.Contains(InTypeId)); });
    if(bHasContextVariable || (!bHasExactId && InTypeId.StartsWith(TEXT("Variables|")))) Entries.RemoveAll([](const FEntry& Entry) { return !Entry.bContextVariable; });
    TSet<FString> MatchingTypes;
    for (const FEntry& Entry : Entries)
        if (Entry.TypeId == InTypeId || (!bHasExactId && (Entry.LegacyTypeId == InTypeId || Entry.Aliases.Contains(InTypeId)))) MatchingTypes.Add(Entry.TypeId);
    if (MatchingTypes.Num() > 1)
    {
        OutNote = TEXT("ambiguous display type ID; use a class-qualified Variable|owner|Get/Set|name or Function|path ID from node-type discovery");
        return ESpawnResult::NoSuchTypeId;
    }
	for (const FEntry& Entry : Entries)
	{
		if (Entry.TypeId != InTypeId && (bHasExactId || (Entry.LegacyTypeId != InTypeId && !Entry.Aliases.Contains(InTypeId))))
		{
			continue;
		}

        if(Entry.bDesignerGetter || !Entry.InputScope.IsEmpty())
        {
            OutNode=CreateInputGetter(Entry,InGraph);
            if(!OutNode) OutNote=TEXT("function input getter allocation failed");
            return OutNode?ESpawnResult::Spawned:ESpawnResult::SpawnerDeclined;
        }
		FBlueprintActionInfo ActionInfo(Entry.Owner, Entry.Spawner);
		UEdGraphNode* Spawned = Entry.Spawner->Invoke(InGraph, ActionInfo.GetBindings(), FVector2D::ZeroVector);
		if (Spawned == nullptr)
		{
			OutNote = FString::Printf(TEXT("the engine's spawner for '%s' returned no node, so it declined to build one in this graph"), *InTypeId);
			return ESpawnResult::SpawnerDeclined;
		}
		// *** ONLY ALLOCATE PINS THE SPAWNER DID NOT. ***
		//
		// MEASURED, and this line is the only visible difference between the path that WORKS and the path that
		// does NOT: `BuildProbeNode` (L302) returns the spawner's node AS-IS, while this function called
		// `AllocateDefaultPins()` on a node the spawner had just built - whose `Invoke` already creates the pins.
		//
		// The migration this belongs to was measured reporting "changed ... compile: passed; target <id>" for a
		// created target that the graph did not contain afterwards, on a graph whose node CLASS count fell 3 -> 2,
		// and a node-count guard running AFTER the commit saw a CORRECT count - so the created node existed at
		// commit time and vanished later, in the window that contains the tool's own recompile and GC.
		//
		// Re-allocating pins on a registered node resets its pin state, which is exactly the kind of damage a
		// recompile would then resolve by discarding the node. Allocating only when there are none is strictly
		// safer and preserves the original intent.
		if (Spawned->Pins.Num() == 0)
		{
			Spawned->AllocateDefaultPins();
		}
        if (!VerifyVariableIdentity(Entry, Spawned, OutNote))
        {
            Spawned->DestroyNode();
            return ESpawnResult::SpawnerDeclined;
        }
		OutNode = Spawned;
		return ESpawnResult::Spawned;
	}

    OutNote = FString::Printf(TEXT("No supported creation ID in this graph matches '%s'. Use PreviewTurboMCPNodeType to discover a valid ID; nothing was created."), *InTypeId);
    // Guidance only: a familiar word never authorizes a fuzzy creation match.
    // Suggest self only when the filtered stock action actually supports it here.
    if (InTypeId.Contains(TEXT("self"), ESearchCase::IgnoreCase))
    {
        TArray<FString> Suggestions;
        for (const FEntry& Entry : Entries)
            if (Entry.Spawner && Entry.Spawner->NodeClass == UK2Node_Self::StaticClass())
                Suggestions.AddUnique(Entry.TypeId);
        Suggestions.Sort();
        if (!Suggestions.IsEmpty())
            OutNote += TEXT(" Supported self-reference creation ID(s): ") + FString::Join(Suggestions, TEXT(", ")) + TEXT(".");
    }
	return ESpawnResult::NoSuchTypeId;
}

bool FTurboMCPNodeType::Plan(const FString& InAsset, const FString& InGraphName,
	const FString& InGuess, const FString& InLimit, FPlan& OutPlan, FString& OutError)
{
	OutError.Empty();
	OutPlan.Asset = InAsset.TrimStartAndEnd();
	OutPlan.GraphName = InGraphName.TrimStartAndEnd();
	OutPlan.Guess = InGuess.TrimStartAndEnd();

	OutPlan.Limit = 10;
	const FString LimitText = InLimit.TrimStartAndEnd();
	if (!LimitText.IsEmpty())
	{
		int32 Parsed = 0;
		for (const TCHAR Character : LimitText)
		{
			if (Character < TEXT('0') || Character > TEXT('9') || Parsed > 200)
			{
				OutError = TEXT("Limit must contain digits for a whole number from 1 to 200; use 10, or leave it empty for 10.");
				return false;
			}
			Parsed = Parsed * 10 + Character - TEXT('0');
		}
		if (Parsed < 1 || Parsed > 200)
		{
			OutError = TEXT("Limit must be from 1 to 200; use 10, or leave it empty for 10.");
			return false;
		}
		OutPlan.Limit = Parsed;
	}

	FTurboMCPAssetResolver::FResolved Resolved;
	const FString ResolveError = FTurboMCPAssetResolver::Resolve(OutPlan.Asset, Resolved);
	if (!ResolveError.IsEmpty())
	{
		OutError = ResolveError;
		return false;
	}

	// `FResolved` carries PATHS, not objects, so the Blueprint is loaded from its object path.
	// This is the same two-step the other read tools use, deliberately: a second loading route
	// would answer differently for an asset that is in memory but not on disk.
	UBlueprint* Blueprint = Cast<UBlueprint>(LoadObject<UObject>(nullptr, *Resolved.ObjectPath));
	if (Blueprint == nullptr)
	{
		OutError = FString::Printf(
			TEXT("%s is not a Blueprint, so it has no graph nodes."), *Resolved.ObjectPath);
		return false;
	}
	OutPlan.Asset = Resolved.ObjectPath;

	UEdGraph* Graph = nullptr;
	int32 GraphMatches = 0;
	if (OutPlan.GraphName.IsEmpty())
	{
		Graph = FBlueprintEditorUtils::FindEventGraph(Blueprint);
	}
	else
	{
		Graph = TurboMCPNodeId::FindGraphByName(Blueprint, OutPlan.GraphName, GraphMatches);
		if (GraphMatches != 1) Graph = nullptr;
	}

	if (Graph == nullptr)
	{
		// `P7`: name the real alternatives rather than only the failure.
		TArray<FString> Names;
        TArray<UEdGraph*> AvailableGraphs;
        FTurboMCPBlueprintRead::CollectGraphs(Blueprint, AvailableGraphs);
        for (const UEdGraph* Each : AvailableGraphs) Names.Add(FString::Printf(TEXT("%s [%s]"), *Each->GetName(), *Each->GetPathName()));
		Names.Sort();

		if (GraphMatches > 1)
		{
			OutError = FString::Printf(TEXT("'%s' has %d graphs named '%s'; ambiguous graph names are refused. Graph paths are diagnostic, not accepted selectors: %s"), *Blueprint->GetName(), GraphMatches, *OutPlan.GraphName, *FString::Join(Names, TEXT(", ")));
			return false;
		}

		OutError = OutPlan.GraphName.IsEmpty()
			? FString::Printf(
				TEXT("'%s' has no event graph. Its graphs are: %s"),
				*Blueprint->GetName(),
				Names.Num() > 0 ? *FString::Join(Names, TEXT(", ")) : TEXT("(none)"))
			: FString::Printf(
				TEXT("'%s' has no graph named '%s'. Its graphs are: %s"),
				*Blueprint->GetName(), *OutPlan.GraphName,
				Names.Num() > 0 ? *FString::Join(Names, TEXT(", ")) : TEXT("(none)"));
		return false;
	}
	OutPlan.GraphName = Graph->GetName();

	TArray<FEntry> Entries;
	CollectEntries(Blueprint, Graph, Entries);

	for (const FEntry& Entry : Entries)
	{
		OutPlan.AllTypeIds.AddUnique(Entry.TypeId);
		for (const FString& Alias : Entry.Aliases) OutPlan.AllTypeIds.AddUnique(Alias);
	}
	OutPlan.AllTypeIds.Sort();

	// A guess containing a `|` is treated as an id, because that is the shape of one. It is
	// still matched the same way, so a near miss still finds something.
	OutPlan.bLooksLikeTypeId = OutPlan.Guess.Contains(TEXT("|"));

	// An explicit limit permits bounded browsing without search words.
	if (OutPlan.Guess.IsEmpty() && InLimit.TrimStartAndEnd().IsEmpty())
	{
		// A finder with no words is a request for everything, which is hundreds of ids. It is
		// refused with the count and a way to narrow it, rather than printing the lot (`P1`).
		OutError = FString::Printf(
			TEXT("no words to match. This graph accepts %d node type id(s), which is too many ")
			TEXT("to list. Give a word from the node's menu name, for example 'print', 'branch' ")
			TEXT("or 'variable'."),
			OutPlan.AllTypeIds.Num());
		return false;
	}

	// BOTH SIDES ARE NORMALISED THE SAME WAY, through the one matcher `03_TOOLS.md` says every
	// tool shares, so a "did you mean" in another tool ranks identically.
	const FString NormalisedGuess = FTurboMCPToolSchema::NormaliseForMatch(OutPlan.Guess);

	// Words for the "every word present" rule, prepared once.
	TArray<FString> GuessWords;
	OutPlan.Guess.ParseIntoArray(GuessWords, TEXT(" "), /*InCullEmpty=*/true);
	TArray<FString> NormalisedWords;
	for (const FString& Word : GuessWords)
	{
		const FString NormalisedWord = FTurboMCPToolSchema::NormaliseForMatch(Word);
		if (!NormalisedWord.IsEmpty())
		{
			NormalisedWords.Add(NormalisedWord);
		}
	}

    // A display title is not a type identity: different owners and spawners
    // can share one title. Do not infer reader IDs from unrelated live nodes.
    // Returned TypeId values come from the same filtered entries used by append.
	struct FRanked
	{
		FMatch Match;
		int32 Rank = 0;
	};
	TArray<FRanked> Ranked;

	for (const FEntry& Entry : Entries)
	{
		const FString NormalisedId = FTurboMCPToolSchema::NormaliseForMatch(Entry.TypeId);
        const FString LegacyId = FTurboMCPToolSchema::NormaliseForMatch(Entry.LegacyTypeId);

		// RANK 0 is an exact normalised match, 1 a prefix, 2 a contiguous run, 3 every word
		// present in any order. A lower rank always sorts first, so 'print' cannot be beaten by
		// a longer accidental subsequence match.
		int32 Rank = -1;
		FString Reason;

		if (NormalisedId == NormalisedGuess || LegacyId == NormalisedGuess)
		{
			Rank = 0;
			Reason = TEXT("exact match");
		}
		else if (NormalisedId.StartsWith(NormalisedGuess) || LegacyId.StartsWith(NormalisedGuess))
		{
			Rank = 1;
			Reason = TEXT("starts with the words");
		}
		else if (NormalisedId.Contains(NormalisedGuess) || LegacyId.Contains(NormalisedGuess))
		{
			Rank = 2;
			Reason = TEXT("contains the words");
		}
		else if (NormalisedWords.Num() > 0)
		{
			// EVERY WORD PRESENT, in any order. `03_TOOLS.md` says the guess may be PLAIN WORDS,
			// so 'math sin degrees' has to find `Math|Float|sin(degrees)`.
			bool bAllPresent = true;
			for (const FString& Word : NormalisedWords)
			{
				if (!NormalisedId.Contains(Word) && !LegacyId.Contains(Word))
				{
					bAllPresent = false;
					break;
				}
			}
			if (bAllPresent)
			{
				Rank = 3;
				Reason = FString::Printf(TEXT("every one of the %d words is in the id"),
					NormalisedWords.Num());
			}
		}

		// Engine action-menu keywords include operator aliases that are absent from IDs.
		// Keep these below identity/title matches and never use them for writer resolution.
		if (Rank < 0 && NormalisedWords.Num() > 0)
		{
			const FString SearchText = NormalisedId + TEXT(" ") + LegacyId + TEXT(" ")
				+ FTurboMCPToolSchema::NormaliseForMatch(Entry.SearchKeywords.Replace(TEXT("|"), TEXT(" ")));
			bool bAllPresent = true;
			for (const FString& Word : NormalisedWords)
			{
				if (!SearchText.Contains(Word)) { bAllPresent = false; break; }
			}
			if (bAllPresent)
			{
				Rank = 4;
				Reason = TEXT("words matched using Unreal action-menu keywords");
			}
		}

		if (Rank < 0)
		{
			continue;
		}

		FRanked Scored;
		Scored.Rank = Rank;
		Scored.Match.TypeId = Entry.TypeId;
		Scored.Match.Category = CategoryOf(Entry.TypeId);
		Scored.Match.Title = TitleOf(Entry.TypeId);
		Scored.Match.OwningClass = Entry.OwningClass;
		Scored.Match.bOwnedByThisBlueprint = Entry.bOwnedByThisBlueprint;
		Scored.Match.bPure = Entry.bPure;
		Scored.Match.Reason = Reason;

		Ranked.Add(MoveTemp(Scored));
	}

	OutPlan.TotalMatches = Ranked.Num();

	// Rank, then the shorter id, then the name, so the same query twice gives the same list.
	Ranked.Sort([](const FRanked& A, const FRanked& B)
	{
		if (A.Rank != B.Rank) { return A.Rank < B.Rank; }
		if (A.Match.TypeId.Len() != B.Match.TypeId.Len())
		{
			return A.Match.TypeId.Len() < B.Match.TypeId.Len();
		}
		return A.Match.TypeId < B.Match.TypeId;
	});

	for (const FRanked& Scored : Ranked)
	{
		if (OutPlan.Matches.Num() >= OutPlan.Limit)
		{
			break;
		}
		OutPlan.Matches.Add(Scored.Match);
	}

	if (OutPlan.Matches.Num() == 0)
	{
		// `P7`: a refusal lists the closest real names rather than only saying no.
		TArray<FString> Titles;
		for (const FString& TypeId : OutPlan.AllTypeIds)
		{
			Titles.Add(TitleOf(TypeId));
		}
		const TArray<FString> Closest = FTurboMCPToolSchema::ClosestNames(
			OutPlan.Guess, Titles, 10);

		OutError = FString::Printf(
			TEXT("no node type in %s matches '%s'. Closest real names: %s. PreviewTurboMCPNodeType needs a ")
			TEXT("word from the node's own menu name, not from its C++ class."),
			*Graph->GetName(), *OutPlan.Guess,
			Closest.Num() > 0 ? *FString::Join(Closest, TEXT(", ")) : TEXT("(none)"));
		return false;
	}

	// A function or variable added in this session is in the list because the action database
	// rebuilds, but the engine's own creation tool can still refuse the id until the asset is
	// compiled. `03_TOOLS.md` requires that to be SAID rather than discovered.
	for (const FMatch& Match : OutPlan.Matches)
	{
		if (Match.bOwnedByThisBlueprint)
		{
			OutPlan.bCompileRecommended = true;
			break;
		}
	}

	return true;
}

FString FTurboMCPNodeType::Render(const FPlan& InPlan)
{
	FString Out = FString::Printf(TEXT("read: node types in %s.%s matching '%s'"),
		*InPlan.Asset, *InPlan.GraphName, *InPlan.Guess);

	Out += FString::Printf(TEXT("\n%d of %d match(es), showing %d"),
		InPlan.TotalMatches, InPlan.AllTypeIds.Num(), InPlan.Matches.Num());

	for (const FMatch& Match : InPlan.Matches)
	{
		Out += FString::Printf(TEXT("\n\n%s"), *Match.TypeId);
		Out += FString::Printf(TEXT("\n  %s"), Match.bPure ? TEXT("pure") : TEXT("not pure"));
		if (!Match.OwningClass.IsEmpty())
		{
			Out += FString::Printf(TEXT("   declared by %s"), *Match.OwningClass);
		}
		if (Match.bOwnedByThisBlueprint)
		{
			Out += TEXT("   defined in this Blueprint");
		}
		Out += FString::Printf(TEXT("\n  why: %s"), *Match.Reason);


	}

	if (InPlan.bCompileRecommended)
	{
		Out += TEXT("\n; this asset must be COMPILED before the engine's creation tool will ")
			TEXT("accept an id this session added, even though the id is correct.");
	}

	Out += TEXT("\nUse the listed IDs with PreviewTurboMCPNodePins or PreviewTurboMCPGraphAppend in this graph. Reader type IDs and native engine search IDs can differ; do not invent a conversion from a node title.");
	Out += TEXT("\nPromotable Utilities|Operators nodes acquire their operand type from the first typed link; connect that link before setting typed defaults.");
	return Out;
}

// =============================================================================================
// PreviewTurboMCPNodePins
// =============================================================================================

bool FTurboMCPNodePins::Plan(const FString& InAsset, const FString& InGraphName,
	const FString& InTypeId, FPlan& OutPlan, FString& OutError)
{
	OutError.Empty();
	OutPlan.Asset = InAsset.TrimStartAndEnd();
	OutPlan.GraphName = InGraphName.TrimStartAndEnd();
	OutPlan.TypeId = InTypeId.TrimStartAndEnd();

	if (OutPlan.TypeId.IsEmpty())
	{
		OutError = TEXT("no node type id given. PreviewTurboMCPNodeType finds the real one for a graph, ")
			TEXT("and its id is what this tool takes.");
		return false;
	}

	// The id is matched the way the engine matches it, EXACTLY, because this tool has to build
	// the node. A near miss here would build a different node than the caller asked for, which
	// is worse than a refusal, so a miss is reported with the closest real ids instead.
	FTurboMCPNodeType::FPlan TypePlan;
	FString TypeError;
	if (!FTurboMCPNodeType::Plan(OutPlan.Asset, OutPlan.GraphName, OutPlan.TypeId, TEXT("10"),
			TypePlan, TypeError))
	{
		OutError = TypeError;
		return false;
	}
	OutPlan.Asset = TypePlan.Asset;
	OutPlan.GraphName = TypePlan.GraphName;

	// Search ranking and display limits must not determine whether an exact ID exists.
	const bool bExact = TypePlan.AllTypeIds.ContainsByPredicate([&](const FString& TypeId)
	{
		return TypeId.Equals(OutPlan.TypeId, ESearchCase::CaseSensitive);
	});
	if (!bExact)
	{
		TArray<FString> NearIds;
		for (const FTurboMCPNodeType::FMatch& Match : TypePlan.Matches)
		{
			NearIds.Add(Match.TypeId);
		}
		OutError = FString::Printf(
			TEXT("'%s' is not a node type id of %s exactly, so this tool refuses rather than ")
			TEXT("building a different node than you asked for. Closest real ids: %s"),
			*OutPlan.TypeId, *OutPlan.GraphName,
			NearIds.Num() > 0 ? *FString::Join(NearIds, TEXT(", ")) : TEXT("(none)"));
		return false;
	}

	// The Blueprint, again, so the probe node can be built.
	FTurboMCPAssetResolver::FResolved Resolved;
	const FString ResolveError = FTurboMCPAssetResolver::Resolve(OutPlan.Asset, Resolved);
	if (!ResolveError.IsEmpty())
	{
		OutError = ResolveError;
		return false;
	}
	// Loaded the same way the type finder loaded it, so the two tools cannot disagree about
	// which asset a path names.
	UBlueprint* Blueprint = Cast<UBlueprint>(LoadObject<UObject>(nullptr, *Resolved.ObjectPath));
	if (Blueprint == nullptr)
	{
		OutError = FString::Printf(TEXT("%s is not a Blueprint."), *Resolved.ObjectPath);
		return false;
	}
	OutPlan.Asset = Resolved.ObjectPath;

	UEdGraph* Graph = TurboMCPNodeId::FindGraphByName(Blueprint, OutPlan.GraphName);

	// *** THE GRAPH IS CHECKED BEFORE ANY PROBE IS BUILT. `BuildProbeNode` ASKS THE ENGINE TO OWN THE NEW NODE, AND
	// WHEN `FindGraphByName` FOUND NOTHING THAT REQUEST REACHED THE ENGINE WITH A NULL OWNER AND THE ASSERTION TOOK
	// THE EDITOR DOWN. A REFUSAL THAT NAMES THE GRAPH COSTS ONE TOOL CALL; THE ASSERTION COST THE SESSION. ***
	// And a batch caller keeps going: this returns false for THIS type, and the next type is planned the same way.
	if (Graph == nullptr)
	{
		OutError = FString::Printf(
			TEXT("'%s' has no graph named '%s', so there is no owner for a probe node and this type is ")
			TEXT("refused. A graph name is matched exactly; the graph list of this Blueprint names the ones ")
			TEXT("it has."),
			*OutPlan.Asset, *OutPlan.GraphName);
		return false;
	}

	// THE NODE GOES INTO A THROWAWAY GRAPH, WHICH IS THE WHOLE POINT OF `BuildProbeNode`.
	// `03_TOOLS.md` states the requirement as "graph node count must not change (P3)", and the
	// engine's own reader adds a probe to the REAL graph and leaves it there - which
	// `07_ENGINE_NOTES.md` section 3.5 records as a trap. That trap is not repeated here.
	FString Note;
	UEdGraphNode* Probe = FTurboMCPNodeType::BuildProbeNode(Blueprint, Graph, OutPlan.TypeId,
		Note);
	if (Probe == nullptr)
	{
		OutError = Note.IsEmpty() ? TEXT("the node could not be built") : Note;
		return false;
	}

	// PURITY AND LATENCY come from the node rather than from the id's shape.
	if (const UK2Node* K2 = Cast<UK2Node>(Probe))
	{
		OutPlan.bPure = K2->IsNodePure();
	}
	OutPlan.bLatent = ClassIsLatent(Probe->GetClass());
	if (const UK2Node_CallFunction* Call = Cast<UK2Node_CallFunction>(Probe))
	{
		// `GetTargetFunction` is declared const on `UK2Node_CallFunction`
		// (`K2Node_CallFunction.h:150`), so a const pointer is enough to read it.
		OutPlan.bLatent = OutPlan.bLatent || FunctionIsLatent(Call->GetTargetFunction());
	}

	// LATENT NODES ARE NOT ALLOWED IN A FUNCTION GRAPH, which `03_TOOLS.md` states. It is
	// reported rather than refused, because the pins are still the pins and a caller may be
	// asking in order to find out. The engine's filter already refused the spawn if the graph
	// could not hold it, so reaching here means it CAN.
	if (OutPlan.bLatent && Graph != nullptr && Graph->GetClass() != nullptr)
	{
		const bool bUbergraph = Blueprint->UbergraphPages.Contains(Graph);
		if (!bUbergraph)
		{
			OutPlan.bAllowedHere = false;
			OutPlan.DisallowedReason = FString::Printf(
				TEXT("'%s' is a latent node and this is a FUNCTION graph, where the engine ")
				TEXT("does not allow latent nodes. Create it in the event graph instead."),
				*OutPlan.TypeId);
		}
	}

	// The pins. `UEdGraphNode::Pins` is a public array (`EdGraphNode.h:293`), and it may be
	// EMPTY on a node built outside the editor, because `AllocateDefaultPins` is normally run
	// when the node is reconstructed for display. Measured on this project: a commandlet never
	// reconstructs a node, so a fresh node has no pins until it is asked for them. The gate is
	// therefore explicit rather than assumed.
	if (Probe->Pins.Num() == 0)
	{
		Probe->AllocateDefaultPins();
	}

	if (Probe->Pins.Num() == 0)
	{
		// `P1`: an empty pin list says WHY, because an empty list and a node that failed to
		// initialise look identical otherwise.
		OutError = FString::Printf(
			TEXT("the node '%s' built, but it reported NO pins even after AllocateDefaultPins. ")
			TEXT("That means the type built a node that cannot be wired in this context, which ")
			TEXT("is worth reporting rather than showing as an empty list."),
			*OutPlan.TypeId);
		return false;
	}

	int32 InputPosition = 0;
	int32 OutputPosition = 0;
	for (const UEdGraphPin* Pin : Probe->Pins)
	{
		if (Pin == nullptr)
		{
			continue;
		}

		FPin Out;
		Out.Name = Pin->PinName.ToString();
		Out.Direction = DirectionSpelling(Pin->Direction);
		Out.bExec = Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec;

        // Preserve exact subtype identity as well as both sides of a map. A map's
        // key category alone is not its type and must never look like a scalar.
        auto DescribeTerminal = [](FName Category, FName SubCategory, const UObject* Object)
        {
            FString Type = Category.ToString();
            if (SubCategory != NAME_None) Type += TEXT("<") + SubCategory.ToString() + TEXT(">");
            if (Object) Type += TEXT("<") + Object->GetPathName() + TEXT(">");
            return Type;
        };
        Out.Type = DescribeTerminal(Pin->PinType.PinCategory, Pin->PinType.PinSubCategory, Pin->PinType.PinSubCategoryObject.Get());
        switch (Pin->PinType.ContainerType)
        {
        case EPinContainerType::Array: Out.Type = TEXT("Array<") + Out.Type + TEXT(">"); break;
        case EPinContainerType::Set: Out.Type = TEXT("Set<") + Out.Type + TEXT(">"); break;
        case EPinContainerType::Map:
        {
            const FEdGraphTerminalType& Value = Pin->PinType.PinValueType;
            Out.Type = TEXT("Map<") + Out.Type + TEXT(", ")
                + DescribeTerminal(Value.TerminalCategory, Value.TerminalSubCategory, Value.TerminalSubCategoryObject.Get()) + TEXT(">");
            break;
        }
        default: break;
        }
        if (Pin->PinType.bIsReference) Out.Type += TEXT(" reference");
        if (Pin->PinType.bIsConst) Out.Type += TEXT(" const");

		// The default, only when the pin has one. An empty default is omitted rather than
		// printed as nothing, because "the pin has no default" and "the default is the empty
		// string" are different facts for a string pin.
		if (!Pin->DefaultValue.IsEmpty())
		{
			Out.Default = Pin->DefaultValue;
		}
		else if (Pin->DefaultObject != nullptr)
		{
			Out.Default = Pin->DefaultObject->GetPathName();
		}

		// THE POSITION IS COUNTED SEPARATELY FOR INPUTS AND OUTPUTS, which is what the engine
		// does and what `07_ENGINE_NOTES.md` section 3.5 records: exec in is input 0 and
		// `self` is input 1.
		if (Pin->Direction == EGPD_Input)
		{
			Out.Position = InputPosition++;
			++OutPlan.InputCount;
		}
		else
		{
			Out.Position = OutputPosition++;
			++OutPlan.OutputCount;
		}

		OutPlan.Pins.Add(MoveTemp(Out));
	}

	// Inputs first, then outputs, each group in position order, so a caller reads the signature
	// the way it will be called.
	OutPlan.Pins.Sort([](const FPin& A, const FPin& B)
	{
		if (A.Direction != B.Direction)
		{
			return A.Direction == TEXT("EGPD_Input");
		}
		return A.Position < B.Position;
	});

	// THE REAL GRAPH IS CHECKED, not assumed. The probe went into a throwaway graph, and this
	// is the read-back that proves the real one is untouched (`P3`). It is reported through the
	// count rather than asserted in a comment.
	return true;
}

FString FTurboMCPNodePins::Render(const FPlan& InPlan)
{
	FString Out = FString::Printf(TEXT("look: pins of %s in %s.%s"),
		*InPlan.TypeId, *InPlan.Asset, *InPlan.GraphName);

	Out += FString::Printf(TEXT("\n%d pin(s): %d input(s), %d output(s)"),
		InPlan.Pins.Num(), InPlan.InputCount, InPlan.OutputCount);
	Out += FString::Printf(TEXT("\n%s%s"),
		InPlan.bPure ? TEXT("pure") : TEXT("not pure"),
		InPlan.bLatent ? TEXT("   latent") : TEXT(""));

	if (!InPlan.bAllowedHere)
	{
		Out += FString::Printf(TEXT("\n; %s"), *InPlan.DisallowedReason);
	}

	// The positions are per direction, which is stated once rather than per row, because a
	// reader who does not know it will mis-read the whole table.
	Out += TEXT("\n; positions are counted separately for inputs and outputs, as the engine ")
		TEXT("does.");
	Out += TEXT("\n; a temporary graph was used to inspect the node; no compile or save was requested. ")
		TEXT("Probe construction can execute editor code; unchanged node counts do not prove that all asset state is unchanged.");

	for (const FPin& Pin : InPlan.Pins)
	{
        const FString DisplayName = Pin.Name.TrimStartAndEnd().IsEmpty()
            ? TurboMCPGraphTextLex::QuoteLiteral(Pin.Name) : Pin.Name;
        Out += FString::Printf(TEXT("\n  %-8s %2d  %-22s %s"),
            *Pin.Direction, Pin.Position, *DisplayName, *Pin.Type);
        if (Pin.Name.TrimStartAndEnd().IsEmpty())
            Out += TEXT("  [whitespace name: copy the quotes exactly]");
		if (Pin.bExec)
		{
			Out += TEXT("  exec");
		}
		if (!Pin.Default.IsEmpty())
		{
			Out += FString::Printf(TEXT("  default=%s"), *Pin.Default);
		}
	}

	if (InPlan.Pins.Num() == 0)
	{
		// Unreachable while Plan refuses an empty list, and kept because a Render that could
		// print nothing would be worse than one that says so (`P1`).
		Out += TEXT("\n  (no pins were reported, which is a failure rather than an answer)");
	}

	return Out;
}
