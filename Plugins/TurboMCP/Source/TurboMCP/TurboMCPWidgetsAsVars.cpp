// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "TurboMCPWidgetsAsVars.h"

#include "BaseWidgetBlueprint.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Widget.h"
#include "Containers/UnrealString.h"
#include "UObject/Object.h"
#include "UObject/UObjectGlobals.h"
#include "WidgetBlueprint.h"
#include "WidgetBlueprintOperationUtils.h"

#include "TurboMCPAssetResolver.h"
#include "TurboMCPNodeId.h"
#include "K2Node_Variable.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "EdGraph/EdGraph.h"

bool FTurboMCPWidgetsAsVars::Plan(const FString& InAsset, const FString& InWidgets,
	bool bInIsVariable, FPlan& OutPlan, FString& OutError)
{
	OutError.Empty();

	OutPlan.Asset = InAsset.TrimStartAndEnd();
	if (OutPlan.Asset.IsEmpty())
	{
		OutError = TEXT("no asset given. Pass a Widget Blueprint in any path form, for ")
			TEXT("example WBP_NodeTest.");
		return false;
	}

	OutPlan.bIsVariable = bInIsVariable;

	// The widget names, as a comma-separated list. The same separator the rest of the plugin
	// uses for a list, so a caller does not learn a second one.
	OutPlan.Widgets = FTurboMCPAssetResolver::SplitList(InWidgets);
	if (OutPlan.Widgets.Num() == 0)
	{
		OutError = TEXT("no widgets given. Pass one or more widget names, separated by ")
			TEXT("commas, for example TitleText, BodyText. An empty list is refused rather ")
			TEXT("than treated as \"every widget\", because that would change a whole tree ")
			TEXT("on a typo.");
		return false;
	}

	// A duplicate would be reported twice and toggled twice, which for a flag set to a value
	// is harmless but for a report is confusing. It is dropped rather than refused, because
	// a caller listing a widget twice asked for the same end state.
	TSet<FName> Seen;
	TArray<FString> Unique;
	for (const FString& Name : OutPlan.Widgets)
	{
		if (!Seen.Contains(FName(*Name)))
		{
			Seen.Add(FName(*Name));
			Unique.Add(Name);
		}
	}
	OutPlan.Widgets = MoveTemp(Unique);

	return true;
}

FString FTurboMCPWidgetsAsVars::Apply(const FPlan& InPlan)
{
	FTurboMCPAssetResolver::FResolved Resolved;
	const FString ResolveError = FTurboMCPAssetResolver::Resolve(InPlan.Asset, Resolved);
	if (!ResolveError.IsEmpty())
	{
		return ResolveError;
	}

	UObject* Loaded = LoadObject<UObject>(nullptr, *Resolved.ObjectPath);
	UWidgetBlueprint* WidgetBlueprint = Cast<UWidgetBlueprint>(Loaded);
	if (WidgetBlueprint == nullptr)
	{
		return FString::Printf(
			TEXT("error: %s is a %s, and only a Widget Blueprint has widgets to mark."),
			*Resolved.ObjectPath,
			Loaded != nullptr ? *Loaded->GetClass()->GetName() : TEXT("missing object"));
	}

	UWidgetTree* Tree = WidgetBlueprint->WidgetTree;
	if (Tree == nullptr)
	{
		return FString::Printf(
			TEXT("error: %s has no widget tree, so there is nothing to mark."),
			*Resolved.ObjectPath);
	}

	// Resolve every name before changing flags to avoid partially applying a misspelled list.
	TArray<UWidget*> Targets;
	TArray<FString> Missing;
	for (const FString& Name : InPlan.Widgets)
	{
		UWidget* Widget = Tree->FindWidget(FName(*Name));
		if (Widget == nullptr)
		{
			Missing.Add(Name);
		}
		else
		{
			Targets.Add(Widget);
		}
	}

	if (Missing.Num() > 0)
	{
		// Include available names so the caller can correct the request.
		TArray<FString> Real;
		Tree->ForEachWidget([&Real](UWidget* Widget)
		{
			if (Widget != nullptr)
			{
				Real.Add(Widget->GetFName().ToString());
			}
		});

		return FString::Printf(
			TEXT("unchanged: %d widget name(s) do not exist in %s, so no widget flags were ")
			TEXT("changed: %s. The widgets are: %s"),
			Missing.Num(), *Resolved.ObjectPath,
			*FString::Join(Missing, TEXT(", ")),
			Real.Num() > 0 ? *FString::Join(Real, TEXT(", ")) : TEXT("(none)"));
	}

    // Refuse the entire batch before changing any flags. Existing self-variable
    // nodes would otherwise lose their generated member on skeleton regeneration.
    // This is a local graph check, not a project-wide dependency scan.
    if(!InPlan.bIsVariable)
    {
        TArray<UEdGraph*> Graphs; WidgetBlueprint->GetAllGraphs(Graphs);
        TArray<FString> References;
        for(UWidget* Widget:Targets) if(Widget->bIsVariable)
            for(UEdGraph* Graph:Graphs) if(Graph) for(UEdGraphNode* Node:Graph->Nodes)
                if(const UK2Node_Variable* Variable=Cast<UK2Node_Variable>(Node))
                    if(Variable->VariableReference.IsSelfContext() && Variable->VariableReference.GetMemberName()==Widget->GetFName())
                        References.Add(FString::Printf(TEXT("%s: %s"),*Widget->GetName(),*TurboMCPNodeId::MakeForNode(Node,true)));
        if(!References.IsEmpty())
        {
            References.Sort();
            return FString::Printf(TEXT("unchanged: no widget flags changed. Existing graph references use the requested widget variables: %s. Repair or remove those references before disabling Is Variable. This check covers self-variable nodes in this Blueprint, not external or dynamic references."),*FString::Join(References,TEXT("; ")));
        }
    }

	// The changes, each with the state it had and the state it now has.
	int32 Changed = 0;
	int32 AlreadySet = 0;
	TArray<FString> Lines;

	for (UWidget* Widget : Targets)
	{
		const bool bWas = Widget->bIsVariable;

		if (bWas == InPlan.bIsVariable)
		{
			// Already in the requested state. Reported rather than counted as changed,
			// because a caller that asked for three and got one change needs to know which.
			++AlreadySet;
			Lines.Add(FString::Printf(TEXT("\n  %s: was %s, already %s, not touched"),
				*Widget->GetFName().ToString(),
				bWas ? TEXT("variable") : TEXT("not a variable"),
				bWas ? TEXT("variable") : TEXT("not a variable")));
			continue;
		}

		// The editor's own call, the same one the details panel's checkbox makes.
		FWidgetBlueprintOperationUtils::ToggleWidgetAsVariable(
			WidgetBlueprint, Widget, InPlan.bIsVariable, /*bMarkBlueprintModified=*/true);

		const bool bIs = Widget->bIsVariable;
        if(bIs!=InPlan.bIsVariable)
            return FString::Printf(TEXT("partial: %d widget flag(s) changed before '%s' failed to reach the requested state. Inspect ReadTurboMCPWidgetTree before retrying. No rollback, full compile or save was performed."),Changed,*Widget->GetName());
        ++Changed;
		Lines.Add(FString::Printf(TEXT("\n  %s: was %s, now %s"),
			*Widget->GetFName().ToString(),
			bWas ? TEXT("variable") : TEXT("not a variable"),
			bIs ? TEXT("variable") : TEXT("not a variable")));
	}

	FString Out = FString::Printf(TEXT("%s: %d of %d widget(s) in %s"),
		Changed > 0 ? TEXT("changed") : TEXT("unchanged"),
		Changed, Targets.Num(), *Resolved.ObjectPath);
	Out += FString::Printf(TEXT("\nset to: %s"),
		InPlan.bIsVariable ? TEXT("variable") : TEXT("not a variable"));
	if (AlreadySet > 0)
	{
		Out += FString::Printf(TEXT("\nalready in that state: %d"), AlreadySet);
	}

	for (const FString& Line : Lines)
	{
		Out += Line;
	}

	Out += TEXT("\nIs Variable exposes the existing widget to Blueprint graph references; it does not create a widget, change its appearance or wire graph nodes.");
	Out += TEXT("\n; Changes are in memory; this tool does not save the asset.");
	Out += TEXT("\n; Changing a flag may regenerate the Blueprint skeleton and run editor callbacks; this is not a full compile.");
	Out += TEXT("\n; No dedicated undo transaction is created. Review the flags, then use EditTurboMCPCompile and EditTurboMCPSave as needed.");

	return Out;
}
