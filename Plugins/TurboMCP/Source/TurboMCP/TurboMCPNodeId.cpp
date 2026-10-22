// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "TurboMCPNodeId.h"

#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "Engine/Blueprint.h"
#include "K2Node.h"
#include "K2Node_CallParentFunction.h"
#include "K2Node_CallFunction.h"
#include "K2Node_PromotableOperator.h"
#include "BlueprintTypePromotion.h"
#include "UObject/Class.h"
#include "UObject/UnrealType.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_FunctionResult.h"
#include "K2Node_Variable.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Misc/Char.h"
#include "Misc/Guid.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"

namespace
{
	int32 FindLastUnquotedColon(const FString& InText)
	{
		bool bInsideQuotes = false;
		for (int32 Index = InText.Len() - 1; Index >= 0; --Index)
		{
			if (InText[Index] == TEXT('"'))
			{
				bInsideQuotes = !bInsideQuotes;
			}
			else if (InText[Index] == TEXT(':') && !bInsideQuotes)
			{
				return Index;
			}
		}
		return INDEX_NONE;
	}

	int32 FindFirstUnquotedColon(const FString& InText)
	{
		bool bInsideQuotes = false;
		for (int32 Index = 0; Index < InText.Len(); ++Index)
		{
			if (InText[Index] == TEXT('"'))
			{
				bInsideQuotes = !bInsideQuotes;
			}
			else if (InText[Index] == TEXT(':') && !bInsideQuotes)
			{
				return Index;
			}
		}
		return INDEX_NONE;
	}
}

namespace TurboMCPNodeId
{
	FString MakeKey(const FGuid& InGuid, int32 InDigits)
	{
		// DigitsLower rather than Digits: `3.1` requires lower case, and the
		// engine's other formats are upper case.
		const FString Full = InGuid.ToString(EGuidFormats::DigitsLower);

		if (InDigits <= 0)
		{
			InDigits = StandardKeyDigits;
		}
		if (InDigits >= Full.Len())
		{
			return Full;
		}
		return Full.Left(InDigits);
	}

	FString Make(const FString& InAssetPath, const FString& InGraphName,
		const FGuid& InNodeGuid, bool bLongForm, int32 InDigits)
	{
		if (InAssetPath.IsEmpty() || InGraphName.IsEmpty())
		{
			// A partial ID would resolve to the wrong node or to none, so an
			// unusable input produces an empty ID the caller must refuse on.
			return FString();
		}

		// `3.1`: the AssetName is the asset's own name in full, and the long form
		// is the full object path. A caller passing the object path is reduced
		// here rather than being trusted, so both spellings give one ID.
		FString AssetPart = InAssetPath;
		if (!bLongForm)
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

		// `3.1` says to quote the graph name if it has spaces. A colon needs the
		// same treatment so it cannot be mistaken for an ID separator. The quotes
		// are printed so the ID can be read back unambiguously.
		const FString GraphPart = InGraphName.Contains(TEXT(" "))
			|| InGraphName.Contains(TEXT(":"))
			? FString::Printf(TEXT("\"%s\""), *InGraphName)
			: InGraphName;

		return FString::Printf(TEXT("%s:%s:%s"), *AssetPart, *GraphPart,
			*MakeKey(InNodeGuid, InDigits));
	}

	FString MakeForNode(const UEdGraphNode* InNode, bool bLongForm)
	{
		if (InNode == nullptr)
		{
			return FString();
		}

		const UEdGraph* Graph = InNode->GetGraph();
		if (Graph == nullptr)
		{
			// A node with no graph cannot be addressed, because the ID carries the
			// graph name. Returning an empty ID makes the caller refuse rather
			// than print something that will not resolve later.
			return FString();
		}

		// The asset is the object the graph lives in: the Blueprint, or the
		// material, or whatever else holds a graph.
		const UObject* Asset = Graph->GetOuter();
		if (Asset == nullptr)
		{
			return FString();
		}

		FString AssetPath = Asset->GetPathName();
		if (bLongForm)
		{
			if (const UPackage* Package = Asset->GetOutermost())
			{
				AssetPath = FString::Printf(TEXT("%s.%s"), *Package->GetName(),
					*Asset->GetName());
			}
		}

		const int32 Digits = UniqueKeyDigits(Graph, InNode->NodeGuid);
		return Make(AssetPath, Graph->GetName(), InNode->NodeGuid, bLongForm, Digits);
	}

	FParsed Parse(const FString& InId)
	{
		FParsed Result;
		if (InId.IsEmpty())
		{
			return Result;
		}

		// Split from the right, so a quoted colon in a graph name cannot steal
		// the node key. Separators inside quotes are part of the graph name.
		FString Remainder = InId;

		const int32 LastColon = FindLastUnquotedColon(Remainder);
		if (LastColon == INDEX_NONE)
		{
			// One part only. It can only be an asset.
			Result.AssetName = Remainder;
			Result.bIsLongForm = Remainder.StartsWith(TEXT("/"));
			return Result;
		}

		Result.NodeKey = Remainder.Mid(LastColon + 1).TrimStartAndEnd();
		Remainder = Remainder.Left(LastColon);

		// The asset name never contains a colon, so the first unquoted colon in
		// what is left separates the asset from the graph.
		const int32 AssetEnd = FindFirstUnquotedColon(Remainder);
		if (AssetEnd != INDEX_NONE)
		{
			Result.GraphName = Remainder.Mid(AssetEnd + 1);
			Remainder = Remainder.Left(AssetEnd);
		}
		else
		{
			// Two parts. The spec's example has three, and a node needs all three
			// to resolve, so this is recorded as a graph with no node key rather
			// than being guessed at.
			Result.GraphName = Remainder;
			Remainder.Empty();
		}

		Result.AssetName = Remainder.TrimStartAndEnd();
		Result.GraphName.TrimStartAndEndInline();
		Result.bIsLongForm = Result.AssetName.StartsWith(TEXT("/"));

		// `3.1` allows the graph name to be quoted when it holds spaces, and this
		// format also quotes colons. Strip the quotes so the name matches the graph.
		if (Result.GraphName.Len() >= 2
			&& Result.GraphName.StartsWith(TEXT("\""))
			&& Result.GraphName.EndsWith(TEXT("\"")))
		{
			Result.GraphName = Result.GraphName.Mid(1, Result.GraphName.Len() - 2);
		}

		return Result;
	}

	FString NormaliseKey(const FString& InKey)
	{
		FString Trimmed = InKey.TrimStartAndEnd().ToLower();
		if (Trimmed.IsEmpty())
		{
			return FString();
		}

		// Only hex survives. Anything else means the caller wrote something that
		// is not a key, and comparing it would silently match nothing.
		for (const TCHAR Ch : Trimmed)
		{
			const bool bIsHex =
				(Ch >= TEXT('0') && Ch <= TEXT('9'))
				|| (Ch >= TEXT('a') && Ch <= TEXT('f'));
			if (!bIsHex)
			{
				return FString();
			}
		}

		// The writer emits at least the standard width. Refuse shorter input rather
		// than inventing a different prefix by padding it with zeros.
		if (Trimmed.Len() < StandardKeyDigits)
		{
			return FString();
		}
		return Trimmed;
	}

	int32 FindNodesByKey(const UEdGraph* InGraph, const FString& InKey,
		TArray<UEdGraphNode*>& OutMatches)
	{
		OutMatches.Reset();
		if (InGraph == nullptr)
		{
			return 0;
		}

		const FString Wanted = NormaliseKey(InKey);
		if (Wanted.IsEmpty())
		{
			return 0;
		}

		for (UEdGraphNode* Node : InGraph->Nodes)
		{
			if (Node == nullptr)
			{
				continue;
			}

			const FString NodeKey = Node->NodeGuid.ToString(EGuidFormats::DigitsLower);
			if (NodeKey.StartsWith(Wanted))
			{
				OutMatches.Add(Node);
			}
		}

		// The count is returned rather than a bool so the caller can tell "gone"
		// from "ambiguous". `3.1` says to use more digits when two nodes share the
		// first six, and that advice is only reachable if the count is known.
		return OutMatches.Num();
	}

	int32 UniqueKeyDigits(const UEdGraph* InGraph, const FGuid& InGuid)
	{
		if (InGraph == nullptr)
		{
			return StandardKeyDigits;
		}

		const FString Mine = InGuid.ToString(EGuidFormats::DigitsLower);
		int32 Digits = StandardKeyDigits;

		// Grow one digit at a time while some other node shares the prefix. The
		// loop is bounded by the GUID's own length, so it always ends.
		while (Digits < Mine.Len())
		{
			const FString Prefix = Mine.Left(Digits);
			bool bClash = false;

			for (const UEdGraphNode* Node : InGraph->Nodes)
			{
				if (Node == nullptr || Node->NodeGuid == InGuid)
				{
					continue;
				}
				if (Node->NodeGuid.ToString(EGuidFormats::DigitsLower).StartsWith(Prefix))
				{
					bClash = true;
					break;
				}
			}

			if (!bClash)
			{
				return Digits;
			}
			++Digits;
		}

		return Mine.Len();
	}

	UEdGraph* FindGraphByName(const UBlueprint* InBlueprint, const FString& InGraphName, int32& OutMatchCount)
	{
		OutMatchCount = 0;

		if (InBlueprint == nullptr || InGraphName.IsEmpty())
		{
			return nullptr;
		}

		// Every graph kind is searched, because an ID names a graph and not a kind
		// of graph. A caller asking for `EventGraph` should find it whichever
		// array the Blueprint happens to keep it in.
		TArray<UEdGraph*> AllGraphs;
		InBlueprint->GetAllGraphs(AllGraphs);
		for (UEdGraph* EventGraph : InBlueprint->EventGraphs)
		{
			if (EventGraph)
			{
				AllGraphs.Add(EventGraph);
				EventGraph->GetAllChildrenGraphs(AllGraphs);
			}
		}
		TArray<UEdGraph*> Candidates;
		for (UEdGraph* Graph : AllGraphs)
		{
			if (Graph) Candidates.AddUnique(Graph);
		}

		UEdGraph* First = nullptr;
		for (UEdGraph* Graph : Candidates)
		{
			if (Graph->GetName().Equals(InGraphName, ESearchCase::IgnoreCase))
			{
				++OutMatchCount;
				if (First == nullptr)
				{
					First = Graph;
				}
			}
		}

		// The FIRST match is still the answer, so no existing caller changes behaviour. The COUNT is the new
		// information: `OutMatchCount > 1` means the name is ambiguous and a mutating caller must refuse.
		return First;
	}

	// Callers without an ambiguity count must never receive an arbitrary graph.
	UEdGraph* FindGraphByName(const UBlueprint* InBlueprint, const FString& InGraphName)
	{
		int32 MatchCount = 0;
		UEdGraph* Graph = FindGraphByName(InBlueprint, InGraphName, MatchCount);
		return MatchCount == 1 ? Graph : nullptr;
	}

	FString Unlocalised(const FText& InText)
	{
		// FTextInspector::GetSourceString is the static that returns the text
		// before translation, which is what makes a type ID the same on every
		// machine's locale. `FText` itself has no such member in this engine
		// version: the method of that name at Text.h line 365 belongs to
		// FTextFormat, which is an easy and silent mistake to make.
		if (const FString* Source = FTextInspector::GetSourceString(InText))
		{
			if (!Source->IsEmpty())
			{
				return *Source;
			}
		}
		return InText.ToString();
	}

	FString GetVariableNodeTypeId(const UK2Node_Variable* InNode)
	{
		if (InNode == nullptr)
		{
			return FString();
		}

		const FName VarName = InNode->GetVarName();
		if (VarName.IsNone())
		{
			return FString();
		}

        // Preserve the declaring class in reader-emitted identities. Category/name
        // alone is not unique across Blueprint or engine classes.
        const FProperty* Property=InNode->GetPropertyForVariableFromSkeleton();
        if(!Property) Property=InNode->GetPropertyForVariable();
        if(Property)
        {
            const UClass* PropertyOwner=Property->GetOwnerClass();
            const UBlueprint* DeclaringBlueprint=PropertyOwner?Cast<UBlueprint>(PropertyOwner->ClassGeneratedBy):nullptr;
            return FString::Printf(TEXT("Variable|%s|%s|%s"),
                *(DeclaringBlueprint?DeclaringBlueprint->GetPathName():PropertyOwner?PropertyOwner->GetPathName():Property->GetOwnerStruct()->GetPathName()),
                InNode->IsA<UK2Node_VariableSet>()?TEXT("Set"):TEXT("Get"),*Property->GetName());
        }

		// The variable's category lives on the Blueprint, not on the node, so the
		// owning Blueprint is found through the graph. A node with no Blueprint
		// behind it cannot be given a canonical ID.
		UBlueprint* Blueprint = nullptr;
		if (const UEdGraph* Graph = InNode->GetGraph())
		{
			Blueprint = FBlueprintEditorUtils::FindBlueprintForGraph(Graph);
		}
		if (Blueprint == nullptr)
		{
			return FString();
		}

		// The engine's writer calls an uncategorised variable's category
		// `Default`, which is what makes `Variables|Default|SetHealth` the
		// canonical form. A variable with a real category uses that instead.
		FString Category = TEXT("Default");
		for (const FBPVariableDescription& Variable : Blueprint->NewVariables)
		{
			if (Variable.VarName == VarName)
			{
				const FString Declared = Unlocalised(Variable.Category);
				if (!Declared.IsEmpty())
				{
					Category = Declared;
				}
				break;
			}
		}

		const bool bIsSet = InNode->IsA<UK2Node_VariableSet>();
		return FString::Printf(TEXT("Variables|%s|%s%s"),
			*Category, bIsSet ? TEXT("Set") : TEXT("Get"), *VarName.ToString());
	}

	FString GetNodeTitle(const UEdGraphNode* InNode)
	{
		if (InNode == nullptr)
		{
			return FString();
		}
		return Unlocalised(InNode->GetNodeTitle(ENodeTitleType::ListView));
	}

	FString GetNodeTypeId(const UEdGraphNode* InNode)
	{
		if (InNode == nullptr)
		{
			return FString();
		}

		// Every whitespace character goes, not just the outer ones. A title like
		// `sin ( degrees )` and one like `sin(degrees)` must give one type ID, and
		// the engine's writer compares the string exactly.
		auto StripWhitespace = [](const FString& InText)
		{
			FString Out;
			Out.Reserve(InText.Len());
			for (const TCHAR Ch : InText)
			{
				if (!FChar::IsWhitespace(Ch))
				{
					Out.AppendChar(Ch);
				}
			}
			return Out;
		};

		const FString Title = StripWhitespace(GetNodeTitle(InNode));
		if (Title.IsEmpty())
		{
			return FString();
		}

		// The custom event form is not derivable from category and title, so it is
		// special-cased exactly as the engine's toolset does. Without this an
		// agent would be handed a type ID the writer cannot build.
		if (InNode->IsA<UK2Node_CustomEvent>())
		{
			return FString::Printf(TEXT("AddEvent|Custom|%s"), *Title);
		}

		// Terminators and parent calls have no palette category, but both are
		// addressable graph nodes and must round-trip through the reader/checker.
		if (InNode->IsA<UK2Node_FunctionResult>())
		{
			return TEXT("Function|Result");
		}
		if (InNode->IsA<UK2Node_CallParentFunction>())
		{
			return FString::Printf(TEXT("Parent|%s"), *Title);
		}

		if (const UK2Node_PromotableOperator* Operator = Cast<UK2Node_PromotableOperator>(InNode))
			return TEXT("Utilities|Operators|") + StripWhitespace(Unlocalised(
				FTypePromotion::GetUserFacingOperatorName(Operator->GetOperationName())));

        if (InNode->GetClass() == UK2Node_CallFunction::StaticClass())
            if (const UFunction* Function = CastChecked<UK2Node_CallFunction>(InNode)->GetTargetFunction())
            {
                const UClass* Owner = Function->GetOuterUClass();
                const UBlueprint* Source = Owner ? Cast<UBlueprint>(Owner->ClassGeneratedBy) : nullptr;
                return TEXT("Function|") + (Source
                    ? Source->GetPathName() + TEXT(".") + Function->GetName() : Function->GetPathName());
            }

		// Variable get and set nodes are the other special case: they are not
		// palette nodes, so they have no menu category at all, and the generic rule
		// below would refuse them. That made every variable access in a graph
		// unprintable until this was added.
		if (const UK2Node_Variable* VariableNode = Cast<UK2Node_Variable>(InNode))
		{
			return GetVariableNodeTypeId(VariableNode);
		}

		// The category comes from the node's menu category, which is what the
		// engine's own Blueprint toolset reads. `UEdGraphNode` has no category, so
		// a node that is not a UK2Node (a material expression, for example) has
		// none, and is reported as having no addressable type ID rather than half
		// of one.
		const UK2Node* K2Node = Cast<UK2Node>(InNode);
		const FString Category = K2Node != nullptr
			? StripWhitespace(Unlocalised(K2Node->GetMenuCategory()))
			: FString();

		if (Category.IsEmpty())
		{
			return FString();
		}

		return FString::Printf(TEXT("%s|%s"), *Category, *Title);
	}
}
