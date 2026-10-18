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
		FString LastError;
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
				return bRetireSucceeded;
			}
			bRetired = true;
			const bool bHadOwnedObject = Fixture.Package != nullptr || Fixture.Blueprint != nullptr;
			bool bSkippedRooted = false;
			auto RetireObject = [&bSkippedRooted](UObject* Object)
			{
				if (Object == nullptr)
				{
					return;
				}
				if (Object->IsRooted())
				{
					bSkippedRooted = true;
					return;
				}
				Object->MarkAsGarbage();
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
			// MarkAsGarbage is retirement, not collection. Rooted objects are
			// reported as a cleanup limit rather than claimed as retired.
			bRetireSucceeded = bHadOwnedObject && !bSkippedRooted;
			return bRetireSucceeded;
		}

		FFixture& Fixture;
		bool bRetired = false;
		bool bRetireSucceeded = false;
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
		if (const UCanvasPanel* RootPanel = Cast<UCanvasPanel>(Tree->RootWidget))
		{
			Out += TEXT("children=");
			for (int32 Index = 0; Index < RootPanel->GetChildrenCount(); ++Index)
			{
				const UWidget* Child = RootPanel->GetChildAt(Index);
				Out += FString::Printf(TEXT("%s%s"), Index == 0 ? TEXT("") : TEXT(","),
					Child != nullptr ? *Child->GetName() : TEXT("none"));
			}
			Out += TEXT("\n");
		}

		TArray<UWidget*> Widgets;
		Tree->GetAllWidgets(Widgets);
		// TArray::Sort dereferences raw pointers before invoking the predicate.
		Widgets.RemoveAll([](const UWidget* Widget) { return Widget == nullptr; });
		Widgets.Sort([](const UWidget& A, const UWidget& B)
		{
			return A.GetName() < B.GetName();
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
			FString Line = FString::Printf(TEXT("%s|%s|parent=%s|variable=%d"),
				*Widget->GetName(), *Widget->GetClass()->GetName(), *ParentName,
				Widget->bIsVariable ? 1 : 0);
			if (const UTextBlock* Text = Cast<UTextBlock>(Widget))
			{
				Line += TEXT("|text=") + Text->GetText().ToString();
			}
			if (const UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Slot))
			{
				const FAnchorData Layout = CanvasSlot->GetLayout();
				Line += FString::Printf(
					TEXT("|offsets=%.17g,%.17g,%.17g,%.17g|anchors=%.17g,%.17g,%.17g,%.17g")
					TEXT("|alignment=%.17g,%.17g|z=%d|auto=%d"),
					static_cast<double>(Layout.Offsets.Left), static_cast<double>(Layout.Offsets.Top),
					static_cast<double>(Layout.Offsets.Right), static_cast<double>(Layout.Offsets.Bottom),
					static_cast<double>(Layout.Anchors.Minimum.X),
					static_cast<double>(Layout.Anchors.Minimum.Y),
					static_cast<double>(Layout.Anchors.Maximum.X),
					static_cast<double>(Layout.Anchors.Maximum.Y),
					static_cast<double>(Layout.Alignment.X), static_cast<double>(Layout.Alignment.Y),
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

	bool ValidateGraphIdentity(UEdGraph* InGraph, FString& OutError)
	{
		if (InGraph == nullptr)
		{
			OutError = TEXT("graph is null");
			return false;
		}
		TSet<FGuid> NodeGuids;
		TSet<FGuid> PinGuids;
		for (const UEdGraphNode* Node : InGraph->Nodes)
		{
			if (Node == nullptr)
			{
				OutError = TEXT("null node");
				return false;
			}
			if (!Node->NodeGuid.IsValid() || NodeGuids.Contains(Node->NodeGuid))
			{
				OutError = FString::Printf(TEXT("node GUID invalid or duplicate: %s"),
					*Node->GetName());
				return false;
			}
			NodeGuids.Add(Node->NodeGuid);
			for (const UEdGraphPin* Pin : Node->Pins)
			{
				if (Pin == nullptr)
				{
					OutError = FString::Printf(TEXT("null pin on %s"), *Node->GetName());
					return false;
				}
				if (!Pin->PinId.IsValid() || PinGuids.Contains(Pin->PinId))
				{
					OutError = FString::Printf(TEXT("pin GUID invalid or duplicate on %s:%s"),
						*Node->GetName(), *Pin->PinName.ToString());
					return false;
				}
				PinGuids.Add(Pin->PinId);
			}
		}
		return true;
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

	/**
	 * A separate disposable Widget fixture for comparator negative controls. It
	 * never touches the acceptance fixture's package, so deliberate comparator
	 * mutations cannot contaminate Plan/Apply clean-state evidence.
	 */
	bool BuildComparatorFixture(FFixture& OutFixture)
	{
		const FString Suffix = FGuid::NewGuid().ToString(EGuidFormats::DigitsLower);
		const FString Leaf = FString::Printf(TEXT("TurboMCPWidgetComparator_%s"), *Suffix);
		OutFixture.Package = CreatePackage(*FString::Printf(TEXT("/Game/Tests/%s"), *Leaf));
		if (OutFixture.Package == nullptr)
		{
			OutFixture.LastError = TEXT("comparator package was not created");
			return false;
		}
		// Never set RF_Transient here: UObjectBaseUtility::MarkPackageDirty returns
		// early for any transient outer and Apply's dirty signal would be hidden.
		// The package is unique and is never saved.
		OutFixture.Blueprint = FWidgetBlueprintOperationUtils::CreateWidgetBlueprint(
			OutFixture.Package, FName(*Leaf), BPTYPE_Normal, UUserWidget::StaticClass(),
			UCanvasPanel::StaticClass(), TEXT("TurboMCPWidgetAppendAcceptanceTest"), false);
		if (OutFixture.Blueprint == nullptr || OutFixture.Blueprint->WidgetTree == nullptr)
		{
			OutFixture.LastError = TEXT("comparator WidgetBlueprint was not created");
			return false;
		}
		OutFixture.AssetPath = OutFixture.Blueprint->GetPathName();
		OutFixture.Tree = OutFixture.Blueprint->WidgetTree;
		OutFixture.Root = Cast<UCanvasPanel>(OutFixture.Tree->RootWidget);
		if (OutFixture.Root == nullptr)
		{
			OutFixture.LastError = TEXT("comparator CanvasPanel root is missing");
			return false;
		}
		// Keep the same construction order as the acceptance fixture: each widget
		// is constructed, added and toggled before the next is constructed.
		FText WidgetError;
		OutFixture.Title = OutFixture.Tree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(), TEXT("ComparatorTitle"));
		if (OutFixture.Title == nullptr
			|| !FWidgetBlueprintOperationUtils::AddWidget(OutFixture.Blueprint, OutFixture.Title,
				OutFixture.Root, 0, WidgetError))
		{
			OutFixture.LastError = TEXT("comparator Title add failed: ") + WidgetError.ToString();
			return false;
		}
		OutFixture.TitleSlot = Cast<UCanvasPanelSlot>(OutFixture.Title->Slot);
		if (OutFixture.TitleSlot == nullptr)
		{
			OutFixture.LastError = TEXT("comparator Title slot is missing");
			return false;
		}
		OutFixture.TitleSlot->SetZOrder(4);
		FWidgetBlueprintOperationUtils::ToggleWidgetAsVariable(OutFixture.Blueprint,
			OutFixture.Title, true);

		OutFixture.Status = OutFixture.Tree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(), TEXT("ComparatorStatus"));
		if (OutFixture.Status == nullptr
			|| !FWidgetBlueprintOperationUtils::AddWidget(OutFixture.Blueprint, OutFixture.Status,
				OutFixture.Root, 1, WidgetError))
		{
			OutFixture.LastError = TEXT("comparator Status add failed: ") + WidgetError.ToString();
			return false;
		}
		OutFixture.StatusSlot = Cast<UCanvasPanelSlot>(OutFixture.Status->Slot);
		if (OutFixture.StatusSlot == nullptr)
		{
			OutFixture.LastError = TEXT("comparator Status slot is missing");
			return false;
		}
		OutFixture.StatusSlot->SetZOrder(1);
		FWidgetBlueprintOperationUtils::ToggleWidgetAsVariable(OutFixture.Blueprint,
			OutFixture.Status, false);
		if (!OutFixture.Blueprint->WidgetVariableNameToGuidMap.Contains(OutFixture.Title->GetFName())
			|| !OutFixture.Blueprint->WidgetVariableNameToGuidMap.Contains(OutFixture.Status->GetFName()))
		{
			OutFixture.LastError = TEXT("comparator widgets did not receive GUIDs before compilation");
			return false;
		}
		{
			TArray<UWidget*> AllWidgets;
			OutFixture.Tree->GetAllWidgets(AllWidgets);
			for (UWidget* Widget : AllWidgets)
			{
				if (Widget != nullptr
					&& !OutFixture.Blueprint->WidgetVariableNameToGuidMap.Contains(Widget->GetFName()))
				{
					OutFixture.Blueprint->WidgetVariableNameToGuidMap.Add(Widget->GetFName(),
						FGuid::NewGuid());
				}
			}
		}
		OutFixture.Target = AddFunctionGraph(OutFixture, TEXT("ComparatorTarget"));
		OutFixture.Sibling = AddFunctionGraph(OutFixture, TEXT("ComparatorSibling"));
		if (OutFixture.Target == nullptr || OutFixture.Sibling == nullptr)
		{
			OutFixture.LastError = TEXT("comparator function graphs were not created");
			return false;
		}
		OutFixture.Entry = FindEntry(OutFixture.Target);
		OutFixture.PrintA = AddPrint(OutFixture.Target, TEXT("control-a"), 100, 100, 0x44440001);
		OutFixture.PrintB = AddPrint(OutFixture.Target, TEXT("control-b"), 420, 100, 0x55550001);
		UK2Node_FunctionEntry* SiblingEntry = FindEntry(OutFixture.Sibling);
		UK2Node_CallFunction* SiblingPrint = AddPrint(OutFixture.Sibling, TEXT("sibling"),
			100, 100, 0x66660001);
		if (OutFixture.Entry == nullptr || OutFixture.PrintA == nullptr
			|| OutFixture.PrintB == nullptr || SiblingEntry == nullptr || SiblingPrint == nullptr)
		{
			OutFixture.LastError = TEXT("comparator graph nodes are missing");
			return false;
		}
		if (!Link(OutFixture.Target, OutFixture.Entry->FindPin(TEXT("then"), EGPD_Output),
				OutFixture.PrintA->FindPin(TEXT("execute"), EGPD_Input))
			|| !Link(OutFixture.Target, OutFixture.PrintA->FindPin(TEXT("then"), EGPD_Output),
				OutFixture.PrintB->FindPin(TEXT("execute"), EGPD_Input))
			|| !Link(OutFixture.Sibling, SiblingEntry->FindPin(TEXT("then"), EGPD_Output),
				SiblingPrint->FindPin(TEXT("execute"), EGPD_Input)))
		{
			OutFixture.LastError = TEXT("comparator seeded links could not be made");
			return false;
		}
		FCompilerResultsLog CompilerLog;
		FKismetEditorUtilities::CompileBlueprint(OutFixture.Blueprint,
			EBlueprintCompileOptions::None, &CompilerLog);
		if (CompilerLog.NumErrors != 0 || !OutFixture.Blueprint->IsUpToDate())
		{
			OutFixture.LastError = FString::Printf(
				TEXT("comparator compilation failed with %d error(s)"), CompilerLog.NumErrors);
			return false;
		}
		return true;
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
	// Keep the package non-transient so Apply can mark it dirty. The unique
	// /Game/Tests path is never saved and no pre-existing asset is touched.
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
	Fixture.AssetPath = Fixture.Blueprint->GetPathName();
	Fixture.Tree = Fixture.Blueprint->WidgetTree;
	Fixture.Root = Cast<UCanvasPanel>(Fixture.Tree->RootWidget);
	if (Fixture.Root == nullptr)
	{
		AddError(TEXT("widget fixture: CanvasPanel root is missing"));
		return false;
	}

	// AddWidget calls MarkBlueprintAsStructurallyModified, which can flush the
	// compilation queue immediately. A widget constructed but not yet added is
	// still visible to the compiler's source sweep, so construct, add and toggle
	// each widget in sequence instead of constructing both before the first add.
	FText WidgetError;
	Fixture.Title = Fixture.Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),
		TEXT("TitleText"));
	if (Fixture.Title == nullptr
		|| !FWidgetBlueprintOperationUtils::AddWidget(Fixture.Blueprint, Fixture.Title,
			Fixture.Root, 0, WidgetError))
	{
		AddError(TEXT("widget fixture: TitleText add failed: ") + WidgetError.ToString());
		return false;
	}
	Fixture.Title->SetText(FText::FromString(TEXT("Title")));
	Fixture.TitleSlot = Cast<UCanvasPanelSlot>(Fixture.Title->Slot);
	if (Fixture.TitleSlot == nullptr)
	{
		AddError(TEXT("widget fixture: TitleText CanvasPanelSlot is missing"));
		return false;
	}
	Fixture.TitleSlot->SetAnchors(FAnchors(0.0f, 0.0f, 0.0f, 0.0f));
	Fixture.TitleSlot->SetOffsets(FMargin(12.0f, 34.0f, 180.0f, 40.0f));
	Fixture.TitleSlot->SetAlignment(FVector2D(0.25, 0.5));
	Fixture.TitleSlot->SetAutoSize(false);
	Fixture.TitleSlot->SetZOrder(3);
	FWidgetBlueprintOperationUtils::ToggleWidgetAsVariable(Fixture.Blueprint, Fixture.Title, true);
	TestTrue(TEXT("TitleText has a GUID before the next widget is constructed"),
		Fixture.Blueprint->WidgetVariableNameToGuidMap.Contains(Fixture.Title->GetFName()));

	Fixture.Status = Fixture.Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),
		TEXT("StatusText"));
	if (Fixture.Status == nullptr
		|| !FWidgetBlueprintOperationUtils::AddWidget(Fixture.Blueprint, Fixture.Status,
			Fixture.Root, 1, WidgetError))
	{
		AddError(TEXT("widget fixture: StatusText add failed: ") + WidgetError.ToString());
		return false;
	}
	Fixture.Status->SetText(FText::FromString(TEXT("Status")));
	Fixture.StatusSlot = Cast<UCanvasPanelSlot>(Fixture.Status->Slot);
	if (Fixture.StatusSlot == nullptr)
	{
		AddError(TEXT("widget fixture: StatusText CanvasPanelSlot is missing"));
		return false;
	}
	Fixture.StatusSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 0.0f));
	Fixture.StatusSlot->SetOffsets(FMargin(0.0f, 120.0f, 0.0f, 24.0f));
	Fixture.StatusSlot->SetAlignment(FVector2D(0.5, 0.0));
	Fixture.StatusSlot->SetAutoSize(false);
	Fixture.StatusSlot->SetZOrder(2);
	FWidgetBlueprintOperationUtils::ToggleWidgetAsVariable(Fixture.Blueprint, Fixture.Status, true);
	TestTrue(TEXT("StatusText has a GUID before compilation"),
		Fixture.Blueprint->WidgetVariableNameToGuidMap.Contains(Fixture.Status->GetFName()));
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
	TestTrue(TEXT("widget fixture compilation status is successful/up to date"),
		Fixture.Blueprint->IsUpToDate());
	if (CompilerLog.NumErrors != 0 || !Fixture.Blueprint->IsUpToDate())
	{
		return false;
	}

	Package->SetDirtyFlag(false);
	const FGraphSnapshot TargetBefore = CaptureGraph(Fixture.Target);
	const FGraphSnapshot SiblingBefore = CaptureGraph(Fixture.Sibling);
	const FString WidgetsBefore = CaptureWidgets(Fixture.Blueprint);
	const bool bDirtyBefore = Package->IsDirty();
	FString IdentityError;
	TestTrue(TEXT("target fixture has unique valid node and pin GUIDs"),
		ValidateGraphIdentity(Fixture.Target, IdentityError));
	TestTrue(TEXT("sibling fixture has unique valid node and pin GUIDs"),
		ValidateGraphIdentity(Fixture.Sibling, IdentityError));
	TestEqual(TEXT("target fixture has the two seeded exec links"),
		TargetBefore.Links.Num(), 2);
	TestEqual(TEXT("sibling fixture has the one seeded exec link"),
		SiblingBefore.Links.Num(), 1);
	TestTrue(TEXT("target fixture contains the seeded entry and PrintString nodes"),
		Fixture.Entry != nullptr && Fixture.PrintA != nullptr && Fixture.PrintB != nullptr
		&& TargetBefore.Nodes.Num() >= 3);
	TestTrue(TEXT("widget fixture preserves the seeded child order"),
		WidgetsBefore.Contains(TEXT("children=TitleText,StatusText")));
	if (TargetBefore.Links.Num() != 2 || SiblingBefore.Links.Num() != 1)
	{
		return false;
	}
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

	// Repeated Plan must remain clean and must not touch either graph.
	FTurboMCPGraphAppend::FPlan RepeatedPlan;
	FString RepeatedError;
	TestTrue(TEXT("a repeated Plan still accepts the intended text"),
		FTurboMCPGraphAppend::Plan(Fixture.AssetPath, Fixture.Target->GetName(), AppendText,
			RepeatedPlan, RepeatedError, /*bRequestPlacement=*/true));
	TestTrue(TEXT("the repeated plan is acceptable"), RepeatedPlan.bAcceptable());
	TestTrue(TEXT("a repeated Plan did not mutate the target graph"),
		SameGraph(TargetBefore, CaptureGraph(Fixture.Target)));
	TestTrue(TEXT("a repeated Plan did not mutate the sibling graph"),
		SameGraph(SiblingBefore, CaptureGraph(Fixture.Sibling)));
	TestEqual(TEXT("a repeated Plan did not mutate the widget tree"),
		CaptureWidgets(Fixture.Blueprint), WidgetsBefore);
	TestFalse(TEXT("a repeated clean Plan did not change the dirty flag"),
		Package->IsDirty());

	const FString RefusalText = TEXT(
		"(Development|PrintString\n    execute (-> DefinitelyMissing:then)\n    InString \"refused\")");

	// Independent fresh fixture: only this Plan can dirty it.
	FFixture RefusalPlanFixture;
	FScopedFixtureCleanup RefusalPlanCleanup(RefusalPlanFixture);
	if (!BuildComparatorFixture(RefusalPlanFixture))
	{
		AddError(TEXT("refusal-plan fixture: ") + RefusalPlanFixture.LastError);
		return false;
	}
	RefusalPlanFixture.Package->SetDirtyFlag(false);
	const FGraphSnapshot RefusalPlanTarget = CaptureGraph(RefusalPlanFixture.Target);
	const FGraphSnapshot RefusalPlanSibling = CaptureGraph(RefusalPlanFixture.Sibling);
	const FString RefusalPlanWidgets = CaptureWidgets(RefusalPlanFixture.Blueprint);
	FTurboMCPGraphAppend::FPlan RefusalPlan;
	FString RefusalError;
	TestTrue(TEXT("the refusal text plans"),
		FTurboMCPGraphAppend::Plan(RefusalPlanFixture.AssetPath,
			RefusalPlanFixture.Target->GetName(), RefusalText, RefusalPlan, RefusalError,
			/*bRequestPlacement=*/true));
	TestFalse(TEXT("the refusal plan is not acceptable"), RefusalPlan.bAcceptable());
	TestTrue(TEXT("the refusal plan records a refusal"), RefusalPlan.Refusals.Num() > 0);
	TestTrue(TEXT("a refused Plan did not mutate the target graph"),
		SameGraph(RefusalPlanTarget, CaptureGraph(RefusalPlanFixture.Target)));
	TestTrue(TEXT("a refused Plan did not mutate the sibling graph"),
		SameGraph(RefusalPlanSibling, CaptureGraph(RefusalPlanFixture.Sibling)));
	TestEqual(TEXT("a refused Plan did not mutate the widget tree"),
		CaptureWidgets(RefusalPlanFixture.Blueprint), RefusalPlanWidgets);
	TestFalse(TEXT("a refused Plan on a fresh fixture left it clean"),
		RefusalPlanFixture.Package->IsDirty());

	// Independent fresh fixture for refused Apply. Plan necessarily runs first;
	// Apply is measured against the dirty state immediately before Apply, so it
	// cannot be blamed for Plan's mutation or contaminated by another case.
	FFixture RefusalApplyFixture;
	FScopedFixtureCleanup RefusalApplyCleanup(RefusalApplyFixture);
	if (!BuildComparatorFixture(RefusalApplyFixture))
	{
		AddError(TEXT("refusal-apply fixture: ") + RefusalApplyFixture.LastError);
		return false;
	}
	RefusalApplyFixture.Package->SetDirtyFlag(false);
	const FGraphSnapshot RefusalApplyTarget = CaptureGraph(RefusalApplyFixture.Target);
	const FGraphSnapshot RefusalApplySibling = CaptureGraph(RefusalApplyFixture.Sibling);
	const FString RefusalApplyWidgets = CaptureWidgets(RefusalApplyFixture.Blueprint);
	FTurboMCPGraphAppend::FPlan RefusalApplyPlan;
	FString RefusalApplyPlanError;
	TestTrue(TEXT("the refusal text plans for Apply"),
		FTurboMCPGraphAppend::Plan(RefusalApplyFixture.AssetPath,
			RefusalApplyFixture.Target->GetName(), RefusalText, RefusalApplyPlan,
			RefusalApplyPlanError, /*bRequestPlacement=*/true));
	TestFalse(TEXT("the refusal Apply plan is not acceptable"), RefusalApplyPlan.bAcceptable());
	const bool bDirtyBeforeRefusedApply = RefusalApplyFixture.Package->IsDirty();
	TestFalse(TEXT("the refused Apply plan left its fresh fixture clean"),
		bDirtyBeforeRefusedApply);
	FString RefusalReport;
	FString RefusalApplyError;
	TestTrue(TEXT("Apply on a refused plan returns its preview without writing"),
		FTurboMCPGraphAppend::Apply(RefusalApplyPlan, RefusalReport, RefusalApplyError));
	TestEqual(TEXT("Apply on a refused plan reports no execution error"),
		RefusalApplyError, FString());
	TestTrue(TEXT("Apply on a refused plan reports unchanged"),
		RefusalReport.StartsWith(TEXT("unchanged:")));
	TestTrue(TEXT("Apply on a refused plan reports the preview changes nothing"),
		RefusalReport.Contains(TEXT("preview changes NOTHING")));
	TestTrue(TEXT("Apply on a refused plan did not mutate the target graph"),
		SameGraph(RefusalApplyTarget, CaptureGraph(RefusalApplyFixture.Target)));
	TestTrue(TEXT("Apply on a refused plan did not mutate the sibling graph"),
		SameGraph(RefusalApplySibling, CaptureGraph(RefusalApplyFixture.Sibling)));
	TestEqual(TEXT("Apply on a refused plan did not mutate the widget tree"),
		CaptureWidgets(RefusalApplyFixture.Blueprint), RefusalApplyWidgets);
	TestEqual(TEXT("Apply on a refused plan did not change the dirty flag"),
		RefusalApplyFixture.Package->IsDirty(), bDirtyBeforeRefusedApply);

	// Negative controls run on a separate disposable Widget fixture, so they
	// cannot contaminate the acceptance fixture's clean Plan/Apply baseline.
	FFixture Comparator;
	FScopedFixtureCleanup ComparatorCleanup(Comparator);
	if (!BuildComparatorFixture(Comparator))
	{
		AddError(TEXT("comparator fixture: ") + Comparator.LastError);
		return false;
	}
	const FGraphSnapshot ComparatorTarget = CaptureGraph(Comparator.Target);
	const FGraphSnapshot ComparatorSibling = CaptureGraph(Comparator.Sibling);
	const FString ComparatorWidgets = CaptureWidgets(Comparator.Blueprint);
	FString ComparatorIdentityError;
	TestTrue(TEXT("comparator fixture has unique valid node and pin GUIDs"),
		ValidateGraphIdentity(Comparator.Target, ComparatorIdentityError));
	TestTrue(TEXT("comparator sibling has unique valid node and pin GUIDs"),
		ValidateGraphIdentity(Comparator.Sibling, ComparatorIdentityError));
	{
		const int32 OriginalX = Comparator.PrintA->NodePosX;
		Comparator.PrintA->NodePosX = OriginalX + 1;
		TestFalse(TEXT("the graph comparator detects a moved node"),
			SameGraph(ComparatorTarget, CaptureGraph(Comparator.Target)));
		Comparator.PrintA->NodePosX = OriginalX;
		TestTrue(TEXT("the moved-node control restores exactly"),
			SameGraph(ComparatorTarget, CaptureGraph(Comparator.Target)));
	}
	{
		UEdGraphPin* InString = Comparator.PrintA->FindPin(TEXT("InString"), EGPD_Input);
		if (InString == nullptr)
		{
			AddError(TEXT("comparator fixture: PrintA InString pin is missing"));
			return false;
		}
		const FString Original = InString->DefaultValue;
		InString->DefaultValue = TEXT("perturbed");
		TestFalse(TEXT("the graph comparator detects a changed default"),
			SameGraph(ComparatorTarget, CaptureGraph(Comparator.Target)));
		InString->DefaultValue = Original;
		TestTrue(TEXT("the changed-default control restores exactly"),
			SameGraph(ComparatorTarget, CaptureGraph(Comparator.Target)));
	}
	{
		UEdGraphPin* Then = Comparator.PrintA->FindPin(TEXT("then"), EGPD_Output);
		UEdGraphPin* Exec = Comparator.PrintB->FindPin(TEXT("execute"), EGPD_Input);
		if (Then == nullptr || Exec == nullptr || Comparator.Target->GetSchema() == nullptr)
		{
			AddError(TEXT("comparator fixture: seeded link pins are missing"));
			return false;
		}
		Comparator.Target->GetSchema()->BreakSinglePinLink(Then, Exec);
		TestFalse(TEXT("the graph comparator detects a removed link"),
			SameGraph(ComparatorTarget, CaptureGraph(Comparator.Target)));
		TestTrue(TEXT("the removed link is restored"),
			Comparator.Target->GetSchema()->TryCreateConnection(Then, Exec));
		TestTrue(TEXT("the restored link matches the baseline"),
			SameGraph(ComparatorTarget, CaptureGraph(Comparator.Target)));
	}
	{
		UEdGraphNode* SiblingNode = Comparator.Sibling->Nodes.Num() > 0
			? Comparator.Sibling->Nodes[0] : nullptr;
		if (SiblingNode == nullptr)
		{
			AddError(TEXT("comparator fixture: sibling node is missing"));
			return false;
		}
		const int32 OriginalY = SiblingNode->NodePosY;
		SiblingNode->NodePosY = OriginalY + 7;
		TestFalse(TEXT("the sibling comparator detects a moved node"),
			SameGraph(ComparatorSibling, CaptureGraph(Comparator.Sibling)));
		SiblingNode->NodePosY = OriginalY;
		TestTrue(TEXT("the sibling control restores exactly"),
			SameGraph(ComparatorSibling, CaptureGraph(Comparator.Sibling)));
	}
	{
		const int32 OriginalZ = Comparator.TitleSlot->GetZOrder();
		Comparator.TitleSlot->SetZOrder(OriginalZ + 1);
		TestFalse(TEXT("the widget comparator detects a changed slot property"),
			CaptureWidgets(Comparator.Blueprint) == ComparatorWidgets);
		Comparator.TitleSlot->SetZOrder(OriginalZ);
		TestEqual(TEXT("the widget slot control restores exactly"),
			CaptureWidgets(Comparator.Blueprint), ComparatorWidgets);
	}
	{
		const FName StatusName = Comparator.Status->GetFName();
		const bool bOriginalVariable = Comparator.Status->bIsVariable;
		const bool bHadGuidEntry = Comparator.Blueprint->WidgetVariableNameToGuidMap.Contains(StatusName);
		const FGuid OriginalGuid = bHadGuidEntry
			? Comparator.Blueprint->WidgetVariableNameToGuidMap.FindChecked(StatusName) : FGuid();
		FWidgetBlueprintOperationUtils::ToggleWidgetAsVariable(Comparator.Blueprint,
			Comparator.Status, !bOriginalVariable);
		TestFalse(TEXT("the widget comparator detects a changed variable flag"),
			CaptureWidgets(Comparator.Blueprint) == ComparatorWidgets);
		FWidgetBlueprintOperationUtils::ToggleWidgetAsVariable(Comparator.Blueprint,
			Comparator.Status, bOriginalVariable);
		if (bHadGuidEntry)
		{
			Comparator.Blueprint->WidgetVariableNameToGuidMap.Add(StatusName, OriginalGuid);
		}
		else
		{
			Comparator.Blueprint->WidgetVariableNameToGuidMap.Remove(StatusName);
		}
		TestEqual(TEXT("the widget variable-flag control restores exactly"),
			CaptureWidgets(Comparator.Blueprint), ComparatorWidgets);
	}
	TestTrue(TEXT("comparator fixture retirement completed"),
		ComparatorCleanup.Retire());

	TestFalse(TEXT("the success fixture is clean immediately before Apply"),
		Package->IsDirty());
	FString Report;
	FString ApplyError;
	TestTrue(TEXT("the real Append Apply succeeds"),
		FTurboMCPGraphAppend::Apply(Plan, Report, ApplyError));
	TestEqual(TEXT("Apply has no error"), ApplyError, FString());
	TestTrue(TEXT("Apply reports changed first"), Report.StartsWith(TEXT("changed:")));
	TestTrue(TEXT("Apply marks the fixture dirty"), Package->IsDirty());

	const FGraphSnapshot TargetAfter = CaptureGraph(Fixture.Target);
	FString PostIdentityError;
	TestTrue(TEXT("Apply leaves unique valid node and pin GUIDs"),
		ValidateGraphIdentity(Fixture.Target, PostIdentityError));
	TestEqual(TEXT("Apply increases the target node count by exactly one"),
		TargetAfter.Nodes.Num(), TargetBefore.Nodes.Num() + 1);
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
			UK2Node_CallFunction* NewPrint = Cast<UK2Node_CallFunction>(NewNode);
			TestTrue(TEXT("the new node is a PrintString call node"),
				NewPrint != nullptr && NewPrint->GetTargetFunction() != nullptr
				&& NewPrint->GetTargetFunction()->GetName() == TEXT("PrintString"));
			UEdGraphPin* InString = NewNode->FindPin(TEXT("InString"), EGPD_Input);
			TestEqual(TEXT("the new PrintString has the intended InString default"),
				InString != nullptr ? InString->DefaultValue : FString(),
				FString(TEXT("appended")));
			UEdGraphPin* Then = Fixture.PrintB->FindPin(TEXT("then"), EGPD_Output);
			UEdGraphPin* Exec = NewNode->FindPin(TEXT("execute"), EGPD_Input);
			TestTrue(TEXT("the intended link exists"),
				Then != nullptr && Exec != nullptr
				&& Then->LinkedTo.Contains(Exec));
		}
	}
	TestTrue(TEXT("the target baseline is no longer equal after Apply"),
		!SameGraph(TargetBefore, TargetAfter));

	// Already-dirty Plan preservation is a separate owned fixture. It never
	// shares state with the success Apply case above.
	FFixture DirtyFixture;
	FScopedFixtureCleanup DirtyCleanup(DirtyFixture);
	if (!BuildComparatorFixture(DirtyFixture))
	{
		AddError(TEXT("already-dirty fixture: ") + DirtyFixture.LastError);
		return false;
	}
	DirtyFixture.Package->SetDirtyFlag(true);
	const FGraphSnapshot DirtyTarget = CaptureGraph(DirtyFixture.Target);
	const FGraphSnapshot DirtySibling = CaptureGraph(DirtyFixture.Sibling);
	const FString DirtyWidgets = CaptureWidgets(DirtyFixture.Blueprint);
	const FString DirtyAppendText = FString::Printf(
		TEXT("(Development|PrintString\n    execute (-> %s:then)\n    InString \"appended\")"),
		*TurboMCPNodeId::MakeForNode(DirtyFixture.PrintB, /*bLongForm=*/true));
	FTurboMCPGraphAppend::FPlan DirtyPlan;
	FString DirtyError;
	TestTrue(TEXT("Plan accepts the intended text on an already-dirty package"),
		FTurboMCPGraphAppend::Plan(DirtyFixture.AssetPath, DirtyFixture.Target->GetName(),
			DirtyAppendText, DirtyPlan, DirtyError, /*bRequestPlacement=*/true));
	TestTrue(TEXT("the already-dirty plan is acceptable"), DirtyPlan.bAcceptable());
	TestTrue(TEXT("Plan preserved the already-dirty state"), DirtyFixture.Package->IsDirty());
	TestTrue(TEXT("an already-dirty Plan did not mutate the target graph"),
		SameGraph(DirtyTarget, CaptureGraph(DirtyFixture.Target)));
	TestTrue(TEXT("an already-dirty Plan did not mutate the sibling graph"),
		SameGraph(DirtySibling, CaptureGraph(DirtyFixture.Sibling)));
	TestEqual(TEXT("an already-dirty Plan did not mutate the widget tree"),
		CaptureWidgets(DirtyFixture.Blueprint), DirtyWidgets);
	TestTrue(TEXT("already-dirty fixture retirement completed"), DirtyCleanup.Retire());

	TestTrue(TEXT("refusal-plan fixture retirement completed"), RefusalPlanCleanup.Retire());
	TestTrue(TEXT("refusal-apply fixture retirement completed"), RefusalApplyCleanup.Retire());
	TestTrue(TEXT("scoped fixture cleanup completes"), Cleanup.Retire());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
