// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Misc/AutomationTest.h"

#include "TurboMCPGraphAppend.h"
#include "TurboMCPNodeId.h"
#include "Tools/TurboMCPToolsets.h"

#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraph/EdGraphSchema.h"
#include "EdGraphNode_Comment.h"
#include "EdGraphSchema_K2.h"
#include "Editor.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "GameFramework/Actor.h"
#include "K2Node_CallFunction.h"
#include "K2Node_CustomEvent.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Math/NumericLimits.h"
#include "Misc/Guid.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"

/**
 * `FTurboMCPGraphAppend` placement, driven through the real Plan and Apply on disposable in-memory graphs.
 *
 * The fixture forces positions and stored sizes so the column is a property of the fixture rather than of
 * Slate. The geometry is stored/estimated, not measured, so the test never claims visual non-overlap.
 * Every Apply case proves the existing nodes, positions, defaults and links are preserved and that the only
 * additions are the planned nodes and the requested link deltas. The fixture is transient and is retired by
 * a scoped cleanup on every return.
 */
namespace
{
	constexpr int32 EstimatedWidth = 320;
	constexpr int32 EstimatedHeight = 160;
	constexpr int32 HorizontalGap = 160;
	constexpr int32 VerticalGap = 64;
	constexpr int32 VerticalStride = EstimatedHeight + VerticalGap;

	struct FFixture
	{
		UPackage* Package = nullptr;
		UBlueprint* Blueprint = nullptr;
		UEdGraph* Empty = nullptr;
		UEdGraph* Populated = nullptr;
		UEdGraph* Negative = nullptr;
		UEdGraph* Estimate = nullptr;
		UEdGraph* MixedWide = nullptr;
		UEdGraph* MixedTall = nullptr;
		UEdGraph* Overflow = nullptr;
		UEdGraph* OverflowRight = nullptr;
		UEdGraph* OverflowBottom = nullptr;
		UEdGraph* Boundary = nullptr;
		UEdGraph* BoundaryHigh = nullptr;
		UEdGraph* ApplyTarget = nullptr;
		UEdGraph* Legacy = nullptr;
		UEdGraphNode_Comment* ApplyComment = nullptr;
		UK2Node_CallFunction* ApplyA = nullptr;
		UK2Node_CallFunction* ApplyB = nullptr;
		UK2Node_CallFunction* ApplyC = nullptr;
		UEdGraphPin* SeedBThen = nullptr;
		UEdGraphPin* SeedCExec = nullptr;
		FString AssetPath;
	};

	struct FNodeSnapshot
	{
		FString Guid;
		FString Text;
	};

	struct FGraphSnapshot
	{
		TArray<FNodeSnapshot> Nodes;
		TSet<FString> Links;
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
			auto RetireGraph = [&Retire](UEdGraph* Graph)
			{
				if (Graph == nullptr)
				{
					return;
				}
				for (UEdGraphNode* Node : Graph->Nodes)
				{
					Retire(Node);
				}
				Retire(Graph);
			};

			if (Fixture.Blueprint != nullptr)
			{
				Retire(Fixture.Blueprint->GeneratedClass.Get());
				Retire(Fixture.Blueprint->SkeletonGeneratedClass.Get());
				Retire(Fixture.Blueprint);
			}
			RetireGraph(Fixture.Empty);
			RetireGraph(Fixture.Populated);
			RetireGraph(Fixture.Negative);
			RetireGraph(Fixture.Estimate);
			RetireGraph(Fixture.MixedWide);
			RetireGraph(Fixture.MixedTall);
			RetireGraph(Fixture.Overflow);
			RetireGraph(Fixture.OverflowRight);
			RetireGraph(Fixture.OverflowBottom);
			RetireGraph(Fixture.Boundary);
			RetireGraph(Fixture.BoundaryHigh);
			RetireGraph(Fixture.ApplyTarget);
			RetireGraph(Fixture.Legacy);
			if (Fixture.Package != nullptr)
			{
				Fixture.Package->SetDirtyFlag(false);
				Retire(Fixture.Package);
			}
		}

		FFixture& Fixture;
	};

	void ForceGuid(UEdGraphNode* InNode, uint32 InA, uint32 InB = 0, uint32 InC = 0, uint32 InD = 0)
	{
		if (InNode == nullptr)
		{
			return;
		}
		InNode->CreateNewGuid();
		InNode->NodeGuid = FGuid(InA, InB, InC, InD);
	}

	UEdGraph* AddPage(UBlueprint* InBlueprint, const TCHAR* InName)
	{
		UEdGraph* Graph = FBlueprintEditorUtils::CreateNewGraph(
			InBlueprint, FName(InName), UEdGraph::StaticClass(), UEdGraphSchema_K2::StaticClass());
		if (Graph != nullptr)
		{
			InBlueprint->UbergraphPages.Add(Graph);
		}
		return Graph;
	}

	UK2Node_CallFunction* PlacePrintString(UEdGraph* InGraph, uint32 InGuid, int32 InX, int32 InY,
		int32 InWidth = 0, int32 InHeight = 0)
	{
		UK2Node_CallFunction* Node = NewObject<UK2Node_CallFunction>(InGraph);
		if (Node == nullptr)
		{
			return nullptr;
		}
		UFunction* Function = UKismetSystemLibrary::StaticClass()->FindFunctionByName(TEXT("PrintString"));
		if (Function == nullptr)
		{
			return nullptr;
		}
		Node->SetFromFunction(Function);
		Node->NodePosX = InX;
		Node->NodePosY = InY;
		InGraph->AddNode(Node, /*bUserAction=*/false, /*bSelectNewNode=*/false);
		ForceGuid(Node, InGuid);
		Node->AllocateDefaultPins();
		if (InWidth > 0)
		{
			Node->NodeWidth = InWidth;
		}
		if (InHeight > 0)
		{
			Node->NodeHeight = InHeight;
		}
		return Node;
	}

	UEdGraphNode_Comment* PlaceComment(UEdGraph* InGraph, uint32 InGuid, int32 InX, int32 InY,
		int32 InWidth, int32 InHeight)
	{
		UEdGraphNode_Comment* Node = NewObject<UEdGraphNode_Comment>(InGraph);
		if (Node == nullptr)
		{
			return nullptr;
		}
		Node->NodePosX = InX;
		Node->NodePosY = InY;
		Node->NodeWidth = InWidth;
		Node->NodeHeight = InHeight;
		InGraph->AddNode(Node, /*bUserAction=*/false, /*bSelectNewNode=*/false);
		ForceGuid(Node, InGuid);
		Node->AllocateDefaultPins();
		return Node;
	}

	FString PinTypeText(const UEdGraphPin* InPin)
	{
		const FString SubObject = InPin->PinType.PinSubCategoryObject != nullptr
			? InPin->PinType.PinSubCategoryObject->GetPathName() : FString();
		return FString::Printf(TEXT("%s|%s|%s container=%d"),
			*InPin->PinType.PinCategory.ToString(), *InPin->PinType.PinSubCategory.ToString(),
			*SubObject, static_cast<int32>(InPin->PinType.ContainerType));
	}

	FString LinkKey(const UEdGraphPin* InA, const UEdGraphPin* InB)
	{
		auto Endpoint = [](const UEdGraphPin* Pin)
		{
			const UEdGraphNode* Node = Pin != nullptr ? Pin->GetOwningNode() : nullptr;
			const FString Guid = Node != nullptr ? Node->NodeGuid.ToString(EGuidFormats::DigitsLower) : FString();
			return FString::Printf(TEXT("%s|%s|%s|%d"), *Guid,
				Pin != nullptr ? *Pin->PinId.ToString(EGuidFormats::DigitsLower) : TEXT(""),
				Pin != nullptr ? *Pin->PinName.ToString() : TEXT(""),
				Pin != nullptr && Pin->Direction == EGPD_Input ? 1 : 0);
		};
		const FString First = Endpoint(InA);
		const FString Second = Endpoint(InB);
		return First < Second ? First + TEXT(" <-> ") + Second : Second + TEXT(" <-> ") + First;
	}

	FGraphSnapshot CaptureGraph(const UEdGraph* InGraph)
	{
		FGraphSnapshot Snapshot;
		if (InGraph == nullptr)
		{
			return Snapshot;
		}
		for (const UEdGraphNode* Node : InGraph->Nodes)
		{
			if (Node == nullptr)
			{
				continue;
			}
			FNodeSnapshot NodeSnapshot;
			NodeSnapshot.Guid = Node->NodeGuid.ToString(EGuidFormats::DigitsLower);
			FString Text = FString::Printf(TEXT("class=%s pos=%d,%d size=%d,%d"),
				*Node->GetClass()->GetName(), Node->NodePosX, Node->NodePosY,
				Node->NodeWidth, Node->NodeHeight);
			for (int32 PinIndex = 0; PinIndex < Node->Pins.Num(); ++PinIndex)
			{
				const UEdGraphPin* Pin = Node->Pins[PinIndex];
				if (Pin == nullptr)
				{
					continue;
				}
				const FString DefaultObject = Pin->DefaultObject != nullptr
					? Pin->DefaultObject->GetPathName() : FString();
				Text += FString::Printf(
					TEXT("\n pin=%s id=%s dir=%d index=%d type=%s default='%s' object='%s' text='%s'"),
					*Pin->PinName.ToString(), *Pin->PinId.ToString(EGuidFormats::DigitsLower),
					Pin->Direction == EGPD_Input ? 1 : 0, PinIndex, *PinTypeText(Pin),
					*Pin->DefaultValue, *DefaultObject, *Pin->DefaultTextValue.ToString());
			}
			NodeSnapshot.Text = Text;
			Snapshot.Nodes.Add(NodeSnapshot);

			for (const UEdGraphPin* Pin : Node->Pins)
			{
				if (Pin == nullptr)
				{
					continue;
				}
				for (const UEdGraphPin* Linked : Pin->LinkedTo)
				{
					if (Linked != nullptr)
					{
						Snapshot.Links.Add(LinkKey(Pin, Linked));
					}
				}
			}
		}
		return Snapshot;
	}

	bool SameGraph(const FGraphSnapshot& InA, const FGraphSnapshot& InB)
	{
		if (InA.Nodes.Num() != InB.Nodes.Num() || InA.Links.Num() != InB.Links.Num())
		{
			return false;
		}
		for (int32 Index = 0; Index < InA.Nodes.Num(); ++Index)
		{
			if (InA.Nodes[Index].Guid != InB.Nodes[Index].Guid
				|| InA.Nodes[Index].Text != InB.Nodes[Index].Text)
			{
				return false;
			}
		}
		for (const FString& Link : InA.Links)
		{
			if (!InB.Links.Contains(Link))
			{
				return false;
			}
		}
		return true;
	}

	/** Every existing node keeps its exact row and graph order, and every old link survives. */
	bool ExistingPreserved(const FGraphSnapshot& InBefore, const FGraphSnapshot& InAfter,
		TArray<FString>& OutNewGuids)
	{
		TMap<FString, const FNodeSnapshot*> AfterByGuid;
		for (const FNodeSnapshot& Node : InAfter.Nodes)
		{
			AfterByGuid.Add(Node.Guid, &Node);
		}

		TSet<FString> BeforeGuids;
		for (const FNodeSnapshot& Node : InBefore.Nodes)
		{
			BeforeGuids.Add(Node.Guid);
			const FNodeSnapshot* const* Found = AfterByGuid.Find(Node.Guid);
			if (Found == nullptr || (*Found)->Text != Node.Text)
			{
				return false;
			}
		}

		TArray<FString> AfterExistingOrder;
		for (const FNodeSnapshot& Node : InAfter.Nodes)
		{
			if (BeforeGuids.Contains(Node.Guid))
			{
				AfterExistingOrder.Add(Node.Guid);
			}
			else
			{
				OutNewGuids.Add(Node.Guid);
			}
		}
		if (AfterExistingOrder.Num() != InBefore.Nodes.Num())
		{
			return false;
		}
		for (int32 Index = 0; Index < AfterExistingOrder.Num(); ++Index)
		{
			if (AfterExistingOrder[Index] != InBefore.Nodes[Index].Guid)
			{
				return false;
			}
		}

		for (const FString& Link : InBefore.Links)
		{
			if (!InAfter.Links.Contains(Link))
			{
				return false;
			}
		}
		return true;
	}

	bool PlanText(UBlueprint* InBlueprint, UEdGraph* InGraph, const FString& InText,
		bool bRequestPlacement, FTurboMCPGraphAppend::FPlan& OutPlan, FString& OutError)
	{
		OutPlan = FTurboMCPGraphAppend::FPlan();
		return FTurboMCPGraphAppend::Plan(InBlueprint->GetPathName(), InGraph->GetName(), InText,
			OutPlan, OutError, bRequestPlacement);
	}

	bool BuildFixture(FFixture& OutFixture, FString& OutError)
	{
		const FString Suffix = FGuid::NewGuid().ToString(EGuidFormats::DigitsLower);
		const FString Leaf = FString::Printf(TEXT("TurboMCPAppendPlacement_%s"), *Suffix);
		UPackage* Package = CreatePackage(*FString::Printf(TEXT("/Game/Tests/%s"), *Leaf));
		if (Package == nullptr)
		{
			OutError = TEXT("could not create the in-memory package");
			return false;
		}
		Package->SetFlags(RF_Transient);
		OutFixture.Package = Package;

		OutFixture.Blueprint = FKismetEditorUtilities::CreateBlueprint(
			AActor::StaticClass(), Package, FName(*Leaf), BPTYPE_Normal,
			UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass(),
			TEXT("TurboMCPGraphAppendPlacementTest"));
		if (OutFixture.Blueprint == nullptr)
		{
			OutError = TEXT("could not create the in-memory Blueprint");
			return false;
		}
		OutFixture.Blueprint->SetFlags(RF_Transient);
		OutFixture.AssetPath = OutFixture.Blueprint->GetPathName();

		OutFixture.Empty = AddPage(OutFixture.Blueprint, TEXT("EmptyGraph"));
		OutFixture.Populated = AddPage(OutFixture.Blueprint, TEXT("PopulatedGraph"));
		OutFixture.Negative = AddPage(OutFixture.Blueprint, TEXT("NegativeGraph"));
		OutFixture.Estimate = AddPage(OutFixture.Blueprint, TEXT("EstimateGraph"));
		OutFixture.MixedWide = AddPage(OutFixture.Blueprint, TEXT("MixedWideGraph"));
		OutFixture.MixedTall = AddPage(OutFixture.Blueprint, TEXT("MixedTallGraph"));
		OutFixture.Overflow = AddPage(OutFixture.Blueprint, TEXT("OverflowGraph"));
		OutFixture.OverflowRight = AddPage(OutFixture.Blueprint, TEXT("OverflowRightGraph"));
		OutFixture.OverflowBottom = AddPage(OutFixture.Blueprint, TEXT("OverflowBottomGraph"));
		OutFixture.Boundary = AddPage(OutFixture.Blueprint, TEXT("BoundaryGraph"));
		OutFixture.BoundaryHigh = AddPage(OutFixture.Blueprint, TEXT("BoundaryHighGraph"));
		OutFixture.ApplyTarget = AddPage(OutFixture.Blueprint, TEXT("ApplyGraph"));
		OutFixture.Legacy = AddPage(OutFixture.Blueprint, TEXT("LegacyGraph"));
		if (OutFixture.Empty == nullptr || OutFixture.Populated == nullptr
			|| OutFixture.Negative == nullptr || OutFixture.Estimate == nullptr
			|| OutFixture.MixedWide == nullptr || OutFixture.MixedTall == nullptr
			|| OutFixture.Overflow == nullptr || OutFixture.OverflowRight == nullptr
			|| OutFixture.OverflowBottom == nullptr || OutFixture.Boundary == nullptr
			|| OutFixture.BoundaryHigh == nullptr
			|| OutFixture.ApplyTarget == nullptr || OutFixture.Legacy == nullptr)
		{
			OutError = TEXT("could not create the fixture graphs");
			return false;
		}

		// The comment is the rightmost bound, so ignoring comments would move the column left.
		if (PlaceComment(OutFixture.Populated, 0x90000001, 900, -100, 300, 200) == nullptr
			|| PlacePrintString(OutFixture.Populated, 0x90000002, 200, 50, 100, 80) == nullptr
			|| PlacePrintString(OutFixture.Populated, 0x90000003, 600, -200, 200, 120) == nullptr)
		{
			OutError = TEXT("could not build the populated fixture");
			return false;
		}

		if (PlacePrintString(OutFixture.Negative, 0x90000011, -500, -300, 200, 100) == nullptr
			|| PlacePrintString(OutFixture.Negative, 0x90000012, -900, 50, 100, 100) == nullptr)
		{
			OutError = TEXT("could not build the negative fixture");
			return false;
		}

		if (PlacePrintString(OutFixture.Estimate, 0x90000021, 100, 100) == nullptr)
		{
			OutError = TEXT("could not build the estimate fixture");
			return false;
		}

		// A known wide bound with no stored height: width 1200 must stay authoritative.
		if (PlacePrintString(OutFixture.MixedWide, 0x90000023, 100, 100, 1200, 0) == nullptr)
		{
			OutError = TEXT("could not build the mixed-width fixture");
			return false;
		}
		// A known tall bound with no stored width: height 1200 must stay authoritative for its row.
		if (PlacePrintString(OutFixture.MixedTall, 0x90000024, 100, 100, 0, 1200) == nullptr)
		{
			OutError = TEXT("could not build the mixed-height fixture");
			return false;
		}

		if (PlacePrintString(OutFixture.Overflow, 0x90000031, MAX_int32 - 50, 0, 100, 100) == nullptr)
		{
			OutError = TEXT("could not build the overflow fixture");
			return false;
		}
		// The top-left fits, but x+320 would leave int32.
		if (PlacePrintString(OutFixture.OverflowRight, 0x90000032, MAX_int32 - 300, 0, 100, 100) == nullptr)
		{
			OutError = TEXT("could not build the right-overflow fixture");
			return false;
		}
		// The top-left fits, but y+160 would leave int32.
		if (PlacePrintString(OutFixture.OverflowBottom, 0x90000033, 0, MAX_int32 - 100, 100, 100) == nullptr)
		{
			OutError = TEXT("could not build the bottom-overflow fixture");
			return false;
		}

		if (PlacePrintString(OutFixture.Boundary, 0x90000041, MIN_int32, MIN_int32, 100, 100) == nullptr)
		{
			OutError = TEXT("could not build the boundary fixture");
			return false;
		}

		if (PlacePrintString(OutFixture.BoundaryHigh, 0x90000042, MAX_int32 - 1000, 0, 100, 100) == nullptr)
		{
			OutError = TEXT("could not build the upper-boundary fixture");
			return false;
		}

		OutFixture.ApplyComment = PlaceComment(OutFixture.ApplyTarget, 0x90000051, 0, 0, 400, 200);
		OutFixture.ApplyA = PlacePrintString(OutFixture.ApplyTarget, 0x90000052, 100, 100, 200, 100);
		OutFixture.ApplyB = PlacePrintString(OutFixture.ApplyTarget, 0x90000053, 400, 100, 200, 100);
		OutFixture.ApplyC = PlacePrintString(OutFixture.ApplyTarget, 0x90000054, 700, 100, 200, 100);
		if (OutFixture.ApplyComment == nullptr || OutFixture.ApplyA == nullptr
			|| OutFixture.ApplyB == nullptr || OutFixture.ApplyC == nullptr)
		{
			OutError = TEXT("could not build the apply fixture");
			return false;
		}

		OutFixture.SeedBThen = OutFixture.ApplyB->FindPin(TEXT("then"), EGPD_Output);
		OutFixture.SeedCExec = OutFixture.ApplyC->FindPin(TEXT("execute"), EGPD_Input);
		UEdGraphPin* SeedString = OutFixture.ApplyC->FindPin(TEXT("InString"), EGPD_Input);
		const UEdGraphSchema* Schema = OutFixture.ApplyTarget->GetSchema();
		if (Schema == nullptr || OutFixture.SeedBThen == nullptr || OutFixture.SeedCExec == nullptr
			|| SeedString == nullptr)
		{
			OutError = TEXT("the apply fixture pins are missing");
			return false;
		}
		if (!Schema->TryCreateConnection(OutFixture.SeedBThen, OutFixture.SeedCExec))
		{
			OutError = TEXT("the apply fixture link could not be made");
			return false;
		}
        auto* Entry = NewObject<UK2Node_CustomEvent>(OutFixture.ApplyTarget);
        Entry->CustomFunctionName = TEXT("PlacementEntry");
        OutFixture.ApplyTarget->AddNode(Entry, false, false);
        Entry->CreateNewGuid(); Entry->AllocateDefaultPins();
        Entry->NodePosX = -600; Entry->NodePosY = 100;
        if (!Schema->TryCreateConnection(Entry->FindPinChecked(TEXT("then")), OutFixture.ApplyA->FindPinChecked(TEXT("execute"))))
        { OutError = TEXT("could not connect the placement entry"); return false; }
		SeedString->DefaultValue = TEXT("seeded");
		SeedString->DefaultTextValue = FText::FromString(TEXT("seeded text"));

		if (PlacePrintString(OutFixture.Legacy, 0x90000061, 100, 100, 200, 100) == nullptr)
		{
			OutError = TEXT("could not build the legacy fixture");
			return false;
		}

        auto* LegacyEntry = NewObject<UK2Node_CustomEvent>(OutFixture.Legacy);
        LegacyEntry->CustomFunctionName = TEXT("LegacyPlacementEntry");
        OutFixture.Legacy->AddNode(LegacyEntry, false, false);
        LegacyEntry->CreateNewGuid(); LegacyEntry->AllocateDefaultPins();
        LegacyEntry->NodePosX = -600; LegacyEntry->NodePosY = 100;
        if (!OutFixture.Legacy->GetSchema()->TryCreateConnection(LegacyEntry->FindPinChecked(TEXT("then")), OutFixture.Legacy->Nodes[0]->FindPinChecked(TEXT("execute"))))
        { OutError = TEXT("could not connect the legacy placement entry"); return false; }

		return true;
	}

	FString OneForm()
	{
		return TEXT("(Development|PrintString\n    InString \"new\")");
	}

	FString TwoForms()
	{
		return TEXT("(Development|PrintString\n    InString \"one\")\n")
			TEXT("(Development|PrintString\n    InString \"two\")");
	}

    FString ConnectedForms(UEdGraph* Graph, int32 Count)
    {
        // Resolve the real tail after earlier appends rather than reconnecting an
        // occupied output. A missing tail deliberately yields an invalid fixture.
        FString Tail;
        for (UEdGraphNode* Node : Graph->Nodes)
            if (Node && Node->IsA<UK2Node_CallFunction>())
                if (UEdGraphPin* Then = Node->FindPin(TEXT("then"), EGPD_Output))
                    if (Then->LinkedTo.IsEmpty()) Tail = TurboMCPNodeId::MakeForNode(Node, true);
        FString Text;
        for (int32 I = 0; I < Count; ++I)
        {
            const FString Source = I == 0 ? Tail : FString::Printf(TEXT("Connected%d"), I - 1);
            Text += FString::Printf(TEXT("(Development|PrintString\n    ; label Connected%d\n    InString \"connected\"\n    execute (-> %s:then))\n"), I, *Source);
        }
        return Text;
    }

    FString ApplyText(const FString& InAKey, const FString& InBKey)
    {
        // One reachable chain: existing A -> new A -> new B -> existing B -> C.
        return FString::Printf(
            TEXT("(Development|PrintString\n    ; label NewA\n    InString \"new a\"\n    execute (-> %s:then))\n")
            TEXT("(Development|PrintString ; %s\n    execute (-> NewB:then))\n")
            TEXT("(Development|PrintString\n    ; label NewB\n    execute (-> NewA:then))"),
            *InAKey, *InBKey);
    }

	UEdGraphNode* FindNodeByKey(UEdGraph* InGraph, const FString& InKey)
	{
		for (UEdGraphNode* Node : InGraph->Nodes)
		{
			if (Node != nullptr && TurboMCPNodeId::MakeForNode(Node, /*bLongForm=*/true) == InKey)
			{
				return Node;
			}
		}
		return nullptr;
	}
}

static bool CheckPerAnchorPlacement(FAutomationTestBase& Test)
{
    UEdGraph* Graph = NewObject<UEdGraph>(GetTransientPackage());
    TArray<FTurboMCPGraphAppend::FNewNode> Forms;
    TArray<FTurboMCPGraphAppend::FNewLink> Links;
    TArray<UEdGraphNode*> Anchors;
    for (int32 I = 0; I < 4; ++I)
    {
        UEdGraphNode* Anchor = NewObject<UEdGraphNode>(Graph);
        Anchor->CreateNewGuid(); Anchor->NodePosX = 0; Anchor->NodePosY = I * 2000;
        Anchor->NodeWidth = 200; Anchor->NodeHeight = 100;
        Graph->AddNode(Anchor, false, false); Anchors.Add(Anchor);
        FTurboMCPGraphAppend::FNewNode Form;
        Form.Line = I + 1; Form.MeasuredWidth = 200; Form.MeasuredHeight = 100;
        Form.ProbeExecInputs.Add(TEXT("execute")); Forms.Add(Form);
        FTurboMCPGraphAppend::FNewLink Link;
        Link.FromNodeKey = TurboMCPNodeId::MakeForNode(Anchor, true);
        Link.ToFormIndex = I; Link.ToPin = TEXT("execute"); Links.Add(Link);
    }
    TArray<FTurboMCPGraphAppend::FPlannedPosition> Positions, Repeat;
    bool Estimated = false; FString Error;
    if (!Test.TestTrue(TEXT("Four independent anchors plan"), FTurboMCPGraphAppend::PlanPlacement(
        Graph, Forms, Positions, Estimated, Error, &Links, false, true)))
    { Test.AddError(Error); return false; }
    Test.TestEqual(TEXT("Every form gets a position"), Positions.Num(), 4);
    for (const auto& Position : Positions)
    {
        Test.TestTrue(TEXT("Each component stays near its own source"), FMath::Abs(Position.Y - Anchors[Position.FormIndex]->NodePosY) < 500);
        Test.TestEqual(TEXT("Existing source remains fixed"), Anchors[Position.FormIndex]->NodePosY, Position.FormIndex * 2000);
    }
    Test.TestTrue(TEXT("Repeated planning succeeds"), FTurboMCPGraphAppend::PlanPlacement(Graph, Forms, Repeat, Estimated, Error, &Links, false, true));
    if (Repeat.Num() == Positions.Num()) for (int32 I = 0; I < Repeat.Num(); ++I)
    { Test.TestEqual(TEXT("Deterministic X"), Repeat[I].X, Positions[I].X); Test.TestEqual(TEXT("Deterministic Y"), Repeat[I].Y, Positions[I].Y); }
    // Nearby independent sources compete for the same space. Earlier components
    // must reserve their rectangles instead of accepting overlapping placements.
    for (UEdGraphNode* Anchor : Anchors) Anchor->NodePosY = 0;
    Test.TestTrue(TEXT("Nearby independent anchors plan"), FTurboMCPGraphAppend::PlanPlacement(Graph, Forms, Repeat, Estimated, Error, &Links, false, true));
    Test.TestEqual(TEXT("Nearby anchors retain every form"), Repeat.Num(), 4);
    for (int32 I = 0; I < Repeat.Num(); ++I)
        for (int32 J = I + 1; J < Repeat.Num(); ++J)
            Test.TestTrue(TEXT("Independent components do not overlap"),
                Repeat[I].X + 200 <= Repeat[J].X || Repeat[J].X + 200 <= Repeat[I].X
                || Repeat[I].Y + 100 <= Repeat[J].Y || Repeat[J].Y + 100 <= Repeat[I].Y);
    {
        TArray<FTurboMCPGraphAppend::FNewNode> SharedForms;
        SharedForms.Add(Forms[0]); SharedForms.Add(Forms[0]); SharedForms.Add(Forms[0]);
        SharedForms[2].ProbeExecInputs.Reset();
        TArray<FTurboMCPGraphAppend::FNewLink> SharedLinks;
        SharedLinks.Add(Links[0]);
        FTurboMCPGraphAppend::FNewLink Chain;
        Chain.FromFormIndex = 0; Chain.ToFormIndex = 1; Chain.FromPin = TEXT("then"); Chain.ToPin = TEXT("execute");
        SharedForms[0].ProbeExecOutputs.Add(TEXT("then")); SharedLinks.Add(Chain);
        for (int32 Consumer = 0; Consumer < 2; ++Consumer)
        {
            FTurboMCPGraphAppend::FNewLink Data;
            Data.FromFormIndex = 2; Data.ToFormIndex = Consumer;
            Data.FromPin = TEXT("Value"); Data.ToPin = TEXT("Value"); SharedLinks.Add(Data);
        }
        TArray<FTurboMCPGraphAppend::FPlannedPosition> SharedPositions;
        Test.TestTrue(TEXT("Shared pure producer plans as one component"), FTurboMCPGraphAppend::PlanPlacement(Graph, SharedForms, SharedPositions, Estimated, Error, &SharedLinks, false, true));
        Test.TestEqual(TEXT("Shared producer is not duplicated"), SharedPositions.Num(), 3);
        if (SharedPositions.Num() == 3)
            Test.TestTrue(TEXT("Shared producer stays left of both consumers"), SharedPositions[2].X < SharedPositions[0].X && SharedPositions[2].X < SharedPositions[1].X);
    }
    // One shared new dependency joins the groups; two execution sources must not
    // be silently resolved using whichever link happens to appear first.
    FTurboMCPGraphAppend::FNewLink Shared;
    Shared.FromFormIndex = 0; Shared.ToFormIndex = 1; Links.Add(Shared);
    Test.TestTrue(TEXT("Nearby compatible execution sources plan"), FTurboMCPGraphAppend::PlanPlacement(Graph, Forms, Repeat, Estimated, Error, &Links, false, true));
    Anchors[1]->NodePosY = 10000;
    Test.TestFalse(TEXT("Conflicting execution sources refuse"), FTurboMCPGraphAppend::PlanPlacement(Graph, Forms, Repeat, Estimated, Error, &Links, false, true));
    Test.TestTrue(TEXT("Refusal explains the anchor conflict"), Error.Contains(TEXT("multiple fixed execution sources")));
    Test.TestEqual(TEXT("Refusal returns no partial geometry"), Repeat.Num(), 0);
    return !Test.HasAnyErrors();
}

static bool CheckExplicitTailTranslation(FAutomationTestBase& Test)
{
    UEdGraph* Graph = NewObject<UEdGraph>(GetTransientPackage());
    TArray<UEdGraphNode*> Nodes;
    TArray<FString> Allowed;
    for (int32 I = 0; I < 3; ++I)
    {
        UEdGraphNode* Node = NewObject<UEdGraphNode>(Graph);
        Node->CreateNewGuid(); Node->NodePosX = 400 + I * 400;
        Node->NodePosY = I == 2 ? 1000 : 0;
        Node->NodeWidth = 200; Node->NodeHeight = 100;
        Graph->AddNode(Node, false, false);
        Nodes.Add(Node); Allowed.Add(TurboMCPNodeId::MakeForNode(Node, true));
    }
    TArray<FTurboMCPGraphAppend::FPlacementObstacle> NewBounds;
    NewBounds.Add({350, 0, 400, 100});
    TArray<FTurboMCPGraphAppend::FExistingPosition> Moves;
    FString Error;
    if (!Test.TestTrue(TEXT("Explicit tail translation plans"),
        FTurboMCPGraphAppend::PlanTailTranslation(Graph, Allowed, NewBounds, Moves, Error)))
    { Test.AddError(Error); return false; }
    Test.TestEqual(TEXT("Only the obstructing tail moves, not the unrelated allowed lane"), Moves.Num(), 2);
    for (const auto& Move : Moves)
    {
        Test.TestEqual(TEXT("Tail keeps its relative horizontal positions"), Move.NewX - Move.OldX, 510);
        Test.TestEqual(TEXT("Tail keeps its vertical positions"), Move.NewY, Move.OldY);
    }
    Test.TestEqual(TEXT("Translation planning does not mutate the graph"), Nodes[0]->NodePosX, 400);
    Allowed.RemoveAt(1);
    Test.TestFalse(TEXT("Fixed downstream obstruction refuses"),
        FTurboMCPGraphAppend::PlanTailTranslation(Graph, Allowed, NewBounds, Moves, Error));
    Test.TestTrue(TEXT("Refusal names the fixed node"), Error.Contains(TurboMCPNodeId::MakeForNode(Nodes[1], true)));
    Test.TestTrue(TEXT("Refusal leaves no partial movement plan"), Moves.IsEmpty());
    Allowed.Reset();
    Test.TestFalse(TEXT("Empty movement scope does not authorize the graph"),
        FTurboMCPGraphAppend::PlanTailTranslation(Graph, Allowed, NewBounds, Moves, Error));
    return !Test.HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTurboMCPGraphAppendPlacementTest,
	"TurboMCP.GraphAppend.Placement",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTurboMCPGraphAppendPlacementTest::RunTest(const FString& Parameters)
{
    if (!CheckPerAnchorPlacement(*this)) return false;
    if (!CheckExplicitTailTranslation(*this)) return false;

	FFixture Fixture;
	FScopedFixtureCleanup Cleanup(Fixture);
	FString FixtureError;
	if (!BuildFixture(Fixture, FixtureError))
	{
		AddError(FString::Printf(TEXT("the placement fixture could not be built: %s"), *FixtureError));
		return false;
	}

	const FString AKey = TurboMCPNodeId::MakeForNode(Fixture.ApplyA, /*bLongForm=*/true);
	const FString BKey = TurboMCPNodeId::MakeForNode(Fixture.ApplyB, /*bLongForm=*/true);

    // Insertion planning combines created placement and authorized tail movement.
    // It must not relocate the live fixture while searching for space.
    {
        UEdGraph* Graph = NewObject<UEdGraph>(Fixture.Blueprint);
        UEdGraphNode* Source = NewObject<UEdGraphNode>(Graph);
        Source->CreateNewGuid(); Source->NodePosX = 0; Source->NodePosY = 80;
        Source->NodeWidth = 200; Source->NodeHeight = 100; Graph->AddNode(Source, false, false);
        UEdGraphNode* Tail = NewObject<UEdGraphNode>(Graph);
        Tail->CreateNewGuid(); Tail->NodePosX = 400; Tail->NodePosY = 80;
        Tail->NodeWidth = 200; Tail->NodeHeight = 100; Graph->AddNode(Tail, false, false);
        FTurboMCPGraphAppend::FPlan Plan;
        Plan.bInsertion = true; Plan.bPlacementRequested = true;
        Plan.MovableNodeIds.Add(TurboMCPNodeId::MakeForNode(Tail, true));
        FTurboMCPGraphAppend::FNewNode Form;
        Form.MeasuredWidth = 300; Form.MeasuredHeight = 100;
        Form.ProbeExecInputs.Add(TEXT("execute")); Form.ProbeExecOutputs.Add(TEXT("then"));
        Plan.Nodes.Add(Form);
        FTurboMCPGraphAppend::FNewLink Edge;
        Edge.FromNodeKey = TurboMCPNodeId::MakeForNode(Source, true);
        Edge.FromPin = TEXT("then"); Edge.ToFormIndex = 0; Edge.ToPin = TEXT("execute");
        Plan.Links.Add(Edge);
        FString Error;
        const bool Planned = FTurboMCPGraphAppend::PlanInsertion(Graph, Plan, Plan.Links, Error);
        TestTrue(TEXT("Explicit insertion plans an occupied execution lane"), Planned);
        if (Planned)
        {
            TestEqual(TEXT("Insertion plans one new node"), Plan.PlannedPositions.Num(), 1);
            TestEqual(TEXT("Insertion moves only the allowed tail"), Plan.ExistingPositions.Num(), 1);
            if (Plan.PlannedPositions.Num() == 1)
            {
                TestEqual(TEXT("Insertion stays directly after source"), Plan.PlannedPositions[0].X, 360);
                TestEqual(TEXT("Insertion preserves execution lane"), Plan.PlannedPositions[0].Y, 80);
            }
            if (Plan.ExistingPositions.Num() == 1)
                TestEqual(TEXT("Tail clears the inserted footprint and gap"), Plan.ExistingPositions[0].NewX, 820);
        }
        TestEqual(TEXT("Insertion planning keeps live tail fixed"), Tail->NodePosX, 400);
        Plan.MovableNodeIds = {TurboMCPNodeId::MakeForNode(Source, true)};
        TestFalse(TEXT("Insertion cannot authorize moving its own attachment"),
            FTurboMCPGraphAppend::PlanInsertion(Graph, Plan, Plan.Links, Error));
        TestTrue(TEXT("Attachment refusal names the source"), Error.Contains(Edge.FromNodeKey));
    }

    // Comment growth is evaluated on a copy, including existing membership.
    {
        UEdGraph* Graph = NewObject<UEdGraph>(Fixture.Blueprint);
        UEdGraphNode_Comment* Box = NewObject<UEdGraphNode_Comment>(Graph);
        Box->CreateNewGuid(); Box->NodePosX = 0; Box->NodePosY = 0;
        Box->NodeWidth = 600; Box->NodeHeight = 300; Graph->AddNode(Box, false, false);
        UEdGraphNode* Member = NewObject<UEdGraphNode>(Graph);
        Member->CreateNewGuid(); Member->NodePosX = 100; Member->NodePosY = 80;
        Member->NodeWidth = 100; Member->NodeHeight = 60; Graph->AddNode(Member, false, false);
        FTurboMCPGraphAppend::FExistingPosition Move;
        Move.NodeId = TurboMCPNodeId::MakeForNode(Member, true);
        Move.OldX = 100; Move.OldY = 80; Move.NewX = 700; Move.NewY = 80;
        TArray<FTurboMCPGraphAppend::FExistingPosition> Moves{Move};
        TArray<FTurboMCPGraphAppend::FPlacementObstacle> Bounds{{300, 80, 200, 60}};
        FTurboMCPGraphAppend::FCommentGrowth Growth;
        FString Error;
        const FString BoxId = TurboMCPNodeId::MakeForNode(Box, true);
        TestTrue(TEXT("Named comment can grow rightward around its translated member"),
            FTurboMCPGraphAppend::PlanInsertionComment(Graph, BoxId, Moves, Bounds, Growth, Error));
        TestTrue(TEXT("Comment growth covers the moved member"), Growth.NewWidth >= 832);
        TestEqual(TEXT("Comment planning preserves live width"), Box->NodeWidth, 600);
        TestEqual(TEXT("Comment planning preserves live member position"), Member->NodePosX, 100);
        TestFalse(TEXT("Unnamed fixed comment cannot lose its member"),
            FTurboMCPGraphAppend::PlanInsertionComment(Graph, FString(), Moves, Bounds, Growth, Error));
        TestTrue(TEXT("Comment refusal clears growth output"), Growth.NodeId.IsEmpty());
        UEdGraphNode_Comment* Neighbor = NewObject<UEdGraphNode_Comment>(Graph);
        Neighbor->CreateNewGuid(); Neighbor->NodePosX = 650; Neighbor->NodePosY = 0;
        Neighbor->NodeWidth = 300; Neighbor->NodeHeight = 300; Graph->AddNode(Neighbor, false, false);
        TestFalse(TEXT("Rightward growth refuses a fixed neighboring comment"),
            FTurboMCPGraphAppend::PlanInsertionComment(Graph, BoxId, Moves, Bounds, Growth, Error));
        TestTrue(TEXT("Blocked growth names the fixed comment"), Error.Contains(TurboMCPNodeId::MakeForNode(Neighbor, true)));
    }

    // Empty nested frames still carry layout structure even with no node members.
    {
        UEdGraph* Graph = NewObject<UEdGraph>(Fixture.Blueprint);
        auto AddBox = [&](int32 X, int32 Y, int32 Width, int32 Height)
        {
            auto* Box = NewObject<UEdGraphNode_Comment>(Graph);
            Box->CreateNewGuid(); Box->NodePosX = X; Box->NodePosY = Y;
            Box->NodeWidth = Width; Box->NodeHeight = Height;
            Graph->AddNode(Box, false, false); return Box;
        };
        auto* Outer = AddBox(0, 0, 600, 400);
        auto* Inner = AddBox(100, 80, 200, 200);
        FTurboMCPGraphAppend::FExistingPosition Move;
        Move.NodeId = TurboMCPNodeId::MakeForNode(Inner, true);
        Move.OldX = 100; Move.OldY = 80; Move.NewX = 700; Move.NewY = 80;
        TArray<FTurboMCPGraphAppend::FExistingPosition> Moves{Move};
        TArray<FTurboMCPGraphAppend::FPlacementObstacle> Bounds{{300, 80, 100, 60}};
        FTurboMCPGraphAppend::FCommentGrowth Growth;
        FString Error;
        const FString OuterId = TurboMCPNodeId::MakeForNode(Outer, true);
        TestTrue(TEXT("Named outer frame grows around a translated empty nested frame"),
            FTurboMCPGraphAppend::PlanInsertionComment(Graph, OuterId, Moves, Bounds, Growth, Error));
        TestEqual(TEXT("Nested frame padding contributes to outer growth"), Growth.NewWidth, 932);
        TestFalse(TEXT("An empty nested frame cannot escape an unnamed fixed outer frame"),
            FTurboMCPGraphAppend::PlanInsertionComment(Graph, FString(), Moves, Bounds, Growth, Error));
        TestTrue(TEXT("Nested refusal identifies the structural conflict"), Error.Contains(TEXT("nesting")));
        TestEqual(TEXT("Nested planning does not move live frames"), Inner->NodePosX, 100);
        TestEqual(TEXT("Nested planning does not resize live outer frame"), Outer->NodeWidth, 600);
        auto* Neighbor = AddBox(850, 80, 200, 200);
        TestFalse(TEXT("Translated nested frame cannot overlap a fixed neighboring frame"),
            FTurboMCPGraphAppend::PlanInsertionComment(Graph, OuterId, Moves, Bounds, Growth, Error));
        TestTrue(TEXT("Nested collision reports the neighboring frame"), Error.Contains(TurboMCPNodeId::MakeForNode(Neighbor, true)));
    }

	// ---- 1. EMPTY GRAPH -------------------------------------------------------------------------------
	{
		FTurboMCPGraphAppend::FPlan Plan;
		FString Error;
		TestTrue(TEXT("an empty graph plans one node"),
			PlanText(Fixture.Blueprint, Fixture.Empty, OneForm(), true, Plan, Error));
		TestFalse(TEXT("the empty-graph detached execution plan is refused"), Plan.bAcceptable());
		TestTrue(TEXT("the plan records that placement was requested"), Plan.bPlacementRequested);
		TestEqual(TEXT("the empty graph plans one position"), Plan.PlannedPositions.Num(), 1);
		if (Plan.PlannedPositions.Num() == 1)
		{
			TestEqual(TEXT("the first new node starts at x=0"), Plan.PlannedPositions[0].X, 0);
			TestEqual(TEXT("the first new node starts at y=0"), Plan.PlannedPositions[0].Y, 0);
			TestTrue(TEXT("a new node's footprint is an estimate"), Plan.PlannedPositions[0].bEstimatedSize);
		}

		const FGraphSnapshot Before = CaptureGraph(Fixture.Empty);
		FString TwoError;
		FTurboMCPGraphAppend::FPlan TwoPlan;
		TestTrue(TEXT("an empty graph plans two nodes"),
			PlanText(Fixture.Blueprint, Fixture.Empty, TwoForms(), true, TwoPlan, TwoError));
		TestEqual(TEXT("the second form is offset by the vertical stride"),
			TwoPlan.PlannedPositions.Num() == 2 ? TwoPlan.PlannedPositions[1].Y : -1, VerticalStride);
		TestTrue(TEXT("planning two nodes leaves the empty graph unchanged"),
			SameGraph(Before, CaptureGraph(Fixture.Empty)));
	}

	// ---- 2. POPULATED GRAPH, INCLUDING A COMMENT AS THE RIGHTMOST BOUND --------------------------------
	{
		const FGraphSnapshot Before = CaptureGraph(Fixture.Populated);
		FTurboMCPGraphAppend::FPlan Plan;
		FString Error;
		TestTrue(TEXT("a populated graph plans two nodes"),
			PlanText(Fixture.Blueprint, Fixture.Populated, TwoForms(), true, Plan, Error));
		TestFalse(TEXT("the populated detached execution plan is refused"), Plan.bAcceptable());
		TestEqual(TEXT("the populated graph plans two positions"), Plan.PlannedPositions.Num(), 2);
		if (Plan.PlannedPositions.Num() == 2)
		{
			// Comment right edge 900+300=1200, so the column starts at 1200+160.
			TestEqual(TEXT("the column starts right of the comment bound"),
				Plan.PlannedPositions[0].X, 1200 + HorizontalGap);
			TestEqual(TEXT("the column starts at the topmost existing edge"),
				Plan.PlannedPositions[0].Y, -200);
			TestEqual(TEXT("the second new node follows the vertical stride"),
				Plan.PlannedPositions[1].Y, -200 + VerticalStride);
		}
		TestTrue(TEXT("planning leaves every existing position and link unchanged"),
			SameGraph(Before, CaptureGraph(Fixture.Populated)));
		TestTrue(TEXT("the populated plan reports estimated new-node sizes when preflight refuses"),
			Plan.bPlacementUsedEstimatedSize);
	}

	// ---- 3. NEGATIVE COORDINATES ----------------------------------------------------------------------
	{
		FTurboMCPGraphAppend::FPlan Plan;
		FString Error;
		TestTrue(TEXT("a graph with negative coordinates plans"),
			PlanText(Fixture.Blueprint, Fixture.Negative, TwoForms(), true, Plan, Error));
		TestEqual(TEXT("the negative graph plans two positions"), Plan.PlannedPositions.Num(), 2);
		if (Plan.PlannedPositions.Num() == 2)
		{
			// Rightmost edge -500+200=-300, so x=-300+160=-140; topmost y=-300.
			TestEqual(TEXT("the column starts right of the negative bound"),
				Plan.PlannedPositions[0].X, -300 + HorizontalGap);
			TestEqual(TEXT("the column top is the negative top edge"),
				Plan.PlannedPositions[0].Y, -300);
			TestEqual(TEXT("the second negative position follows the stride"),
				Plan.PlannedPositions[1].Y, -300 + VerticalStride);
		}
	}

	// ---- 4. NON-POSITIVE STORED SIZE USES THE EXPLICIT ESTIMATE ----------------------------------------
	{
		FTurboMCPGraphAppend::FPlan Plan;
		FString Error;
		TestTrue(TEXT("a node without a stored size plans"),
			PlanText(Fixture.Blueprint, Fixture.Estimate, OneForm(), true, Plan, Error));
		TestEqual(TEXT("the estimate graph plans one position"), Plan.PlannedPositions.Num(), 1);
		if (Plan.PlannedPositions.Num() == 1)
		{
			TestEqual(TEXT("the estimated width plus the gap places the column"),
				Plan.PlannedPositions[0].X, 100 + EstimatedWidth + HorizontalGap);
			TestEqual(TEXT("the estimated node keeps the existing top"),
				Plan.PlannedPositions[0].Y, 100);
		}
		TestTrue(TEXT("the plan reports that an estimated size was used"),
			Plan.bPlacementUsedEstimatedSize);
	}

	// ---- 4b. MIXED STORED DIMENSIONS ARE PRESERVED INDEPENDENTLY --------------------------------------
	{
		FTurboMCPGraphAppend::FPlan WidePlan;
		FString WideError;
		TestTrue(TEXT("a width-only stored bound plans"),
			PlanText(Fixture.Blueprint, Fixture.MixedWide, OneForm(), true, WidePlan, WideError));
		TestEqual(TEXT("the mixed-width graph plans one position"), WidePlan.PlannedPositions.Num(), 1);
		if (WidePlan.PlannedPositions.Num() == 1)
		{
			// Width 1200 is authoritative; the missing height is estimated but does not shrink the column.
			TestEqual(TEXT("a known wide bound is respected even when its height is missing"),
				WidePlan.PlannedPositions[0].X, 100 + 1200 + HorizontalGap);
			TestEqual(TEXT("the mixed-width column keeps the existing top"),
				WidePlan.PlannedPositions[0].Y, 100);
		}
		TestTrue(TEXT("the mixed-width plan reports the estimated height"),
			WidePlan.bPlacementUsedEstimatedSize);

		FTurboMCPGraphAppend::FPlan TallPlan;
		FString TallError;
		TestTrue(TEXT("a height-only stored bound plans"),
			PlanText(Fixture.Blueprint, Fixture.MixedTall, OneForm(), true, TallPlan, TallError));
		TestEqual(TEXT("the mixed-height graph plans one position"), TallPlan.PlannedPositions.Num(), 1);
		if (TallPlan.PlannedPositions.Num() == 1)
		{
			TestEqual(TEXT("a known height does not change the column x"),
				TallPlan.PlannedPositions[0].X, 100 + EstimatedWidth + HorizontalGap);
			TestEqual(TEXT("the mixed-height column keeps the existing top"),
				TallPlan.PlannedPositions[0].Y, 100);
		}
		TestTrue(TEXT("the mixed-height plan reports the estimated width"),
			TallPlan.bPlacementUsedEstimatedSize);
	}

	// ---- 5. DETERMINISTIC REPEAT ----------------------------------------------------------------------
	{
		FTurboMCPGraphAppend::FPlan First;
		FTurboMCPGraphAppend::FPlan Second;
		FString FirstError;
		FString SecondError;
		TestTrue(TEXT("the first deterministic plan is built"),
			PlanText(Fixture.Blueprint, Fixture.Populated, TwoForms(), true, First, FirstError));
		TestTrue(TEXT("the second deterministic plan is built"),
			PlanText(Fixture.Blueprint, Fixture.Populated, TwoForms(), true, Second, SecondError));
		bool bSame = First.PlannedPositions.Num() == Second.PlannedPositions.Num();
		for (int32 Index = 0; bSame && Index < First.PlannedPositions.Num(); ++Index)
		{
			bSame = First.PlannedPositions[Index].FormIndex == Second.PlannedPositions[Index].FormIndex
				&& First.PlannedPositions[Index].X == Second.PlannedPositions[Index].X
				&& First.PlannedPositions[Index].Y == Second.PlannedPositions[Index].Y;
		}
		TestTrue(TEXT("two plans from the same graph and text are identical"), bSame);
	}

	// ---- 6. OVERFLOW AND THE VALID INT32 BOUNDARY -----------------------------------------------------
	{
		auto RefusesOverflow = [&](const TCHAR* InLabel, UEdGraph* InGraph)
		{
			const FString Label(InLabel);
			const FGraphSnapshot Before = CaptureGraph(InGraph);
			FTurboMCPGraphAppend::FPlan Plan;
			FString Error;
			TestTrue(*FString::Printf(TEXT("%s: the plan is built"), *Label),
				PlanText(Fixture.Blueprint, InGraph, OneForm(), true, Plan, Error));
			TestTrue(*FString::Printf(TEXT("%s: the plan is refused"), *Label), !Plan.bAcceptable());
			TestEqual(*FString::Printf(TEXT("%s: one refusal"), *Label), Plan.Refusals.Num(), 1);
			TestTrue(*FString::Printf(TEXT("%s: the refusal names placement"), *Label),
				Plan.Refusals.Num() == 1 && Plan.Refusals[0].Contains(TEXT("placement")));
			TestEqual(*FString::Printf(TEXT("%s: no positions"), *Label), Plan.PlannedPositions.Num(), 0);
			TestTrue(*FString::Printf(TEXT("%s: nothing changed"), *Label),
				SameGraph(Before, CaptureGraph(InGraph)));
		};

		RefusesOverflow(TEXT("overflowing column"), Fixture.Overflow);
		RefusesOverflow(TEXT("right-edge footprint overflow"), Fixture.OverflowRight);
		RefusesOverflow(TEXT("bottom-edge footprint overflow"), Fixture.OverflowBottom);

		FTurboMCPGraphAppend::FPlan BoundaryPlan;
		FString BoundaryError;
		TestTrue(TEXT("the valid boundary plan is built"),
			PlanText(Fixture.Blueprint, Fixture.Boundary, OneForm(), true, BoundaryPlan, BoundaryError));
		TestFalse(TEXT("the valid boundary detached execution plan is refused"), BoundaryPlan.bAcceptable());
		TestEqual(TEXT("the valid boundary plan has one position"), BoundaryPlan.PlannedPositions.Num(), 1);
		if (BoundaryPlan.PlannedPositions.Num() == 1)
		{
			TestEqual(TEXT("INT32_MIN plus the gap is a legal x"),
				BoundaryPlan.PlannedPositions[0].X,
				static_cast<int32>(static_cast<int64>(MIN_int32) + 100 + HorizontalGap));
			TestEqual(TEXT("INT32_MIN is a legal y"),
				BoundaryPlan.PlannedPositions[0].Y, MIN_int32);
		}

		FTurboMCPGraphAppend::FPlan HighPlan;
		FString HighError;
		TestTrue(TEXT("the valid upper-boundary plan is built"),
			PlanText(Fixture.Blueprint, Fixture.BoundaryHigh, OneForm(), true, HighPlan, HighError));
		TestFalse(TEXT("the valid upper-boundary detached execution plan is refused"), HighPlan.bAcceptable());
		TestEqual(TEXT("the valid upper-boundary plan has one position"), HighPlan.PlannedPositions.Num(), 1);
		if (HighPlan.PlannedPositions.Num() == 1)
		{
			TestEqual(TEXT("a large but representable x is legal"),
				HighPlan.PlannedPositions[0].X,
				static_cast<int32>(static_cast<int64>(MAX_int32) - 1000 + 100 + HorizontalGap));
			TestEqual(TEXT("the upper-boundary plan keeps the existing top"),
				HighPlan.PlannedPositions[0].Y, 0);
		}
	}

	// ---- 6b. REQUESTED-POSITION GUARDS REFUSE BEFORE ANY MUTATION -------------------------------------
	{
		FTurboMCPGraphAppend::FPlan Base;
		FString BaseError;
		TestTrue(TEXT("the guard fixture plans"),
			PlanText(Fixture.Blueprint, Fixture.ApplyTarget, ApplyText(AKey, BKey), true, Base, BaseError));
		TestTrue(TEXT("the guard plan is acceptable"), Base.bAcceptable());
		TestEqual(TEXT("the guard plan has two positions"), Base.PlannedPositions.Num(), 2);
		int32 LinkOnlyIndex = INDEX_NONE;
		for (int32 Index = 0; Index < Base.Nodes.Num(); ++Index)
		{
			if (Base.Nodes[Index].bLinksToExisting)
			{
				LinkOnlyIndex = Index;
			}
		}
		TestTrue(TEXT("the guard text contains a link-only form"), LinkOnlyIndex != INDEX_NONE);

		auto RefusesRequestedPlan = [&](const TCHAR* InLabel, const FTurboMCPGraphAppend::FPlan& InPlan)
		{
			const FString Label(InLabel);
			const FGraphSnapshot Before = CaptureGraph(Fixture.ApplyTarget);
			UPackage* Package = Fixture.ApplyTarget->GetOutermost();
			const bool bDirtyBefore = Package != nullptr && Package->IsDirty();
			FString Report;
			FString Error;
			const bool bApplied = FTurboMCPGraphAppend::Apply(InPlan, Report, Error);
			TestTrue(*FString::Printf(TEXT("%s: apply refuses"), *Label), !bApplied);
			TestTrue(*FString::Printf(TEXT("%s: the error names placement"), *Label),
				Error.Contains(TEXT("placement")));
			TestTrue(*FString::Printf(TEXT("%s: the error says nothing was written"), *Label),
				Error.Contains(TEXT("NOTHING")));
			TestTrue(*FString::Printf(TEXT("%s: the graph is unchanged"), *Label),
				SameGraph(Before, CaptureGraph(Fixture.ApplyTarget)));
			TestTrue(*FString::Printf(TEXT("%s: the dirty state is unchanged"), *Label),
				Package != nullptr && Package->IsDirty() == bDirtyBefore);
		};

        const int32 OriginalX = Fixture.ApplyB->NodePosX;
        Fixture.ApplyB->NodePosX += 64;
        RefusesRequestedPlan(TEXT("stale existing geometry"), Base);
        Fixture.ApplyB->NodePosX = OriginalX;
        UEdGraphPin* ChangedDefault = Fixture.ApplyC->FindPinChecked(TEXT("InString"));
        const FString OriginalDefault = ChangedDefault->DefaultValue;
        ChangedDefault->DefaultValue = TEXT("changed after planning");
        RefusesRequestedPlan(TEXT("stale existing default"), Base);
        ChangedDefault->DefaultValue = OriginalDefault;
        const FEdGraphPinType OriginalType = ChangedDefault->PinType;
        ChangedDefault->PinType.bIsReference = !OriginalType.bIsReference;
        RefusesRequestedPlan(TEXT("stale existing pin type"), Base);
        ChangedDefault->PinType = OriginalType;

		if (Base.PlannedPositions.Num() == 2)
		{
			FTurboMCPGraphAppend::FPlan Duplicate = Base;
			Duplicate.PlannedPositions[1].FormIndex = Duplicate.PlannedPositions[0].FormIndex;
			RefusesRequestedPlan(TEXT("duplicate form index"), Duplicate);

			FTurboMCPGraphAppend::FPlan OutOfRange = Base;
			OutOfRange.PlannedPositions[0].FormIndex = OutOfRange.Nodes.Num();
			RefusesRequestedPlan(TEXT("out-of-range form index"), OutOfRange);

			if (LinkOnlyIndex != INDEX_NONE)
			{
				FTurboMCPGraphAppend::FPlan LinkOnly = Base;
				LinkOnly.PlannedPositions[0].FormIndex = LinkOnlyIndex;
				RefusesRequestedPlan(TEXT("link-only form index"), LinkOnly);
			}
		}
	}

	// ---- 7. SUCCESSFUL APPLY: ONLY THE PLANNED NODES AND LINK DELTAS ----------------------------------
	{
		const FGraphSnapshot Before = CaptureGraph(Fixture.ApplyTarget);

		// The snapshot must detect an unrelated position and endpoint change, restored immediately.
		const int32 CommentX = Fixture.ApplyComment->NodePosX;
		Fixture.ApplyComment->NodePosX = CommentX + 40;
		TestTrue(TEXT("an unrelated position change is detectable"),
			!SameGraph(Before, CaptureGraph(Fixture.ApplyTarget)));
		Fixture.ApplyComment->NodePosX = CommentX;
		TestTrue(TEXT("restoring the position restores the snapshot"),
			SameGraph(Before, CaptureGraph(Fixture.ApplyTarget)));

		UEdGraphPin* SeedString = Fixture.ApplyC->FindPin(TEXT("InString"), EGPD_Input);
		if (SeedString != nullptr)
		{
			const FString OriginalDefault = SeedString->DefaultValue;
			SeedString->DefaultValue = TEXT("changed");
			TestTrue(TEXT("an unrelated default change is detectable"),
				!SameGraph(Before, CaptureGraph(Fixture.ApplyTarget)));
			SeedString->DefaultValue = OriginalDefault;
			TestTrue(TEXT("restoring the default restores the snapshot"),
				SameGraph(Before, CaptureGraph(Fixture.ApplyTarget)));
		}
		else
		{
			AddError(TEXT("the apply fixture has no seeded string pin, so the default-negative check is vacuous"));
		}

		const UEdGraphSchema* Schema = Fixture.ApplyTarget->GetSchema();
		TestTrue(TEXT("the apply graph exposes its schema"), Schema != nullptr);
		if (Schema != nullptr)
		{
			Schema->BreakSinglePinLink(Fixture.SeedBThen, Fixture.SeedCExec);
			TestTrue(TEXT("an unrelated endpoint change is detectable"),
				!SameGraph(Before, CaptureGraph(Fixture.ApplyTarget)));
			Schema->TryCreateConnection(Fixture.SeedBThen, Fixture.SeedCExec);
			TestTrue(TEXT("restoring the endpoint restores the snapshot"),
				SameGraph(Before, CaptureGraph(Fixture.ApplyTarget)));
		}

		FTurboMCPGraphAppend::FPlan Plan;
		FString PlanError;
		TestTrue(TEXT("the apply plan is built"),
			PlanText(Fixture.Blueprint, Fixture.ApplyTarget, ApplyText(AKey, BKey), true, Plan, PlanError));
		TestTrue(TEXT("the apply plan is acceptable"), Plan.bAcceptable());
        AddInfo(TEXT("Append before capture: ") + UTurboMCPLookToolset::LookTurboMCPCapture(Fixture.AssetPath, Fixture.ApplyTarget->GetName(), TEXT("graph"), TEXT(""), TEXT("1600x900")));
		TestEqual(TEXT("the apply plan creates two nodes"), Plan.PlannedPositions.Num(), 2);
		TestEqual(TEXT("the apply text describes three links"), Plan.Links.Num(), 3);

		FString Report;
		FString ApplyError;
		TestTrue(TEXT("apply succeeds"), FTurboMCPGraphAppend::Apply(Plan, Report, ApplyError));
		TestTrue(TEXT("the apply report starts with changed"), Report.StartsWith(TEXT("changed:")));
		TestTrue(TEXT("the apply report carries placement evidence"), Report.Contains(TEXT("placement:")));
		TestTrue(TEXT("the apply report carries the geometry caveat"),
			Report.Contains(TEXT("missing existing bounds remain estimated")));

        AddInfo(TEXT("Append after capture: ") + UTurboMCPLookToolset::LookTurboMCPCapture(Fixture.AssetPath, Fixture.ApplyTarget->GetName(), TEXT("graph"), TEXT(""), TEXT("1600x900")));
		const FGraphSnapshot After = CaptureGraph(Fixture.ApplyTarget);
		TArray<FString> NewGuids;
		TestTrue(TEXT("apply preserved every existing node, its order, pins, defaults and links"),
			ExistingPreserved(Before, After, NewGuids));
		TestEqual(TEXT("apply created exactly the planned nodes"), NewGuids.Num(), 2);

		// Map each planned form to the created node at its planned position.
		TSet<FString> BeforeGuids;
		for (const FNodeSnapshot& Snapshot : Before.Nodes)
		{
			BeforeGuids.Add(Snapshot.Guid);
		}
		TMap<int32, UEdGraphNode*> CreatedByForm;
		for (const FTurboMCPGraphAppend::FPlannedPosition& Position : Plan.PlannedPositions)
		{
			for (UEdGraphNode* Node : Fixture.ApplyTarget->Nodes)
			{
				const bool bNewNode = Node != nullptr
					&& !BeforeGuids.Contains(Node->NodeGuid.ToString(EGuidFormats::DigitsLower));
				if (bNewNode && Node->NodePosX == Position.X && Node->NodePosY == Position.Y)
				{
					CreatedByForm.Add(Position.FormIndex, Node);
					break;
				}
			}
		}
		TestEqual(TEXT("every planned position identifies a created node"), CreatedByForm.Num(), 2);

		// Exactly the planned link deltas, with no additional rewiring.
		TSet<FString> ExpectedLinks;
		for (const FTurboMCPGraphAppend::FNewLink& Link : Plan.Links)
		{
			UEdGraphNode* From = Link.FromFormIndex != INDEX_NONE
				? CreatedByForm.FindRef(Link.FromFormIndex) : FindNodeByKey(Fixture.ApplyTarget, Link.FromNodeKey);
			UEdGraphNode* To = Link.ToFormIndex != INDEX_NONE
				? CreatedByForm.FindRef(Link.ToFormIndex) : FindNodeByKey(Fixture.ApplyTarget, Link.ToNodeKey);
			TestTrue(TEXT("a planned link resolves both ends"), From != nullptr && To != nullptr);
			if (From != nullptr && To != nullptr)
			{
				UEdGraphPin* FromPin = From->FindPin(*Link.FromPin);
				UEdGraphPin* ToPin = To->FindPin(*Link.ToPin);
				TestTrue(TEXT("a planned link resolves both pins"), FromPin != nullptr && ToPin != nullptr);
				if (FromPin != nullptr && ToPin != nullptr)
				{
					ExpectedLinks.Add(LinkKey(FromPin, ToPin));
				}
			}
		}
		TSet<FString> NewLinks;
		for (const FString& Link : After.Links)
		{
			if (!Before.Links.Contains(Link))
			{
				NewLinks.Add(Link);
			}
		}
		int32 MismatchedLinks = 0;
		for (const FString& Link : ExpectedLinks)
		{
			if (!NewLinks.Contains(Link))
			{
				++MismatchedLinks;
			}
		}
		for (const FString& Link : NewLinks)
		{
			if (!ExpectedLinks.Contains(Link))
			{
				++MismatchedLinks;
			}
		}
		TestTrue(TEXT("apply made exactly the requested link deltas"),
			NewLinks.Num() == ExpectedLinks.Num() && MismatchedLinks == 0);

		if (CreatedByForm.Num() == 2)
		{
			for (const FTurboMCPGraphAppend::FPlannedPosition& Position : Plan.PlannedPositions)
			{
				const UEdGraphNode* Node = CreatedByForm.FindRef(Position.FormIndex);
				TestTrue(TEXT("each created node is at its planned position"),
					Node != nullptr && Node->NodePosX == Position.X && Node->NodePosY == Position.Y);
			}
			for (const TPair<int32, UEdGraphNode*>& Pair : CreatedByForm)
			{
				TestTrue(TEXT("the report names each created node and its coordinates"),
					Report.Contains(TurboMCPNodeId::MakeForNode(Pair.Value)));
			}
		}
	}

	// ---- 8. A REQUESTED PLAN WITH MALFORMED POSITIONS REFUSES BEFORE MUTATION --------------------------
	{
		const FGraphSnapshot Before = CaptureGraph(Fixture.Legacy);
		FTurboMCPGraphAppend::FPlan Plan;
		FString PlanError;
		TestTrue(TEXT("the malformed-plan fixture plans"),
			PlanText(Fixture.Blueprint, Fixture.Legacy, ConnectedForms(Fixture.Legacy, 2), true, Plan, PlanError));
		TestEqual(TEXT("the malformed-plan fixture has two positions"),
			Plan.PlannedPositions.Num(), 2);
		if (Plan.PlannedPositions.Num() == 2)
		{
			Plan.PlannedPositions.RemoveAt(0);
			UPackage* Package = Fixture.Legacy->GetOutermost();
			const bool bDirtyBefore = Package != nullptr && Package->IsDirty();
			FString Report;
			FString ApplyError;
			TestTrue(TEXT("apply returns a failure for the incomplete request"),
				!FTurboMCPGraphAppend::Apply(Plan, Report, ApplyError));
			TestTrue(TEXT("the failure names placement and says nothing was written"),
				ApplyError.Contains(TEXT("placement")) && ApplyError.Contains(TEXT("NOTHING")));
			TestTrue(TEXT("the malformed request changed nothing"),
				SameGraph(Before, CaptureGraph(Fixture.Legacy)));
			TestTrue(TEXT("the malformed request left the dirty state unchanged"),
				Package != nullptr && Package->IsDirty() == bDirtyBefore);
		}
	}

	// ---- 9. LEGACY PATH: NO PLACEMENT REQUEST KEEPS THE ORIGIN PLACEMENT ------------------------------
	{
		const FGraphSnapshot Before = CaptureGraph(Fixture.Legacy);
		FTurboMCPGraphAppend::FPlan Plan;
		FString PlanError;
		TestTrue(TEXT("the legacy plan is built"),
			PlanText(Fixture.Blueprint, Fixture.Legacy, ConnectedForms(Fixture.Legacy, 1), false, Plan, PlanError));
		TestTrue(TEXT("the legacy plan does not request placement"), !Plan.bPlacementRequested);
		TestEqual(TEXT("the legacy plan has no positions"), Plan.PlannedPositions.Num(), 0);

		FString Report;
		FString ApplyError;
		TestTrue(TEXT("the legacy apply succeeds"), FTurboMCPGraphAppend::Apply(Plan, Report, ApplyError));
		TestTrue(TEXT("the legacy report keeps the changed first line"),
			Report.StartsWith(TEXT("changed:")));
		TestTrue(TEXT("the legacy report has no placement section"), !Report.Contains(TEXT("placement:")));

		const FGraphSnapshot After = CaptureGraph(Fixture.Legacy);
		TArray<FString> NewGuids;
		TestTrue(TEXT("the legacy apply preserved existing nodes and links"),
			ExistingPreserved(Before, After, NewGuids));
		TestEqual(TEXT("the legacy apply created one node"), NewGuids.Num(), 1);
		for (UEdGraphNode* Node : Fixture.Legacy->Nodes)
		{
			if (Node != nullptr && NewGuids.Contains(Node->NodeGuid.ToString(EGuidFormats::DigitsLower)))
			{
				TestEqual(TEXT("a legacy-created node stays at x=0"), Node->NodePosX, 0);
				TestEqual(TEXT("a legacy-created node stays at y=0"), Node->NodePosY, 0);
			}
		}
	}

	// ---- 10. PREVIEW EVIDENCE AND REFUSAL RENDERING ---------------------------------------------------
	{
		FTurboMCPGraphAppend::FPlan Plan;
		FString Error;
		TestTrue(TEXT("the preview plan is built"),
			PlanText(Fixture.Blueprint, Fixture.Legacy, ConnectedForms(Fixture.Legacy, 1), true, Plan, Error));
		const FString Preview = FTurboMCPGraphAppend::RenderPreview(Plan);
		TestTrue(TEXT("the preview keeps the preview first line"), Preview.StartsWith(TEXT("preview:")));
		TestTrue(TEXT("the preview reports placement"), Preview.Contains(TEXT("placement:")));
		TestTrue(TEXT("the preview reports the geometry caveat"),
			Preview.Contains(TEXT("scratch-measured")) && Preview.Contains(TEXT("estimated fallback geometry")));

		FTurboMCPGraphAppend::FPlan Refused;
		FString RefusedError;
		TestTrue(TEXT("the overflow preview plan is built"),
			PlanText(Fixture.Blueprint, Fixture.Overflow, OneForm(), true, Refused, RefusedError));
		const FString RefusedPreview = FTurboMCPGraphAppend::RenderPreview(Refused);
		TestTrue(TEXT("a placement refusal keeps the unchanged first line"),
			RefusedPreview.StartsWith(TEXT("unchanged:")));
		TestTrue(TEXT("a placement refusal names the reason"),
			RefusedPreview.Contains(TEXT("placement")));
	}

	AddInfo(FString::Printf(TEXT("the placement fixture %s is transient and never saved"), *Fixture.AssetPath));
    // Direct geometry regression: a connected chain must use nearby space above
    // a tall obstruction, never drift thousands of units below it. Supplied sizes
    // are deliberate geometry inputs; this case does not claim Slate measurement.
    {
        UEdGraph* Graph = Fixture.Negative;
        UK2Node_CallFunction* Anchor = PlacePrintString(Graph, 0x91000001, 0, 0, 200, 100);
        UEdGraphNode_Comment* Obstacle = PlaceComment(Graph, 0x91000002, -2000, -20, 10000, 4000);
        // Populate the obstructed region with distinct nodes. The local search
        // must remain independent of distant graph density and preserve them all.
        for (int32 Index = 0; Index < 80; ++Index)
        {
            PlacePrintString(Graph, 0x92000000 + Index,
                1200 + (Index % 10) * 500, 400 + (Index / 10) * 400, 200, 100);
        }
        TArray<FTurboMCPGraphAppend::FNewNode> Forms;
        Forms.SetNum(2);
        for (auto& Form : Forms)
        {
            Form.MeasuredWidth = 200; Form.MeasuredHeight = 100;
            Form.ProbeExecInputs.Add(TEXT("execute"));
            Form.ProbeExecOutputs.Add(TEXT("then"));
        }
        TArray<FTurboMCPGraphAppend::FNewLink> Links;
        FTurboMCPGraphAppend::FNewLink Incoming;
        Incoming.FromNodeKey = TurboMCPNodeId::MakeForNode(Anchor, true);
        Incoming.FromPin = TEXT("then"); Incoming.ToFormIndex = 0; Incoming.ToPin = TEXT("execute");
        Links.Add(Incoming);
        FTurboMCPGraphAppend::FNewLink Internal;
        Internal.FromFormIndex = 0; Internal.FromPin = TEXT("then");
        Internal.ToFormIndex = 1; Internal.ToPin = TEXT("execute"); Links.Add(Internal);
        const FGraphSnapshot Before = CaptureGraph(Graph);
        TArray<FTurboMCPGraphAppend::FPlannedPosition> Positions;
        bool Estimated = false; FString Error;
        TestTrue(TEXT("Connected chain finds local space above obstruction"),
            FTurboMCPGraphAppend::PlanPlacement(Graph, Forms, Positions, Estimated, Error, &Links));
        TestEqual(TEXT("Both chain nodes receive positions"), Positions.Num(), 2);
        for (const auto& Position : Positions)
            TestTrue(TEXT("Chain stays above the obstruction and near its anchor"), Position.Y + 100 + VerticalGap <= -20 && Position.Y > -1500);
        TestTrue(TEXT("Local planning preserves every existing node"), SameGraph(Before, CaptureGraph(Graph)));
        Obstacle->NodePosX = -20000; Obstacle->NodePosY = -20000;
        Obstacle->NodeWidth = 40000; Obstacle->NodeHeight = 40000;
        TestFalse(TEXT("Fully blocked neighbourhood refuses remote fallback"),
            FTurboMCPGraphAppend::PlanPlacement(Graph, Forms, Positions, Estimated, Error, &Links));
        TestTrue(TEXT("Blocked placement explains how to proceed"), Error.Contains(TEXT("Free space")));
        TestEqual(TEXT("Refusal emits no partial positions"), Positions.Num(), 0);
        const FGraphSnapshot BlockedBefore=CaptureGraph(Graph);
        TestTrue(TEXT("Explicit outside mode escapes blocked neighbourhood"),
            FTurboMCPGraphAppend::PlanPlacement(Graph,Forms,Positions,Estimated,Error,&Links,true));
        TestEqual(TEXT("Outside positions both nodes"),Positions.Num(),2);
        for (const auto& Position:Positions) TestTrue(TEXT("Outside clears obstruction"),Position.X>=20000+HorizontalGap);
        if (Positions.Num()==2) TestTrue(TEXT("Dependency chain remains left to right"),Positions[1].X>=Positions[0].X+200+HorizontalGap);
        TestTrue(TEXT("Outside planning preserves existing graph"),SameGraph(BlockedBefore,CaptureGraph(Graph)));
        Obstacle->NodePosX=MAX_int32-10;Obstacle->NodeWidth=100;
        TestFalse(TEXT("Outside refuses coordinate overflow"),
            FTurboMCPGraphAppend::PlanPlacement(Graph,Forms,Positions,Estimated,Error,&Links,true));
        FTurboMCPGraphAppend::FPlan BadPlan;
        TestFalse(TEXT("Unknown placement mode refuses before graph lookup"),FTurboMCPGraphAppend::Plan(TEXT(""),TEXT(""),TEXT(""),BadPlan,Error,true,TEXT("auto"),TEXT("guess")));
        TestTrue(TEXT("Unknown mode guidance"),Error.Contains(TEXT("local, outside, per-anchor or insert")));

    }

    // Public workflow: use a reachable existing flow, not an isolated geometry
    // array, so scratch measurement and live link application are both exercised.
    {
        UEdGraph* Graph=Fixture.Legacy;
        PlaceComment(Graph,0x93000001,-3000,-3000,6000,6000);
        const FString Text=ConnectedForms(Graph,2);
        const FGraphSnapshot Before=CaptureGraph(Graph);
        TMap<UEdGraphNode*,FIntPoint> Existing;
        for (UEdGraphNode* Node:Graph->Nodes) if (Node) Existing.Add(Node,FIntPoint(Node->NodePosX,Node->NodePosY));
        Fixture.Package->SetDirtyFlag(false);
        const FString Local=UTurboMCPPreviewToolset::PreviewTurboMCPGraphAppend(Fixture.AssetPath,Graph->GetName(),Text);
        TestTrue(TEXT("Crowded public local preview refuses"),Local.Contains(TEXT("no collision-free local space")));
        const FString Preview=UTurboMCPPreviewToolset::PreviewTurboMCPGraphAppend(Fixture.AssetPath,Graph->GetName(),Text,TEXT("auto"),TEXT("outside"));AddInfo(Preview);
        TestTrue(TEXT("Outside preview describes placement"),Preview.Contains(TEXT("placement: outside")));
        TestTrue(TEXT("Public preview preserves graph"),SameGraph(Before,CaptureGraph(Graph)));
        TestFalse(TEXT("Outside preview clean"),Fixture.Package->IsDirty());
        const FString Applied=UTurboMCPEditToolset::EditTurboMCPGraphAppend(Fixture.AssetPath,Graph->GetName(),Text,TEXT("auto"),TEXT("outside"));AddInfo(Applied);
        TestTrue(TEXT("Outside public edit changes graph"),Applied.StartsWith(TEXT("changed")));
        TestEqual(TEXT("Two appended nodes"),Graph->Nodes.Num(),Existing.Num()+2);
        for (const auto& Item:Existing) TestTrue(TEXT("Existing position unchanged"),Item.Key->NodePosX==Item.Value.X && Item.Key->NodePosY==Item.Value.Y);
        for (UEdGraphNode* Node:Graph->Nodes) if (Node && !Existing.Contains(Node))
        {
            TestTrue(TEXT("Measured node outside existing bounds"),Node->NodePosX>=3000+HorizontalGap);
            UEdGraphPin* Input=Node->FindPin(TEXT("execute"),EGPD_Input);
            TestTrue(TEXT("Appended execution input linked"),Input && Input->LinkedTo.Num()==1);
        }
        const FString Capture=UTurboMCPLookToolset::LookTurboMCPCapture(Fixture.AssetPath,Graph->GetName(),TEXT("graph"),TEXT(""),TEXT("1600x900"));AddInfo(Capture);
        TestTrue(TEXT("Outside placement capture generated"),Capture.Contains(TEXT("image:")));
        const FString Detail=UTurboMCPLookToolset::LookTurboMCPCapture(Fixture.AssetPath,Graph->GetName(),TEXT("region"),TEXT("2900,-3250,1400,800"),TEXT("1600x900"));AddInfo(Detail);
        TestTrue(TEXT("Outside detail capture generated"),Detail.Contains(TEXT("image:")));
        for (const TCHAR* Tool:{TEXT("PreviewTurboMCPGraphAppend"),TEXT("EditTurboMCPGraphAppend")})
        {
            const FString Help=UTurboMCPReadToolset::ReadTurboMCPToolHelp(FString(Tool).StartsWith(TEXT("Preview"))?TEXT("TurboMCP.TurboMCPPreviewToolset"):TEXT("TurboMCP.TurboMCPEditToolset"),Tool);
            AddInfo(Help);
            TestTrue(TEXT("Append help exposes placement choice"),Help.Contains(TEXT("Placement")) && Help.Contains(TEXT("outside")));
        }
    }

    // Exercise the public per-anchor path, including scratch measurement and live links.
    {
        UEdGraph* Graph = Fixture.Legacy;
        TArray<UK2Node_CustomEvent*> Sources;
        FString Text;
        for (int32 I = 0; I < 2; ++I)
        {
            auto* Source = NewObject<UK2Node_CustomEvent>(Graph);
            Source->CustomFunctionName = FName(*FString::Printf(TEXT("IndependentPlacement%d"), I));
            Graph->AddNode(Source, false, false);
            Source->CreateNewGuid(); Source->AllocateDefaultPins();
            Source->NodePosX = 12000; Source->NodePosY = I * 2000;
            Sources.Add(Source);
            Text += FString::Printf(TEXT("(Development|PrintString\n    InString \"anchor %d\"\n    execute (-> %s:then))\n"),
                I, *TurboMCPNodeId::MakeForNode(Source, true));
        }
        const FGraphSnapshot Before = CaptureGraph(Graph);
        TMap<UEdGraphNode*, FIntPoint> Existing;
        for (UEdGraphNode* Node : Graph->Nodes) if (Node) Existing.Add(Node, FIntPoint(Node->NodePosX, Node->NodePosY));
        Fixture.Package->SetDirtyFlag(false);
        const FString Preview = UTurboMCPPreviewToolset::PreviewTurboMCPGraphAppend(Fixture.AssetPath, Graph->GetName(), Text, TEXT("auto"), TEXT("per-anchor"));
        AddInfo(Preview);
        TestTrue(TEXT("Public per-anchor preview reports mode"), Preview.Contains(TEXT("placement: per-anchor")));
        TestTrue(TEXT("Per-anchor preview preserves graph"), SameGraph(Before, CaptureGraph(Graph)));
        TestFalse(TEXT("Per-anchor preview leaves package clean"), Fixture.Package->IsDirty());
        const FString Applied = UTurboMCPEditToolset::EditTurboMCPGraphAppend(Fixture.AssetPath, Graph->GetName(), Text, TEXT("auto"), TEXT("per-anchor"));
        AddInfo(Applied);
        TestTrue(TEXT("Public per-anchor apply succeeds"), Applied.StartsWith(TEXT("changed")));
        TestEqual(TEXT("Per-anchor adds exactly two nodes"), Graph->Nodes.Num(), Existing.Num() + 2);
        for (const auto& Item : Existing)
            TestTrue(TEXT("Per-anchor keeps existing positions"), Item.Key->NodePosX == Item.Value.X && Item.Key->NodePosY == Item.Value.Y);
        for (UK2Node_CustomEvent* Source : Sources)
        {
            UEdGraphPin* Output = Source->FindPinChecked(TEXT("then"));
            TestEqual(TEXT("Each source has its own appended consumer"), Output->LinkedTo.Num(), 1);
            if (Output->LinkedTo.Num() == 1)
            {
                UEdGraphNode* Consumer = Output->LinkedTo[0]->GetOwningNode();
                TestFalse(TEXT("Consumer is newly created"), Existing.Contains(Consumer));
                const FString Coordinates = FString::Printf(TEXT("at (%d,%d), footprint="), Consumer->NodePosX, Consumer->NodePosY);
                TestTrue(TEXT("Applied coordinates appear in public preview"), Preview.Contains(Coordinates));
                TestTrue(TEXT("Consumer stays near its source"), FMath::Abs(Consumer->NodePosY - Source->NodePosY) < 1000);
            }
        }
        const FString Capture = UTurboMCPLookToolset::LookTurboMCPCapture(Fixture.AssetPath, Graph->GetName(), TEXT("region"), TEXT("11800,-300,2400,3000"), TEXT("1600x900"));
        AddInfo(TEXT("Per-anchor final capture: ") + Capture);
        TestTrue(TEXT("Per-anchor capture generated"), Capture.Contains(TEXT("image:")));
    }


    // Public insertion must apply the reviewed plan once, and reject stale geometry.
    {
        UEdGraph* Graph = AddPage(Fixture.Blueprint, TEXT("InsertionTokenGraph"));
        auto* Source = NewObject<UK2Node_CustomEvent>(Graph);
        Source->CustomFunctionName = TEXT("InsertionTokenEvent");
        Graph->AddNode(Source, false, false);
        Source->CreateNewGuid(); Source->AllocateDefaultPins();
        Source->NodePosX = 0; Source->NodePosY = 0;
        const FString Text = FString::Printf(TEXT("(Development|PrintString\n InString \"inserted\"\n execute (-> %s:then))"),
            *TurboMCPNodeId::MakeForNode(Source, true));
        const FString Seed = UTurboMCPEditToolset::EditTurboMCPGraphAppend(Fixture.AssetPath, Graph->GetName(), Text);
        TestTrue(TEXT("Insertion fixture creates a reachable tail"), Seed.StartsWith(TEXT("changed")));
        UEdGraphPin* Output = Source->FindPinChecked(TEXT("then"));
        if (Output->LinkedTo.Num() == 1)
        {
            UEdGraphNode* Tail = Output->LinkedTo[0]->GetOwningNode();
            Tail->NodePosX = 400; Tail->NodePosY = 0;
            auto* Container = NewObject<UEdGraphNode_Comment>(Graph);
            Container->CreateNewGuid(); Container->NodePosX = -80; Container->NodePosY = -80;
            Container->NodeWidth = 900; Container->NodeHeight = 800;
            Container->NodeComment = TEXT("Insertion workflow"); Graph->AddNode(Container, false, false);
            const FString ContainerId = TurboMCPNodeId::MakeForNode(Container, true);
            auto* Unrelated = NewObject<UK2Node_CustomEvent>(Graph);
            Unrelated->CustomFunctionName = TEXT("UnrelatedInsertionLane");
            Graph->AddNode(Unrelated, false, false); Unrelated->CreateNewGuid(); Unrelated->AllocateDefaultPins();
            Unrelated->NodePosX = 0; Unrelated->NodePosY = 2000;
            const FString Scope = FString::Printf(TEXT("[\"%s\"]"), *TurboMCPNodeId::MakeForNode(Tail, true));
            auto Preview = [&]() { return UTurboMCPPreviewToolset::PreviewTurboMCPGraphAppend(
                Fixture.AssetPath, Graph->GetName(), Text, TEXT("auto"), TEXT("insert"), Scope, ContainerId); };
            auto TokenFrom = [](const FString& Report)
            {
                const FString Prefix = TEXT("preview_token=");
                const int32 Start = Report.Find(Prefix);
                return Start == INDEX_NONE ? FString() : Report.Mid(Start + Prefix.Len(), 32);
            };
            auto Apply = [&](const FString& Token) { return UTurboMCPEditToolset::EditTurboMCPGraphAppend(
                Fixture.AssetPath, Graph->GetName(), Text, TEXT("auto"), TEXT("insert"), Scope, ContainerId, Token); };
            const FGraphSnapshot Before = CaptureGraph(Graph);
            const FString PreviewText = Preview(); AddInfo(PreviewText);
            const FString Token = TokenFrom(PreviewText);
            TestFalse(TEXT("Insertion preview issues a token"), Token.IsEmpty());
            TestTrue(TEXT("Insertion preview preserves source graph"), SameGraph(Before, CaptureGraph(Graph)));
            if (!Token.IsEmpty())
            {
                const int32 OldX = Tail->NodePosX;
                const FString Applied = Apply(Token); AddInfo(Applied);
                TestTrue(TEXT("Token-authorized insertion succeeds"), Applied.StartsWith(TEXT("changed")));
                if (Applied.StartsWith(TEXT("changed")))
                {
                    TestTrue(TEXT("Insertion shifts the authorized tail"), Tail->NodePosX > OldX);
                    TestEqual(TEXT("Insertion creates exactly one node"), Graph->Nodes.Num(), 5);
                    TestEqual(TEXT("Insertion keeps unrelated event lane fixed"), Unrelated->NodePosY, 2000);
                    TestEqual(TEXT("Insertion keeps containing frame left fixed"), Container->NodePosX, -80);
                    TestEqual(TEXT("Insertion keeps containing frame top fixed"), Container->NodePosY, -80);
                    const FString Image = UTurboMCPLookToolset::LookTurboMCPCapture(Fixture.AssetPath, Graph->GetName(),
                        TEXT("region"), TEXT("-200,-200,2400,1400"), TEXT("1600x900"));
                    AddInfo(TEXT("Insertion workflow capture: ") + Image);
                    TestTrue(TEXT("Insertion workflow capture generated"), Image.Contains(TEXT("image:")));
                    TestTrue(TEXT("Consumed insertion token refuses a second apply"), Apply(Token).Contains(TEXT("PreviewToken")));
                    TestEqual(TEXT("Token replay creates no node"), Graph->Nodes.Num(), 5);
                    if (TestNotNull(TEXT("Editor available for insertion undo"), GEditor))
                    {
                        GEditor->UndoTransaction();
                        TestTrue(TEXT("One undo restores insertion geometry and connections"), SameGraph(Before, CaptureGraph(Graph)));
                    }
                }
                FTurboMCPGraphAppend::FPlan FailurePlan;
                FString FailureError, FailureReport;
                const TArray<FString> Allowed{TurboMCPNodeId::MakeForNode(Tail, true)};
                const bool FailurePlanned = FTurboMCPGraphAppend::Plan(Fixture.AssetPath, Graph->GetName(), Text,
                    FailurePlan, FailureError, true, TEXT("auto"), TEXT("insert"), Allowed, ContainerId);
                TestTrue(TEXT("Failure fixture obtains an insertion plan"), FailurePlanned && FailurePlan.bAcceptable());
                if (FailurePlanned && FailurePlan.bAcceptable() && !FailurePlan.Nodes.IsEmpty())
                {
                    const FGraphSnapshot BeforeFailure = CaptureGraph(Graph);
                    const bool DirtyBeforeFailure = Fixture.Package->IsDirty();
                    // Simulate a spawner disappearing after preflight. The private
                    // test hook reaches the failure after authorized tail movement.
                    FailurePlan.Nodes[0].TypeId = TEXT("MissingInsertionFixtureType");
                    TestFalse(TEXT("Spawner failure aborts insertion after movement"), FTurboMCPGraphAppend::ExecuteGraph(
                        FailurePlan, Fixture.Blueprint, Graph, FailureReport, FailureError, false, false));
                    AddInfo(FailureError);
                    TestTrue(TEXT("Failed insertion restores graph and comment dimensions"), SameGraph(BeforeFailure, CaptureGraph(Graph)));
                    TestEqual(TEXT("Failed insertion restores dirty state"), Fixture.Package->IsDirty(), DirtyBeforeFailure);
                    TestTrue(TEXT("Failed insertion reports verified restoration"), FailureError.Contains(TEXT("package dirty state restored")));
                }
                const FString StaleToken = TokenFrom(Preview());
                TestFalse(TEXT("Fresh insertion preview issues another token"), StaleToken.IsEmpty());
                if (!StaleToken.IsEmpty())
                {
                    Source->NodePosY += 16;
                    const FGraphSnapshot Changed = CaptureGraph(Graph);
                    TestTrue(TEXT("Changed graph refuses insertion token"), Apply(StaleToken).Contains(TEXT("stale")));
                    TestTrue(TEXT("Stale refusal preserves current graph"), SameGraph(Changed, CaptureGraph(Graph)));
                }
            }
        }
    }

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
