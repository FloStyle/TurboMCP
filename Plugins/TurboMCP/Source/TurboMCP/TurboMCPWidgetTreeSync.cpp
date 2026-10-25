// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "TurboMCPWidgetTreeSync.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Blueprint/WidgetTree.h"
#include "Components/NamedSlotInterface.h"
#include "Components/PanelSlot.h"
#include "Components/PanelWidget.h"
#include "Components/Widget.h"
#include "EdGraph/EdGraph.h"
#include "Engine/Blueprint.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"
#include "WidgetBlueprint.h"
#include "UObject/UnrealType.h"

#include "TurboMCPAssetResolver.h"

namespace
{
    // Inspect reflected Common UI context without a hard CommonUI dependency.
    // These comments are inspection metadata, never widget write-back input.
    FString CommonButtonContext(UObject* Object, const FString& Label)
    {
        if (!Object) return FString();
        bool IsCommonButton = false;
        for (const UClass* Class = Object->GetClass(); Class; Class = Class->GetSuperClass())
            if (Class->GetPathName() == TEXT("/Script/CommonUI.CommonButtonBase")) IsCommonButton = true;
        if (!IsCommonButton) return FString();
        FString Out = TEXT("\n; Common Button context: ") + Label + TEXT("; class=") + Object->GetClass()->GetPathName();
        const FClassProperty* Property = FindFProperty<FClassProperty>(Object->GetClass(), TEXT("Style"));
        UClass* Style = Property ? Cast<UClass>(Property->GetObjectPropertyValue_InContainer(Object)) : nullptr;
        Out += TEXT("\n; Style=") + (Style ? Style->GetPathName() : FString(TEXT("None (no assigned style class)")));
        Out += TEXT("\n; Style values and runtime overrides are not included. Inspect the style asset before changing appearance.");
        for (const TCHAR* Name : {TEXT("BP_OnHovered"), TEXT("BP_OnUnhovered")})
            if (UFunction* Function = Object->GetClass()->FindFunctionByName(Name))
                Out += FString::Printf(TEXT("\n; inherited event: %s; owner=%s; override in the button Blueprint, not a parent delegate binding."), Name, *Function->GetOuterUClass()->GetPathName());
        return Out;
    }

	/**
	 * A short visibility name, matching what the existing reader prints so the two tools agree.
	 *
	 * A SECOND SPELLING OF THE SAME ENUM WOULD BE A SECOND DIALECT, and the reader is the thing an agent
	 * compares against, so this restates the mapping rather than inventing one. The values are the engine's
	 * own enumerators (`ESlateVisibility`), spelled the way the details panel shows them.
	 */
	FString VisibilityName(ESlateVisibility InVisibility)
	{
		switch (InVisibility)
		{
		case ESlateVisibility::Visible:              return TEXT("Visible");
		case ESlateVisibility::Collapsed:            return TEXT("Collapsed");
		case ESlateVisibility::Hidden:               return TEXT("Hidden");
		case ESlateVisibility::HitTestInvisible:     return TEXT("HitTestInvisible");
		case ESlateVisibility::SelfHitTestInvisible: return TEXT("SelfHitTestInvisible");
		default:                                     return TEXT("Visible");
		}
	}

	/** The class name in the form the dialect's rule 8.2.5 asks for: the concrete class, not a base. */
	FString ClassNameOf(const UWidget* InWidget)
	{
		return InWidget != nullptr && InWidget->GetClass() != nullptr
			? InWidget->GetClass()->GetName()
			: FString();
	}

	/**
	 * The slot's own assignments, read BY REFLECTION because `UPanelSlot` exposes no getter.
	 *
	 * `Padding` IS THE ONE THE READER ALREADY PRINTS, so it is the one this writes, and the value is emitted
	 * ONLY WHEN IT IS NOT ZERO - the same rule the reader follows, because a line of zeroes on every widget
	 * is noise and an agent reading two tools must see the same shape in both.
	 *
	 * The reflection walk is deliberately limited to `Padding` rather than dumping every property: the
	 * dialect's `(slot ...)` clause is `P/E-WIDGET-SET`'s to expand, and THIS tool's job is to carry what the
	 * reader prints so that a round trip is identity. Writing properties the reader does not print would make
	 * the two disagree, which is the failure this project records under "two tools disagreeing about one
	 * format".
	 */
	void SlotAssignments(const UWidget* InWidget, TArray<TurboMCPWidgetTreeText::FAssignment>& OutSlot)
	{
		if (InWidget == nullptr || InWidget->Slot == nullptr)
		{
			return;
		}

		UPanelSlot* Slot = InWidget->Slot;
		if (FStructProperty* PadProperty = FindFProperty<FStructProperty>(Slot->GetClass(), TEXT("Padding")))
		{
			if (const FMargin* Pad = PadProperty->ContainerPtrToValuePtr<FMargin>(Slot))
			{
				const bool bAnyPadding = Pad->Left != 0.0f
					|| Pad->Top != 0.0f
					|| Pad->Right != 0.0f
					|| Pad->Bottom != 0.0f;
				if (bAnyPadding)
				{
					TurboMCPWidgetTreeText::FAssignment Assignment;
					Assignment.Name = TEXT("padding");
					Assignment.Value = FString::Printf(TEXT("%.9g,%.9g,%.9g,%.9g"),
						Pad->Left, Pad->Top, Pad->Right, Pad->Bottom);
					OutSlot.Add(MoveTemp(Assignment));
				}
			}
		}
	}

	/**
	 * The widget's own assignments, limited to what the reader prints, for the same reason.
	 *
	 * `Visibility` IS PRINTED ONLY WHEN IT IS NOT THE DEFAULT, which is the reader's own rule: the reader
	 * prints the visibility it read, and a tree that wrote `Visibility=Visible` on every widget would
	 * round trip into text the reader never produces. Writing the non-default ones keeps the two shapes
	 * identical, WHICH IS THE PROPERTY `T-WIDGET-4` ASKS FOR.
	 */
	void WidgetAssignments(const UWidget* InWidget, TArray<TurboMCPWidgetTreeText::FAssignment>& OutProperties)
	{
		if (InWidget == nullptr)
		{
			return;
		}

		const ESlateVisibility Visibility = InWidget->GetVisibility();
		if (Visibility != ESlateVisibility::Visible)
		{
			TurboMCPWidgetTreeText::FAssignment Assignment;
			Assignment.Name = TEXT("Visibility");
			Assignment.Value = VisibilityName(Visibility);
			OutProperties.Add(MoveTemp(Assignment));
		}
	}

	/** Joins a parent path and a name the way the dialect derives paths: `/` between segments. */
	FString JoinPath(const FString& InParentPath, const FString& InName)
	{
		return InParentPath.IsEmpty() ? InName : InParentPath + TEXT("/") + InName;
	}

	/**
	 * THE WALK, AND THE REASON THIS FILE EXISTS.
	 *
	 * Children are taken through `UPanelWidget::GetChildrenCount` and `GetChildAt`
	 * (`Components/PanelWidget.h:28,36`) IN INDEX ORDER, because THAT INDEX IS THE SLOT ORDER and slot order
	 * is the tree's draw order - dialect rule 8.2.3. The widget tree's own flat `AllWidgets` array is not
	 * used here on purpose: it is the flattening that loses the fact this dialect was invented to carry.
	 */
	void WalkWidget(const UWidget* InWidget,
		const FString& InParentPath,
		TSet<const UWidget*>& InOutVisited,
		TurboMCPWidgetTreeText::FNode& OutNode)
	{
		OutNode = TurboMCPWidgetTreeText::FNode();
		if (InWidget == nullptr || InOutVisited.Contains(InWidget))
		{
			return;
		}
		InOutVisited.Add(InWidget);

		OutNode.Name = InWidget->GetFName().ToString();
		OutNode.ClassName = ClassNameOf(InWidget);
		OutNode.bIsVariable = InWidget->bIsVariable;

		// DERIVED RATHER THAN AUTHORED, which is what makes it trustworthy - the parser does the same for
		// text it reads. A path a writer invented could disagree with its own nesting.
		OutNode.Path = JoinPath(InParentPath, OutNode.Name);
		OutNode.StatedPath = OutNode.Path;

		SlotAssignments(InWidget, OutNode.Slot);
		WidgetAssignments(InWidget, OutNode.Properties);

		// A placed user widget has a separate WidgetTree and may expose content through
		// INamedSlotInterface. Treat both as descendants of the placed widget. The visited
		// set prevents a named slot that is also reachable through its panel from appearing
		// twice, and prevents malformed cyclic ownership from recursing forever.
		TArray<const UWidget*> Children;
		if (const UPanelWidget* Panel = Cast<UPanelWidget>(InWidget))
		{
			const int32 ChildCount = Panel->GetChildrenCount();
			for (int32 Index = 0; Index < ChildCount; ++Index)
			{
				if (const UWidget* Child = Panel->GetChildAt(Index))
				{
					Children.Add(Child);
				}
			}
		}

		if (const UUserWidget* UserWidget = Cast<UUserWidget>(InWidget))
		{
			if (UserWidget->WidgetTree != nullptr && UserWidget->WidgetTree->RootWidget != nullptr)
			{
				Children.Add(UserWidget->WidgetTree->RootWidget);
			}
		}

		if (const INamedSlotInterface* NamedSlots = Cast<INamedSlotInterface>(InWidget))
		{
			TArray<FName> SlotNames;
			NamedSlots->GetSlotNames(SlotNames);
			for (const FName SlotName : SlotNames)
			{
				if (const UWidget* Content = NamedSlots->GetContentForSlot(SlotName))
				{
					Children.Add(Content);
				}
			}
		}

		for (const UWidget* Child : Children)
		{
			if (InOutVisited.Contains(Child))
			{
				continue;
			}
			TurboMCPWidgetTreeText::FNode ChildNode;
			WalkWidget(Child, OutNode.Path, InOutVisited, ChildNode);
			if (!ChildNode.Name.IsEmpty())
			{
				OutNode.Children.Add(MoveTemp(ChildNode));
			}
		}
	}

	/** Every path in a tree, in the order the tree holds them, so a diff can be a set operation. */
	void CollectPaths(const TurboMCPWidgetTreeText::FNode& InNode,
		TArray<FString>& OutPaths)
	{
		if (InNode.Name.IsEmpty())
		{
			return;
		}

		OutPaths.Add(InNode.Path);
		for (const TurboMCPWidgetTreeText::FNode& Child : InNode.Children)
		{
			CollectPaths(Child, OutPaths);
		}
	}

	/** The node at a path, or null. Paths are what the text addresses widgets BY. */
	const TurboMCPWidgetTreeText::FNode* FindByPath(
		const TurboMCPWidgetTreeText::FNode& InNode, const FString& InPath)
	{
		if (InNode.Path == InPath)
		{
			return &InNode;
		}

		for (const TurboMCPWidgetTreeText::FNode& Child : InNode.Children)
		{
			if (const TurboMCPWidgetTreeText::FNode* Found = FindByPath(Child, InPath))
			{
				return Found;
			}
		}

		return nullptr;
	}

	/**
	 * Loads and validates the target, so every entry point refuses the same way.
	 *
	 * `FTurboMCPAssetResolver::Resolve` RETURNS AN ERROR STRING rather than a bool - empty means success and
	 * a non-empty string is already a complete error line for an agent
	 * (`TurboMCPAssetResolver.h:64-67`). Reading the header settles that; assuming the bool shape did not,
	 * and it cost one build.
	 */
	UBaseWidgetBlueprint* LoadWidgetBlueprint(const FString& InAsset, FString& OutError)
	{
		FTurboMCPAssetResolver::FResolved Resolved;
		const FString ResolveError = FTurboMCPAssetResolver::Resolve(InAsset, Resolved);
		if (!ResolveError.IsEmpty())
		{
			OutError = ResolveError;
			return nullptr;
		}

		// The resolver yields an object path; `LoadObject` wants the whole thing, which is what `FResolved`
		// carries. The package half is not needed here because a Widget Blueprint is addressed by object.
		UObject* Loaded = LoadObject<UObject>(nullptr, *Resolved.ObjectPath);
		if (Loaded == nullptr)
		{
			OutError = FString::Printf(TEXT("%s resolved but could not be loaded."), *InAsset);
			return nullptr;
		}

		UBaseWidgetBlueprint* WidgetBlueprint = Cast<UBaseWidgetBlueprint>(Loaded);
		if (WidgetBlueprint == nullptr)
		{
			// NAMED RATHER THAN SILENTLY EMPTY. A blueprint that is not a Widget Blueprint has no widget
			// tree at all, which is a different answer from a Widget Blueprint with an empty tree.
			OutError = FString::Printf(
				TEXT("%s is a %s, not a Widget Blueprint, so it has no widget tree."),
				*Resolved.ObjectPath, *Loaded->GetClass()->GetName());
			return nullptr;
		}

		return WidgetBlueprint;
	}

	/** The verb's own name, for messages - a caller should never have to map an enum back to a word. */
	FString VerbName(FTurboMCPWidgetTreeSync::EVerb InVerb)
	{
		switch (InVerb)
		{
		case FTurboMCPWidgetTreeSync::EVerb::Append:  return TEXT("append");
		case FTurboMCPWidgetTreeSync::EVerb::Replace: return TEXT("replace");
		case FTurboMCPWidgetTreeSync::EVerb::Patch:   return TEXT("patch");
		default:                                      return TEXT("?");
		}
	}
}

bool FTurboMCPWidgetTreeSync::Walk(UBaseWidgetBlueprint* InWidgetBlueprint,
	TurboMCPWidgetTreeText::FTree& OutTree, FString& OutError)
{
	OutError.Empty();
	OutTree = TurboMCPWidgetTreeText::FTree();

	if (InWidgetBlueprint == nullptr)
	{
		OutError = TEXT("no widget blueprint was given");
		return false;
	}

	UWidgetTree* Tree = InWidgetBlueprint->WidgetTree;
	if (Tree == nullptr)
	{
		// A Widget Blueprint with no tree is a FACT rather than an error, and `bHasTree` stays false so a
		// caller can tell "no tree" from "an empty tree" - the same distinction the reader makes.
		return true;
	}

	if (Tree->RootWidget == nullptr)
	{
		// A tree with no root is likewise a fact: nothing to walk, and not a failure to walk it.
		return true;
	}

	TSet<const UWidget*> Visited;
	WalkWidget(Tree->RootWidget, FString(), Visited, OutTree.Root);
	OutTree.bHasTree = !OutTree.Root.Name.IsEmpty();
	return true;
}

FString FTurboMCPWidgetTreeSync::Read(const FString& Asset)
{
	if (Asset.TrimStartAndEnd().IsEmpty())
	{
		return TEXT("error: no asset given. Pass the Widget Blueprint to read, ")
			TEXT("for example /Game/Tests/Kit/TA07_Card");
	}

	FString Error;
	UBaseWidgetBlueprint* WidgetBlueprint = LoadWidgetBlueprint(Asset, Error);
	if (WidgetBlueprint == nullptr)
	{
		return FString::Printf(TEXT("error: %s"), *Error);
	}

	TurboMCPWidgetTreeText::FTree Tree;
	if (!Walk(WidgetBlueprint, Tree, Error))
	{
		return FString::Printf(TEXT("error: %s"), *Error);
	}

	if (!Tree.bHasTree)
	{
		// SAID RATHER THAN RENDERED AS NOTHING, because an empty answer and an absent tree are the two
		// things this project's `P1` rule exists to keep apart.
		return FString::Printf(
			TEXT("read: %s\n; This Widget Blueprint has no root widget to display. This does not describe its graphs or animations."),
			*Asset);
	}

	// THE SAME RENDERER THE WRITER'S PARSER ACCEPTS, which is `D-15`'s requirement that the reader and the
	// writer share one code path. There is no second printer.
    FString Context;
    if (WidgetBlueprint->GeneratedClass)
        Context += CommonButtonContext(WidgetBlueprint->GeneratedClass->GetDefaultObject(), TEXT("compiled class defaults; not a live instance"));
    if (WidgetBlueprint->WidgetTree)
        WidgetBlueprint->WidgetTree->ForEachWidget([&](UWidget* Widget)
        { Context += CommonButtonContext(Widget, Widget->GetPathName()); });
	return TurboMCPWidgetTreeText::Render(Tree, 0)
		+ TEXT("\n; Coverage: hierarchy, names, classes, paths, variable flags, non-default visibility and selected slot padding.\n")
		+ TEXT("; This is not a complete widget export: other properties, bindings and animations are not included.\n")
		+ TEXT("; Do not use this text as a lossless backup or assume replacement preserves omitted data.") + Context;
}
