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

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/PanelSlot.h"
#include "Components/TextBlock.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraph/EdGraphSchema.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "GameFramework/Actor.h"
#include "K2Node_CallFunction.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/CompilerResultsLog.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/Guid.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"
#include "WidgetBlueprint.h"
#include "WidgetBlueprintOperationUtils.h"

namespace
{
	struct FFixture
	{
		UPackage* Package = nullptr;
		UWidgetBlueprint* Blueprint = nullptr;
		UWidgetTree* Tree = nullptr;
		UCanvasPanel* Root = nullptr;
		UTextBlock* Title = nullptr;
		UTextBlock* Status = nullptr;
		UCanvasPanelSlot* TitleSlot = nullptr;
		UCanvasPanelSlot* StatusSlot = nullptr;
		UEdGraph* Target = nullptr;
		UEdGraph* Sibling = nullptr;
		UK2Node_CallFunction* PrintA = nullptr;
		UK2Node_CallFunction* PrintB = nullptr;
		UK2Node_FunctionEntry* Entry = nullptr;
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
		~FScopedFixtureCleanup() { Retire(); }

		bool Retire()
		{
			if (bRetired)
			{
				return true;
			}
			bRetired = true;
			auto RetireObject = [](UObject* Object)
			{
				if (Object != nullptr && !Object->IsRooted())
				{
					Object->MarkAsGarbage();
				}
			};
			auto RetireGraph = [&RetireObject](UEdGraph* Graph)
			{
				if (Graph == nullptr)
				{
					return;
				}
				for (UEdGraphNode* Node : Graph->Nodes)
				{
					RetireObject(Node);
				}
				RetireObject(Graph);
			};
			RetireGraph(Fixture.Target);
			RetireGraph(Fixture.Sibling);
			if (Fixture.Blueprint != nullptr)
			{
				RetireObject(Fixture.Blueprint->GeneratedClass.Get());
				RetireObject(Fixture.Blueprint->SkeletonGeneratedClass.Get());
				RetireObject(Fixture.Blueprint);
			}
			if (Fixture.Package != nullptr)
			{
				Fixture.Package->SetDirtyFlag(false);
				RetireObject(Fixture.Package);
			}
			return true;
		}

		FFixture& Fixture;
		bool bRetired = false;
	};

	void ForceGuid(UEdGraphNode* InNode, uint32 InA, uint32 InB = 0,
		uint32 InC = 0, uint32 InD = 0)
	{
		if (InNode != nullptr)
		{
			InNode->CreateNewGuid();
			InNode->NodeGuid = FGuid(InA, InB, InC, InD);
		}
	}

	FString PinTypeText(const UEdGraphPin* InPin)
	{
		return FString::Printf(TEXT("%s|%s|%s"),
			InPin != nullptr ? *InPin->PinType.PinCategory.ToString() : TEXT(""),
			InPin != nullptr ? *InPin->PinType.PinSubCategory.ToString() : TEXT(""),
			InPin != nullptr && InPin->PinType.PinSubCategoryObject != nullptr
				? *InPin->PinType.PinSubCategoryObject->GetPathName() : TEXT(""));
	}

	FString LinkKey(const UEdGraphPin* InA, const UEdGraphPin* InB)
	{
		auto Key = [](const UEdGraphPin* InPin)
		{
			if (InPin == nullptr || InPin->GetOwningNode() == nullptr)
			{
				return FString();
			}
			return FString::Printf(TEXT("%s:%s"),
				*InPin->GetOwningNode()->NodeGuid.ToString(EGuidFormats::DigitsLower),
				*InPin->PinId.ToString(EGuidFormats::DigitsLower));
		};
		const FString A = Key(InA);
		const FString B = Key(InB);
		// Canonical endpoint order: each connection is one entry even though both
		// pins expose it through their reciprocal LinkedTo arrays.
		return A < B ? A + TEXT("->") + B : B + TEXT("->") + A;
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
			FNodeSnapshot Row;
			Row.Guid = Node->NodeGuid.ToString(EGuidFormats::DigitsLower);
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
			Row.Text = Text;
			Snapshot.Nodes.Add(Row);

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
		for (const FNodeSnapshot& Node : InAfter.Nodes)
		{
			if (!BeforeGuids.Contains(Node.Guid))
			{
				OutNewGuids.Add(Node.Guid);
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

	FString CaptureWidgets(const UWidgetBlueprint* InBlueprint)
	{
		if (InBlueprint == nullptr || InBlueprint->WidgetTree == nullptr)
		{
			return TEXT("no-widget-tree");
		}
		const UWidgetTree* Tree = InBlueprint->WidgetTree;
		FString Out = FString::Printf(TEXT("root=%s\n"),
			Tree->RootWidget != nullptr ? *Tree->RootWidget->GetName() : TEXT("none"));

		TArray<UWidget*> Widgets;
		Tree->GetAllWidgets(Widgets);
		Widgets.Sort([](const UWidget* A, const UWidget* B)
		{
			return A != nullptr && B != nullptr && A->GetName() < B->GetName();
		});
		for (const UWidget* Widget : Widgets)
		{
			if (Widget == nullptr)
			{
				continue;
			}
			const UPanelSlot* Slot = Widget->Slot;
			const FString ParentName = Slot != nullptr && Slot->Parent != nullptr
				? Slot->Parent->GetName() : FString();
			FString Line = FString::Printf(TEXT("%s|%s|parent=%s"),
				*Widget->GetName(), *Widget->GetClass()->GetName(), *ParentName);
			if (const UTextBlock* Text = Cast<UTextBlock>(Widget))
			{
				Line += TEXT("|text=") + Text->GetText().ToString();
			}
			if (const UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Slot))
			{
				const FAnchorData Layout = CanvasSlot->GetLayout();
				Line += FString::Printf(
					TEXT("|offsets=%.3f,%.3f,%.3f,%.3f|anchors=%.3f,%.3f,%.3f,%.3f")
					TEXT("|alignment=%.3f,%.3f|z=%d|auto=%d"),
					Layout.Offsets.Left, Layout.Offsets.Top, Layout.Offsets.Right,
					Layout.Offsets.Bottom, Layout.Anchors.Minimum.X, Layout.Anchors.Minimum.Y,
					Layout.Anchors.Maximum.X, Layout.Anchors.Maximum.Y,
					Layout.Alignment.X, Layout.Alignment.Y,
					CanvasSlot->GetZOrder(), CanvasSlot->GetAutoSize() ? 1 : 0);
			}
			Out += Line + TEXT("\n");
		}

		TArray<FName> VariableNames;
		InBlueprint->WidgetVariableNameToGuidMap.GetKeys(VariableNames);
		VariableNames.Sort(FNameLexicalLess());
		for (const FName& Name : VariableNames)
		{
			Out += FString::Printf(TEXT("var=%s|guid=%s\n"), *Name.ToString(),
				*InBlueprint->WidgetVariableNameToGuidMap.FindChecked(Name)
					.ToString(EGuidFormats::DigitsLower));
		}
		return Out;
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
		return Graph;
	}

	UK2Node_FunctionEntry* FindEntry(UEdGraph* InGraph)
	{
		if (InGraph != nullptr)
		{
			for (UEdGraphNode* Node : InGraph->Nodes)
			{
				if (Node != nullptr && Node->IsA<UK2Node_FunctionEntry>())
				{
					return Cast<UK2Node_FunctionEntry>(Node);
				}
			}
		}
		return nullptr;
	}

	UK2Node_CallFunction* AddPrint(UEdGraph* InGraph, const TCHAR* InDefault,
		int32 InX, int32 InY, uint32 InGuid)
	{
		if (InGraph == nullptr)
		{
			return nullptr;
		}
		UK2Node_CallFunction* Node = NewObject<UK2Node_CallFunction>(InGraph);
		if (Node == nullptr)
		{
			return nullptr;
		}
		Node->SetFromFunction(
			UKismetSystemLibrary::StaticClass()->FindFunctionByName(TEXT("PrintString")));
		Node->NodePosX = InX;
		Node->NodePosY = InY;
		InGraph->AddNode(Node, /*bUserAction=*/false, /*bSelectNewNode=*/false);
		Node->AllocateDefaultPins();
		if (UEdGraphPin* InString = Node->FindPin(TEXT("InString"), EGPD_Input))
		{
			InString->DefaultValue = InDefault;
		}
		ForceGuid(Node, InGuid);
		return Node;
	}

	bool Link(UEdGraph* InGraph, UEdGraphPin* InFrom, UEdGraphPin* InTo)
	{
		return InGraph != nullptr && InGraph->GetSchema() != nullptr
			&& InFrom != nullptr && InTo != nullptr
			&& InGraph->GetSchema()->TryCreateConnection(InFrom, InTo);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTurboMCPWidgetAppendAcceptanceTest,
	"TurboMCP.Widget.AppendAcceptance",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTurboMCPWidgetAppendAcceptanceTest::RunTest(const FString& Parameters)
{
	FFixture Fixture;
	FScopedFixtureCleanup Cleanup(Fixture);

	const FString Suffix = FGuid::NewGuid().ToString(EGuidFormats::DigitsLower);
	const FString Leaf = FString::Printf(TEXT("TurboMCPWidgetAppend_%s"), *Suffix);
	UPackage* Package = CreatePackage(*FString::Printf(TEXT("/Game/Tests/%s"), *Leaf));
	if (Package == nullptr)
	{
		AddError(TEXT("widget fixture: package could not be created"));
		return false;
	}
	Package->SetFlags(RF_Transient);
	Fixture.Package = Package;
	Fixture.Blueprint = FWidgetBlueprintOperationUtils::CreateWidgetBlueprint(
		Package, FName(*Leaf), BPTYPE_Normal, UUserWidget::StaticClass(),
		UCanvasPanel::StaticClass(), TEXT("TurboMCPWidgetAppendAcceptanceTest"),
		/*bRegisterAndCompile=*/false);
	if (Fixture.Blueprint == nullptr || Fixture.Blueprint->WidgetTree == nullptr)
	{
		AddError(TEXT("widget fixture: WidgetBlueprint or WidgetTree was not created"));
		return false;
	}
	Fixture.Blueprint->SetFlags(RF_Transient);
	Fixture.AssetPath = Fixture.Blueprint->GetPathName();
	Fixture.Tree = Fixture.Blueprint->WidgetTree;
	Fixture.Root = Cast<UCanvasPanel>(Fixture.Tree->RootWidget);
	if (Fixture.Root == nullptr)
	{
		AddError(TEXT("widget fixture: CanvasPanel root is missing"));
		return false;
	}

	Fixture.Title = Fixture.Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),
		TEXT("TitleText"));
	Fixture.Status = Fixture.Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),
		TEXT("StatusText"));
	if (Fixture.Title == nullptr || Fixture.Status == nullptr)
	{
		AddError(TEXT("widget fixture: TitleText or StatusText could not be constructed"));
		return false;
	}
	Fixture.Title->SetText(FText::FromString(TEXT("Title")));
	Fixture.Status->SetText(FText::FromString(TEXT("Status")));

	FText WidgetError;
	if (!FWidgetBlueprintOperationUtils::AddWidget(Fixture.Blueprint, Fixture.Title,
			Fixture.Root, 0, WidgetError)
		|| !FWidgetBlueprintOperationUtils::AddWidget(Fixture.Blueprint, Fixture.Status,
			Fixture.Root, 1, WidgetError))
	{
		AddError(TEXT("widget fixture: AddWidget failed: ") + WidgetError.ToString());
		return false;
	}
	Fixture.TitleSlot = Cast<UCanvasPanelSlot>(Fixture.Title->Slot);
	Fixture.StatusSlot = Cast<UCanvasPanelSlot>(Fixture.Status->Slot);
	if (Fixture.TitleSlot == nullptr || Fixture.StatusSlot == nullptr)
	{
		AddError(TEXT("widget fixture: CanvasPanelSlot is missing"));
		return false;
	}
	Fixture.TitleSlot->SetAnchors(FAnchors(0.0f, 0.0f, 0.0f, 0.0f));
	Fixture.TitleSlot->SetOffsets(FMargin(12.0f, 34.0f, 180.0f, 40.0f));
	Fixture.TitleSlot->SetAlignment(FVector2D(0.25, 0.5));
	Fixture.TitleSlot->SetAutoSize(false);
	Fixture.TitleSlot->SetZOrder(3);
	Fixture.StatusSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 0.0f));
	Fixture.StatusSlot->SetOffsets(FMargin(0.0f, 120.0f, 0.0f, 24.0f));
	Fixture.StatusSlot->SetAlignment(FVector2D(0.5, 0.0));
	Fixture.StatusSlot->SetAutoSize(false);
	Fixture.StatusSlot->SetZOrder(2);

	FWidgetBlueprintOperationUtils::ToggleWidgetAsVariable(Fixture.Blueprint, Fixture.Title, true);
	FWidgetBlueprintOperationUtils::ToggleWidgetAsVariable(Fixture.Blueprint, Fixture.Status, true);
	{
		TArray<UWidget*> AllWidgets;
		Fixture.Tree->GetAllWidgets(AllWidgets);
		for (UWidget* Widget : AllWidgets)
		{
			if (Widget != nullptr
				&& !Fixture.Blueprint->WidgetVariableNameToGuidMap.Contains(Widget->GetFName()))
			{
				Fixture.Blueprint->WidgetVariableNameToGuidMap.Add(Widget->GetFName(),
					FGuid::NewGuid());
			}
		}
	}

	Fixture.Target = AddFunctionGraph(Fixture, TEXT("AppendTarget"));
	Fixture.Sibling = AddFunctionGraph(Fixture, TEXT("AppendSibling"));
	if (Fixture.Target == nullptr || Fixture.Sibling == nullptr)
	{
		AddError(TEXT("widget fixture: function graphs could not be created"));
		return false;
	}
	Fixture.Entry = FindEntry(Fixture.Target);
	Fixture.PrintA = AddPrint(Fixture.Target, TEXT("first"), 100, 100, 0x11110001);
	Fixture.PrintB = AddPrint(Fixture.Target, TEXT("second"), 420, 100, 0x22220001);
	UK2Node_FunctionEntry* SiblingEntry = FindEntry(Fixture.Sibling);
	UK2Node_CallFunction* SiblingPrint = AddPrint(Fixture.Sibling, TEXT("sibling"), 100, 100, 0x33330001);
	if (Fixture.Entry == nullptr || Fixture.PrintA == nullptr || Fixture.PrintB == nullptr
		|| SiblingEntry == nullptr || SiblingPrint == nullptr)
	{
		AddError(TEXT("widget fixture: graph nodes are missing"));
		return false;
	}
	if (!Link(Fixture.Target, Fixture.Entry->FindPin(TEXT("then"), EGPD_Output),
			Fixture.PrintA->FindPin(TEXT("execute"), EGPD_Input))
		|| !Link(Fixture.Target, Fixture.PrintA->FindPin(TEXT("then"), EGPD_Output),
			Fixture.PrintB->FindPin(TEXT("execute"), EGPD_Input))
		|| !Link(Fixture.Sibling, SiblingEntry->FindPin(TEXT("then"), EGPD_Output),
			SiblingPrint->FindPin(TEXT("execute"), EGPD_Input)))
	{
		AddError(TEXT("widget fixture: seeded links could not be made"));
		return false;
	}

	FCompilerResultsLog CompilerLog;
	FKismetEditorUtilities::CompileBlueprint(Fixture.Blueprint,
		EBlueprintCompileOptions::None, &CompilerLog);
	TestEqual(TEXT("widget fixture compiles without errors"), CompilerLog.NumErrors, 0);
	TestEqual(TEXT("widget fixture compilation status is successful"),
		static_cast<int32>(Fixture.Blueprint->Status), static_cast<int32>(BS_Successful));
	if (CompilerLog.NumErrors != 0 || Fixture.Blueprint->Status != BS_Successful)
	{
		return false;
	}

	Package->SetDirtyFlag(false);
	const FGraphSnapshot TargetBefore = CaptureGraph(Fixture.Target);
	const FGraphSnapshot SiblingBefore = CaptureGraph(Fixture.Sibling);
	const FString WidgetsBefore = CaptureWidgets(Fixture.Blueprint);
	const bool bDirtyBefore = Package->IsDirty();
	if (bDirtyBefore)
	{
		AddError(TEXT("widget fixture: baseline dirty state is not clean"));
		return false;
	}

	const FString AppendText = FString::Printf(
		TEXT("(Development|PrintString\n    execute (-> %s:then)\n    InString \"appended\")"),
		*TurboMCPNodeId::MakeForNode(Fixture.PrintB, /*bLongForm=*/true));
	FTurboMCPGraphAppend::FPlan Plan;
	FString Error;
	TestTrue(TEXT("the real Append Plan accepts the intended text"),
		FTurboMCPGraphAppend::Plan(Fixture.AssetPath, Fixture.Target->GetName(), AppendText,
			Plan, Error, /*bRequestPlacement=*/true));
	TestEqual(TEXT("the append plan has no error"), Error, FString());
	TestTrue(TEXT("the append plan is acceptable"), Plan.bAcceptable());
	TestEqual(TEXT("the append plan creates one node"), Plan.Nodes.Num(), 1);
	TestEqual(TEXT("the append plan creates one link"), Plan.Links.Num(), 1);
	TestEqual(TEXT("the append plan records one position"), Plan.PlannedPositions.Num(), 1);
	if (!Plan.bAcceptable() || Plan.PlannedPositions.Num() != 1)
	{
		return false;
	}
	TestTrue(TEXT("Plan did not mutate the target graph"),
		SameGraph(TargetBefore, CaptureGraph(Fixture.Target)));
	TestTrue(TEXT("Plan did not mutate the sibling graph"),
		SameGraph(SiblingBefore, CaptureGraph(Fixture.Sibling)));
	TestEqual(TEXT("Plan did not mutate the widget tree"),
		CaptureWidgets(Fixture.Blueprint), WidgetsBefore);
	TestEqual(TEXT("Plan did not change the dirty flag"), Package->IsDirty(), bDirtyBefore);

	FTurboMCPGraphAppend::FPlan RefusalPlan;
	FString RefusalError;
	TestTrue(TEXT("the refusal text plans"),
		FTurboMCPGraphAppend::Plan(Fixture.AssetPath, Fixture.Target->GetName(),
			TEXT("(Development|PrintString\n    execute (-> DefinitelyMissing:then)\n    InString \"refused\")"),
			RefusalPlan, RefusalError, /*bRequestPlacement=*/true));
	TestFalse(TEXT("the refusal plan is not acceptable"), RefusalPlan.bAcceptable());
	TestTrue(TEXT("the refusal plan records a refusal"), RefusalPlan.Refusals.Num() > 0);
	TestTrue(TEXT("a refused Plan did not mutate the target graph"),
		SameGraph(TargetBefore, CaptureGraph(Fixture.Target)));
	TestTrue(TEXT("a refused Plan did not mutate the sibling graph"),
		SameGraph(SiblingBefore, CaptureGraph(Fixture.Sibling)));
	TestEqual(TEXT("a refused Plan did not mutate the widget tree"),
		CaptureWidgets(Fixture.Blueprint), WidgetsBefore);
	TestEqual(TEXT("a refused Plan did not change the dirty flag"),
		Package->IsDirty(), bDirtyBefore);

	// Negative controls: deliberately perturb a disposable fixture, prove the
	// comparator detects it, restore, and verify the baseline before Apply.
	{
		const int32 OriginalX = Fixture.PrintA->NodePosX;
		Fixture.PrintA->NodePosX = OriginalX + 1;
		TestFalse(TEXT("the graph comparator detects a moved node"),
			SameGraph(TargetBefore, CaptureGraph(Fixture.Target)));
		Fixture.PrintA->NodePosX = OriginalX;
		TestTrue(TEXT("the moved-node control restores exactly"),
			SameGraph(TargetBefore, CaptureGraph(Fixture.Target)));
	}
	{
		UEdGraphPin* InString = Fixture.PrintA->FindPin(TEXT("InString"), EGPD_Input);
		const FString Original = InString != nullptr ? InString->DefaultValue : FString();
		if (InString == nullptr)
		{
			AddError(TEXT("widget fixture: PrintA InString pin is missing"));
			return false;
		}
		InString->DefaultValue = TEXT("perturbed");
		TestFalse(TEXT("the graph comparator detects a changed default"),
			SameGraph(TargetBefore, CaptureGraph(Fixture.Target)));
		InString->DefaultValue = Original;
		TestTrue(TEXT("the changed-default control restores exactly"),
			SameGraph(TargetBefore, CaptureGraph(Fixture.Target)));
	}
	{
		UEdGraphPin* Then = Fixture.PrintA->FindPin(TEXT("then"), EGPD_Output);
		UEdGraphPin* Exec = Fixture.PrintB->FindPin(TEXT("execute"), EGPD_Input);
		if (Then == nullptr || Exec == nullptr || Fixture.Target->GetSchema() == nullptr)
		{
			AddError(TEXT("widget fixture: seeded link pins are missing"));
			return false;
		}
		Fixture.Target->GetSchema()->BreakSinglePinLink(Then, Exec);
		TestFalse(TEXT("the graph comparator detects a removed link"),
			SameGraph(TargetBefore, CaptureGraph(Fixture.Target)));
		TestTrue(TEXT("the removed link is restored"),
			Fixture.Target->GetSchema()->TryCreateConnection(Then, Exec));
		TestTrue(TEXT("the restored link matches the baseline"),
			SameGraph(TargetBefore, CaptureGraph(Fixture.Target)));
	}
	{
		UEdGraphNode* SiblingNode = Fixture.Sibling->Nodes.Num() > 0
			? Fixture.Sibling->Nodes[0] : nullptr;
		if (SiblingNode == nullptr)
		{
			AddError(TEXT("widget fixture: sibling node is missing"));
			return false;
		}
		const int32 OriginalY = SiblingNode->NodePosY;
		SiblingNode->NodePosY = OriginalY + 7;
		TestFalse(TEXT("the sibling comparator detects a moved node"),
			SameGraph(SiblingBefore, CaptureGraph(Fixture.Sibling)));
		SiblingNode->NodePosY = OriginalY;
		TestTrue(TEXT("the sibling control restores exactly"),
			SameGraph(SiblingBefore, CaptureGraph(Fixture.Sibling)));
	}
	{
		const int32 OriginalZ = Fixture.TitleSlot->GetZOrder();
		Fixture.TitleSlot->SetZOrder(OriginalZ + 1);
		TestFalse(TEXT("the widget comparator detects a changed slot property"),
			CaptureWidgets(Fixture.Blueprint) == WidgetsBefore);
		Fixture.TitleSlot->SetZOrder(OriginalZ);
		TestEqual(TEXT("the widget slot control restores exactly"),
			CaptureWidgets(Fixture.Blueprint), WidgetsBefore);
	}

	FString Report;
	FString ApplyError;
	TestTrue(TEXT("the real Append Apply succeeds"),
		FTurboMCPGraphAppend::Apply(Plan, Report, ApplyError));
	TestEqual(TEXT("Apply has no error"), ApplyError, FString());
	TestTrue(TEXT("Apply reports changed first"), Report.StartsWith(TEXT("changed:")));
	TestTrue(TEXT("Apply marks the fixture dirty"), Package->IsDirty());

	const FGraphSnapshot TargetAfter = CaptureGraph(Fixture.Target);
	TArray<FString> NewGuids;
	TestTrue(TEXT("Apply preserves every existing target node"),
		ExistingPreserved(TargetBefore, TargetAfter, NewGuids));
	TestEqual(TEXT("Apply adds exactly one node"), NewGuids.Num(), 1);
	TestTrue(TEXT("Apply preserves the sibling graph"),
		SameGraph(SiblingBefore, CaptureGraph(Fixture.Sibling)));
	TestEqual(TEXT("Apply preserves the widget tree and slots"),
		CaptureWidgets(Fixture.Blueprint), WidgetsBefore);
	TestEqual(TEXT("Apply adds exactly one link"),
		TargetAfter.Links.Num(), TargetBefore.Links.Num() + 1);
	if (NewGuids.Num() == 1)
	{
		UEdGraphNode* NewNode = nullptr;
		for (UEdGraphNode* Node : Fixture.Target->Nodes)
		{
			if (Node != nullptr
				&& Node->NodeGuid.ToString(EGuidFormats::DigitsLower) == NewGuids[0])
			{
				NewNode = Node;
				break;
			}
		}
		TestTrue(TEXT("the new node exists"), NewNode != nullptr);
		if (NewNode != nullptr)
		{
			TestEqual(TEXT("the new node is at the planned X"), NewNode->NodePosX,
				Plan.PlannedPositions[0].X);
			TestEqual(TEXT("the new node is at the planned Y"), NewNode->NodePosY,
				Plan.PlannedPositions[0].Y);
			UEdGraphPin* Then = Fixture.PrintB->FindPin(TEXT("then"), EGPD_Output);
			UEdGraphPin* Exec = NewNode->FindPin(TEXT("execute"), EGPD_Input);
			TestTrue(TEXT("the intended link exists"),
				Then != nullptr && Exec != nullptr
				&& Then->LinkedTo.Contains(Exec));
		}
	}
	TestTrue(TEXT("the target baseline is no longer equal after Apply"),
		!SameGraph(TargetBefore, TargetAfter));

	TestTrue(TEXT("scoped fixture cleanup completes"), Cleanup.Retire());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
