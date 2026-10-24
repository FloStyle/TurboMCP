// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "TurboMCPWidgetRetarget.h"
#include "TurboMCPBlueprintRead.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Widget.h"
#include "Containers/UnrealString.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "K2Node_CallFunction.h"
#include "UObject/Class.h"
#include "UObject/Object.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UnrealType.h"
#include "WidgetBlueprint.h"

#include "TurboMCPAssetResolver.h"

namespace
{
	/**
	 * The delegate a widget binding needs, and the type of property it was found as.
	 *
	 * THE ENGINE'S OWN CHECK IS TOO NARROW FOR REAL WIDGET EVENTS, AND THIS WIDENS IT.
	 *
	 * `FDelegateEditorBinding::IsBindingValid` looks for a `FDelegateProperty` named
	 * `<PropertyName>Delegate` or `<PropertyName>`. Measured against the engine's own headers:
	 * `UButton::OnClicked` is declared `DECLARE_DYNAMIC_MULTICAST_DELEGATE`, so it is a
	 * `FMulticastDelegateProperty` and the engine's check NEVER finds it. A rule that rejected
	 * every normal widget event would be useless, and its "ok" case would be unreachable — which
	 * means a test of the broken case alone would have passed while proving nothing about the
	 * good one.
	 *
	 * So both property kinds count, and which one was found is carried back so the result can say
	 * it rather than flattening the two.
	 */
	enum class EDelegateKind : uint8 { None, Single, Multicast };

	EDelegateKind FindBindingDelegate(
		UClass* InClass, const FName InPropertyName, FString& OutLookedFor)
	{
		const FString WithSuffix = InPropertyName.ToString() + TEXT("Delegate");
		OutLookedFor = WithSuffix;

		// `<Name>Delegate` first, then the bare name, in the order the engine tries them.
		const FName Candidates[2] = { FName(*WithSuffix), InPropertyName };

		for (const FName& Candidate : Candidates)
		{
			if (FindFProperty<FDelegateProperty>(InClass, Candidate) != nullptr)
			{
				return EDelegateKind::Single;
			}
		}
		for (const FName& Candidate : Candidates)
		{
			if (FindFProperty<FMulticastDelegateProperty>(InClass, Candidate) != nullptr)
			{
				return EDelegateKind::Multicast;
			}
		}
		return EDelegateKind::None;
	}
}

int32 FTurboMCPWidgetRetarget::FPlan::NumBroken() const
{
	int32 Count = 0;
	for (const FBinding& Binding : Bindings)
	{
		if (!Binding.bDelegateExists)
		{
			++Count;
		}
	}
	return Count;
}

bool FTurboMCPWidgetRetarget::Plan(
	const FString& InAsset, FPlan& OutPlan, FString& OutError)
{
	OutError.Empty();
	OutPlan.Asset = InAsset.TrimStartAndEnd();

	if (OutPlan.Asset.IsEmpty())
	{
		OutError = TEXT("no asset given. Pass a Widget Blueprint in any path form, for ")
			TEXT("example WBP_NodeTest.");
		return false;
	}

	FTurboMCPAssetResolver::FResolved Resolved;
	const FString ResolveError = FTurboMCPAssetResolver::Resolve(OutPlan.Asset, Resolved);
	if (!ResolveError.IsEmpty())
	{
		OutError = ResolveError;
		return false;
	}

	UObject* Loaded = LoadObject<UObject>(nullptr, *Resolved.ObjectPath);
	UWidgetBlueprint* WidgetBlueprint = Cast<UWidgetBlueprint>(Loaded);
	if (WidgetBlueprint == nullptr)
	{
		OutError = FString::Printf(
			TEXT("%s is a %s, and only a Widget Blueprint has widget bindings."),
			*Resolved.ObjectPath,
			Loaded != nullptr ? *Loaded->GetClass()->GetName() : TEXT("missing object"));
		return false;
	}

	OutPlan.Asset = Resolved.ObjectPath;

	UWidgetTree* Tree = WidgetBlueprint->WidgetTree;

	for (const FDelegateEditorBinding& Binding : WidgetBlueprint->Bindings)
	{
		FBinding Found;
		Found.ObjectName = Binding.ObjectName;
		Found.PropertyName = Binding.PropertyName.ToString();
		Found.FunctionName = Binding.FunctionName.ToString();

		// THE SAME LOOKUP THE ENGINE'S OWN `IsBindingValid` DOES, so "the new class cannot
		// satisfy this" is the engine's answer rather than a rule invented here.
		UWidget* TargetWidget = Tree != nullptr
			? Tree->FindWidget(FName(*Binding.ObjectName)) : nullptr;

		if (TargetWidget == nullptr)
		{
			Found.bWidgetMissing = true;
			Found.bDelegateExists = false;
			Found.WidgetClass = TEXT("(widget not found)");
			OutPlan.Bindings.Add(MoveTemp(Found));
			continue;
		}

		UClass* WidgetClass = TargetWidget->GetClass();
		Found.WidgetClass = WidgetClass->GetName();

		// The widened lookup, in a helper so the Preview and the Edit cannot drift apart.
		FString LookedFor;
		const EDelegateKind Kind = FindBindingDelegate(
			WidgetClass, Binding.PropertyName, LookedFor);

		Found.DelegateLookedFor = LookedFor;
		Found.bDelegateExists = Kind != EDelegateKind::None;
		Found.bIsMulticast = Kind == EDelegateKind::Multicast;

		OutPlan.Bindings.Add(MoveTemp(Found));
	}

	// THE CALL NODES. A call node naming a function that no longer exists anywhere is stale, and
	// it is reported rather than removed: a caller may be about to add that function, and removing
	// a node is a bigger change than the spec asks for here.
	TArray<UEdGraph*> Graphs;
    FTurboMCPBlueprintRead::CollectGraphs(WidgetBlueprint, Graphs);

	const UClass* Parent = WidgetBlueprint->ParentClass;

	for (UEdGraph* Graph : Graphs)
	{
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			UK2Node_CallFunction* Call = Cast<UK2Node_CallFunction>(Node);
			if (Call == nullptr)
			{
				continue;
			}

			const FName FunctionName = Call->GetFunctionName();
			if (FunctionName.IsNone())
			{
				continue;
			}

			// A function is fine if the parent class, the generated class, or one of this
			// Blueprint's own function graphs provides it.
			bool bResolves = (Parent != nullptr
					&& Parent->FindFunctionByName(FunctionName) != nullptr)
				|| (WidgetBlueprint->GeneratedClass != nullptr
					&& WidgetBlueprint->GeneratedClass->FindFunctionByName(FunctionName) != nullptr);

			if (!bResolves)
			{
				for (UEdGraph* FunctionGraph : WidgetBlueprint->FunctionGraphs)
				{
					if (FunctionGraph != nullptr
						&& FunctionGraph->GetFName() == FunctionName)
					{
						bResolves = true;
						break;
					}
				}
			}

			if (!bResolves)
			{
				OutPlan.StaleCalls.Add(FString::Printf(
					TEXT("%s names '%s', which the current class does not have (in '%s')"),
					*Call->GetName(), *FunctionName.ToString(), *Graph->GetName()));
			}
		}
	}

	return true;
}

FString FTurboMCPWidgetRetarget::RenderPreview(const FPlan& InPlan)
{
	FString Out = FString::Printf(TEXT("preview: retarget %s"),
		*InPlan.Asset);
	Out += FString::Printf(TEXT("\n%d binding(s), %d the current class cannot satisfy"),
		InPlan.Bindings.Num(), InPlan.NumBroken());

	for (const FBinding& Binding : InPlan.Bindings)
	{
		if (Binding.bDelegateExists)
		{
			// WHICH KIND IT IS, because a single delegate and a multicast one are not the same
			// thing and flattening them would hide that the widget's events are multicast.
			Out += FString::Printf(TEXT("\n  ok      %s.%s  on %s  (%s delegate)  -> %s"),
				*Binding.ObjectName, *Binding.PropertyName, *Binding.WidgetClass,
				Binding.bIsMulticast ? TEXT("a multicast") : TEXT("a single"),
				*Binding.FunctionName);
		}
		else if (Binding.bWidgetMissing)
		{
			Out += FString::Printf(
				TEXT("\n  BROKEN  %s.%s  the widget does not exist in the tree"),
				*Binding.ObjectName, *Binding.PropertyName);
		}
		else
		{
			Out += FString::Printf(
				TEXT("\n  BROKEN  %s.%s  %s has no delegate '%s'  -> %s"),
				*Binding.ObjectName, *Binding.PropertyName, *Binding.WidgetClass,
				*Binding.DelegateLookedFor, *Binding.FunctionName);
		}
	}

	if (InPlan.StaleCalls.Num() > 0)
	{
		Out += FString::Printf(TEXT("\n%d call node(s) name something the class does not have:"),
			InPlan.StaleCalls.Num());
		for (const FString& Call : InPlan.StaleCalls)
		{
			Out += FString::Printf(TEXT("\n  %s"), *Call);
		}
		Out += TEXT("\n; call nodes are REPORTED and not touched, because removing a node is a "
			"bigger change than this tool makes.");
	}

	if (InPlan.NumBroken() == 0)
	{
		Out += TEXT("\nthe edit would have nothing to do.");
	}
	else
	{
		Out += FString::Printf(
			TEXT("\nthe edit would REMOVE the %d broken binding(s) and leave the rest."),
			InPlan.NumBroken());
	}

	Out += TEXT("\n; this checks widget and delegate presence only, not bound-function existence, signature compatibility or runtime behavior.");
	return Out;
}

FString FTurboMCPWidgetRetarget::Apply(const FPlan& InPlan)
{
	FTurboMCPAssetResolver::FResolved Resolved;
	const FString ResolveError = FTurboMCPAssetResolver::Resolve(InPlan.Asset, Resolved);
	if (!ResolveError.IsEmpty())
	{
		return ResolveError;
	}

	UWidgetBlueprint* WidgetBlueprint = Cast<UWidgetBlueprint>(
		LoadObject<UObject>(nullptr, *Resolved.ObjectPath));
	if (WidgetBlueprint == nullptr)
	{
		return FString::Printf(TEXT("error: %s is not a Widget Blueprint."),
			*Resolved.ObjectPath);
	}

	if (InPlan.NumBroken() == 0)
	{
		return FString::Printf(
			TEXT("unchanged: %s has %d binding(s) and the current classes satisfy all of them, ")
			TEXT("so nothing was removed."),
			*Resolved.ObjectPath, InPlan.Bindings.Num());
	}

	// THE REMOVAL IS BY THE PAIR THE ENGINE USES AS A BINDING'S IDENTITY — `ObjectName` and
	// `PropertyName` — so a binding is removed rather than a list index, which would shift under
	// the loop.
	TArray<FString> Removed;
	int32 Remaining = 0;

	for (int32 Index = WidgetBlueprint->Bindings.Num() - 1; Index >= 0; --Index)
	{
		const FDelegateEditorBinding& Binding = WidgetBlueprint->Bindings[Index];

		// The same test the Plan made, re-made here so the write does not depend on a list that
		// may have been built from an asset read earlier.
		UWidget* TargetWidget = WidgetBlueprint->WidgetTree != nullptr
			? WidgetBlueprint->WidgetTree->FindWidget(FName(*Binding.ObjectName)) : nullptr;

		bool bOk = false;
		if (TargetWidget != nullptr)
		{
			FString LookedFor;
			bOk = FindBindingDelegate(TargetWidget->GetClass(), Binding.PropertyName,
				LookedFor) != EDelegateKind::None;
		}

		if (bOk)
		{
			++Remaining;
			continue;
		}

		Removed.Add(FString::Printf(TEXT("%s.%s"),
			*Binding.ObjectName, *Binding.PropertyName.ToString()));
		WidgetBlueprint->Modify();
		WidgetBlueprint->Bindings.RemoveAt(Index);
	}

	FString Out = FString::Printf(TEXT("changed: removed %d binding(s) from %s"),
		Removed.Num(), *Resolved.ObjectPath);
	Out += FString::Printf(TEXT("\nkept: %d binding(s) the current classes still satisfy"),
		Remaining);

	for (const FString& Name : Removed)
	{
		Out += FString::Printf(TEXT("\n  removed %s, because the widget's current class has no ")
			TEXT("such delegate"), *Name);
	}

	Out += FString::Printf(TEXT("\n%d call node(s) were reported and NOT touched, because ")
		TEXT("removing a node is a bigger change than this tool makes."),
		InPlan.StaleCalls.Num());

	// THE READ-BACK: the bindings are RE-COUNTED from the Blueprint, and the count is compared
	// with what the loop believes it left. Writing `N of N` would have been tautological and
	// proved nothing, which is the trap this project keeps having to avoid.
	int32 StillBroken = 0;
	for (const FDelegateEditorBinding& Binding : WidgetBlueprint->Bindings)
	{
		UWidget* TargetWidget = WidgetBlueprint->WidgetTree != nullptr
			? WidgetBlueprint->WidgetTree->FindWidget(FName(*Binding.ObjectName)) : nullptr;
		bool bOk = false;
		if (TargetWidget != nullptr)
		{
			FString LookedFor;
			bOk = FindBindingDelegate(TargetWidget->GetClass(), Binding.PropertyName,
				LookedFor) != EDelegateKind::None;
		}
		if (!bOk)
		{
			++StillBroken;
		}
	}

	Out += FString::Printf(
		TEXT("\nread back from the Blueprint: %d binding(s) remain, %d of them broken"),
		WidgetBlueprint->Bindings.Num(), StillBroken);

	if (StillBroken > 0)
	{
		// Saying so beats reporting a success whose write did not fully take.
		Out += TEXT("\n; some broken bindings are STILL there, so the removal did not take. ")
			TEXT("Report this rather than trusting the count above.");
	}

	Out += TEXT("\n; changes are in memory; no compile or save was requested. Function and signature validity remain unchecked. Use EditTurboMCPCompile to validate, then EditTurboMCPSave when ready.");

	return Out;
}
