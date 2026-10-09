// Copyright FloStyle. All Rights Reserved.

#include "TurboMCPNodeId.h"

#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "Engine/Blueprint.h"
#include "K2Node.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_Variable.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Misc/Char.h"
#include "Misc/Guid.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"

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

		// `3.1` says to quote the graph name if it has spaces. The quotes are
		// printed so the ID can be read back unambiguously.
		const FString GraphPart = InGraphName.Contains(TEXT(" "))
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

		// Split from the right, so a graph name containing a colon cannot steal
		// the node key. The key is always the last part.
		FString Remainder = InId;

		int32 LastColon = INDEX_NONE;
		if (!Remainder.FindLastChar(TEXT(':'), LastColon))
		{
			// One part only. It can only be an asset.
			Result.AssetName = Remainder;
			Result.bIsLongForm = Remainder.StartsWith(TEXT("/"));
			return Result;
		}

		Result.NodeKey = Remainder.Mid(LastColon + 1).TrimStartAndEnd();
		Remainder = Remainder.Left(LastColon);

		// The asset name never contains a colon, so the first colon in what is
		// left separates the asset from the graph. A graph name containing a colon
		// cannot be handled by an ID anyway, and this is why the key is taken from
		// the right above.
		int32 AssetEnd = INDEX_NONE;
		if (Remainder.FindChar(TEXT(':'), AssetEnd))
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

		// `3.1` allows the graph name to be quoted when it holds spaces. Strip the
		// quotes so the name matches the real graph.
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

		// Pad a short key with leading zeros, because a GUID's leading zeros are
		// not significant and a caller may write fewer digits by hand. Appending
		// an explicit run of zeros rather than padding to a width: `3.1` counts
		// digits from the left, so the zeros belong on the right.
		if (Trimmed.Len() < StandardKeyDigits)
		{
			Trimmed += FString::ChrN(StandardKeyDigits - Trimmed.Len(), TEXT('0'));
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

		const FString Wanted = InKey.TrimStartAndEnd().ToLower();
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

	UEdGraph* FindGraphByName(const UBlueprint* InBlueprint, const FString& InGraphName)
	{
		if (InBlueprint == nullptr || InGraphName.IsEmpty())
		{
			return nullptr;
		}

		// Every graph kind is searched, because an ID names a graph and not a kind
		// of graph. A caller asking for `EventGraph` should find it whichever
		// array the Blueprint happens to keep it in.
		TArray<UEdGraph*> Candidates;
		auto Collect = [&Candidates](const TArray<TObjectPtr<UEdGraph>>& InGraphs)
		{
			for (const TObjectPtr<UEdGraph>& Graph : InGraphs)
			{
				if (Graph != nullptr)
				{
					Candidates.Add(Graph);
				}
			}
		};

		Collect(InBlueprint->UbergraphPages);
		Collect(InBlueprint->FunctionGraphs);
		Collect(InBlueprint->MacroGraphs);
		Collect(InBlueprint->EventGraphs);

		for (UEdGraph* Graph : Candidates)
		{
			if (Graph->GetName().Equals(InGraphName, ESearchCase::IgnoreCase))
			{
				return Graph;
			}
		}

		return nullptr;
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
