// Copyright (c) 2026 FloStyle
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

// Validate designer-binding compatibility before replacing any existing metadata.
FString UTurboMCPEditToolset::EditTurboMCPWidgetBindAdd(
	const FString& Asset, const FString& Widget, const FString& Property, const FString& Function)
{
	const FString WidgetName = Widget.TrimStartAndEnd();
	const FString PropertyName = Property.TrimStartAndEnd();
	const FString FunctionName = Function.TrimStartAndEnd();

	if (WidgetName.IsEmpty() || PropertyName.IsEmpty())
	{
		return TEXT("error: a widget name and a property name are both required. The property ")
			TEXT("is a bindable property, for example Text on a TextBlock.");
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


    // Match the single-cast delegate path used by FDelegateEditorBinding.
    // Property names are stored without the Delegate suffix for native property bindings.
    UClass* WidgetClass=TargetWidget->GetClass();
    FString CanonicalProperty=PropertyName;
    FDelegateProperty* Delegate=FindFProperty<FDelegateProperty>(WidgetClass,FName(*(PropertyName+TEXT("Delegate"))));
    bool NeedsPure=Delegate!=nullptr;
    if(!Delegate) {
        Delegate=FindFProperty<FDelegateProperty>(WidgetClass,FName(*PropertyName));
        if(Delegate && PropertyName.EndsWith(TEXT("Delegate"),ESearchCase::IgnoreCase)) {
            CanonicalProperty=PropertyName.LeftChop(8);NeedsPure=true;
        }
    }
    if(!Delegate || !Delegate->SignatureFunction)
        return FString::Printf(TEXT("unchanged: '%s' is not a supported single-cast designer binding on %s. No binding changed. Multicast events such as OnClicked need a graph Bind Event node; PreviewTurboMCPBindNewEvent can preview its handler."),*PropertyName,*WidgetClass->GetPathName());
    // Use reflected casing so alias/case variants identify the same binding.
    CanonicalProperty=NeedsPure?Delegate->GetName().LeftChop(8):Delegate->GetName();
    const FName WantedFunction(*(FunctionName.IsEmpty()?TargetWidget->GetName()+TEXT("_On")+CanonicalProperty:FunctionName));
    UClass* SignatureClass=WidgetBlueprint->SkeletonGeneratedClass;
    UFunction* TargetFunction=SignatureClass?SignatureClass->FindFunctionByName(WantedFunction):nullptr;
    if(!TargetFunction)
        return FString::Printf(TEXT("unchanged: function '%s' was not found in the Widget Blueprint's reflected skeleton. No binding changed. Create the function first; compile if reflection is stale, then retry."),*WantedFunction.ToString());
    if(!TargetFunction->IsSignatureCompatibleWith(Delegate->SignatureFunction,UFunction::GetDefaultIgnoredSignatureCompatibilityFlags()|CPF_ReturnParm))
        return FString::Printf(TEXT("unchanged: function '%s' has an incompatible signature for %s.%s. No binding changed. Match the delegate's parameter and return types; a text property getter returns text with no input parameters."),*TargetFunction->GetName(),*TargetWidget->GetName(),*CanonicalProperty);
    if(NeedsPure && !TargetFunction->HasAnyFunctionFlags(FUNC_Const|FUNC_BlueprintPure))
        return FString::Printf(TEXT("unchanged: property binding '%s' requires a pure or const function; '%s' is neither. No binding changed."),*CanonicalProperty,*TargetFunction->GetName());
    auto Matches=[&](const FDelegateEditorBinding& B) {
        FString Name=B.PropertyName.ToString();
        if(NeedsPure && Name.Equals(Delegate->GetName(),ESearchCase::IgnoreCase))Name=CanonicalProperty;
        return FName(*B.ObjectName)==TargetWidget->GetFName() && FName(*Name)==FName(*CanonicalProperty);
    };
    TArray<FString> Previous;
    int32 MatchCount=0;bool Same=false;
    for(const FDelegateEditorBinding& B:WidgetBlueprint->Bindings)if(Matches(B)) {
        ++MatchCount;Previous.Add(B.FunctionName.ToString());
        Same=B.Kind==EBindingKind::Function && B.FunctionName==WantedFunction && B.SourcePath.IsEmpty()
            && !B.MemberGuid.IsValid() && B.PropertyName==FName(*CanonicalProperty) && B.ObjectName==TargetWidget->GetName();
    }
    if(MatchCount==1 && Same)
        return FString::Printf(TEXT("unchanged: %s.%s already calls %s. No binding metadata changed; no compile or save was requested."),*TargetWidget->GetName(),*CanonicalProperty,*TargetFunction->GetName());
    FDelegateEditorBinding Binding;
    Binding.ObjectName=TargetWidget->GetName();Binding.PropertyName=FName(*CanonicalProperty);
    Binding.FunctionName=TargetFunction->GetFName();Binding.Kind=EBindingKind::Function;
    WidgetBlueprint->Modify();
    WidgetBlueprint->Bindings.RemoveAll(Matches);
    WidgetBlueprint->Bindings.Add(Binding);
    FBlueprintEditorUtils::MarkBlueprintAsModified(WidgetBlueprint);
    return FString::Printf(TEXT("changed: bound %s.%s to %s in %s\ndelegate: %s\nreplaced functions: %s\nValidated the reflected signature%s. This does not prove runtime behavior.\nBinding metadata changed in memory. No full compile, save or dedicated undo transaction was performed. Compile with EditTurboMCPCompile, then inspect the displayed value before saving."),
        *Binding.ObjectName,*CanonicalProperty,*TargetFunction->GetName(),*Resolved.ObjectPath,*Delegate->GetName(),
        Previous.IsEmpty()?TEXT("none"):*FString::Join(Previous,TEXT(", ")),NeedsPure?TEXT(" and pure/const requirement"):TEXT(""));
}
