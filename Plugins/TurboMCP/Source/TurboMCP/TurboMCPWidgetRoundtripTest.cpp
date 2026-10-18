// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Misc/AutomationTest.h"

#include "TurboMCPGraphAppend.h"
#include "TurboMCPGraphText.h"
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
		TArray<const UEdGraphNode*> Objects;
		TSet<FString> Links;
	};

	struct FWidgetIdentity
	{
		const UWidgetTree* Tree = nullptr;
		const UWidget* Root = nullptr;
		TArray<const UWidget*> Widgets;
		TArray<const UPanelSlot*> Slots;
		TArray<const UObject*> Parents;
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
			Snapshot.Objects.Add(Node);
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
		if (InA.Nodes.Num() != InB.Nodes.Num() || InA.Links.Num() != InB.Links.Num()
			|| InA.Objects.Num() != InB.Objects.Num())
		{
			return false;
		}
		for (int32 Index = 0; Index < InA.Nodes.Num(); ++Index)
		{
			if (InA.Nodes[Index].Guid != InB.Nodes[Index].Guid
				|| InA.Nodes[Index].Text != InB.Nodes[Index].Text
				|| InA.Objects[Index] != InB.Objects[Index])
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
		TMap<FString, int32> AfterByGuid;
		for (int32 Index = 0; Index < InAfter.Nodes.Num(); ++Index)
		{
			AfterByGuid.Add(InAfter.Nodes[Index].Guid, Index);
		}
		TSet<FString> BeforeGuids;
		for (int32 BeforeIndex = 0; BeforeIndex < InBefore.Nodes.Num(); ++BeforeIndex)
		{
			const FNodeSnapshot& Node = InBefore.Nodes[BeforeIndex];
			BeforeGuids.Add(Node.Guid);
			const int32* Found = AfterByGuid.Find(Node.Guid);
			if (Found == nullptr
				|| InAfter.Nodes[*Found].Text != Node.Text
				|| InAfter.Objects[*Found] != InBefore.Objects[BeforeIndex])
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
		Widgets.RemoveAll([](const UWidget* Widget) { return Widget == nullptr; });
		Widgets.Sort([](const UWidget& A, const UWidget& B)
		{
			return A.GetName() < B.GetName();
		});
		for (const UWidget* Widget : Widgets)
		{
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

	FWidgetIdentity CaptureWidgetIdentity(const UWidgetBlueprint* InBlueprint)
	{
		FWidgetIdentity Identity;
		if (InBlueprint == nullptr || InBlueprint->WidgetTree == nullptr)
		{
			return Identity;
		}
		Identity.Tree = InBlueprint->WidgetTree;
		Identity.Root = Identity.Tree->RootWidget;
		TArray<UWidget*> Widgets;
		Identity.Tree->GetAllWidgets(Widgets);
		Widgets.RemoveAll([](const UWidget* Widget) { return Widget == nullptr; });
		Widgets.Sort([](const UWidget& A, const UWidget& B)
		{
			return A.GetName() < B.GetName();
		});
		for (const UWidget* Widget : Widgets)
		{
			Identity.Widgets.Add(Widget);
			Identity.Slots.Add(Widget->Slot);
			Identity.Parents.Add(Widget->Slot != nullptr ? Widget->Slot->Parent.Get() : nullptr);
		}
		return Identity;
	}

	bool SameWidgetIdentity(const FWidgetIdentity& InA, const FWidgetIdentity& InB)
	{
		if (InA.Tree != InB.Tree || InA.Root != InB.Root
			|| InA.Widgets.Num() != InB.Widgets.Num()
			|| InA.Slots.Num() != InB.Slots.Num()
			|| InA.Parents.Num() != InB.Parents.Num())
		{
			return false;
		}
		for (int32 Index = 0; Index < InA.Widgets.Num(); ++Index)
		{
			if (InA.Widgets[Index] != InB.Widgets[Index]
				|| InA.Slots[Index] != InB.Slots[Index]
				|| InA.Parents[Index] != InB.Parents[Index])
			{
				return false;
			}
		}
		return true;
	}

	/**
	 * Fixture-specific extraction from production reader text. Selects exactly
	 * one form by a unique InString marker and extracts the emitted node identity
	 * from its header comment. No fallback to fixture MakeForNode is allowed.
	 */
	bool ExtractFormByMarker(const FString& InText, const FString& InMarker,
		FString& OutRef, FString& OutForm, FString& OutError)
	{
		OutRef.Empty();
		OutForm.Empty();
		OutError.Empty();
		TArray<FString> Lines;
		InText.ParseIntoArrayLines(Lines, /*bCullEmpty=*/false);
		TArray<int32> Matches;
		for (int32 Index = 0; Index < Lines.Num(); ++Index)
		{
			if (Lines[Index].Contains(InMarker))
			{
				Matches.Add(Index);
			}
		}
		if (Matches.Num() == 0)
		{
			OutError = TEXT("marker is absent from the reader text");
			return false;
		}
		if (Matches.Num() != 1)
		{
			OutError = TEXT("marker is ambiguous in the reader text");
			return false;
		}
		int32 HeaderIndex = INDEX_NONE;
		for (int32 Index = Matches[0] - 1; Index >= 0; --Index)
		{
			if (Lines[Index].TrimStartAndEnd().StartsWith(TEXT("(")))
			{
				HeaderIndex = Index;
				break;
			}
		}
		if (HeaderIndex == INDEX_NONE)
		{
			OutError = TEXT("no form header precedes the marker");
			return false;
		}
		int32 EndIndex = Lines.Num();
		for (int32 Index = HeaderIndex + 1; Index < Lines.Num(); ++Index)
		{
			if (Lines[Index].TrimStartAndEnd().StartsWith(TEXT("(")))
			{
				EndIndex = Index;
				break;
			}
		}
		TArray<FString> FormLines;
		for (int32 Index = HeaderIndex; Index < EndIndex; ++Index)
		{
			FormLines.Add(Lines[Index]);
		}
		OutForm = FString::Join(FormLines, TEXT("\n"));
		const FString Header = Lines[HeaderIndex].TrimStartAndEnd();
		const int32 Separator = Header.Find(TEXT(" ; "));
		if (Separator == INDEX_NONE)
		{
			OutError = TEXT("the selected form has no emitted identity comment");
			return false;
		}
		FString Ref = Header.Mid(Separator + 3);
		const int32 Space = Ref.Find(TEXT(" "));
		if (Space != INDEX_NONE)
		{
			Ref = Ref.Left(Space);
		}
		Ref = Ref.TrimStartAndEnd();
		if (Ref.IsEmpty())
		{
			OutError = TEXT("the selected form emitted an empty identity");
			return false;
		}
		OutRef = Ref;
		return true;
	}

	bool ExtractFormLink(const FString& InForm, FString& OutRef)
	{
		OutRef.Empty();
		const FString Prefix = TEXT("execute (-> ");
		const int32 Start = InForm.Find(Prefix);
		if (Start == INDEX_NONE)
		{
			return false;
		}
		const int32 RefStart = Start + Prefix.Len();
		const int32 End = InForm.Find(TEXT(")"), ESearchCase::IgnoreCase, ESearchDir::FromStart, RefStart);
		if (End == INDEX_NONE)
		{
			return false;
		}
		OutRef = InForm.Mid(RefStart, End - RefStart).TrimStartAndEnd();
		return !OutRef.IsEmpty();
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

	bool BuildFixture(FFixture& OutFixture)
	{
		const FString Suffix = FGuid::NewGuid().ToString(EGuidFormats::DigitsLower);
		const FString Leaf = FString::Printf(TEXT("TurboMCPWidgetRoundtrip_%s"), *Suffix);
		OutFixture.Package = CreatePackage(*FString::Printf(TEXT("/Game/Tests/%s"), *Leaf));
		if (OutFixture.Package == nullptr)
		{
			OutFixture.LastError = TEXT("package was not created");
			return false;
		}
		// Unique, never saved, and non-transient so dirty tracking is observable.
		OutFixture.Blueprint = FWidgetBlueprintOperationUtils::CreateWidgetBlueprint(
			OutFixture.Package, FName(*Leaf), BPTYPE_Normal, UUserWidget::StaticClass(),
			UCanvasPanel::StaticClass(), TEXT("TurboMCPWidgetRoundtripTest"), false);
		if (OutFixture.Blueprint == nullptr || OutFixture.Blueprint->WidgetTree == nullptr)
		{
			OutFixture.LastError = TEXT("WidgetBlueprint or WidgetTree was not created");
			return false;
		}
		OutFixture.AssetPath = OutFixture.Blueprint->GetPathName();
		OutFixture.Tree = OutFixture.Blueprint->WidgetTree;
		OutFixture.Root = Cast<UCanvasPanel>(OutFixture.Tree->RootWidget);
		if (OutFixture.Root == nullptr)
		{
			OutFixture.LastError = TEXT("CanvasPanel root is missing");
			return false;
		}

		FText WidgetError;
		OutFixture.Title = OutFixture.Tree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(), TEXT("TitleText"));
		if (OutFixture.Title == nullptr
			|| !FWidgetBlueprintOperationUtils::AddWidget(OutFixture.Blueprint, OutFixture.Title,
				OutFixture.Root, 0, WidgetError))
		{
			OutFixture.LastError = TEXT("TitleText add failed: ") + WidgetError.ToString();
			return false;
		}
		OutFixture.Title->SetText(FText::FromString(TEXT("Title")));
		OutFixture.TitleSlot = Cast<UCanvasPanelSlot>(OutFixture.Title->Slot);
		if (OutFixture.TitleSlot == nullptr)
		{
			OutFixture.LastError = TEXT("TitleText slot is missing");
			return false;
		}
		OutFixture.TitleSlot->SetAnchors(FAnchors(0.0f, 0.0f, 0.0f, 0.0f));
		OutFixture.TitleSlot->SetOffsets(FMargin(12.0f, 34.0f, 180.0f, 40.0f));
		OutFixture.TitleSlot->SetAlignment(FVector2D(0.25, 0.5));
		OutFixture.TitleSlot->SetAutoSize(false);
		OutFixture.TitleSlot->SetZOrder(3);
		FWidgetBlueprintOperationUtils::ToggleWidgetAsVariable(OutFixture.Blueprint,
			OutFixture.Title, true);

		OutFixture.Status = OutFixture.Tree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(), TEXT("StatusText"));
		if (OutFixture.Status == nullptr
			|| !FWidgetBlueprintOperationUtils::AddWidget(OutFixture.Blueprint, OutFixture.Status,
				OutFixture.Root, 1, WidgetError))
		{
			OutFixture.LastError = TEXT("StatusText add failed: ") + WidgetError.ToString();
			return false;
		}
		OutFixture.Status->SetText(FText::FromString(TEXT("Status")));
		OutFixture.StatusSlot = Cast<UCanvasPanelSlot>(OutFixture.Status->Slot);
		if (OutFixture.StatusSlot == nullptr)
		{
			OutFixture.LastError = TEXT("StatusText slot is missing");
			return false;
		}
		OutFixture.StatusSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 0.0f));
		OutFixture.StatusSlot->SetOffsets(FMargin(0.0f, 120.0f, 0.0f, 24.0f));
		OutFixture.StatusSlot->SetAlignment(FVector2D(0.5, 0.0));
		OutFixture.StatusSlot->SetAutoSize(false);
		OutFixture.StatusSlot->SetZOrder(2);
		FWidgetBlueprintOperationUtils::ToggleWidgetAsVariable(OutFixture.Blueprint,
			OutFixture.Status, true);
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

		OutFixture.Target = AddFunctionGraph(OutFixture, TEXT("RoundtripTarget"));
		OutFixture.Sibling = AddFunctionGraph(OutFixture, TEXT("RoundtripSibling"));
		if (OutFixture.Target == nullptr || OutFixture.Sibling == nullptr)
		{
			OutFixture.LastError = TEXT("function graphs were not created");
			return false;
		}
		OutFixture.Entry = FindEntry(OutFixture.Target);
		OutFixture.PrintA = AddPrint(OutFixture.Target, TEXT("first"), 100, 100, 0x11110001);
		OutFixture.PrintB = AddPrint(OutFixture.Target, TEXT("second"), 420, 100, 0x22220001);
		UK2Node_FunctionEntry* SiblingEntry = FindEntry(OutFixture.Sibling);
		UK2Node_CallFunction* SiblingPrint = AddPrint(OutFixture.Sibling, TEXT("sibling"),
			100, 100, 0x33330001);
		if (OutFixture.Entry == nullptr || OutFixture.PrintA == nullptr
			|| OutFixture.PrintB == nullptr || SiblingEntry == nullptr || SiblingPrint == nullptr)
		{
			OutFixture.LastError = TEXT("graph nodes are missing");
			return false;
		}
		if (!Link(OutFixture.Target, OutFixture.Entry->FindPin(TEXT("then"), EGPD_Output),
				OutFixture.PrintA->FindPin(TEXT("execute"), EGPD_Input))
			|| !Link(OutFixture.Target, OutFixture.PrintA->FindPin(TEXT("then"), EGPD_Output),
				OutFixture.PrintB->FindPin(TEXT("execute"), EGPD_Input))
			|| !Link(OutFixture.Sibling, SiblingEntry->FindPin(TEXT("then"), EGPD_Output),
				SiblingPrint->FindPin(TEXT("execute"), EGPD_Input)))
		{
			OutFixture.LastError = TEXT("seeded links could not be made");
			return false;
		}
		FCompilerResultsLog CompilerLog;
		FKismetEditorUtilities::CompileBlueprint(OutFixture.Blueprint,
			EBlueprintCompileOptions::None, &CompilerLog);
		if (CompilerLog.NumErrors != 0 || !OutFixture.Blueprint->IsUpToDate())
		{
			OutFixture.LastError = FString::Printf(
				TEXT("compilation failed with %d error(s)"), CompilerLog.NumErrors);
			return false;
		}
		return true;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTurboMCPWidgetRoundtripTest,
	"TurboMCP.Widget.RoundtripAcceptance",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTurboMCPWidgetRoundtripTest::RunTest(const FString& Parameters)
{
	FFixture Fixture;
	FScopedFixtureCleanup Cleanup(Fixture);
	if (!BuildFixture(Fixture))
	{
		AddError(TEXT("roundtrip fixture: ") + Fixture.LastError);
		return false;
	}
	Fixture.Package->SetDirtyFlag(false);

	const FGraphSnapshot TargetBefore = CaptureGraph(Fixture.Target);
	const FGraphSnapshot SiblingBefore = CaptureGraph(Fixture.Sibling);
	const FString WidgetsBefore = CaptureWidgets(Fixture.Blueprint);
	const FWidgetIdentity WidgetIdentityBefore = CaptureWidgetIdentity(Fixture.Blueprint);

	// Real production reader: this is the supported read text, not a whole-dump
	// append input. Only the source node identity needed to target one new form
	// is taken from it.
	TArray<FString> ReadNotes;
	const FTurboMCPGraphText Reader;
	const FString ReadBefore = Reader.Render(Fixture.Target,
		FTurboMCPGraphText::EDetail::Bodies, Fixture.AssetPath, ReadNotes);
	TestFalse(TEXT("the real reader produced text"), ReadBefore.IsEmpty());
	TestFalse(TEXT("the read text contains no reader error"),
		ReadBefore.Contains(TEXT("; error:")));
	const FString SourceOracle = TurboMCPNodeId::MakeForNode(Fixture.PrintB);
	FString ExtractedSource;
	FString ExtractedForm;
	FString ExtractError;
	TestTrue(TEXT("the reader form carrying the unique second literal yields an identity"),
		ExtractFormByMarker(ReadBefore, TEXT("InString \"second\""), ExtractedSource,
			ExtractedForm, ExtractError));
	if (ExtractedSource.IsEmpty())
	{
		AddError(TEXT("reader extraction failed: ") + ExtractError);
		return false;
	}
	TestEqual(TEXT("the extracted reader identity matches the fixture oracle"),
		ExtractedSource, SourceOracle);
	TestTrue(TEXT("the extracted form contains the second literal"),
		ExtractedForm.Contains(TEXT("InString \"second\"")));

	// Negative controls for extraction: missing, ambiguous and corrupted emitted
	// identity must all refuse rather than fall back to fixture identity.
	{
		FString MissingRef, MissingForm, MissingError;
		TestFalse(TEXT("extraction refuses a missing marker"),
			ExtractFormByMarker(ReadBefore, TEXT("InString \"absent\""), MissingRef,
				MissingForm, MissingError));
		TestTrue(TEXT("a missing marker yields no fallback identity"), MissingRef.IsEmpty());
	}
	{
		const FString Ambiguous = ReadBefore
			+ TEXT("\n  (Development|PrintString ; deadbeef\n      InString \"second\")\n");
		FString AmbiguousRef, AmbiguousForm, AmbiguousError;
		TestFalse(TEXT("extraction refuses an ambiguous marker"),
			ExtractFormByMarker(Ambiguous, TEXT("InString \"second\""), AmbiguousRef,
				AmbiguousForm, AmbiguousError));
		TestTrue(TEXT("an ambiguous marker yields no fallback identity"), AmbiguousRef.IsEmpty());
	}
	{
		FString CorruptRef, CorruptForm, CorruptError;
		TestFalse(TEXT("extraction refuses a corrupted emitted identity"),
			ExtractFormByMarker(TEXT("(Development|PrintString\n    InString \"second\")"),
				TEXT("InString \"second\""), CorruptRef, CorruptForm, CorruptError));
		TestTrue(TEXT("a corrupted form yields no fallback identity"), CorruptRef.IsEmpty());
	}

	// A targeted append form, not the whole dump: one new PrintString linked from
	// the identity extracted from the production reader output.
	const FString AppendText = FString::Printf(
		TEXT("(Development|PrintString\n    execute (-> %s:then)\n    InString \"roundtrip\")"),
		*ExtractedSource);

	FTurboMCPGraphAppend::FPlan Plan;
	FString PlanError;
	TestTrue(TEXT("Plan accepts the targeted append form"),
		FTurboMCPGraphAppend::Plan(Fixture.AssetPath, Fixture.Target->GetName(), AppendText,
			Plan, PlanError, /*bRequestPlacement=*/true));
	TestEqual(TEXT("Plan reports no error"), PlanError, FString());
	TestTrue(TEXT("Plan is acceptable"), Plan.bAcceptable());
	TestEqual(TEXT("Plan creates one node"), Plan.Nodes.Num(), 1);
	TestEqual(TEXT("Plan creates one link"), Plan.Links.Num(), 1);
	TestEqual(TEXT("Plan records one position"), Plan.PlannedPositions.Num(), 1);
	TestTrue(TEXT("Plan did not mutate the target graph"),
		SameGraph(TargetBefore, CaptureGraph(Fixture.Target)));
	TestTrue(TEXT("Plan did not mutate the sibling graph"),
		SameGraph(SiblingBefore, CaptureGraph(Fixture.Sibling)));
	TestEqual(TEXT("Plan did not mutate the widget tree"),
		CaptureWidgets(Fixture.Blueprint), WidgetsBefore);
	TestTrue(TEXT("Plan did not replace any widget, tree, slot or parent"),
		SameWidgetIdentity(WidgetIdentityBefore, CaptureWidgetIdentity(Fixture.Blueprint)));
	TestFalse(TEXT("Plan left the clean fixture clean"), Fixture.Package->IsDirty());
	if (!Plan.bAcceptable() || Plan.PlannedPositions.Num() != 1)
	{
		return false;
	}

	FString Report;
	FString ApplyError;
	TestTrue(TEXT("Apply accepts the plan"),
		FTurboMCPGraphAppend::Apply(Plan, Report, ApplyError));
	TestEqual(TEXT("Apply reports no error"), ApplyError, FString());
	TestTrue(TEXT("Apply reports changed"), Report.StartsWith(TEXT("changed:")));
	TestTrue(TEXT("Apply marks the fixture dirty"), Fixture.Package->IsDirty());

	const FGraphSnapshot TargetAfter = CaptureGraph(Fixture.Target);
	TArray<FString> NewGuids;
	TestTrue(TEXT("Apply preserves every existing target node"),
		ExistingPreserved(TargetBefore, TargetAfter, NewGuids));
	TestEqual(TEXT("Apply adds exactly one node"), NewGuids.Num(), 1);
	TestEqual(TEXT("Apply increases the target node count by exactly one"),
		TargetAfter.Nodes.Num(), TargetBefore.Nodes.Num() + 1);
	TestTrue(TEXT("Apply preserves the sibling graph"),
		SameGraph(SiblingBefore, CaptureGraph(Fixture.Sibling)));
	TestEqual(TEXT("Apply preserves the widget tree"),
		CaptureWidgets(Fixture.Blueprint), WidgetsBefore);
	TestTrue(TEXT("Apply preserves every widget, tree, slot and parent identity"),
		SameWidgetIdentity(WidgetIdentityBefore, CaptureWidgetIdentity(Fixture.Blueprint)));
	TestEqual(TEXT("Apply adds exactly one link"),
		TargetAfter.Links.Num(), TargetBefore.Links.Num() + 1);

	UEdGraphNode* NewNode = nullptr;
	if (NewGuids.Num() == 1)
	{
		for (UEdGraphNode* Node : Fixture.Target->Nodes)
		{
			if (Node != nullptr
				&& Node->NodeGuid.ToString(EGuidFormats::DigitsLower) == NewGuids[0])
			{
				NewNode = Node;
				break;
			}
		}
	}
	TestTrue(TEXT("the intended new node exists"), NewNode != nullptr);
	if (NewNode != nullptr)
	{
		UK2Node_CallFunction* NewPrint = Cast<UK2Node_CallFunction>(NewNode);
		TestTrue(TEXT("the new node is a PrintString"),
			NewPrint != nullptr && NewPrint->GetTargetFunction() != nullptr
			&& NewPrint->GetTargetFunction()->GetName() == TEXT("PrintString"));
		TestEqual(TEXT("the new node is at the planned X"), NewNode->NodePosX,
			Plan.PlannedPositions[0].X);
		TestEqual(TEXT("the new node is at the planned Y"), NewNode->NodePosY,
			Plan.PlannedPositions[0].Y);
		UEdGraphPin* InString = NewNode->FindPin(TEXT("InString"), EGPD_Input);
		TestEqual(TEXT("the new node carries the append literal"),
			InString != nullptr ? InString->DefaultValue : FString(),
			FString(TEXT("roundtrip")));
		UEdGraphPin* Then = Fixture.PrintB->FindPin(TEXT("then"), EGPD_Output);
		UEdGraphPin* Exec = NewNode->FindPin(TEXT("execute"), EGPD_Input);
		TestTrue(TEXT("the intended link exists"),
			Then != nullptr && Exec != nullptr && Then->LinkedTo.Contains(Exec));

		// Fresh production reader: the new form and its link must be visible.
		TArray<FString> FreshNotes;
		const FString ReadAfter = Reader.Render(Fixture.Target,
			FTurboMCPGraphText::EDetail::Bodies, Fixture.AssetPath, FreshNotes);
		const FString NewShortId = TurboMCPNodeId::MakeForNode(NewNode);
		TestFalse(TEXT("the fresh read contains no reader error"),
			ReadAfter.Contains(TEXT("; error:")));
		FString FreshRef;
		FString FreshForm;
		FString FreshError;
		TestTrue(TEXT("the fresh reader yields the intended new form by its literal"),
			ExtractFormByMarker(ReadAfter, TEXT("InString \"roundtrip\""), FreshRef,
				FreshForm, FreshError));
		TestEqual(TEXT("the fresh form identity is the new node"), FreshRef, NewShortId);
		TestTrue(TEXT("the fresh form carries the append literal"),
			FreshForm.Contains(TEXT("InString \"roundtrip\"")));
		FString FreshLink;
		TestTrue(TEXT("the fresh form exposes one execute source"), ExtractFormLink(
			FreshForm, FreshLink));
		TestEqual(TEXT("the fresh form's execute source is the reader source identity"),
			FreshLink, SourceOracle);
	}

	// Refusal must not mutate a fresh fixture. It is built separately so it
	// cannot inherit the successful Apply's dirty state.
	FFixture RefusalFixture;
	FScopedFixtureCleanup RefusalCleanup(RefusalFixture);
	if (!BuildFixture(RefusalFixture))
	{
		AddError(TEXT("refusal fixture: ") + RefusalFixture.LastError);
		return false;
	}
	RefusalFixture.Package->SetDirtyFlag(false);
	const FGraphSnapshot RefusalTarget = CaptureGraph(RefusalFixture.Target);
	const FGraphSnapshot RefusalSibling = CaptureGraph(RefusalFixture.Sibling);
	const FString RefusalWidgets = CaptureWidgets(RefusalFixture.Blueprint);
	const FWidgetIdentity RefusalWidgetIdentity = CaptureWidgetIdentity(RefusalFixture.Blueprint);
	FTurboMCPGraphAppend::FPlan RefusalPlan;
	FString RefusalError;
	TestTrue(TEXT("the refusal form plans"),
		FTurboMCPGraphAppend::Plan(RefusalFixture.AssetPath, RefusalFixture.Target->GetName(),
			TEXT("(Development|PrintString\n    execute (-> DefinitelyMissing:then)\n    InString \"refused\")"),
			RefusalPlan, RefusalError, /*bRequestPlacement=*/true));
	TestFalse(TEXT("the refusal plan is not acceptable"), RefusalPlan.bAcceptable());
	TestTrue(TEXT("the refusal plan records a refusal"), RefusalPlan.Refusals.Num() > 0);
	TestTrue(TEXT("a refused Plan did not mutate the target graph"),
		SameGraph(RefusalTarget, CaptureGraph(RefusalFixture.Target)));
	TestTrue(TEXT("a refused Plan did not mutate the sibling graph"),
		SameGraph(RefusalSibling, CaptureGraph(RefusalFixture.Sibling)));
	TestEqual(TEXT("a refused Plan did not mutate the widget tree"),
		CaptureWidgets(RefusalFixture.Blueprint), RefusalWidgets);
	TestTrue(TEXT("a refused Plan preserved widget, tree, slot and parent identity"),
		SameWidgetIdentity(RefusalWidgetIdentity,
			CaptureWidgetIdentity(RefusalFixture.Blueprint)));
	TestFalse(TEXT("a refused Plan left the fresh fixture clean"),
		RefusalFixture.Package->IsDirty());
	FString RefusalReport;
	FString RefusalApplyError;
	TestTrue(TEXT("Apply on a refused plan returns its preview"),
		FTurboMCPGraphAppend::Apply(RefusalPlan, RefusalReport, RefusalApplyError));
	TestTrue(TEXT("Apply on a refused plan reports unchanged"),
		RefusalReport.StartsWith(TEXT("unchanged:")));
	TestTrue(TEXT("a refused Apply did not mutate the target graph"),
		SameGraph(RefusalTarget, CaptureGraph(RefusalFixture.Target)));
	TestTrue(TEXT("a refused Apply did not mutate the sibling graph"),
		SameGraph(RefusalSibling, CaptureGraph(RefusalFixture.Sibling)));
	TestEqual(TEXT("a refused Apply did not mutate the widget tree"),
		CaptureWidgets(RefusalFixture.Blueprint), RefusalWidgets);
	TestTrue(TEXT("a refused Apply preserved widget, tree, slot and parent identity"),
		SameWidgetIdentity(RefusalWidgetIdentity,
			CaptureWidgetIdentity(RefusalFixture.Blueprint)));
	TestFalse(TEXT("a refused Apply left the fresh fixture clean"),
		RefusalFixture.Package->IsDirty());

	// Comparator negative controls after the real operations, so they cannot
	// contaminate the clean Plan evidence.
	const FGraphSnapshot TargetAfterControls = CaptureGraph(Fixture.Target);
	const FString WidgetsAfterControls = CaptureWidgets(Fixture.Blueprint);
	const FWidgetIdentity WidgetIdentityAfterControls = CaptureWidgetIdentity(Fixture.Blueprint);
	{
		const int32 OriginalX = Fixture.PrintA->NodePosX;
		Fixture.PrintA->NodePosX = OriginalX + 1;
		TestFalse(TEXT("the graph comparator detects a moved node"),
			SameGraph(TargetAfterControls, CaptureGraph(Fixture.Target)));
		Fixture.PrintA->NodePosX = OriginalX;
		TestTrue(TEXT("the moved-node control restores exactly"),
			SameGraph(TargetAfterControls, CaptureGraph(Fixture.Target)));
	}
	{
		UEdGraphPin* InString = Fixture.PrintA->FindPin(TEXT("InString"), EGPD_Input);
		TestTrue(TEXT("the default control prerequisite pin exists"), InString != nullptr);
		if (InString == nullptr)
		{
			return false;
		}
		const FString Original = InString->DefaultValue;
		InString->DefaultValue = TEXT("perturbed");
		TestFalse(TEXT("the graph comparator detects a changed default"),
			SameGraph(TargetAfterControls, CaptureGraph(Fixture.Target)));
		InString->DefaultValue = Original;
		TestTrue(TEXT("the changed-default control restores exactly"),
			SameGraph(TargetAfterControls, CaptureGraph(Fixture.Target)));
	}
	{
		UEdGraphPin* Then = Fixture.PrintA->FindPin(TEXT("then"), EGPD_Output);
		UEdGraphPin* Exec = Fixture.PrintB->FindPin(TEXT("execute"), EGPD_Input);
		TestTrue(TEXT("the link control prerequisite pins and schema exist"),
			Then != nullptr && Exec != nullptr && Fixture.Target->GetSchema() != nullptr);
		if (Then == nullptr || Exec == nullptr || Fixture.Target->GetSchema() == nullptr)
		{
			return false;
		}
		Fixture.Target->GetSchema()->BreakSinglePinLink(Then, Exec);
		TestFalse(TEXT("the graph comparator detects a removed link"),
			SameGraph(TargetAfterControls, CaptureGraph(Fixture.Target)));
		TestTrue(TEXT("the removed link is recreated"),
			Fixture.Target->GetSchema()->TryCreateConnection(Then, Exec));
		TestTrue(TEXT("the removed-link control restores exactly"),
			SameGraph(TargetAfterControls, CaptureGraph(Fixture.Target)));
	}
	{
		TestTrue(TEXT("the widget-slot control prerequisite slot exists"),
			Fixture.TitleSlot != nullptr);
		if (Fixture.TitleSlot == nullptr)
		{
			return false;
		}
		const int32 OriginalZ = Fixture.TitleSlot->GetZOrder();
		Fixture.TitleSlot->SetZOrder(OriginalZ + 1);
		TestFalse(TEXT("the widget comparator detects a changed slot property"),
			CaptureWidgets(Fixture.Blueprint) == WidgetsAfterControls);
		Fixture.TitleSlot->SetZOrder(OriginalZ);
		TestEqual(TEXT("the widget-slot control restores exactly"),
			CaptureWidgets(Fixture.Blueprint), WidgetsAfterControls);
		TestTrue(TEXT("the widget-slot control preserves object identities"),
			SameWidgetIdentity(WidgetIdentityAfterControls,
				CaptureWidgetIdentity(Fixture.Blueprint)));
	}

	TestTrue(TEXT("roundtrip fixture retirement completed"), Cleanup.Retire());
	TestTrue(TEXT("refusal fixture retirement completed"), RefusalCleanup.Retire());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
