// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Misc/AutomationTest.h"
#include "Editor.h"
#include "ScopedTransaction.h"
#include "Tools/TurboMCPToolsets.h"

#include "TurboMCPGraphAppend.h"
#include "TurboMCPGraphText.h"
#include "TurboMCPGraphTextLex.h"
#include "TurboMCPNodeId.h"
#include "TurboMCPWidgetTree.h"
#include "TurboMCPWidgetTreeSync.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"

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
#include "Kismet/KismetStringLibrary.h"
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
		UK2Node_CallFunction* Source = nullptr;
		UK2Node_FunctionEntry* Entry = nullptr;
		FString AssetPath;
		FString LastError;
	};

	struct FNodeSnapshot
	{
		FString Guid;
		FString Text;
		int32 X = 0;
		int32 Y = 0;
		int32 Width = 0;
		int32 Height = 0;
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
			TSet<UEdGraph*> RetiredGraphs;
			auto RetireGraph = [&RetireObject, &RetiredGraphs](UEdGraph* Graph)
			{
				if (Graph == nullptr || RetiredGraphs.Contains(Graph))
				{
					return;
				}
				RetiredGraphs.Add(Graph);
				for (UEdGraphNode* Node : Graph->Nodes)
				{
					RetireObject(Node);
				}
				RetireObject(Graph);
			};
			if (Fixture.Blueprint != nullptr)
			{
				// Reacquire the Blueprint's current graph objects from its public
				// graph arrays. The cached Fixture.Target/Sibling pointers may be
				// stale after compilation and must not be dereferenced here.
				auto RetireGraphList = [&RetireGraph](const auto& InGraphs)
				{
					for (const TObjectPtr<UEdGraph>& Graph : InGraphs)
					{
						RetireGraph(Graph.Get());
					}
				};
				RetireGraphList(Fixture.Blueprint->UbergraphPages);
				RetireGraphList(Fixture.Blueprint->FunctionGraphs);
				RetireGraphList(Fixture.Blueprint->MacroGraphs);
				RetireGraphList(Fixture.Blueprint->EventGraphs);
				RetireObject(Fixture.Blueprint->WidgetTree.Get());
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
			Row.X = Node->NodePosX;
			Row.Y = Node->NodePosY;
			Row.Width = Node->NodeWidth;
			Row.Height = Node->NodeHeight;
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

	// THE CREATED NODE HAS NO STORED SIZE UNTIL SLATE DRAWS IT, and Apply records only
	// its position. The planned footprint is therefore the documented 320x160 estimate
	// (`FPlannedPosition::bEstimatedSize`, TurboMCPGraphAppend.h), and an existing node
	// without a stored positive size uses the same estimate. This is geometry, not a
	// measured Slate footprint, and the test must not claim a visual result.
	constexpr int32 PlacementFootprintWidth = 320;
	constexpr int32 PlacementFootprintHeight = 160;

	int32 FootprintWidth(const UEdGraphNode* InNode)
	{
		return InNode != nullptr && InNode->NodeWidth > 0
			? InNode->NodeWidth : PlacementFootprintWidth;
	}

	int32 FootprintHeight(const UEdGraphNode* InNode)
	{
		return InNode != nullptr && InNode->NodeHeight > 0
			? InNode->NodeHeight : PlacementFootprintHeight;
	}

	bool FootprintsOverlap(const UEdGraphNode* InA, const UEdGraphNode* InB)
	{
		if (InA == nullptr || InB == nullptr)
		{
			return false;
		}
		const int64 ALeft = InA->NodePosX;
		const int64 ATop = InA->NodePosY;
		const int64 ARight = ALeft + FootprintWidth(InA);
		const int64 ABottom = ATop + FootprintHeight(InA);
		const int64 BLeft = InB->NodePosX;
		const int64 BTop = InB->NodePosY;
		const int64 BRight = BLeft + FootprintWidth(InB);
		const int64 BBottom = BTop + FootprintHeight(InB);
		return ALeft < BRight && BLeft < ARight && ATop < BBottom && BTop < ABottom;
	}

	// Layout acceptance: every pre-existing node keeps its stored position and size, and
	// the newly placed node's footprint does not overlap any of them.
	void CheckPlacementGeometry(FAutomationTestBase& InTest, const TCHAR* InLabel,
		const FGraphSnapshot& InBefore, const UEdGraph* InGraph, const UEdGraphNode* InNewNode)
	{
		TMap<FString, const UEdGraphNode*> CurrentByGuid;
		if (InGraph != nullptr)
		{
			for (const UEdGraphNode* Node : InGraph->Nodes)
			{
				if (Node != nullptr)
				{
					CurrentByGuid.Add(Node->NodeGuid.ToString(EGuidFormats::DigitsLower), Node);
				}
			}
		}

		int32 Checked = 0;
		int32 Moved = 0;
		for (const FNodeSnapshot& Before : InBefore.Nodes)
		{
			const UEdGraphNode* const* Current = CurrentByGuid.Find(Before.Guid);
			if (Current == nullptr || *Current == nullptr)
			{
				++Moved;
				continue;
			}
			++Checked;
			if ((*Current)->NodePosX != Before.X || (*Current)->NodePosY != Before.Y
				|| (*Current)->NodeWidth != Before.Width
				|| (*Current)->NodeHeight != Before.Height)
			{
				++Moved;
			}
		}
		InTest.TestTrue(
			*FString::Printf(TEXT("%s: the layout check has pre-existing nodes"), InLabel),
			InBefore.Nodes.Num() > 0);
		InTest.TestEqual(
			*FString::Printf(TEXT("%s: every pre-existing node is still in the graph"), InLabel),
			Checked, InBefore.Nodes.Num());
		InTest.TestEqual(
			*FString::Printf(TEXT("%s: every pre-existing node keeps its stored position and size"), InLabel),
			Moved, 0);

		int32 Compared = 0;
		int32 Overlaps = 0;
		if (InGraph != nullptr && InNewNode != nullptr)
		{
			for (const UEdGraphNode* Other : InGraph->Nodes)
			{
				if (Other == nullptr || Other == InNewNode)
				{
					continue;
				}
				++Compared;
				if (FootprintsOverlap(InNewNode, Other))
				{
					++Overlaps;
				}
			}
		}
		InTest.TestTrue(
			*FString::Printf(TEXT("%s: the overlap check compares other nodes"), InLabel),
			Compared > 0);
		InTest.TestEqual(
			*FString::Printf(TEXT("%s: the new node's planned footprint overlaps no existing node"), InLabel),
			Overlaps, 0);
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
	bool IsValidEmittedRef(const FString& InRef)
	{
		if (InRef.IsEmpty())
		{
			return false;
		}
		const TurboMCPNodeId::FParsed Parsed = TurboMCPNodeId::Parse(InRef);
		return Parsed.IsComplete()
			&& !TurboMCPNodeId::NormaliseKey(Parsed.NodeKey).IsEmpty();
	}

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
		if (HeaderIndex >= Matches[0] || EndIndex <= Matches[0])
		{
			OutError = TEXT("selected form boundaries do not contain the marker");
			return false;
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
		const int32 ExtraSeparator = Header.Find(TEXT(" ; "), ESearchCase::IgnoreCase,
			ESearchDir::FromStart, Separator + 3);
		if (ExtraSeparator != INDEX_NONE
			&& !Header.Mid(ExtraSeparator + 3).TrimStartAndEnd().StartsWith(TEXT("target ")))
		{
			OutError = TEXT("the selected form has ambiguous identity fields");
			return false;
		}
		FString Ref = Header.Mid(Separator + 3);
		const int32 Space = Ref.Find(TEXT(" "));
		if (Space != INDEX_NONE)
		{
			Ref = Ref.Left(Space);
		}
		Ref = Ref.TrimStartAndEnd();
		if (!IsValidEmittedRef(Ref))
		{
			OutError = TEXT("the selected form emitted a malformed identity");
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
		if (InForm.Find(Prefix, ESearchCase::IgnoreCase, ESearchDir::FromStart,
				Start + Prefix.Len()) != INDEX_NONE)
		{
			// More than one link field in one form is ambiguous; refuse rather
			// than selecting the first.
			return false;
		}
		const int32 RefStart = Start + Prefix.Len();
		const int32 End = InForm.Find(TEXT(")"), ESearchCase::IgnoreCase, ESearchDir::FromStart, RefStart);
		if (End == INDEX_NONE)
		{
			return false;
		}
		OutRef = InForm.Mid(RefStart, End - RefStart).TrimStartAndEnd();
		return IsValidEmittedRef(OutRef);
	}

	bool ExtractValueLink(const FString& InForm, const FString& InPinName,
		FString& OutNode, FString& OutPin, FString& OutError)
	{
		OutNode.Empty();
		OutPin.Empty();
		OutError.Empty();
		const FString PinPrefix = InPinName + TEXT(" (-> ");
		const int32 PinStart = InForm.Find(PinPrefix);
		if (PinStart == INDEX_NONE)
		{
			OutError = TEXT("the selected form has no connected value on the requested pin");
			return false;
		}
		if (InForm.Find(PinPrefix, ESearchCase::IgnoreCase, ESearchDir::FromStart,
				PinStart + PinPrefix.Len()) != INDEX_NONE)
		{
			OutError = TEXT("the selected form has ambiguous connected value fields");
			return false;
		}
		const int32 ValueStart = InForm.Find(TEXT("(-> "), ESearchCase::IgnoreCase,
			ESearchDir::FromStart, PinStart);
		const int32 ValueEnd = InForm.Find(TEXT(")"), ESearchCase::IgnoreCase,
			ESearchDir::FromStart, ValueStart + 4);
		if (ValueStart == INDEX_NONE || ValueEnd == INDEX_NONE)
		{
			OutError = TEXT("the connected value has no closed link form");
			return false;
		}
		const FString Value = InForm.Mid(ValueStart, ValueEnd - ValueStart + 1);
		TurboMCPGraphTextLex::FLinkForm Link;
		FString Reason;
		if (!TurboMCPGraphTextLex::ParseLinkValue(Value, Link, Reason))
		{
			OutError = Reason;
			return false;
		}
		if (!IsValidEmittedRef(Link.FromNode) || Link.FromPin.IsEmpty())
		{
			OutError = TEXT("the connected value has a malformed source identity or pin");
			return false;
		}
		OutNode = Link.FromNode;
		OutPin = Link.FromPin;
		return true;
	}

	UEdGraphNode* FindNodeByGuid(UEdGraph* InGraph, const FString& InGuidDigits)
	{
		if (InGraph == nullptr)
		{
			return nullptr;
		}
		for (UEdGraphNode* Node : InGraph->Nodes)
		{
			if (Node != nullptr
				&& Node->NodeGuid.ToString(EGuidFormats::DigitsLower) == InGuidDigits)
			{
				return Node;
			}
		}
		return nullptr;
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

	UK2Node_CallFunction* AddConcat(UEdGraph* InGraph, int32 InX, int32 InY, uint32 InGuid)
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
		Node->SetFromFunction(UKismetStringLibrary::StaticClass()->
			FindFunctionByName(TEXT("Concat_StrStr")));
		Node->NodePosX = InX;
		Node->NodePosY = InY;
		InGraph->AddNode(Node, /*bUserAction=*/false, /*bSelectNewNode=*/false);
		Node->AllocateDefaultPins();
		ForceGuid(Node, InGuid);
		return Node;
	}

	bool Link(UEdGraph* InGraph, UEdGraphPin* InFrom, UEdGraphPin* InTo)
	{
		return InGraph != nullptr && InGraph->GetSchema() != nullptr
			&& InFrom != nullptr && InTo != nullptr
			&& InGraph->GetSchema()->TryCreateConnection(InFrom, InTo);
	}

	bool BuildFixture(FFixture& OutFixture, bool bConnectedData = false)
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
		if (bConnectedData)
		{
			OutFixture.Source = AddConcat(OutFixture.Target, 100, 360, 0x44440001);
			if (OutFixture.Source == nullptr)
			{
				OutFixture.LastError = TEXT("connected source node was not created");
				return false;
			}
			UEdGraphPin* Duration = OutFixture.PrintB->FindPin(TEXT("Duration"), EGPD_Input);
			if (Duration == nullptr)
			{
				OutFixture.LastError = TEXT("connected fixture PrintB Duration pin is missing");
				return false;
			}
			Duration->DefaultValue = TEXT("7.25");
		}
		if (!Link(OutFixture.Target, OutFixture.Entry->FindPin(TEXT("then"), EGPD_Output),
				OutFixture.PrintA->FindPin(TEXT("execute"), EGPD_Input))
			|| !Link(OutFixture.Target, OutFixture.PrintA->FindPin(TEXT("then"), EGPD_Output),
				OutFixture.PrintB->FindPin(TEXT("execute"), EGPD_Input))
			|| !Link(OutFixture.Sibling, SiblingEntry->FindPin(TEXT("then"), EGPD_Output),
				SiblingPrint->FindPin(TEXT("execute"), EGPD_Input))
			|| (bConnectedData
				&& (!Link(OutFixture.Target,
						OutFixture.Source->FindPin(TEXT("ReturnValue"), EGPD_Output),
						OutFixture.PrintA->FindPin(TEXT("InString"), EGPD_Input))
					|| !Link(OutFixture.Target,
						OutFixture.Source->FindPin(TEXT("ReturnValue"), EGPD_Output),
						OutFixture.PrintB->FindPin(TEXT("InString"), EGPD_Input)))))
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
	// Exercise both reader formats on fractional and very small nonzero padding.
	UWidgetBlueprint* PaddingBlueprint = NewObject<UWidgetBlueprint>(GetTransientPackage());
	PaddingBlueprint->WidgetTree = NewObject<UWidgetTree>(PaddingBlueprint);
	UOverlay* PaddingRoot = PaddingBlueprint->WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("PaddingRoot"));
	PaddingBlueprint->WidgetTree->RootWidget = PaddingRoot;
	UTextBlock* PaddingChild = PaddingBlueprint->WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("PaddingChild"));
	UOverlaySlot* PaddingSlot = PaddingRoot->AddChildToOverlay(PaddingChild);
	const FMargin Cases[] = { FMargin(1.25f, -2.5f, 0.125f, 3.75f), FMargin(0.0000001f), FMargin(0.0f) };
	for (const FMargin& Expected : Cases)
	{
		PaddingSlot->SetPadding(Expected);
		TArray<FTurboMCPWidgetTree::FEntry> Entries;
		FString PaddingError;
		TestTrue(TEXT("flat padding reader succeeds"), FTurboMCPWidgetTree::Collect(PaddingBlueprint, Entries, PaddingError));
		const FTurboMCPWidgetTree::FEntry* Entry = Entries.FindByPredicate([](const FTurboMCPWidgetTree::FEntry& Item) { return Item.Name == TEXT("PaddingChild"); });
		if (!TestNotNull(TEXT("padding child appears in flat reader"), Entry)) return false;
		const bool bZero = Expected.Left == 0.0f && Expected.Top == 0.0f && Expected.Right == 0.0f && Expected.Bottom == 0.0f;
		if (bZero) TestTrue(TEXT("zero padding remains omitted"), Entry->Padding.IsEmpty());
		else
		{
			TArray<FString> Values;
			Entry->Padding.ParseIntoArray(Values, TEXT(","));
			if (!TestEqual(TEXT("padding has four components"), Values.Num(), 4)) return false;
			TestEqual(TEXT("left padding retains precision"), FCString::Atof(*Values[0]), Expected.Left);
			TestEqual(TEXT("top padding retains precision"), FCString::Atof(*Values[1]), Expected.Top);
			TestEqual(TEXT("right padding retains precision"), FCString::Atof(*Values[2]), Expected.Right);
			TestEqual(TEXT("bottom padding retains precision"), FCString::Atof(*Values[3]), Expected.Bottom);
		}
		TurboMCPWidgetTreeText::FTree PaddingTree;
		TestTrue(TEXT("hierarchy padding reader succeeds"), FTurboMCPWidgetTreeSync::Walk(PaddingBlueprint, PaddingTree, PaddingError));
		const FString PaddingText = TurboMCPWidgetTreeText::Render(PaddingTree);
		TestTrue(TEXT("hierarchy reader preserves the same padding values"), bZero ? !PaddingText.Contains(TEXT("padding=")) : PaddingText.Contains(TEXT("padding=") + Entry->Padding));
	}
	PaddingBlueprint->MarkAsGarbage();

	FFixture Fixture;
	FScopedFixtureCleanup Cleanup(Fixture);
	if (!BuildFixture(Fixture))
	{
		AddError(TEXT("roundtrip fixture: ") + Fixture.LastError);
		return false;
	}
	Fixture.Package->SetDirtyFlag(false);
	// FIELD-02: exercise the public compile wrapper inside an enclosing edit scope.
	// Refusing before compilation prevents reinstance side effects from entering undo.
	if (GEditor == nullptr || GEditor->IsTransactionActive())
	{
		AddError(TEXT("compile guard fixture requires an editor without an enclosing transaction"));
		return false;
	}
	const FGraphSnapshot GuardGraph = CaptureGraph(Fixture.Target);
	const FString GuardWidgets = CaptureWidgets(Fixture.Blueprint);
	const FWidgetIdentity GuardIdentity = CaptureWidgetIdentity(Fixture.Blueprint);
	UClass* const GuardClass = Fixture.Blueprint->GeneratedClass;
	{
		FScopedTransaction Transaction(FText::FromString(TEXT("TurboMCP compile refusal fixture")));
		TestTrue(TEXT("compile guard negative control owns an active transaction"), GEditor->IsTransactionActive());
		const FString Refused = UTurboMCPEditToolset::EditTurboMCPCompile(Fixture.AssetPath);
		TestTrue(TEXT("public compile refuses inside a transaction"), Refused.StartsWith(TEXT("error: compile refused")));
		TestTrue(TEXT("compile refusal preserves the caller transaction"), GEditor->IsTransactionActive());
		TestTrue(TEXT("compile refusal keeps graph objects and contents"), SameGraph(GuardGraph, CaptureGraph(Fixture.Target)));
		TestEqual(TEXT("compile refusal keeps widget hierarchy"), CaptureWidgets(Fixture.Blueprint), GuardWidgets);
		TestTrue(TEXT("compile refusal keeps widget identities"), SameWidgetIdentity(GuardIdentity, CaptureWidgetIdentity(Fixture.Blueprint)));
		TestTrue(TEXT("compile refusal keeps the generated class"), Fixture.Blueprint->GeneratedClass == GuardClass);
		TestFalse(TEXT("compile refusal leaves the package clean"), Fixture.Package->IsDirty());
	}
	TestFalse(TEXT("test transaction ends normally"), GEditor->IsTransactionActive());
	const FString CompiledOutside = UTurboMCPEditToolset::EditTurboMCPCompile(Fixture.AssetPath);
	TestTrue(TEXT("public compile still works outside a transaction"), CompiledOutside.Contains(TEXT("1 clean, 0 with warnings, 0 with errors")));
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
	{
		FString MalformedRef, MalformedForm, MalformedError;
		TestFalse(TEXT("extraction refuses a nonempty malformed identity"),
			ExtractFormByMarker(
				TEXT("(Development|PrintString ; Asset:Graph:zzzzzz\n    InString \"second\")"),
				TEXT("InString \"second\""), MalformedRef, MalformedForm, MalformedError));
		TestTrue(TEXT("a malformed nonempty identity yields no fallback identity"),
			MalformedRef.IsEmpty());
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
		CheckPlacementGeometry(*this, TEXT("literal append"), TargetBefore,
			Fixture.Target, NewNode);
		UEdGraphPin* InString = NewNode->FindPin(TEXT("InString"), EGPD_Input);
		TestEqual(TEXT("the new node carries the append literal"),
			InString != nullptr ? InString->DefaultValue : FString(),
			FString(TEXT("roundtrip")));
		UEdGraphPin* Then = Fixture.PrintB->FindPin(TEXT("then"), EGPD_Output);
		UEdGraphPin* Exec = NewNode->FindPin(TEXT("execute"), EGPD_Input);
		TestTrue(TEXT("the intended link exists"),
			Then != nullptr && Exec != nullptr && Then->LinkedTo.Contains(Exec));

		const FString NewGuidDigits = NewGuids[0];
		const FString LiteralGraphName = Fixture.Target->GetName();
		FCompilerResultsLog CompileLog;
		FKismetEditorUtilities::CompileBlueprint(Fixture.Blueprint,
			EBlueprintCompileOptions::None, &CompileLog);
		TestEqual(TEXT("post-write literal compile has no errors"), CompileLog.NumErrors, 0);
		TestTrue(TEXT("post-write literal Blueprint is up to date"),
			Fixture.Blueprint->IsUpToDate());
		UClass* LiteralGeneratedClass = Fixture.Blueprint->GeneratedClass;
		const bool bLiteralGeneratedUsable = LiteralGeneratedClass != nullptr
			&& LiteralGeneratedClass->GetDefaultObject() != nullptr
			&& Cast<UUserWidget>(LiteralGeneratedClass->GetDefaultObject()) != nullptr;
		TestTrue(TEXT("post-write literal generated class is usable"),
			bLiteralGeneratedUsable);
		if (CompileLog.NumErrors != 0 || !Fixture.Blueprint->IsUpToDate()
			|| !bLiteralGeneratedUsable)
		{
			return false;
		}
		UEdGraph* LiteralCompiledGraph = TurboMCPNodeId::FindGraphByName(
			Fixture.Blueprint, LiteralGraphName);
		UEdGraphNode* LiteralCompiledNewNode = FindNodeByGuid(LiteralCompiledGraph,
			NewGuidDigits);
		TestTrue(TEXT("post-write literal graph and new node are reacquired by identity"),
			LiteralCompiledGraph != nullptr && LiteralCompiledNewNode != nullptr);
		if (LiteralCompiledGraph == nullptr || LiteralCompiledNewNode == nullptr)
		{
			return false;
		}

		// Fresh production reader after compilation: the new form and its link
		// must still be visible. The pre-compile node pointer is not reused.
		TArray<FString> FreshNotes;
		const FString ReadAfter = Reader.Render(LiteralCompiledGraph,
			FTurboMCPGraphText::EDetail::Bodies, Fixture.AssetPath, FreshNotes);
		const FString NewShortId = TurboMCPNodeId::MakeForNode(LiteralCompiledNewNode);
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

	// One connected-data-input case: an existing supported pure Concat_StrStr
	// feeds two existing PrintString consumers, so the reader emits the second
	// consumer's value as a link. The append creates another consumer of that
	// emitted source, not a literal-only PrintString.
	FFixture Connected;
	FScopedFixtureCleanup ConnectedCleanup(Connected);
	if (!BuildFixture(Connected, /*bConnectedData=*/true))
	{
		AddError(TEXT("connected fixture: ") + Connected.LastError);
		return false;
	}
	Connected.Package->SetDirtyFlag(false);
	const FGraphSnapshot ConnectedTarget = CaptureGraph(Connected.Target);
	const FGraphSnapshot ConnectedSibling = CaptureGraph(Connected.Sibling);
	const FString ConnectedWidgets = CaptureWidgets(Connected.Blueprint);
	const FWidgetIdentity ConnectedWidgetIdentity = CaptureWidgetIdentity(Connected.Blueprint);

	TArray<FString> ConnectedReadNotes;
	const FTurboMCPGraphText ConnectedReader;
	const FString ConnectedRead = ConnectedReader.Render(Connected.Target,
		FTurboMCPGraphText::EDetail::Bodies, Connected.AssetPath, ConnectedReadNotes);
	FString ConnectedPrintBRef, ConnectedPrintBForm, ConnectedExtractError;
	TestTrue(TEXT("the reader emits the second consumer form"),
		ExtractFormByMarker(ConnectedRead, TEXT("Duration 7.25"), ConnectedPrintBRef,
			ConnectedPrintBForm, ConnectedExtractError));
	FString ConnectedSourceRef, ConnectedSourcePin, ConnectedValueError;
	TestTrue(TEXT("the second consumer's InString is a connected value"),
		ExtractValueLink(ConnectedPrintBForm, TEXT("InString"), ConnectedSourceRef,
			ConnectedSourcePin, ConnectedValueError));
	const FString ConnectedSourceOracle = TurboMCPNodeId::MakeForNode(Connected.Source);
	TestEqual(TEXT("the connected source reference is the reader-emitted source"),
		ConnectedSourceRef, ConnectedSourceOracle);
	TestEqual(TEXT("the connected source pin is ReturnValue"), ConnectedSourcePin,
		FString(TEXT("ReturnValue")));
	if (ConnectedPrintBRef.IsEmpty() || ConnectedSourceRef.IsEmpty()
		|| ConnectedSourcePin.IsEmpty())
	{
		return false;
	}

	// Invalid source must refuse without mutation on the same clean fixture.
	{
		const FString InvalidText = FString::Printf(
			TEXT("(Development|PrintString\n    execute (-> %s:then)\n")
			TEXT("    InString (-> DefinitelyMissing:ReturnValue)\n    Duration 8.5)"),
			*ConnectedPrintBRef);
		FTurboMCPGraphAppend::FPlan InvalidPlan;
		FString InvalidError;
		TestTrue(TEXT("the invalid data source form plans"),
			FTurboMCPGraphAppend::Plan(Connected.AssetPath, Connected.Target->GetName(),
				InvalidText, InvalidPlan, InvalidError, /*bRequestPlacement=*/true));
		TestFalse(TEXT("the invalid data source form is not acceptable"),
			InvalidPlan.bAcceptable());
		TestTrue(TEXT("the invalid data source form records a refusal"),
			InvalidPlan.Refusals.Num() > 0);
		if (InvalidPlan.Refusals.Num() == 0)
		{
			AddError(TEXT("invalid connected source did not produce a refusal"));
			return false;
		}
		TestTrue(TEXT("an invalid data source did not mutate the target graph"),
			SameGraph(ConnectedTarget, CaptureGraph(Connected.Target)));
		TestTrue(TEXT("an invalid data source did not mutate the sibling graph"),
			SameGraph(ConnectedSibling, CaptureGraph(Connected.Sibling)));
		TestEqual(TEXT("an invalid data source did not mutate the widget tree"),
			CaptureWidgets(Connected.Blueprint), ConnectedWidgets);
		TestTrue(TEXT("an invalid data source preserved widget/tree/slot identities"),
			SameWidgetIdentity(ConnectedWidgetIdentity,
				CaptureWidgetIdentity(Connected.Blueprint)));
		TestFalse(TEXT("an invalid data source left the clean fixture clean"),
			Connected.Package->IsDirty());

		FString InvalidReport;
		FString InvalidApplyError;
		const bool bInvalidDirtyBefore = Connected.Package->IsDirty();
		TestTrue(TEXT("Apply on the invalid data source plan returns a preview"),
			FTurboMCPGraphAppend::Apply(InvalidPlan, InvalidReport, InvalidApplyError));
		TestEqual(TEXT("Apply on the invalid data source reports no execution error"),
			InvalidApplyError, FString());
		TestTrue(TEXT("Apply on the invalid data source reports unchanged"),
			InvalidReport.StartsWith(TEXT("unchanged:")));
		TestTrue(TEXT("refused connected Apply did not mutate the target graph"),
			SameGraph(ConnectedTarget, CaptureGraph(Connected.Target)));
		TestTrue(TEXT("refused connected Apply did not mutate the sibling graph"),
			SameGraph(ConnectedSibling, CaptureGraph(Connected.Sibling)));
		TestEqual(TEXT("refused connected Apply did not mutate the widget tree"),
			CaptureWidgets(Connected.Blueprint), ConnectedWidgets);
		TestTrue(TEXT("refused connected Apply preserved widget/tree/slot identities"),
			SameWidgetIdentity(ConnectedWidgetIdentity,
				CaptureWidgetIdentity(Connected.Blueprint)));
		TestEqual(TEXT("refused connected Apply did not change dirty state"),
			Connected.Package->IsDirty(), bInvalidDirtyBefore);
	}

	const FString ConnectedAppendText = FString::Printf(
		TEXT("(Development|PrintString\n    execute (-> %s:then)\n")
		TEXT("    InString (-> %s:%s)\n    Duration 8.5)"),
		*ConnectedPrintBRef, *ConnectedSourceRef, *ConnectedSourcePin);
	FTurboMCPGraphAppend::FPlan ConnectedPlan;
	FString ConnectedPlanError;
	TestTrue(TEXT("Plan accepts the connected-data-input form"),
		FTurboMCPGraphAppend::Plan(Connected.AssetPath, Connected.Target->GetName(),
			ConnectedAppendText, ConnectedPlan, ConnectedPlanError, /*bRequestPlacement=*/true));
	TestTrue(TEXT("the connected plan is acceptable"), ConnectedPlan.bAcceptable());
	TestEqual(TEXT("the connected plan creates one node"), ConnectedPlan.Nodes.Num(), 1);
	TestEqual(TEXT("the connected plan creates two links"), ConnectedPlan.Links.Num(), 2);
	TestEqual(TEXT("the connected plan records one position"),
		ConnectedPlan.PlannedPositions.Num(), 1);
	if (!ConnectedPlan.bAcceptable() || ConnectedPlan.Nodes.Num() != 1
		|| ConnectedPlan.Links.Num() != 2 || ConnectedPlan.PlannedPositions.Num() != 1)
	{
		AddError(TEXT("connected plan did not meet the expected counts"));
		return false;
	}
	TestTrue(TEXT("the connected Plan did not mutate the target graph"),
		SameGraph(ConnectedTarget, CaptureGraph(Connected.Target)));
	TestTrue(TEXT("the connected Plan did not mutate the sibling graph"),
		SameGraph(ConnectedSibling, CaptureGraph(Connected.Sibling)));
	TestEqual(TEXT("the connected Plan did not mutate the widget tree"),
		CaptureWidgets(Connected.Blueprint), ConnectedWidgets);
	TestTrue(TEXT("the connected Plan preserved widget/tree/slot identities"),
		SameWidgetIdentity(ConnectedWidgetIdentity,
			CaptureWidgetIdentity(Connected.Blueprint)));
	TestFalse(TEXT("the connected Plan left the clean fixture clean"),
		Connected.Package->IsDirty());

	FString ConnectedReport;
	FString ConnectedApplyError;
	const bool bConnectedApplied = FTurboMCPGraphAppend::Apply(
		ConnectedPlan, ConnectedReport, ConnectedApplyError);
	TestTrue(TEXT("Apply accepts the connected plan"), bConnectedApplied);
	if (!bConnectedApplied)
	{
		return false;
	}
	TestTrue(TEXT("connected Apply reports changed"),
		ConnectedReport.StartsWith(TEXT("changed:")));
	TestTrue(TEXT("connected Apply marks the fixture dirty"), Connected.Package->IsDirty());
	const FGraphSnapshot ConnectedAfter = CaptureGraph(Connected.Target);
	TArray<FString> ConnectedNewGuids;
	TestTrue(TEXT("connected Apply preserves every existing node"),
		ExistingPreserved(ConnectedTarget, ConnectedAfter, ConnectedNewGuids));
	TestEqual(TEXT("connected Apply adds exactly one node"), ConnectedNewGuids.Num(), 1);
	TestEqual(TEXT("connected Apply adds exactly two links"),
		ConnectedAfter.Links.Num(), ConnectedTarget.Links.Num() + 2);
	TestTrue(TEXT("connected Apply preserves the sibling graph"),
		SameGraph(ConnectedSibling, CaptureGraph(Connected.Sibling)));
	TestEqual(TEXT("connected Apply preserves the widget tree"),
		CaptureWidgets(Connected.Blueprint), ConnectedWidgets);
	TestTrue(TEXT("connected Apply preserves widget/tree/slot identities"),
		SameWidgetIdentity(ConnectedWidgetIdentity,
			CaptureWidgetIdentity(Connected.Blueprint)));

	UEdGraphNode* ConnectedNewNode = nullptr;
	if (ConnectedNewGuids.Num() == 1)
	{
		for (UEdGraphNode* Node : Connected.Target->Nodes)
		{
			if (Node != nullptr
				&& Node->NodeGuid.ToString(EGuidFormats::DigitsLower) == ConnectedNewGuids[0])
			{
				ConnectedNewNode = Node;
				break;
			}
		}
	}
	TestTrue(TEXT("the connected new node exists"), ConnectedNewNode != nullptr);
	if (ConnectedNewNode == nullptr)
	{
		return false;
	}
	{
		UK2Node_CallFunction* ConnectedNewPrint = Cast<UK2Node_CallFunction>(ConnectedNewNode);
		TestTrue(TEXT("the connected new node is the intended PrintString"),
			ConnectedNewPrint != nullptr && ConnectedNewPrint->GetTargetFunction() != nullptr
			&& ConnectedNewPrint->GetTargetFunction()->GetName() == TEXT("PrintString"));
		UEdGraphPin* InString = ConnectedNewNode->FindPin(TEXT("InString"), EGPD_Input);
		TestTrue(TEXT("the connected new node has one ReturnValue data source"),
			InString != nullptr && InString->LinkedTo.Num() == 1
			&& InString->LinkedTo[0] != nullptr
			&& InString->LinkedTo[0]->GetOwningNode() == Connected.Source
			&& InString->LinkedTo[0]->PinName == FName(TEXT("ReturnValue")));
		UEdGraphPin* Exec = ConnectedNewNode->FindPin(TEXT("execute"), EGPD_Input);
		TestTrue(TEXT("the connected new node has the intended then exec source"),
			Exec != nullptr && Exec->LinkedTo.Num() == 1
			&& Exec->LinkedTo[0] != nullptr
			&& Exec->LinkedTo[0]->GetOwningNode() == Connected.PrintB
			&& Exec->LinkedTo[0]->PinName == FName(TEXT("then")));
		UEdGraphPin* Duration = ConnectedNewNode->FindPin(TEXT("Duration"), EGPD_Input);
		TestEqual(TEXT("the connected new node carries the duration literal"),
			Duration != nullptr ? Duration->DefaultValue : FString(),
			FString(TEXT("8.5")));
		TestEqual(TEXT("the connected new node is at the planned X"),
			ConnectedNewNode->NodePosX, ConnectedPlan.PlannedPositions[0].X);
		TestEqual(TEXT("the connected new node is at the planned Y"),
			ConnectedNewNode->NodePosY, ConnectedPlan.PlannedPositions[0].Y);
		CheckPlacementGeometry(*this, TEXT("connected append"), ConnectedTarget,
			Connected.Target, ConnectedNewNode);

		const FString ConnectedNewGuidDigits = ConnectedNewGuids[0];
		const FString ConnectedGraphName = Connected.Target->GetName();
		const FString ConnectedSourceGuid = Connected.Source->NodeGuid.ToString(
			EGuidFormats::DigitsLower);
		const FString ConnectedPrintBGuid = Connected.PrintB->NodeGuid.ToString(
			EGuidFormats::DigitsLower);
		FCompilerResultsLog ConnectedCompileLog;
		FKismetEditorUtilities::CompileBlueprint(Connected.Blueprint,
			EBlueprintCompileOptions::None, &ConnectedCompileLog);
		TestEqual(TEXT("post-write connected compile has no errors"),
			ConnectedCompileLog.NumErrors, 0);
		TestTrue(TEXT("post-write connected Blueprint is up to date"),
			Connected.Blueprint->IsUpToDate());
		UClass* ConnectedGeneratedClass = Connected.Blueprint->GeneratedClass;
		const bool bConnectedGeneratedUsable = ConnectedGeneratedClass != nullptr
			&& ConnectedGeneratedClass->GetDefaultObject() != nullptr
			&& Cast<UUserWidget>(ConnectedGeneratedClass->GetDefaultObject()) != nullptr;
		TestTrue(TEXT("post-write connected generated class is usable"),
			bConnectedGeneratedUsable);
		if (ConnectedCompileLog.NumErrors != 0 || !Connected.Blueprint->IsUpToDate()
			|| !bConnectedGeneratedUsable)
		{
			return false;
		}
		UEdGraph* ConnectedCompiledGraph = TurboMCPNodeId::FindGraphByName(
			Connected.Blueprint, ConnectedGraphName);
		UEdGraphNode* ConnectedCompiledNewNode = FindNodeByGuid(ConnectedCompiledGraph,
			ConnectedNewGuidDigits);
		UEdGraphNode* ConnectedCompiledSource = FindNodeByGuid(ConnectedCompiledGraph,
			ConnectedSourceGuid);
		UEdGraphNode* ConnectedCompiledPrintB = FindNodeByGuid(ConnectedCompiledGraph,
			ConnectedPrintBGuid);
		TestTrue(TEXT("post-write connected references are reacquired by identity"),
			ConnectedCompiledGraph != nullptr && ConnectedCompiledNewNode != nullptr
			&& ConnectedCompiledSource != nullptr && ConnectedCompiledPrintB != nullptr);
		if (ConnectedCompiledGraph == nullptr || ConnectedCompiledNewNode == nullptr
			|| ConnectedCompiledSource == nullptr || ConnectedCompiledPrintB == nullptr)
		{
			return false;
		}
		TestEqual(TEXT("post-write connected source identity is stable"),
			TurboMCPNodeId::MakeForNode(ConnectedCompiledSource), ConnectedSourceRef);
		TestEqual(TEXT("post-write connected consumer identity is stable"),
			TurboMCPNodeId::MakeForNode(ConnectedCompiledPrintB), ConnectedPrintBRef);
		UEdGraphPin* CompiledInString = ConnectedCompiledNewNode->FindPin(
			TEXT("InString"), EGPD_Input);
		UEdGraphPin* CompiledExec = ConnectedCompiledNewNode->FindPin(
			TEXT("execute"), EGPD_Input);
		TestTrue(TEXT("post-write connected data link is preserved"),
			CompiledInString != nullptr && CompiledInString->LinkedTo.Num() == 1
			&& CompiledInString->LinkedTo[0] != nullptr
			&& CompiledInString->LinkedTo[0]->GetOwningNode() == ConnectedCompiledSource
			&& CompiledInString->LinkedTo[0]->PinName == FName(TEXT("ReturnValue")));
		TestTrue(TEXT("post-write connected exec link is preserved"),
			CompiledExec != nullptr && CompiledExec->LinkedTo.Num() == 1
			&& CompiledExec->LinkedTo[0] != nullptr
			&& CompiledExec->LinkedTo[0]->GetOwningNode() == ConnectedCompiledPrintB
			&& CompiledExec->LinkedTo[0]->PinName == FName(TEXT("then")));

		TArray<FString> FreshNotes;
		const FString ConnectedFresh = ConnectedReader.Render(ConnectedCompiledGraph,
			FTurboMCPGraphText::EDetail::Bodies, Connected.AssetPath, FreshNotes);
		FString ConnectedFreshRef, ConnectedFreshForm, ConnectedFreshError;
		TestTrue(TEXT("the fresh reader emits the connected new form"),
			ExtractFormByMarker(ConnectedFresh, TEXT("Duration 8.5"), ConnectedFreshRef,
				ConnectedFreshForm, ConnectedFreshError));
		TestEqual(TEXT("the fresh connected form is the new node"),
			ConnectedFreshRef, TurboMCPNodeId::MakeForNode(ConnectedCompiledNewNode));
		FString FreshSourceRef, FreshSourcePin, FreshValueError;
		TestTrue(TEXT("the fresh connected form exposes its data source"),
			ExtractValueLink(ConnectedFreshForm, TEXT("InString"), FreshSourceRef,
				FreshSourcePin, FreshValueError));
		TestEqual(TEXT("the fresh read-back source is the reader source"),
			FreshSourceRef, ConnectedSourceRef);
		TestEqual(TEXT("the fresh read-back source pin is preserved"),
			FreshSourcePin, ConnectedSourcePin);
		FString FreshExecRef;
		TestTrue(TEXT("the fresh connected form exposes its exec source"),
			ExtractFormLink(ConnectedFreshForm, FreshExecRef));
		TestEqual(TEXT("the fresh connected form's exec source is the consumer"),
			FreshExecRef, ConnectedPrintBRef);
	}

	TestTrue(TEXT("roundtrip fixture retirement completed"), Cleanup.Retire());
	TestTrue(TEXT("refusal fixture retirement completed"), RefusalCleanup.Retire());
	TestTrue(TEXT("connected fixture retirement completed"), ConnectedCleanup.Retire());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
