// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "TurboMCPWidgetTree.h"

#include "BaseWidgetBlueprint.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/UserWidget.h"
#include "Components/NamedSlot.h"
#include "Components/NamedSlotInterface.h"
#include "Components/PanelSlot.h"
#include "Components/PanelWidget.h"
#include "Components/Widget.h"
#include "Containers/UnrealString.h"
#include "UObject/Class.h"
#include "UObject/UnrealType.h"
#include "UObject/UObjectGlobals.h"
#include "WidgetBlueprint.h"

namespace
{
	/** How many widgets are rendered before the list caps. */
	constexpr int32 MaxWidgets = 300;

	/** The visibility as a short name, since the enum's own name is verbose. */
	FString VisibilityName(ESlateVisibility InVisibility)
	{
		switch (InVisibility)
		{
		case ESlateVisibility::Visible:                  return TEXT("Visible");
		case ESlateVisibility::Collapsed:                return TEXT("Collapsed");
		case ESlateVisibility::Hidden:                   return TEXT("Hidden");
		case ESlateVisibility::HitTestInvisible:         return TEXT("HitTestInvisible");
		case ESlateVisibility::SelfHitTestInvisible:     return TEXT("SelfHitTestInvisible");
		default:                                         return TEXT("(unknown)");
		}
	}

	/** The delegates bound on widgets, read from the Blueprint's own binding list. */
	void CollectBoundEvents(UWidget* InWidget, UWidgetBlueprint* InWidgetBlueprint,
		TArray<FString>& OutBound)
	{
		if (InWidget == nullptr || InWidgetBlueprint == nullptr)
		{
			return;
		}

		// `UWidgetBlueprint::Bindings` is where the designer writes a binding, so it is the
		// source of truth rather than a reconstruction.
		//
		// WALKING THE GENERATED CLASS'S multicast delegate properties and matching them by name
		// prefix WOULD BE A RECONSTRUCTION of the same fact from a less direct source, and it only
		// sees a binding once the Blueprint has been compiled. This list exists before compilation and
		// names the widget exactly.
		const FString WidgetName = InWidget->GetFName().ToString();

		for (const FDelegateEditorBinding& Binding : InWidgetBlueprint->Bindings)
		{
			if (Binding.ObjectName != WidgetName)
			{
				continue;
			}

			// `FunctionName` is the function graph the delegate is bound to. A property
			// binding instead has a `SourceProperty` and no function, and that is named
			// too rather than reported as a binding to nothing.
			if (!Binding.FunctionName.IsNone())
			{
				OutBound.Add(FString::Printf(TEXT("%s -> %s"),
					*Binding.PropertyName.ToString(),
					*Binding.FunctionName.ToString()));
			}
			else if (!Binding.SourceProperty.IsNone())
			{
				OutBound.Add(FString::Printf(TEXT("%s -> property %s"),
					*Binding.PropertyName.ToString(),
					*Binding.SourceProperty.ToString()));
			}
			else
			{
				OutBound.Add(FString::Printf(TEXT("%s -> (bound, target not named)"),
					*Binding.PropertyName.ToString()));
			}
		}
	}

	/** The instance-editable properties whose value differs from the class default. */
	void CollectNonDefaultProperties(UWidget* InWidget, TArray<FString>& OutDiff)
	{
		if (InWidget == nullptr)
		{
			return;
		}

		const UClass* WidgetClass = InWidget->GetClass();
		// A placed user widget is the case the spec names. For anything else the copy and
		// its class default are the same object, so there is nothing to compare.
		UObject* Default = WidgetClass->GetDefaultObject();
		if (Default == nullptr || Default == InWidget)
		{
			return;
		}

		for (TFieldIterator<FProperty> It(WidgetClass); It; ++It)
		{
			if (!It->HasAnyPropertyFlags(CPF_Edit | CPF_BlueprintVisible))
			{
				continue;
			}

			// A property that holds an OBJECT is skipped. `Slot` is the case that forced
			// this: it is a reference to this placed copy's own slot, so it differs from
			// the class default on every widget in the tree and buries the properties a
			// caller actually wants. The spec is asking for the instance-editable VALUES a
			// placed copy stores, not for the identity of its slot.
			//
			// One check covers the family: `FSoftObjectProperty`, `FWeakObjectProperty` and
			// `FObjectProperty` all derive from `FObjectPropertyBase` through
			// `TFObjectPropertyBase`. `FInterfaceProperty` does not, so it is named.
			if (It->IsA<FObjectPropertyBase>() || It->IsA<FInterfaceProperty>())
			{
				continue;
			}

			const void* ThisValue = It->ContainerPtrToValuePtr<void>(InWidget);
			const void* DefaultValue = It->ContainerPtrToValuePtr<void>(Default);
			if (ThisValue == nullptr || DefaultValue == nullptr)
			{
				continue;
			}

			// BOTH SIDES ARE SHOWN, because one side is not enough to see what changed.
			//
			// Printing only this widget's value makes a difference read as
			// `Text="Text Block"`, which looks like no difference at all. The value that
			// matters is the PAIR: this copy says this, the class default says that.
			//
			// `FText` is compared with its own operator and not by exported string. The
			// version before that exported both sides and compared the strings, which
			// reported a difference on every TextBlock in the tree: `ExportText` writes a
			// text literal as `NSLOCTEXT(namespace, key, value)`, and two sides can carry
			// different keys for the same visible string. `FText::EqualTo` compares what
			// the user would actually see.
			if (const FTextProperty* TextProperty = CastField<FTextProperty>(*It))
			{
				const FText* ThisText = TextProperty->ContainerPtrToValuePtr<FText>(InWidget);
				const FText* DefaultText = TextProperty->ContainerPtrToValuePtr<FText>(Default);
				if (ThisText != nullptr && DefaultText != nullptr
					&& !ThisText->EqualTo(*DefaultText))
				{
					OutDiff.Add(FString::Printf(TEXT("%s=\"%s\" (class default \"%s\")"),
						*It->GetName(), *ThisText->ToString(), *DefaultText->ToString()));
				}
				continue;
			}

			// ExportText on both sides for everything else, so it agrees with what the
			// details panel shows as modified.
			FString ThisText;
			FString DefaultText;
			It->ExportTextItem_Direct(ThisText, ThisValue, nullptr, nullptr, PPF_None);
			It->ExportTextItem_Direct(DefaultText, DefaultValue, nullptr, nullptr, PPF_None);

			if (ThisText != DefaultText)
			{
				OutDiff.Add(FString::Printf(TEXT("%s=%s (class default %s)"),
					*It->GetName(), *ThisText, *DefaultText));
			}
		}
	}
}

bool FTurboMCPWidgetTree::Collect(UBaseWidgetBlueprint* InWidgetBlueprint,
	TArray<FEntry>& OutEntries, FString& OutError)
{
	OutError.Empty();

	if (InWidgetBlueprint == nullptr)
	{
		OutError = TEXT("no widget blueprint was given");
		return false;
	}

	UWidgetTree* Tree = InWidgetBlueprint->WidgetTree;
	if (Tree == nullptr)
	{
		// A Widget Blueprint with no tree is a fact worth stating rather than an error.
		return true;
	}

	// `Bindings` lives on `UWidgetBlueprint`, not on the `UBaseWidgetBlueprint` base, so the
	// cast is attempted once. A Blueprint whose class is only the base has no binding list
	// and is treated as having no bindings, which is the truth.
	UWidgetBlueprint* Concrete = Cast<UWidgetBlueprint>(InWidgetBlueprint);

	// The tree is walked depth-first. A user widget owns another widget tree, and it can
	// expose content through named slots; both are children of the placed user widget,
	// not detached trees. Keep the slot host with the pending child so the compact reader
	// can tell an ordinary panel child from supplied named-slot content.
	struct FPendingWidget
	{
		UWidget* Widget = nullptr;
		int32 Depth = 0;
		FName NamedSlotHost;
	};

	TSet<const UWidget*> Visited;
	TArray<FPendingWidget> Stack;
	if (Tree->RootWidget != nullptr)
	{
		Stack.Add({ Tree->RootWidget, 0, NAME_None });
	}

	while (Stack.Num() > 0)
	{
		const FPendingWidget Pending = Stack.Pop();
		UWidget* Widget = Pending.Widget;
		if (Widget == nullptr || Visited.Contains(Widget))
		{
			continue;
		}
		Visited.Add(Widget);

		const int32 Depth = Pending.Depth;

		FEntry Entry;
		Entry.Name = Widget->GetFName();
		Entry.ClassName = Widget->GetClass()->GetName();
		Entry.bIsVariable = Widget->bIsVariable;
		Entry.Visibility = VisibilityName(Widget->GetVisibility());
		Entry.NamedSlotHost = Pending.NamedSlotHost;
		Entry.Depth = Depth;

		if (UPanelSlot* Slot = Widget->Slot)
		{
			// Padding is read by REFLECTION rather than from a getter, because
			// `UPanelSlot` has none: padding lives on the concrete slot types
			// (`UOverlaySlot`, `UBorderSlot`, and so on) and not on the base. A property
			// search covers every slot type at once, including ones added later.
			//
			// It is printed only when it is not zero, as the spec asks, because a line of
			// zeroes on every widget is noise.
			if (FStructProperty* PadProperty =
					FindFProperty<FStructProperty>(Slot->GetClass(), TEXT("Padding")))
			{
				if (const FMargin* Pad =
						PadProperty->ContainerPtrToValuePtr<FMargin>(Slot))
				{
					const bool bAnyPadding = !FMath::IsNearlyZero(Pad->Left)
						|| !FMath::IsNearlyZero(Pad->Top)
						|| !FMath::IsNearlyZero(Pad->Right)
						|| !FMath::IsNearlyZero(Pad->Bottom);
					if (bAnyPadding)
					{
						Entry.Padding = FString::Printf(TEXT("%.0f,%.0f,%.0f,%.0f"),
							Pad->Left, Pad->Top, Pad->Right, Pad->Bottom);
					}
				}
			}
		}

		CollectBoundEvents(Widget, Concrete, Entry.BoundEvents);
		CollectNonDefaultProperties(Widget, Entry.NonDefaultProperties);

		OutEntries.Add(MoveTemp(Entry));

		// Push in reverse because the stack is LIFO. This preserves panel slot order,
		// then the nested widget tree root, then named-slot content in slot-name order.
		TArray<FPendingWidget> Children;
		if (UPanelWidget* Panel = Cast<UPanelWidget>(Widget))
		{
			const FName PanelNamedSlot = Cast<UNamedSlot>(Widget) != nullptr
				? Widget->GetFName() : NAME_None;
			const int32 SlotCount = Panel->GetChildrenCount();
			for (int32 Index = 0; Index < SlotCount; ++Index)
			{
				if (UWidget* Child = Panel->GetChildAt(Index))
				{
					Children.Add({ Child, Depth + 1, PanelNamedSlot });
				}
			}
		}

		if (UUserWidget* UserWidget = Cast<UUserWidget>(Widget))
		{
			if (UserWidget->WidgetTree != nullptr && UserWidget->WidgetTree->RootWidget != nullptr)
			{
				Children.Add({ UserWidget->WidgetTree->RootWidget, Depth + 1, NAME_None });
			}
		}

		if (INamedSlotInterface* NamedSlots = Cast<INamedSlotInterface>(Widget))
		{
			TArray<FName> SlotNames;
			NamedSlots->GetSlotNames(SlotNames);
			for (const FName SlotName : SlotNames)
			{
				if (UWidget* Content = NamedSlots->GetContentForSlot(SlotName))
				{
					Children.Add({ Content, Depth + 1, SlotName });
				}
			}
		}

		for (int32 Index = Children.Num() - 1; Index >= 0; --Index)
		{
			Stack.Add(Children[Index]);
		}
	}

	return true;
}

FString FTurboMCPWidgetTree::Render(UBaseWidgetBlueprint* InWidgetBlueprint, FString& OutError)
{
	TArray<FEntry> Entries;
	if (!Collect(InWidgetBlueprint, Entries, OutError))
	{
		return FString();
	}

	if (Entries.Num() == 0)
	{
		// `P1`: an empty tree says so in words rather than printing nothing.
		return TEXT("  This Widget Blueprint has no widgets, so there is no tree to show.");
	}

	FString Out;
	Out += FString::Printf(TEXT("  widget tree: %d widget(s), depth-first from the root"),
		Entries.Num());

	int32 Shown = 0;
	for (const FEntry& Entry : Entries)
	{
		if (Shown >= MaxWidgets)
		{
			Out += FString::Printf(TEXT("\n    (%d more widget(s) not shown)"),
				Entries.Num() - Shown);
			break;
		}
		++Shown;

		FString Line = FString::Printf(TEXT("\n    %s%s  [%s]"),
			*FString::ChrN(Entry.Depth * 2, TEXT(' ')),
			*Entry.Name.ToString(), *Entry.ClassName);

		if (Entry.bIsVariable)
		{
			Line += TEXT("  variable");
		}

		if (Entry.Visibility != TEXT("Visible"))
		{
			// Only a non-default visibility is worth the characters, and naming the
			// default would bury the ones that matter.
			Line += FString::Printf(TEXT("  visibility=%s"), *Entry.Visibility);
		}

		if (!Entry.Padding.IsEmpty())
		{
			Line += FString::Printf(TEXT("  padding=%s"), *Entry.Padding);
		}

		if (!Entry.NamedSlotHost.IsNone())
		{
			Line += FString::Printf(TEXT("  named-slot=%s"), *Entry.NamedSlotHost.ToString());
		}

		Out += Line;

		for (const FString& Bound : Entry.BoundEvents)
		{
			Out += FString::Printf(TEXT("\n        bound: %s"), *Bound);
		}

		if (Entry.NonDefaultProperties.Num() > 0)
		{
			// Capped, because a placed widget with many edited properties would otherwise
			// bury the rest of the tree.
			const int32 Cap = 6;
			Out += TEXT("\n        differs from the class default: ");
			for (int32 Index = 0; Index < Entry.NonDefaultProperties.Num() && Index < Cap; ++Index)
			{
				if (Index > 0)
				{
					Out += TEXT(", ");
				}
				Out += Entry.NonDefaultProperties[Index];
			}
			if (Entry.NonDefaultProperties.Num() > Cap)
			{
				Out += FString::Printf(TEXT(" (+%d more)"),
					Entry.NonDefaultProperties.Num() - Cap);
			}
		}
	}

	Out += TEXT("\n  ; 'variable' means the designer marked it as one. This section does ");
	Out += TEXT("NOT claim a widget is unused: a widget that is not a variable can still ");
	Out += TEXT("be driven by a binding or an animation, and neither is checked here.");

	return Out;
}
