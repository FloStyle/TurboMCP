// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Misc/AutomationTest.h"

#include "TurboMCPGraphText.h"
#include "TurboMCPNodeId.h"

#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraph/EdGraphSchema.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/EngineTypes.h"
#include "GameFramework/Actor.h"
#include "K2Node_BreakStruct.h"
#include "K2Node_CallFunction.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_Knot.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/KismetStringLibrary.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/Guid.h"
#include "UObject/Package.h"

/**
 * WHAT THIS PROVES: a pure value used by more than one consumer is rendered as
 * ONE complete inline form followed by supported `(-> node:pin)` links, never
 * as the undefined `$id` token. The first form is only a definition when the
 * whole child render is complete; partial/cycle/depth results are not reused.
 *
 * WHAT IT DOES NOT PROVE: writer round-trip of reader text, a saved asset, or a
 * reopened editor. The fixture is transient and never saved; `Render` is the
 * production one-graph text writer.
 */
namespace
{
	struct FFixture
	{
		UPackage* Package = nullptr;
		UBlueprint* Blueprint = nullptr;
		TArray<UEdGraph*> Graphs;
	};

	struct FScopedFixtureCleanup
	{
		explicit FScopedFixtureCleanup(FFixture& InFixture) : Fixture(InFixture) {}

		~FScopedFixtureCleanup()
		{
			auto Retire = [](UObject* Object)
			{
				if (Object != nullptr && !Object->IsRooted())
				{
					Object->MarkAsGarbage();
				}
			};
			for (UEdGraph* Graph : Fixture.Graphs)
			{
				if (Graph == nullptr)
				{
					continue;
				}
				for (UEdGraphNode* Node : Graph->Nodes)
				{
					Retire(Node);
				}
				Retire(Graph);
			}
			if (Fixture.Blueprint != nullptr)
			{
				Retire(Fixture.Blueprint->GeneratedClass.Get());
				Retire(Fixture.Blueprint->SkeletonGeneratedClass.Get());
				Retire(Fixture.Blueprint);
			}
			if (Fixture.Package != nullptr)
			{
				Fixture.Package->SetDirtyFlag(false);
				Retire(Fixture.Package);
			}
		}

		FFixture& Fixture;
	};

	bool BuildBlueprint(FFixture& OutFixture, const TCHAR* InPurpose)
	{
		const FString Suffix = FGuid::NewGuid().ToString(EGuidFormats::DigitsLower);
		const FString Leaf = FString::Printf(TEXT("TurboMCPSharedValue_%s_%s"), InPurpose, *Suffix);
		UPackage* Package = CreatePackage(*FString::Printf(TEXT("/Game/Tests/%s"), *Leaf));
		if (Package == nullptr)
		{
			return false;
		}
		Package->SetFlags(RF_Transient);
		OutFixture.Package = Package;
		OutFixture.Blueprint = FKismetEditorUtilities::CreateBlueprint(
			AActor::StaticClass(), Package, FName(*Leaf), BPTYPE_Normal,
			UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass(),
			TEXT("TurboMCPGraphTextSharedValueTest"));
		if (OutFixture.Blueprint == nullptr)
		{
			return false;
		}
		OutFixture.Blueprint->SetFlags(RF_Transient);
		return true;
	}

	UEdGraph* AddFunctionGraph(FFixture& Fixture, const TCHAR* InName)
	{
		UEdGraph* Graph = FBlueprintEditorUtils::CreateNewGraph(
			Fixture.Blueprint, FName(InName), UEdGraph::StaticClass(), UEdGraphSchema_K2::StaticClass());
		if (Graph == nullptr)
		{
			return nullptr;
		}
		FBlueprintEditorUtils::AddFunctionGraph<UFunction>(Fixture.Blueprint, Graph,
			/*bIsUserCreated=*/true, /*SignatureFromObject=*/nullptr);
		Fixture.Graphs.Add(Graph);
		return Graph;
	}



	UK2Node_CallFunction* AddCall(UEdGraph* InGraph, UFunction* InFunction)
	{
		if (InGraph == nullptr || InFunction == nullptr)
		{
			return nullptr;
		}
		UK2Node_CallFunction* Node = NewObject<UK2Node_CallFunction>(InGraph);
		if (Node == nullptr)
		{
			return nullptr;
		}
		Node->SetFromFunction(InFunction);
		InGraph->AddNode(Node, /*bUserAction=*/false, /*bSelectNewNode=*/false);
		Node->AllocateDefaultPins();
		return Node;
	}

	UEdGraphPin* PinOf(UEdGraphNode* InNode, const TCHAR* InName, EEdGraphPinDirection InDirection)
	{
		return InNode != nullptr ? InNode->FindPin(InName, InDirection) : nullptr;
	}

	bool Link(UEdGraph* InGraph, UEdGraphPin* InFrom, UEdGraphPin* InTo)
	{
		return InGraph != nullptr && InGraph->GetSchema() != nullptr
			&& InFrom != nullptr && InTo != nullptr
			&& InGraph->GetSchema()->TryCreateConnection(InFrom, InTo);
	}

	bool LinkNodes(UEdGraph* InGraph, UEdGraphNode* InFromNode, const TCHAR* InFromPin,
		UEdGraphNode* InToNode, const TCHAR* InToPin)
	{
		return Link(InGraph,
			PinOf(InFromNode, InFromPin, EGPD_Output),
			PinOf(InToNode, InToPin, EGPD_Input));
	}

	int32 CountText(const FString& InText, const FString& InNeedle)
	{
		if (InNeedle.IsEmpty())
		{
			return 0;
		}
		int32 Count = 0;
		int32 Index = 0;
		while ((Index = InText.Find(InNeedle, ESearchCase::CaseSensitive, ESearchDir::FromStart,
			Index)) != INDEX_NONE)
		{
			++Count;
			Index += InNeedle.Len();
		}
		return Count;
	}

	FString RenderBodies(UEdGraph* InGraph, const FString& InAssetPath, TArray<FString>& OutNotes)
	{
		if (InGraph == nullptr)
		{
			return FString();
		}
		const FTurboMCPGraphText Writer;
		return Writer.Render(InGraph, FTurboMCPGraphText::EDetail::Bodies, InAssetPath, OutNotes);
	}

	FString RenderFull(UEdGraph* InGraph, const FString& InAssetPath, TArray<FString>& OutNotes)
	{
		if (InGraph == nullptr)
		{
			return FString();
		}
		const FTurboMCPGraphText Writer;
		return Writer.Render(InGraph, FTurboMCPGraphText::EDetail::Full, InAssetPath, OutNotes);
	}

	UK2Node_CallFunction* AddPrintString(UEdGraph* InGraph)
	{
		return AddCall(InGraph,
			UKismetSystemLibrary::StaticClass()->FindFunctionByName(TEXT("PrintString")));
	}

	UK2Node_CallFunction* AddConcat(UEdGraph* InGraph)
	{
		return AddCall(InGraph,
			UKismetStringLibrary::StaticClass()->FindFunctionByName(TEXT("Concat_StrStr")));
	}

	UK2Node_CallFunction* AddBuildString(UEdGraph* InGraph)
	{
		return AddCall(InGraph,
			UKismetStringLibrary::StaticClass()->FindFunctionByName(TEXT("BuildString_Float")));
	}

	UK2Node_BreakStruct* AddBreakVector(UEdGraph* InGraph)
	{
		UK2Node_BreakStruct* Node = NewObject<UK2Node_BreakStruct>(InGraph);
		if (Node == nullptr)
		{
			return nullptr;
		}
		Node->StructType = TBaseStructure<FVector>::Get();
		InGraph->AddNode(Node, /*bUserAction=*/false, /*bSelectNewNode=*/false);
		Node->AllocateDefaultPins();
		return Node;
	}

	UK2Node_Knot* AddKnot(UEdGraph* InGraph)
	{
		UK2Node_Knot* Node = NewObject<UK2Node_Knot>(InGraph);
		if (Node == nullptr)
		{
			return nullptr;
		}
		InGraph->AddNode(Node, /*bUserAction=*/false, /*bSelectNewNode=*/false);
		Node->AllocateDefaultPins();
		return Node;
	}

	UK2Node_CustomEvent* AddCustomEvent(UEdGraph* InGraph, const TCHAR* InName)
	{
		UK2Node_CustomEvent* Node = NewObject<UK2Node_CustomEvent>(InGraph);
		if (Node == nullptr)
		{
			return nullptr;
		}
		Node->CustomFunctionName = FName(InName);
		InGraph->AddNode(Node, /*bUserAction=*/false, /*bSelectNewNode=*/false);
		Node->AllocateDefaultPins();
		return Node;
	}

	UEdGraphNode* FindFunctionEntry(UEdGraph* InGraph)
	{
		if (InGraph != nullptr)
		{
			for (UEdGraphNode* Node : InGraph->Nodes)
			{
				if (Node != nullptr && Node->IsA<UK2Node_FunctionEntry>())
				{
					return Node;
				}
			}
		}
		return nullptr;
	}

	bool LinkEntryToPrint(UEdGraph* InGraph, UEdGraphNode* InEntry, UK2Node_CallFunction* InPrint)
	{
		return LinkNodes(InGraph, InEntry, TEXT("then"), InPrint, TEXT("execute"));
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTurboMCPGraphTextSharedValueTest,
	"TurboMCP.GraphText.SharedValueRepair",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTurboMCPGraphTextSharedValueTest::RunTest(const FString& Parameters)
{
	FFixture Fixture;
	FScopedFixtureCleanup Cleanup(Fixture);
	if (!BuildBlueprint(Fixture, TEXT("Main")))
	{
		AddError(TEXT("could not create the transient Blueprint fixture"));
		return false;
	}
	const FString AssetPath = Fixture.Blueprint->GetPathName();

	// ---- direct and rerouted multi-output shared value -------------------
	{
		UEdGraph* Graph = AddFunctionGraph(Fixture, TEXT("SharedMultiOutput"));
		UK2Node_BreakStruct* Break = Graph != nullptr ? AddBreakVector(Graph) : nullptr;
		UK2Node_CallFunction* BuildA = Graph != nullptr ? AddBuildString(Graph) : nullptr;
		UK2Node_CallFunction* BuildB = Graph != nullptr ? AddBuildString(Graph) : nullptr;
		UK2Node_CallFunction* PrintA = Graph != nullptr ? AddPrintString(Graph) : nullptr;
		UK2Node_CallFunction* PrintB = Graph != nullptr ? AddPrintString(Graph) : nullptr;
		UK2Node_Knot* Knot = Graph != nullptr ? AddKnot(Graph) : nullptr;
		UEdGraphNode* Entry = Graph != nullptr ? FindFunctionEntry(Graph) : nullptr;
		if (Break == nullptr || BuildA == nullptr || BuildB == nullptr || PrintA == nullptr
			|| PrintB == nullptr || Knot == nullptr || Entry == nullptr)
		{
			AddError(TEXT("shared multi-output fixture nodes are missing"));
			return false;
		}

		const bool bLinks =
			LinkEntryToPrint(Graph, Entry, PrintA)
			&& LinkNodes(Graph, PrintA, TEXT("then"), PrintB, TEXT("execute"))
			&& LinkNodes(Graph, Break, TEXT("X"), BuildA, TEXT("InFloat"))
			&& Link(Graph, PinOf(Break, TEXT("Y"), EGPD_Output), Knot->GetInputPin())
			&& Link(Graph, Knot->GetOutputPin(), PinOf(BuildB, TEXT("InFloat"), EGPD_Input))
			&& LinkNodes(Graph, BuildA, TEXT("ReturnValue"), PrintA, TEXT("InString"))
			&& LinkNodes(Graph, BuildB, TEXT("ReturnValue"), PrintB, TEXT("InString"));
		if (!bLinks)
		{
			AddError(TEXT("shared multi-output fixture links could not be made"));
			return false;
		}

		TArray<FString> Notes;
		const FString Text = RenderBodies(Graph, AssetPath, Notes);
		const FString BreakId = TurboMCPNodeId::MakeForNode(Break);
		const FString BreakType = TurboMCPNodeId::GetNodeTypeId(Break);
		TestFalse(TEXT("multi-output shared value emits no $id token"), Text.Contains(TEXT("$")));
		TestEqual(TEXT("multi-output shared value is defined exactly once"),
			CountText(Text, FString::Printf(TEXT("(%s"), *BreakType)), 1);
		TestTrue(TEXT("the rerouted use links the real shared node and its Y pin"),
			Text.Contains(FString::Printf(TEXT("(-> %s:Y)"), *BreakId)));
		TestFalse(TEXT("the direct use is not emitted as an X link"),
			Text.Contains(FString::Printf(TEXT("(-> %s:X)"), *BreakId)));

		TArray<FString> FullNotes;
		const FString FullText = RenderFull(Graph, AssetPath, FullNotes);
		TestTrue(TEXT("full detail keeps the knot reference"), FullText.Contains(TEXT("via knot")));
		TestFalse(TEXT("full detail emits no $id token"), FullText.Contains(TEXT("$")));
	}

	// ---- cross-entry sharing --------------------------------------------
	{
		UEdGraph* Graph = Fixture.Blueprint->UbergraphPages.Num() > 0
			? Fixture.Blueprint->UbergraphPages[0].Get() : nullptr;
		UK2Node_CustomEvent* Alpha = Graph != nullptr ? AddCustomEvent(Graph, TEXT("Alpha")) : nullptr;
		UK2Node_CustomEvent* Beta = Graph != nullptr ? AddCustomEvent(Graph, TEXT("Beta")) : nullptr;
		UK2Node_CallFunction* Shared = Graph != nullptr ? AddConcat(Graph) : nullptr;
		UK2Node_CallFunction* PrintA = Graph != nullptr ? AddPrintString(Graph) : nullptr;
		UK2Node_CallFunction* PrintB = Graph != nullptr ? AddPrintString(Graph) : nullptr;
		if (Graph == nullptr || Alpha == nullptr || Beta == nullptr || Shared == nullptr
			|| PrintA == nullptr || PrintB == nullptr)
		{
			AddError(TEXT("cross-entry fixture nodes are missing"));
			return false;
		}
		const bool bLinks =
			LinkEntryToPrint(Graph, Alpha, PrintA)
			&& LinkEntryToPrint(Graph, Beta, PrintB)
			&& LinkNodes(Graph, Shared, TEXT("ReturnValue"), PrintA, TEXT("InString"))
			&& LinkNodes(Graph, Shared, TEXT("ReturnValue"), PrintB, TEXT("InString"));
		if (!bLinks)
		{
			AddError(TEXT("cross-entry fixture links could not be made"));
			return false;
		}

		TArray<FString> Notes;
		const FString Text = RenderBodies(Graph, AssetPath, Notes);
		const FString SharedId = TurboMCPNodeId::MakeForNode(Shared);
		const FString SharedType = TurboMCPNodeId::GetNodeTypeId(Shared);
		TestFalse(TEXT("cross-entry sharing emits no $id token"), Text.Contains(TEXT("$")));
		TestEqual(TEXT("cross-entry shared value has one inline definition"),
			CountText(Text, FString::Printf(TEXT("(%s"), *SharedType)), 1);
		TestEqual(TEXT("cross-entry later use is one supported link"),
			CountText(Text, FString::Printf(TEXT("(-> %s:ReturnValue)"), *SharedId)), 1);
	}

	// ---- depth partial is not a reusable definition ---------------------
	{
		UEdGraph* Graph = AddFunctionGraph(Fixture, TEXT("SharedDepth"));
		UK2Node_CallFunction* Top = Graph != nullptr ? AddConcat(Graph) : nullptr;
		UK2Node_CallFunction* PrintA = Graph != nullptr ? AddPrintString(Graph) : nullptr;
		UK2Node_CallFunction* PrintB = Graph != nullptr ? AddPrintString(Graph) : nullptr;
		UEdGraphNode* Entry = Graph != nullptr ? FindFunctionEntry(Graph) : nullptr;
		if (Graph == nullptr || Top == nullptr || PrintA == nullptr || PrintB == nullptr || Entry == nullptr)
		{
			AddError(TEXT("depth fixture nodes are missing"));
			return false;
		}

		UK2Node_CallFunction* Previous = Top;
		for (int32 Index = 0; Index < 14; ++Index)
		{
			UK2Node_CallFunction* Next = AddConcat(Graph);
			if (Next == nullptr || !LinkNodes(Graph, Next, TEXT("ReturnValue"), Previous, TEXT("A")))
			{
				AddError(TEXT("depth chain could not be built"));
				return false;
			}
			Previous = Next;
		}
		const bool bLinks =
			LinkEntryToPrint(Graph, Entry, PrintA)
			&& LinkNodes(Graph, PrintA, TEXT("then"), PrintB, TEXT("execute"))
			&& LinkNodes(Graph, Top, TEXT("ReturnValue"), PrintA, TEXT("InString"))
			&& LinkNodes(Graph, Top, TEXT("ReturnValue"), PrintB, TEXT("InString"));
		if (!bLinks)
		{
			AddError(TEXT("depth fixture links could not be made"));
			return false;
		}

		TArray<FString> Notes;
		const FString Text = RenderBodies(Graph, AssetPath, Notes);
		const FString TopId = TurboMCPNodeId::MakeForNode(Top);
		TestFalse(TEXT("depth-partial shared value emits no $id"), Text.Contains(TEXT("$")));
		TestTrue(TEXT("depth guard is visible"), Text.Contains(TEXT("depth limit")));
		TestFalse(TEXT("a partial first form is not reused as a link"),
			Text.Contains(FString::Printf(TEXT("(-> %s:"), *TopId)));
	}

	// ---- cycle partial is not a reusable definition ---------------------
	{
		UEdGraph* Graph = AddFunctionGraph(Fixture, TEXT("SharedCycle"));
		UK2Node_CallFunction* A = Graph != nullptr ? AddConcat(Graph) : nullptr;
		UK2Node_CallFunction* B = Graph != nullptr ? AddConcat(Graph) : nullptr;
		UK2Node_CallFunction* PrintA = Graph != nullptr ? AddPrintString(Graph) : nullptr;
		UK2Node_CallFunction* PrintB = Graph != nullptr ? AddPrintString(Graph) : nullptr;
		UEdGraphNode* Entry = Graph != nullptr ? FindFunctionEntry(Graph) : nullptr;
		if (Graph == nullptr || A == nullptr || B == nullptr || PrintA == nullptr
			|| PrintB == nullptr || Entry == nullptr)
		{
			AddError(TEXT("cycle fixture nodes are missing"));
			return false;
		}
		UEdGraphPin* AOut = PinOf(A, TEXT("ReturnValue"), EGPD_Output);
		UEdGraphPin* AIn = PinOf(A, TEXT("A"), EGPD_Input);
		UEdGraphPin* BOut = PinOf(B, TEXT("ReturnValue"), EGPD_Output);
		UEdGraphPin* BIn = PinOf(B, TEXT("A"), EGPD_Input);
		if (AOut == nullptr || AIn == nullptr || BOut == nullptr || BIn == nullptr)
		{
			AddError(TEXT("cycle fixture pins are missing"));
			return false;
		}
		// The value cycle is deliberately made outside the schema's connection
		// path, as the acceptance ValueCycleProbe does; a cycle may be rejected by
		// the editor's normal link validation.
		AOut->MakeLinkTo(BIn);
		BOut->MakeLinkTo(AIn);
		const bool bLinks =
			LinkEntryToPrint(Graph, Entry, PrintA)
			&& LinkNodes(Graph, PrintA, TEXT("then"), PrintB, TEXT("execute"))
			&& LinkNodes(Graph, A, TEXT("ReturnValue"), PrintA, TEXT("InString"))
			&& LinkNodes(Graph, A, TEXT("ReturnValue"), PrintB, TEXT("InString"));
		if (!bLinks)
		{
			AddError(TEXT("cycle fixture links could not be made"));
			return false;
		}

		TArray<FString> Notes;
		const FString Text = RenderBodies(Graph, AssetPath, Notes);
		const FString AId = TurboMCPNodeId::MakeForNode(A);
		TestFalse(TEXT("cycle shared value emits no $id"), Text.Contains(TEXT("$")));
		TestTrue(TEXT("cycle guard is visible"), Text.Contains(TEXT("value cycle")));
		TestFalse(TEXT("a cycle-partial form is not reused as a link"),
			Text.Contains(FString::Printf(TEXT("(-> %s:"), *AId)));
	}

	// ---- malformed knot and ordinary single use -------------------------
	{
		UEdGraph* Graph = AddFunctionGraph(Fixture, TEXT("SharedMalformedKnot"));
		UK2Node_CallFunction* Print = Graph != nullptr ? AddPrintString(Graph) : nullptr;
		UK2Node_Knot* Knot = Graph != nullptr ? AddKnot(Graph) : nullptr;
		UEdGraphNode* Entry = Graph != nullptr ? FindFunctionEntry(Graph) : nullptr;
		if (Graph == nullptr || Print == nullptr || Knot == nullptr || Entry == nullptr)
		{
			AddError(TEXT("malformed-knot fixture nodes are missing"));
			return false;
		}
		if (!LinkEntryToPrint(Graph, Entry, Print)
			|| !Link(Graph, Knot->GetOutputPin(), PinOf(Print, TEXT("InString"), EGPD_Input)))
		{
			AddError(TEXT("malformed-knot fixture links could not be made"));
			return false;
		}

		TArray<FString> Notes;
		const FString Text = RenderBodies(Graph, AssetPath, Notes);
		TestFalse(TEXT("malformed knot emits no $id"), Text.Contains(TEXT("$")));
		TestTrue(TEXT("malformed knot is diagnosed"), Text.Contains(TEXT("unresolved knot chain")));
	}

	{
		UEdGraph* Graph = AddFunctionGraph(Fixture, TEXT("SingleUsePure"));
		UK2Node_CallFunction* Shared = Graph != nullptr ? AddConcat(Graph) : nullptr;
		UK2Node_CallFunction* Print = Graph != nullptr ? AddPrintString(Graph) : nullptr;
		UEdGraphNode* Entry = Graph != nullptr ? FindFunctionEntry(Graph) : nullptr;
		if (Graph == nullptr || Shared == nullptr || Print == nullptr || Entry == nullptr)
		{
			AddError(TEXT("single-use fixture nodes are missing"));
			return false;
		}
		if (!LinkEntryToPrint(Graph, Entry, Print)
			|| !LinkNodes(Graph, Shared, TEXT("ReturnValue"), Print, TEXT("InString")))
		{
			AddError(TEXT("single-use fixture links could not be made"));
			return false;
		}

		TArray<FString> Notes;
		const FString First = RenderBodies(Graph, AssetPath, Notes);
		TArray<FString> SecondNotes;
		const FString Second = RenderBodies(Graph, AssetPath, SecondNotes);
		const FString SharedId = TurboMCPNodeId::MakeForNode(Shared);
		const FString SharedType = TurboMCPNodeId::GetNodeTypeId(Shared);
		TestEqual(TEXT("ordinary single-use text is stable"), First, Second);
		TestFalse(TEXT("single-use text emits no $id"), First.Contains(TEXT("$")));
		TestEqual(TEXT("single-use pure node is inline once"),
			CountText(First, FString::Printf(TEXT("(%s"), *SharedType)), 1);
		TestFalse(TEXT("single-use pure node is not a link"),
			First.Contains(FString::Printf(TEXT("(-> %s:"), *SharedId)));
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
