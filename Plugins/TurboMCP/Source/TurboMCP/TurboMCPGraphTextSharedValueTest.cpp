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
#include "K2Node_FunctionResult.h"
#include "K2Node_Knot.h"
#include "K2Node_Self.h"
#include "K2Node_VariableGet.h"
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

	UEdGraph* AddUbergraphPage(FFixture& Fixture, const TCHAR* InName)
	{
		UEdGraph* Graph = FBlueprintEditorUtils::CreateNewGraph(
			Fixture.Blueprint, FName(InName), UEdGraph::StaticClass(), UEdGraphSchema_K2::StaticClass());
		if (Graph == nullptr)
		{
			return nullptr;
		}
		FBlueprintEditorUtils::AddUbergraphPage(Fixture.Blueprint, Graph);
		Fixture.Graphs.Add(Graph);
		return Graph;
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
		// Pinned 5.8 has BuildString_Double (DisplayName "Build String (Float)").
		// There is no BuildString_Float symbol in this engine source.
		return AddCall(InGraph,
			UKismetStringLibrary::StaticClass()->FindFunctionByName(TEXT("BuildString_Double")));
	}

	UK2Node_CallFunction* AddBreakVector(UEdGraph* InGraph)
	{
		// `UK2Node_BreakStruct` has no dependable reader type ID in this build
		// (the automation run produced an empty ID). The pinned math library
		// exposes `BreakVector` as a BlueprintPure multi-output function with a
		// real menu category and fixed X/Y/Z output pins.
		return AddCall(InGraph,
			UKismetMathLibrary::StaticClass()->FindFunctionByName(TEXT("BreakVector")));
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

	void ForceNodeGuid(UEdGraphNode* InNode, uint32 InA, uint32 InB = 0,
		uint32 InC = 0, uint32 InD = 0)
	{
		if (InNode != nullptr)
		{
			InNode->CreateNewGuid();
			InNode->NodeGuid = FGuid(InA, InB, InC, InD);
		}
	}

	void AssignDistinctGuids(UEdGraph* InGraph, uint32 InBase)
	{
		if (InGraph == nullptr)
		{
			return;
		}
		uint32 Index = 0;
		for (UEdGraphNode* Node : InGraph->Nodes)
		{
			if (Node != nullptr)
			{
				ForceNodeGuid(Node, InBase + Index, 0x1000, 0, 0);
				++Index;
			}
		}
	}

	bool VerifyFixtureGuids(FAutomationTestBase& Test, UEdGraph* InGraph, const TCHAR* InLabel)
	{
		if (InGraph == nullptr)
		{
			return false;
		}
		bool bOk = true;
		TSet<FGuid> Seen;
		for (UEdGraphNode* Node : InGraph->Nodes)
		{
			if (Node == nullptr)
			{
				bOk = false;
				continue;
			}
			Test.TestTrue(*FString::Printf(TEXT("%s: every fixture node has a valid GUID"), InLabel),
				Node->NodeGuid.IsValid());
			Test.TestFalse(*FString::Printf(TEXT("%s: fixture GUIDs are unique"), InLabel),
				Seen.Contains(Node->NodeGuid));
			if (!Node->NodeGuid.IsValid() || Seen.Contains(Node->NodeGuid))
			{
				bOk = false;
			}
			Seen.Add(Node->NodeGuid);
		}
		return bOk;
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

	/** Remove only the auto-created result node so the exact fixture has no disconnected tail. */
	void RemoveFunctionResults(UEdGraph* InGraph)
	{
		if (InGraph == nullptr)
		{
			return;
		}
		for (int32 Index = InGraph->Nodes.Num() - 1; Index >= 0; --Index)
		{
			if (UEdGraphNode* Node = InGraph->Nodes[Index])
			{
				if (Node->IsA<UK2Node_FunctionResult>())
				{
					InGraph->RemoveNode(Node);
				}
			}
		}
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

	// A fixture prerequisite names the exact missing object instead of a single
	// combined message, so a construction failure is diagnosable from the report.
	auto Missing = [this](const TCHAR* InDetail)
	{
		AddError(FString::Printf(TEXT("fixture prerequisite missing: %s"), InDetail));
		return false;
	};

	// ---- direct and rerouted multi-output shared value -------------------
	{
		UEdGraph* Graph = AddFunctionGraph(Fixture, TEXT("SharedMultiOutput"));
		UK2Node_CallFunction* Break = Graph != nullptr ? AddBreakVector(Graph) : nullptr;
		UK2Node_CallFunction* BuildA = Graph != nullptr ? AddBuildString(Graph) : nullptr;
		UK2Node_CallFunction* BuildB = Graph != nullptr ? AddBuildString(Graph) : nullptr;
		UK2Node_CallFunction* PrintA = Graph != nullptr ? AddPrintString(Graph) : nullptr;
		UK2Node_CallFunction* PrintB = Graph != nullptr ? AddPrintString(Graph) : nullptr;
		UK2Node_Knot* Knot = Graph != nullptr ? AddKnot(Graph) : nullptr;
		UEdGraphNode* Entry = Graph != nullptr ? FindFunctionEntry(Graph) : nullptr;
		if (Graph == nullptr)
		{
			AddError(TEXT("shared multi-output fixture: function graph was not created"));
			return false;
		}
		if (Break == nullptr)
		{
			AddError(TEXT("shared multi-output fixture: BreakVector call node was not created"));
			return false;
		}
		if (BuildA == nullptr || BuildB == nullptr)
		{
			AddError(TEXT("shared multi-output fixture: BuildString_Double call node was not created"));
			return false;
		}
		if (PrintA == nullptr || PrintB == nullptr)
		{
			AddError(TEXT("shared multi-output fixture: PrintString call node was not created"));
			return false;
		}
		if (Knot == nullptr)
		{
			AddError(TEXT("shared multi-output fixture: knot node was not created"));
			return false;
		}
		if (Entry == nullptr)
		{
			AddError(TEXT("shared multi-output fixture: function entry was not created"));
			return false;
		}

		const bool bLinks =
			LinkEntryToPrint(Graph, Entry, PrintA)
			&& LinkNodes(Graph, PrintA, TEXT("then"), PrintB, TEXT("execute"))
			&& LinkNodes(Graph, Break, TEXT("X"), BuildA, TEXT("InDouble"))
			&& Link(Graph, PinOf(Break, TEXT("Y"), EGPD_Output), Knot->GetInputPin())
			&& Link(Graph, Knot->GetOutputPin(), PinOf(BuildB, TEXT("InDouble"), EGPD_Input))
			&& LinkNodes(Graph, BuildA, TEXT("ReturnValue"), PrintA, TEXT("InString"))
			&& LinkNodes(Graph, BuildB, TEXT("ReturnValue"), PrintB, TEXT("InString"));
		if (!bLinks)
		{
			AddError(TEXT("shared multi-output fixture links could not be made"));
			return false;
		}
		AssignDistinctGuids(Graph, 0x51000000);
		if (!VerifyFixtureGuids(*this, Graph, TEXT("multi-output")))
		{
			AddError(TEXT("multi-output fixture GUID preconditions failed"));
			return false;
		}

		TArray<FString> Notes;
		const FString Text = RenderBodies(Graph, AssetPath, Notes);
		const FString BreakId = TurboMCPNodeId::MakeForNode(Break);
		const FString BreakType = TurboMCPNodeId::GetNodeTypeId(Break);
		TestFalse(TEXT("multi-output fixture source has a supported type ID"), BreakType.IsEmpty());
		if (BreakType.IsEmpty())
		{
			AddError(TEXT("multi-output fixture source has no type ID; the renderer cannot represent it"));
			AddInfo(FString::Printf(TEXT("emitted text (first 1200 chars): %s"), *Text.Left(1200)));
			return false;
		}
		const int32 BreakTypeCount = CountText(Text, FString::Printf(TEXT("(%s"), *BreakType));
		const bool bHasYLink = Text.Contains(FString::Printf(TEXT("(-> %s:Y)"), *BreakId));
		TestFalse(TEXT("multi-output shared value emits no $id token"), Text.Contains(TEXT("$")));
		TestEqual(TEXT("multi-output shared value is defined exactly once"), BreakTypeCount, 1);
		TestTrue(TEXT("the rerouted use links the real shared node and its Y pin"), bHasYLink);
		TestFalse(TEXT("the direct use is not emitted as an X link"),
			Text.Contains(FString::Printf(TEXT("(-> %s:X)"), *BreakId)));
		if (BreakTypeCount != 1 || !bHasYLink)
		{
			AddInfo(FString::Printf(TEXT("multi-output emitted text (first 1600 chars): %s"),
				*Text.Left(1600)));
		}

		TArray<FString> FullNotes;
		const FString FullText = RenderFull(Graph, AssetPath, FullNotes);
		TestTrue(TEXT("full detail keeps the knot reference"), FullText.Contains(TEXT("via knot")));
		TestFalse(TEXT("full detail emits no $id token"), FullText.Contains(TEXT("$")));
	}

	// ---- cross-entry sharing --------------------------------------------
	{
		UEdGraph* Graph = AddUbergraphPage(Fixture, TEXT("SharedEntries"));
		UK2Node_CustomEvent* Alpha = Graph != nullptr ? AddCustomEvent(Graph, TEXT("Alpha")) : nullptr;
		UK2Node_CustomEvent* Beta = Graph != nullptr ? AddCustomEvent(Graph, TEXT("Beta")) : nullptr;
		UK2Node_CallFunction* Shared = Graph != nullptr ? AddConcat(Graph) : nullptr;
		UK2Node_CallFunction* PrintA = Graph != nullptr ? AddPrintString(Graph) : nullptr;
		UK2Node_CallFunction* PrintB = Graph != nullptr ? AddPrintString(Graph) : nullptr;
		if (Graph == nullptr) { return Missing(TEXT("cross-entry EventGraph")); }
		if (Alpha == nullptr) { return Missing(TEXT("cross-entry Alpha custom event")); }
		if (Beta == nullptr) { return Missing(TEXT("cross-entry Beta custom event")); }
		if (Shared == nullptr) { return Missing(TEXT("cross-entry Concat_StrStr")); }
		if (PrintA == nullptr || PrintB == nullptr) { return Missing(TEXT("cross-entry PrintString")); }
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
		AssignDistinctGuids(Graph, 0x52000000);
		if (!VerifyFixtureGuids(*this, Graph, TEXT("cross-entry")))
		{
			AddError(TEXT("cross-entry fixture GUID preconditions failed"));
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
		if (Graph == nullptr) { return Missing(TEXT("depth function graph")); }
		if (Top == nullptr) { return Missing(TEXT("depth top Concat_StrStr")); }
		if (PrintA == nullptr || PrintB == nullptr) { return Missing(TEXT("depth PrintString")); }
		if (Entry == nullptr) { return Missing(TEXT("depth function entry")); }

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
		AssignDistinctGuids(Graph, 0x53000000);
		if (!VerifyFixtureGuids(*this, Graph, TEXT("depth")))
		{
			AddError(TEXT("depth fixture GUID preconditions failed"));
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
		if (Graph == nullptr) { return Missing(TEXT("cycle function graph")); }
		if (A == nullptr || B == nullptr) { return Missing(TEXT("cycle Concat_StrStr")); }
		if (PrintA == nullptr || PrintB == nullptr) { return Missing(TEXT("cycle PrintString")); }
		if (Entry == nullptr) { return Missing(TEXT("cycle function entry")); }
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
		AssignDistinctGuids(Graph, 0x54000000);
		if (!VerifyFixtureGuids(*this, Graph, TEXT("cycle")))
		{
			AddError(TEXT("cycle fixture GUID preconditions failed"));
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
		if (Graph == nullptr) { return Missing(TEXT("malformed-knot function graph")); }
		if (Print == nullptr) { return Missing(TEXT("malformed-knot PrintString")); }
		if (Knot == nullptr) { return Missing(TEXT("malformed-knot knot node")); }
		if (Entry == nullptr) { return Missing(TEXT("malformed-knot function entry")); }
		if (!LinkEntryToPrint(Graph, Entry, Print)
			|| !Link(Graph, Knot->GetOutputPin(), PinOf(Print, TEXT("InString"), EGPD_Input)))
		{
			AddError(TEXT("malformed-knot fixture links could not be made"));
			return false;
		}
		AssignDistinctGuids(Graph, 0x55000000);
		if (!VerifyFixtureGuids(*this, Graph, TEXT("malformed-knot")))
		{
			AddError(TEXT("malformed-knot fixture GUID preconditions failed"));
			return false;
		}

		TArray<FString> Notes;
		const FString Text = RenderBodies(Graph, AssetPath, Notes);
		TestFalse(TEXT("malformed knot emits no $id"), Text.Contains(TEXT("$")));
		TestTrue(TEXT("malformed knot is diagnosed"), Text.Contains(TEXT("unresolved knot chain")));
	}

	// ---- unsupported shared first form ----------------------------------
	{
		UEdGraph* Graph = AddFunctionGraph(Fixture, TEXT("SharedUnsupported"));
		UK2Node_CallFunction* PrintA = Graph != nullptr ? AddPrintString(Graph) : nullptr;
		UK2Node_CallFunction* PrintB = Graph != nullptr ? AddPrintString(Graph) : nullptr;
		UEdGraphNode* Entry = Graph != nullptr ? FindFunctionEntry(Graph) : nullptr;
		UEdGraphNode* Bad = Graph != nullptr ? NewObject<UEdGraphNode>(Graph) : nullptr;
		if (Graph == nullptr) { return Missing(TEXT("unsupported-node function graph")); }
		if (PrintA == nullptr || PrintB == nullptr) { return Missing(TEXT("unsupported-node PrintString")); }
		if (Entry == nullptr) { return Missing(TEXT("unsupported-node function entry")); }
		if (Bad == nullptr) { return Missing(TEXT("unsupported-node plain UEdGraphNode")); }
		Graph->AddNode(Bad, /*bUserAction=*/false, /*bSelectNewNode=*/false);
		UEdGraphPin* BadOut = Bad->CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_String,
			FName(TEXT("BadOut")));
		UEdGraphPin* PrintAString = PinOf(PrintA, TEXT("InString"), EGPD_Input);
		UEdGraphPin* PrintBString = PinOf(PrintB, TEXT("InString"), EGPD_Input);
		if (BadOut == nullptr || PrintAString == nullptr || PrintBString == nullptr
			|| !LinkEntryToPrint(Graph, Entry, PrintA)
			|| !LinkNodes(Graph, PrintA, TEXT("then"), PrintB, TEXT("execute")))
		{
			AddError(TEXT("unsupported-node fixture links could not be made"));
			return false;
		}
		BadOut->MakeLinkTo(PrintAString);
		BadOut->MakeLinkTo(PrintBString);
		AssignDistinctGuids(Graph, 0x56000000);
		if (!VerifyFixtureGuids(*this, Graph, TEXT("unsupported")))
		{
			AddError(TEXT("unsupported-node fixture GUID preconditions failed"));
			return false;
		}
		const FString BadId = TurboMCPNodeId::MakeForNode(Bad);
		TArray<FString> Notes;
		const FString Text = RenderBodies(Graph, AssetPath, Notes);
		TestFalse(TEXT("unsupported shared value emits no $id"), Text.Contains(TEXT("$")));
		TestTrue(TEXT("unsupported shared value is diagnosed"),
			Text.Contains(TEXT("has no node type ID")));
        TestTrue(TEXT("Unsupported node has bounded edit guidance"),
            Text.Contains(TEXT("Retain this node")) && Text.Contains(TEXT("PreviewTurboMCPGraphConnect"))
            && Text.Contains(TEXT("Check compatibility and any replaced wires"))
            && Text.Contains(TEXT("not a writable node form")));
        TestTrue(TEXT("Reader preserves unsupported node links"),
            BadOut->LinkedTo.Contains(PrintAString) && BadOut->LinkedTo.Contains(PrintBString));
		TestFalse(TEXT("an unsupported first form is not reused as a link"),
			Text.Contains(FString::Printf(TEXT("(-> %s:"), *BadId)));
	}

	// ---- unsupported child under a valid shared parent ------------------
	{
		UEdGraph* Graph = AddFunctionGraph(Fixture, TEXT("SharedUnsupportedChild"));
		UK2Node_CallFunction* Shared = Graph != nullptr ? AddConcat(Graph) : nullptr;
		UK2Node_CallFunction* PrintA = Graph != nullptr ? AddPrintString(Graph) : nullptr;
		UK2Node_CallFunction* PrintB = Graph != nullptr ? AddPrintString(Graph) : nullptr;
		UEdGraphNode* Entry = Graph != nullptr ? FindFunctionEntry(Graph) : nullptr;
		UEdGraphNode* Bad = Graph != nullptr ? NewObject<UEdGraphNode>(Graph) : nullptr;
		if (Graph == nullptr) { return Missing(TEXT("unsupported-child function graph")); }
		if (Shared == nullptr) { return Missing(TEXT("unsupported-child Concat_StrStr")); }
		if (PrintA == nullptr || PrintB == nullptr) { return Missing(TEXT("unsupported-child PrintString")); }
		if (Entry == nullptr) { return Missing(TEXT("unsupported-child function entry")); }
		if (Bad == nullptr) { return Missing(TEXT("unsupported-child plain UEdGraphNode")); }
		Graph->AddNode(Bad, /*bUserAction=*/false, /*bSelectNewNode=*/false);
		UEdGraphPin* BadOut = Bad->CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_String,
			FName(TEXT("BadOut")));
		UEdGraphPin* SharedA = PinOf(Shared, TEXT("A"), EGPD_Input);
		if (BadOut == nullptr || SharedA == nullptr
			|| !LinkEntryToPrint(Graph, Entry, PrintA)
			|| !LinkNodes(Graph, PrintA, TEXT("then"), PrintB, TEXT("execute"))
			|| !LinkNodes(Graph, Shared, TEXT("ReturnValue"), PrintA, TEXT("InString"))
			|| !LinkNodes(Graph, Shared, TEXT("ReturnValue"), PrintB, TEXT("InString")))
		{
			AddError(TEXT("unsupported-child fixture links could not be made"));
			return false;
		}
		BadOut->MakeLinkTo(SharedA);
		TestTrue(TEXT("unsupported child has no type ID as a setup precondition"),
			TurboMCPNodeId::GetNodeTypeId(Bad).IsEmpty());
		AssignDistinctGuids(Graph, 0x58000000);
		if (!VerifyFixtureGuids(*this, Graph, TEXT("unsupported-child")))
		{
			AddError(TEXT("unsupported-child fixture GUID preconditions failed"));
			return false;
		}
		const FString SharedId = TurboMCPNodeId::MakeForNode(Shared);
		const FString SharedType = TurboMCPNodeId::GetNodeTypeId(Shared);
		TArray<FString> Notes;
		const FString Text = RenderBodies(Graph, AssetPath, Notes);
		TestFalse(TEXT("unsupported child under a shared parent emits no $id"),
			Text.Contains(TEXT("$")));
		TestTrue(TEXT("unsupported child diagnostic repeats across parent attempts"),
			CountText(Text, TEXT("has no node type ID")) >= 2);
		TestTrue(TEXT("the shared parent is rendered again rather than linked"),
			CountText(Text, FString::Printf(TEXT("(%s"), *SharedType)) >= 2);
		TestFalse(TEXT("a parent with a failed child is not reused as a link"),
			Text.Contains(FString::Printf(TEXT("(-> %s:"), *SharedId)));
	}

	// ---- malformed nonempty link under a shared parent -------------------
	{
		UEdGraph* Graph = AddFunctionGraph(Fixture, TEXT("SharedMalformedLink"));
		UK2Node_CallFunction* Shared = Graph != nullptr ? AddConcat(Graph) : nullptr;
		UK2Node_CallFunction* PrintA = Graph != nullptr ? AddPrintString(Graph) : nullptr;
		UK2Node_CallFunction* PrintB = Graph != nullptr ? AddPrintString(Graph) : nullptr;
		UEdGraphNode* Entry = Graph != nullptr ? FindFunctionEntry(Graph) : nullptr;
		if (Graph == nullptr) { return Missing(TEXT("malformed-link function graph")); }
		if (Shared == nullptr) { return Missing(TEXT("malformed-link Concat_StrStr")); }
		if (PrintA == nullptr || PrintB == nullptr) { return Missing(TEXT("malformed-link PrintString")); }
		if (Entry == nullptr) { return Missing(TEXT("malformed-link function entry")); }
		if (!LinkEntryToPrint(Graph, Entry, PrintA)
			|| !LinkNodes(Graph, PrintA, TEXT("then"), PrintB, TEXT("execute"))
			|| !LinkNodes(Graph, Shared, TEXT("ReturnValue"), PrintA, TEXT("InString"))
			|| !LinkNodes(Graph, Shared, TEXT("ReturnValue"), PrintB, TEXT("InString")))
		{
			AddError(TEXT("malformed-link fixture links could not be made"));
			return false;
		}
		UEdGraphPin* SharedA = PinOf(Shared, TEXT("A"), EGPD_Input);
		if (SharedA == nullptr)
		{
			AddError(TEXT("malformed-link fixture input pin is missing"));
			return false;
		}
		SharedA->LinkedTo.Add(nullptr);
		AssignDistinctGuids(Graph, 0x57000000);
		if (!VerifyFixtureGuids(*this, Graph, TEXT("malformed-link")))
		{
			AddError(TEXT("malformed-link fixture GUID preconditions failed"));
			return false;
		}
		const FString SharedId = TurboMCPNodeId::MakeForNode(Shared);
		TArray<FString> Notes;
		const FString Text = RenderBodies(Graph, AssetPath, Notes);
		TestFalse(TEXT("malformed nested link emits no $id"), Text.Contains(TEXT("$")));
		TestTrue(TEXT("malformed nested link is diagnosed"), Text.Contains(TEXT("malformed link")));
		TestFalse(TEXT("a malformed-first shared parent is not reused"),
			Text.Contains(FString::Printf(TEXT("(-> %s:"), *SharedId)));
	}

	// ---- exact ordinary single-use output (approved-base behavior) --------
	{
		UEdGraph* Graph = AddFunctionGraph(Fixture, TEXT("ExactSingleUse"));
		RemoveFunctionResults(Graph);
		UK2Node_CallFunction* Print = Graph != nullptr ? AddPrintString(Graph) : nullptr;
		UEdGraphNode* Entry = Graph != nullptr ? FindFunctionEntry(Graph) : nullptr;
		FEdGraphPinType StringType;
		StringType.PinCategory = UEdGraphSchema_K2::PC_String;
		FBlueprintEditorUtils::AddMemberVariable(Fixture.Blueprint, FName(TEXT("ExactValue")), StringType,
			TEXT("seed"));
		UK2Node_VariableGet* Getter = Graph != nullptr ? NewObject<UK2Node_VariableGet>(Graph) : nullptr;
		if (Graph == nullptr) { return Missing(TEXT("exact single-use function graph")); }
		if (Print == nullptr) { return Missing(TEXT("exact single-use PrintString")); }
		if (Entry == nullptr) { return Missing(TEXT("exact single-use function entry")); }
		if (Getter == nullptr) { return Missing(TEXT("exact single-use variable getter")); }
		Getter->VariableReference.SetSelfMember(FName(TEXT("ExactValue")));
		Graph->AddNode(Getter, /*bUserAction=*/false, /*bSelectNewNode=*/false);
		Getter->AllocateDefaultPins();
		// The exact literal covers only the ordinary InString input. Clear the
		// supplied defaults on PrintString's other unlinked inputs so they do not
		// add unrelated pin lines to the expected shape.
		for (UEdGraphPin* Pin : Print->Pins)
		{
			if (Pin != nullptr && Pin->Direction == EGPD_Input
				&& !Pin->PinName.ToString().Equals(TEXT("InString"), ESearchCase::CaseSensitive))
			{
				Pin->DefaultValue.Empty();
				Pin->DefaultObject = nullptr;
				Pin->DefaultTextValue = FText::GetEmpty();
			}
		}
		UEdGraphPin* GetterOut = PinOf(Getter, TEXT("ExactValue"), EGPD_Output);
		if (GetterOut == nullptr)
		{
			for (UEdGraphPin* Pin : Getter->Pins)
			{
				if (Pin != nullptr && Pin->Direction == EGPD_Output)
				{
					GetterOut = Pin;
					break;
				}
			}
		}
		if (GetterOut == nullptr
			|| !LinkEntryToPrint(Graph, Entry, Print)
			|| !Link(Graph, GetterOut, PinOf(Print, TEXT("InString"), EGPD_Input)))
		{
			AddError(TEXT("exact single-use fixture links could not be made"));
			return false;
		}
		ForceNodeGuid(Entry, 0x11110001);
		ForceNodeGuid(Print, 0x22220001);
		ForceNodeGuid(Getter, 0x33330001);
		if (!VerifyFixtureGuids(*this, Graph, TEXT("exact-single-use")))
		{
			AddError(TEXT("exact single-use fixture GUID preconditions failed"));
			return false;
		}

		const UFunction* PrintFunction = Print->GetTargetFunction();
		const FString PrintTargetPath = PrintFunction != nullptr ? PrintFunction->GetPathName() : FString();
		TestTrue(TEXT("exact fixture PrintString has a real target function"), !PrintTargetPath.IsEmpty());
		TestEqual(TEXT("exact fixture PrintString type ID is the expected literal"),
			TurboMCPNodeId::GetNodeTypeId(Print), FString(TEXT("Function|/Script/Engine.KismetSystemLibrary:PrintString")));
		TestEqual(TEXT("exact fixture getter type ID is the expected literal"),
			TurboMCPNodeId::GetNodeTypeId(Getter), FString(TEXT("Variable|")) + AssetPath + TEXT("|Get|ExactValue"));
		if (PrintTargetPath.IsEmpty())
		{
			return false;
		}

		TArray<FString> Notes;
		const FString First = RenderBodies(Graph, AssetPath, Notes);
		TArray<FString> SecondNotes;
		const FString Second = RenderBodies(Graph, AssetPath, SecondNotes);
		// Provenance: this literal is the current documented RenderNode shape derived
		// from source, including the unconditional `; target <UFunction path>`
		// annotation for UK2Node_CallFunction and the ordinary single-use inline.
		// It is not derived from candidate Render output; the fixture metadata
		// supplies only dynamic IDs and the real function path. It has not been
		// run against the approved base here. Repeatability below is extra.
		const FString PrintId = TurboMCPNodeId::MakeForNode(Print);
		const FString EntryId = TurboMCPNodeId::MakeForNode(Entry);
		const FString GetterId = TurboMCPNodeId::MakeForNode(Getter);
		const FString Expected = FString::Printf(
			TEXT("; graph %s.ExactSingleUse\n"
				"(fn ExactSingleUse ; %s\n"
				"  (Function|/Script/Engine.KismetSystemLibrary:PrintString ; %s ; target %s\n"
				"      execute (-> %s)\n"
				"      InString (Variable|%s|Get|ExactValue ; %s\n  )\n  )\n  (:then\n  )\n)"),
			*AssetPath, *TurboMCPNodeId::MakeForNode(Entry, true), *PrintId, *PrintTargetPath, *EntryId, *AssetPath, *GetterId);
		TestEqual(TEXT("ordinary single-use text exactly matches the reflected identity shape"), First, Expected);
		TestEqual(TEXT("ordinary single-use text is repeatable"), First, Second);
		TestFalse(TEXT("ordinary single-use text emits no $id"), First.Contains(TEXT("$")));
	}

	// Supported multi-target calls must not be diagnosed as broken value links.
	{
		UEdGraph* Graph = AddFunctionGraph(Fixture, TEXT("MultipleTargets"));
		UK2Node_CallFunction* Call = AddCall(Graph, AActor::StaticClass()->FindFunctionByName(TEXT("SetActorHiddenInGame")));
		if (!Graph || !Call) { AddError(TEXT("multi-target fixture creation failed")); return false; }
		UEdGraphPin* SelfPin = PinOf(Call, TEXT("self"), EGPD_Input);
		if (!SelfPin) { AddError(TEXT("multi-target call has no self pin")); return false; }
		for (int32 Index = 0; Index < 2; ++Index)
		{
			UK2Node_Self* Target = NewObject<UK2Node_Self>(Graph);
			Graph->AddNode(Target, false, false);
			Target->CreateNewGuid(); Target->AllocateDefaultPins();
			if (!Link(Graph, PinOf(Target, TEXT("self"), EGPD_Output), SelfPin))
			{ AddError(TEXT("engine schema refused the multi-target fixture")); return false; }
		}
		Call->CreateNewGuid();
		if (!LinkEntryToPrint(Graph, FindFunctionEntry(Graph), Call))
		{ AddError(TEXT("multi-target call could not connect to the function entry")); return false; }
		if (SelfPin->LinkedTo.Num() != 2)
		{ AddError(TEXT("engine schema did not preserve both targets")); return false; }
		const auto Links = SelfPin->LinkedTo;
		const bool bDirty = Fixture.Package->IsDirty();
		TArray<FString> Notes;
		const FString Text = RenderFull(Graph, AssetPath, Notes);
		TestTrue(TEXT("multiple targets are identified as inspection evidence"), Text.Contains(TEXT("multiple call targets (inspection only")));
		TestFalse(TEXT("valid multi-target self is not malformed"), Text.Contains(TEXT("malformed link")));
		for (const UEdGraphPin* Target : Links)
			TestTrue(TEXT("each exact target is reported"), Text.Contains(TurboMCPNodeId::MakeForNode(Target->GetOwningNodeUnchecked(), true)));
		TestTrue(TEXT("reader preserves target links"), SelfPin->LinkedTo == Links);
		TestEqual(TEXT("reader preserves package dirty state"), Fixture.Package->IsDirty(), bDirty);
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
