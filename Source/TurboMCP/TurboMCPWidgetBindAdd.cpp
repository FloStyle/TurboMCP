// Copyright (c) 2026 Florian ESCHALIER (FloStyle)
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "Tools/TurboMCPToolsets.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Widget.h"
#include "Containers/UnrealString.h"
#include "Engine/Blueprint.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "UObject/Class.h"
#include "UObject/Object.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UnrealType.h"
#include "WidgetBlueprint.h"

#include "TurboMCPAssetResolver.h"

/**
 * `E-WIDGET-BIND-ADD`, which exists because `P/E-WIDGET-RETARGET` HAS NOTHING TO REPAIR WITHOUT IT.
 *
 * WHY THIS TOOL IS HERE
 * ---------------------
 * `P/E-WIDGET-RETARGET` decides which widget bindings a class swap has left broken. Its own test
 * needs a broken binding to find, and a binding cannot be made by hand: it is a
 * `FDelegateEditorBinding` inside the `UWidgetBlueprint`, and the editor only ever writes one when
 * a user picks an event in the details panel, which does not exist headless.
 *
 * Without this tool the retarget capability could only ever be tested against an asset with NO
 * bindings, which would pass while proving nothing. That is the shape of check this project keeps
 * having to catch, so the binding is made on purpose instead.
 *
 * The binding names the delegate ON THE WIDGET'S OWN CLASS, so binding a delegate the class does
 * not carry is possible — which is exactly the state a class swap leaves behind, and exactly what
 * the retarget tool is for.
 */
FString UTurboMCPEditToolset::EditTurboMCPWidgetBindAdd(
	const FString& Asset, const FString& Widget, const FString& Property, const FString& Function)
{
	const FString WidgetName = Widget.TrimStartAndEnd();
	const FString PropertyName = Property.TrimStartAndEnd();
	const FString FunctionName = Function.TrimStartAndEnd();

	if (WidgetName.IsEmpty() || PropertyName.IsEmpty())
	{
		return TEXT("error: a widget name and a property name are both required. The property ")
			TEXT("is the delegate on the widget, for example OnClicked.");
	}

	FTurboMCPAssetResolver::FResolved Resolved;
	const FString ResolveError = FTurboMCPAssetResolver::Resolve(Asset, Resolved);
	if (!ResolveError.IsEmpty())
	{
		return ResolveError;
	}

	UObject* Loaded = LoadObject<UObject>(nullptr, *Resolved.ObjectPath);
	UWidgetBlueprint* WidgetBlueprint = Cast<UWidgetBlueprint>(Loaded);
	if (WidgetBlueprint == nullptr)
	{
		return FString::Printf(
			TEXT("error: %s is a %s, and only a Widget Blueprint holds widget bindings."),
			*Resolved.ObjectPath,
			Loaded != nullptr ? *Loaded->GetClass()->GetName() : TEXT("missing object"));
	}

	UWidgetTree* Tree = WidgetBlueprint->WidgetTree;
	UWidget* TargetWidget = Tree != nullptr
		? Tree->FindWidget(FName(*WidgetName)) : nullptr;

	if (TargetWidget == nullptr)
	{
		TArray<FString> Real;
		if (Tree != nullptr)
		{
			Tree->ForEachWidget([&Real](UWidget* Found)
			{
				if (Found != nullptr) { Real.Add(Found->GetFName().ToString()); }
			});
		}
		return FString::Printf(
			TEXT("error: %s has no widget named '%s'. Its widgets are: %s"),
			*Resolved.ObjectPath, *WidgetName,
			Real.Num() > 0 ? *FString::Join(Real, TEXT(", ")) : TEXT("(none)"));
	}

	// IS THE DELEGATE REAL? The answer decides whether this makes a WORKING binding or a BROKEN
	// one, and the result says which. A caller testing the retarget tool WANTS the broken kind, and
	// a caller binding a real event wants to know they got it.
	UClass* WidgetClass = TargetWidget->GetClass();
	const FString WithSuffix = PropertyName + TEXT("Delegate");

	// BOTH KINDS COUNT. Widget events are `DECLARE_DYNAMIC_MULTICAST_DELEGATE`, so they are
	// `FMulticastDelegateProperty`; a single `FDelegateProperty` is what a custom delegate on a
	// user struct looks like. Checking only the engine`'s narrower kind would report every real
	// widget event as broken.
	const bool bHasDelegate =
		FindFProperty<FDelegateProperty>(WidgetClass, FName(*WithSuffix)) != nullptr
		|| FindFProperty<FDelegateProperty>(WidgetClass, FName(*PropertyName)) != nullptr
		|| FindFProperty<FMulticastDelegateProperty>(WidgetClass, FName(*WithSuffix)) != nullptr
		|| FindFProperty<FMulticastDelegateProperty>(WidgetClass, FName(*PropertyName)) != nullptr;

	// A binding is identified by the widget and the property, so a duplicate is replaced rather
	// than added twice: the engine's own `operator==` compares exactly this pair and the editor
	// only allows one binding per property.
	for (int32 Index = WidgetBlueprint->Bindings.Num() - 1; Index >= 0; --Index)
	{
		const FDelegateEditorBinding& Existing = WidgetBlueprint->Bindings[Index];
		if (Existing.ObjectName == WidgetName
			&& Existing.PropertyName.ToString() == PropertyName)
		{
			WidgetBlueprint->Modify();
			WidgetBlueprint->Bindings.RemoveAt(Index);
		}
	}

	FDelegateEditorBinding Binding;
	Binding.ObjectName = WidgetName;
	Binding.PropertyName = FName(*PropertyName);
	// The function the binding calls. A real one is usually a generated stub; the name is what
	// the compiler looks for, so a caller testing the retarget tool can leave it as the default.
	Binding.FunctionName = FName(*(FunctionName.IsEmpty()
		? WidgetName + TEXT("_On") + PropertyName : FunctionName));
	Binding.Kind = EBindingKind::Function;

	WidgetBlueprint->Modify();
	WidgetBlueprint->Bindings.Add(MoveTemp(Binding));

	// THE READ-BACK: the binding is re-found in the list rather than assumed to be there.
	int32 Found = 0;
	for (const FDelegateEditorBinding& Check : WidgetBlueprint->Bindings)
	{
		if (Check.ObjectName == WidgetName
			&& Check.PropertyName.ToString() == PropertyName)
		{
			++Found;
		}
	}

	FString Out = FString::Printf(TEXT("changed: bound %s.%s in %s"),
		*WidgetName, *PropertyName, *Resolved.ObjectPath);
	Out += FString::Printf(TEXT("\nwidget class: %s"), *WidgetClass->GetName());
	Out += FString::Printf(TEXT("\nbinding found after the add: %s"),
		Found == 1 ? TEXT("yes") : TEXT("NO"));

	if (bHasDelegate)
	{
		Out += FString::Printf(
			TEXT("\nthe class HAS a delegate '%s', so this binding is VALID and ")
			TEXT("P-WIDGET-RETARGET will report it as ok."), *WithSuffix);
	}
	else
	{
		// Named plainly, because a caller who did this by accident needs to know at once.
		Out += FString::Printf(
			TEXT("\nthe class has NO delegate '%s' (nor '%s'), so this binding is BROKEN. ")
			TEXT("P-WIDGET-RETARGET will report it as BROKEN and remove it; E-WIDGET-BIND-REMOVE ")
			TEXT("takes it away without a retarget."),
			*WithSuffix, *PropertyName);
	}

	Out += FString::Printf(TEXT("\nbindings on the Blueprint now: %d"),
		WidgetBlueprint->Bindings.Num());
	Out += TEXT("\n; the change is in memory and NOT saved. Call E-SAVE.");

	return Out;
}
