// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "Tools/TurboMCPToolsets.h"

#include "Containers/UnrealString.h"

#include "TurboMCPWidgetTreeSync.h"

#include "TurboMCPAssetResolver.h"
#include "Editor.h"
#include "WidgetBlueprint.h"
#include "WidgetBlueprintOperationUtils.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Widget.h"
#include "Components/PanelWidget.h"
#include "Components/PanelSlot.h"
#include "UObject/UnrealType.h"
#include "Misc/OutputDevice.h"
#include "ScopedTransaction.h"
#include "Misc/ScopeExit.h"
#include "Kismet2/Kismet2NameValidators.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Animation/WidgetAnimation.h"
#include "EdGraph/EdGraph.h"
#include "K2Node_Variable.h"
#include "K2Node_ComponentBoundEvent.h"
#include "TurboMCPNodeId.h"
#include "Blueprint/WidgetNavigation.h"

// Read selected widget-tree data through the shared inspector; no writer is invoked here.
FString UTurboMCPReadToolset::ReadTurboMCPWidgetTree(const FString& Asset)
{
	const FString Text = FTurboMCPWidgetTreeSync::Read(Asset);

	// `Read` already prefixes its own refusals, so this only ensures exactly one prefix survives. The guard
	// is on the RESULT rather than on a flag, because a flag can disagree with the string it describes.
	return Text.StartsWith(TEXT("error:")) || Text.StartsWith(TEXT("read:"))
		? Text
		: FString::Printf(TEXT("read: %s"), *Text);
}


// Use the editor's rename operation so graph and designer references follow the name.
// This does not rewrite arbitrary strings or promise a project-wide reference migration.

FString UTurboMCPEditToolset::EditTurboMCPWidgetRename(
    const FString& Asset, const FString& Widget, const FString& NewName)
{
    if (!IsInGameThread() || !GEditor || !GEditor->CanTransact() ||
        GEditor->IsPlaySessionInProgress() || GEditor->IsTransactionActive())
        return TEXT("error: Widget rename requires an idle editor outside PIE and transactions. Nothing changed.");
    // Reject rather than silently sanitize a requested identifier.
    const FString Name = NewName.TrimStartAndEnd();
    if (Name.IsEmpty() || Name.Len() > 100 ||
        !(FChar::IsAlpha(Name[0]) || Name[0] == TCHAR('_')))
        return TEXT("error: NewName must be an identifier of 1 to 100 letters, digits or underscores, starting with a letter or underscore. Nothing changed.");
    for (TCHAR Character : Name)
        if (!(FChar::IsAlnum(Character) || Character == TCHAR('_')))
            return TEXT("error: NewName must contain only letters, digits or underscores. Display labels with spaces are not supported. Nothing changed.");
    FTurboMCPAssetResolver::FResolved Resolved;
    const FString Error = FTurboMCPAssetResolver::Resolve(Asset, Resolved);
    if (!Error.IsEmpty()) return Error;
    UWidgetBlueprint* Blueprint = LoadObject<UWidgetBlueprint>(nullptr, *Resolved.ObjectPath);
    if (!Blueprint || !Blueprint->WidgetTree)
        return TEXT("error: Asset must be a Widget Blueprint with a widget tree. Nothing changed.");
    UWidget* Target = Blueprint->WidgetTree->FindWidget(FName(*Widget.TrimStartAndEnd()));
    if (!Target)
        return TEXT("error: Widget was not found in the named asset. Use ReadTurboMCPWidgetTree for its exact designer object name. Nothing changed.");
    const FString OldName = Target->GetName();
    if (Target->GetFName() == FName(*Name))
        return FString::Printf(TEXT("unchanged: Widget is already named %s (names are case-insensitive)."), *OldName);
    FKismetNameValidator Validator(Blueprint, Target->GetFName());
    if (Blueprint->WidgetTree->FindWidget(FName(*Name)) || Validator.IsValid(Name) != EValidatorResult::Ok)
        return TEXT("error: NewName conflicts with an existing widget, Blueprint member or reserved name. Choose an unused identifier. Nothing changed.");
    // The stock operation owns its undo transaction and structural refresh. It also
    // updates designer bindings, animation bindings, navigation and graph references.
    const bool bRenamed = FWidgetBlueprintOperationUtils::RenameWidget(Blueprint, Target, Name);
    if (!bRenamed || Target->GetFName() != FName(*Name))
        return FString::Printf(TEXT("error: Unreal did not confirm the requested rename from %s to %s. Current widget name: %s. Inspect the tree before retrying; no save was requested."), *OldName, *Name, *Target->GetName());
    return FString::Printf(TEXT("edited: Renamed widget %s to %s in %s using Unreal's editor rename operation. Graph and designer references handled by Unreal were refreshed. Dynamic string lookups and external assets are not verified. Structural refresh may affect loaded dependent Blueprints. Compile and inspect references before saving; no full compile or save was requested."), *OldName, *Target->GetName(), *Resolved.ObjectPath);
}

namespace
{
FString RemoveDesignerWidget(const FString& Asset, const FString& Widget, bool bApply)
{
    if (!IsInGameThread() || !GEditor || !GEditor->CanTransact() ||
        GEditor->IsPlaySessionInProgress() || GEditor->IsTransactionActive())
        return TEXT("error: Widget removal requires an idle editor outside PIE and transactions. Nothing changed.");
    FTurboMCPAssetResolver::FResolved Resolved;
    const FString Error = FTurboMCPAssetResolver::Resolve(Asset, Resolved);
    if (!Error.IsEmpty()) return Error;
    UWidgetBlueprint* Blueprint = LoadObject<UWidgetBlueprint>(nullptr, *Resolved.ObjectPath);
    if (!Blueprint || !Blueprint->WidgetTree)
        return TEXT("error: Asset must be a Widget Blueprint with a widget tree. Nothing changed.");
    UWidget* Target = Blueprint->WidgetTree->FindWidget(FName(*Widget.TrimStartAndEnd()));
    if (!Target)
        return TEXT("error: Widget was not found. Use ReadTurboMCPWidgetTree for its exact designer object name. Nothing changed.");
    TSet<FName> Names;
    TArray<FString> RemovedNames;
    // The engine visitor enumerates descendants only; include the selected root.
    Names.Add(Target->GetFName());
    RemovedNames.Add(Target->GetName());
    UWidgetTree::ForWidgetAndChildren(Target, [&](UWidget* Item)
    {
        Names.Add(Item->GetFName());
        RemovedNames.Add(Item->GetName());
    });
    RemovedNames.Sort();
    TArray<FString> Blockers;
    for (const FName Name : Names)
        if (FBlueprintEditorUtils::IsVariableUsed(Blueprint, Name))
            Blockers.Add(FString::Printf(TEXT("graph reference to %s"), *Name.ToString()));
    // GUID-based engine lookup can miss stale/uncompiled widget variables. Scan
    // explicit local references by name as well, including bound widget events.
    TArray<UEdGraph*> Graphs;
    Blueprint->GetAllGraphs(Graphs);
    for (UEdGraph* Graph : Graphs)
        if (Graph)
            for (UEdGraphNode* Node : Graph->Nodes)
            {
                const UK2Node_Variable* Variable = Cast<UK2Node_Variable>(Node);
                const UK2Node_ComponentBoundEvent* Event = Cast<UK2Node_ComponentBoundEvent>(Node);
                if ((Variable && Variable->VariableReference.IsSelfContext() && Names.Contains(Variable->VariableReference.GetMemberName())) ||
                    (Event && Names.Contains(Event->GetComponentPropertyName())))
                    Blockers.Add(FString::Printf(TEXT("graph node %s references this subtree"), *TurboMCPNodeId::MakeForNode(Node, true)));
            }
    // Refuse designer references as well: the stock delete operation can remove
    // some bindings silently, which is inappropriate for a reference-checked tool.
    for (const FDelegateEditorBinding& Binding : Blueprint->Bindings)
        if (Names.Contains(FName(*Binding.ObjectName)))
            Blockers.Add(FString::Printf(TEXT("designer binding on %s"), *Binding.ObjectName));
    for (const UWidgetAnimation* Animation : Blueprint->Animations)
        if (Animation)
            for (const FWidgetAnimationBinding& Binding : Animation->AnimationBindings)
                if (Names.Contains(Binding.WidgetName))
                    Blockers.Add(FString::Printf(TEXT("animation %s uses %s"), *Animation->GetName(), *Binding.WidgetName.ToString()));
    Blueprint->WidgetTree->ForEachWidget([&](UWidget* Item)
    {
        if (Names.Contains(Item->GetFName()) || !Item->Navigation) return;
        for (const FWidgetNavigationData* Rule : { &Item->Navigation->Up, &Item->Navigation->Down,
            &Item->Navigation->Left, &Item->Navigation->Right, &Item->Navigation->Next, &Item->Navigation->Previous })
            if (Rule->Rule == EUINavigationRule::Explicit && Names.Contains(Rule->WidgetToFocus))
                Blockers.Add(FString::Printf(TEXT("explicit navigation from %s to %s"), *Item->GetName(), *Rule->WidgetToFocus.ToString()));
    });
    if (!Blockers.IsEmpty())
    {
        Blockers.Sort();
        return FString::Printf(TEXT("unchanged: Widget removal refused. Resolve these references before retrying:\n%s\nNothing was removed. Checks cover this Blueprint, not external assets or dynamic string lookups."), *FString::Join(Blockers, TEXT("\n")));
    }
    const FString Scope = FString::Printf(TEXT("asset: %s\nwidgets (%d): %s\nChecks cover graph references, designer bindings, animation bindings and explicit widget navigation in this Blueprint. External assets, dynamic string lookups and custom reference systems are not verified."), *Resolved.ObjectPath, RemovedNames.Num(), *FString::Join(RemovedNames, TEXT(", ")));
    if (!bApply)
        return TEXT("preview: Remove the named widget and its descendants. No changes made.\n") + Scope + TEXT("\nEdit replans from current state. Removing the root leaves an empty designer tree. Compile and inspect the result before saving.");
    // The engine owns the undo transaction, tree detachment and structural refresh.
    // All supported reference checks above run before that destructive operation.
    FText Failure;
    if (!FWidgetBlueprintOperationUtils::RemoveWidget(Blueprint, Target, Failure))
        return TEXT("error: Unreal did not confirm widget removal: ") + Failure.ToString() + TEXT(". Inspect the tree before retrying; no save was requested.");
    for (const FName Name : Names)
        if (Blueprint->WidgetTree->FindWidget(Name))
            return TEXT("partial: A requested widget remains after removal. Inspect ReadTurboMCPWidgetTree before retrying. No save was requested.");
    return TEXT("edited: Removed the widget subtree.\n") + Scope + TEXT("\nStructural refresh requested; loaded dependent Blueprints may be affected. No full compile or save was requested. Compile and inspect the result before saving.");
}
}

FString UTurboMCPPreviewToolset::PreviewTurboMCPWidgetRemove(const FString& Asset, const FString& Widget)
{
    return RemoveDesignerWidget(Asset, Widget, false);
}

FString UTurboMCPEditToolset::EditTurboMCPWidgetRemove(const FString& Asset, const FString& Widget)
{
    return RemoveDesignerWidget(Asset, Widget, true);
}

namespace
{
FString AddDesignerWidget(const FString& Asset, const FString& Parent, const FString& WidgetClass,
    const FString& Name, bool bApply)
{
    if (!IsInGameThread() || !GEditor || !GEditor->CanTransact() || GEditor->IsPlaySessionInProgress() || GEditor->IsTransactionActive())
        return TEXT("error: Widget add requires an idle editor outside PIE and transactions. Nothing changed.");
    const FString NewName = Name.TrimStartAndEnd();
    if (NewName.IsEmpty() || NewName.Len() > 100 || !(FChar::IsAlpha(NewName[0]) || NewName[0] == TCHAR('_')))
        return TEXT("error: Name must be an identifier of 1 to 100 letters, digits or underscores, starting with a letter or underscore. Nothing changed.");
    for (TCHAR Character : NewName)
        if (!(FChar::IsAlnum(Character) || Character == TCHAR('_')))
            return TEXT("error: Name must contain only letters, digits or underscores. Nothing changed.");
    FTurboMCPAssetResolver::FResolved Resolved;
    FString Error = FTurboMCPAssetResolver::Resolve(Asset, Resolved);
    if (!Error.IsEmpty()) return Error;
    UWidgetBlueprint* Blueprint = LoadObject<UWidgetBlueprint>(nullptr, *Resolved.ObjectPath);
    if (!Blueprint || !Blueprint->WidgetTree)
        return TEXT("error: Asset must be a Widget Blueprint with a widget tree. Nothing changed.");
    UWidget* ParentWidget = Blueprint->WidgetTree->FindWidget(FName(*Parent.TrimStartAndEnd()));
    if (!ParentWidget)
        return TEXT("error: Parent must name an existing designer panel. Use ReadTurboMCPWidgetTree. Empty does not select the root. Nothing changed.");
    FText Failure;
    if (!FWidgetBlueprintOperationUtils::CanAddToParent(Blueprint, ParentWidget, Failure))
        return TEXT("error: Parent cannot accept a child: ") + Failure.ToString() + TEXT(" Nothing changed.");
    FKismetNameValidator Validator(Blueprint);
    if (Validator.IsValid(NewName) != EValidatorResult::Ok ||
        StaticFindObject(UObject::StaticClass(), Blueprint->WidgetTree, *NewName))
        return TEXT("error: Name conflicts with a widget, Blueprint member, reserved name or tree object. Choose an unused identifier. Nothing changed.");
    UClass* Class = nullptr;
    const FString ClassInput = WidgetClass.TrimStartAndEnd();
    if (ClassInput.StartsWith(TEXT("/Script/")))
        Class = LoadObject<UClass>(nullptr, *ClassInput);
    else
    {
        FTurboMCPAssetResolver::FResolved ClassAsset;
        Error = FTurboMCPAssetResolver::Resolve(ClassInput, ClassAsset);
        if (!Error.IsEmpty()) return Error;
        UWidgetBlueprint* Source = LoadObject<UWidgetBlueprint>(nullptr, *ClassAsset.ObjectPath);
        if (!Source || !Source->GeneratedClass || (Source->Status != BS_UpToDate && Source->Status != BS_UpToDateWithWarnings))
            return TEXT("error: WidgetClass must be a compiled Widget Blueprint or a native /Script/Module.Class widget class. Compile the source widget first. Nothing changed.");
        Class = Source->GeneratedClass;
    }
    if (!Class || !Class->IsChildOf(UWidget::StaticClass()) || Class->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists))
        return TEXT("error: WidgetClass must be a concrete, current widget class. Nothing changed.");
    // Traverse class ancestry and designer dependencies before constructing anything.
    // This also catches an indirect dependency back to the destination Blueprint.
    TArray<UClass*> Pending { Class };
    TSet<UClass*> Visited;
    while (!Pending.IsEmpty())
    {
        UClass* Current = Pending.Pop();
        if (!Current || Visited.Contains(Current)) continue;
        Visited.Add(Current);
        if (Current->ClassGeneratedBy == Blueprint)
            return TEXT("error: Adding this class would create a circular widget dependency. Nothing changed.");
        Pending.Add(Current->GetSuperClass());
        if (UWidgetBlueprint* Dependency = Cast<UWidgetBlueprint>(Current->ClassGeneratedBy))
            if (Dependency->WidgetTree)
                Dependency->WidgetTree->ForEachWidget([&](UWidget* Item) { Pending.Add(Item->GetClass()); });
    }
    const FString Details = FString::Printf(TEXT("asset: %s\nparent: %s\nwidget: %s\nclass: %s"), *Resolved.ObjectPath, *ParentWidget->GetName(), *NewName, *Class->GetPathName());
    if (!bApply)
        return TEXT("preview: Append one widget to the named parent. No widget constructed or added.\n") + Details + TEXT("\nClass defaults and a new default parent slot will be used. Configure layout separately. Edit replans from current state.");
    // The stock palette path creates a designer template, not a live PIE widget.
    // Native constructors/palette hooks may run; AddWidget owns the transaction.
    UWidget* Created = Blueprint->WidgetTree->ConstructWidget<UWidget>(Class, FName(*NewName));
    if (Created) Created->CreatedFromPalette();
    if (!Created)
        return TEXT("error: Unreal did not create the designer widget. No child was attached; no save was requested.");
    if (Created->GetFName() != FName(*NewName))
    {
        Created->Rename(nullptr, GetTransientPackage(), REN_DontCreateRedirectors | REN_NonTransactional);
        return TEXT("error: Unreal changed the requested name. The unattached template was discarded; no child was attached or saved.");
    }
    if (!FWidgetBlueprintOperationUtils::AddWidget(Blueprint, Created, ParentWidget, -1, Failure))
        return TEXT("error: Unreal did not confirm widget attachment: ") + Failure.ToString() + TEXT(" Inspect the tree before retrying; no save was requested.");
    if (Blueprint->WidgetTree->FindWidget(FName(*NewName)) != Created || Created->GetParent() != ParentWidget)
        return TEXT("partial: Widget attachment could not be verified. Inspect ReadTurboMCPWidgetTree before retrying. No save was requested.");
    return TEXT("edited: Added one widget.\n") + Details + FString::Printf(TEXT("\nslot class: %s\nClass defaults and default slot layout were used. Structural refresh requested; loaded dependent Blueprints may be affected. Native constructors and palette hooks may run. No full compile or save was requested. Read the tree, configure layout and compile before saving."), Created->Slot ? *Created->Slot->GetClass()->GetPathName() : TEXT("none"));
}
}

FString UTurboMCPPreviewToolset::PreviewTurboMCPWidgetAdd(const FString& Asset, const FString& Parent, const FString& WidgetClass, const FString& Name)
{
    return AddDesignerWidget(Asset, Parent, WidgetClass, Name, false);
}

FString UTurboMCPEditToolset::EditTurboMCPWidgetAdd(const FString& Asset, const FString& Parent, const FString& WidgetClass, const FString& Name)
{
    return AddDesignerWidget(Asset, Parent, WidgetClass, Name, true);
}

namespace
{
// Deliberately exclude reference-bearing data and localized text: this operation
// changes layout values, not object ownership, delegates or localization identity.
bool IsWidgetValueType(const FProperty* Property, int32 Depth = 0)
{
    if (!Property || Property->ArrayDim != 1 || Depth > 12) return false;
    if (const FStructProperty* Struct = CastField<FStructProperty>(Property))
    {
        for (TFieldIterator<FProperty> It(Struct->Struct); It; ++It)
            if (!IsWidgetValueType(*It, Depth + 1)) return false;
        return true;
    }
    return CastField<FNumericProperty>(Property) || CastField<FBoolProperty>(Property) ||
        CastField<FEnumProperty>(Property) || CastField<FStrProperty>(Property) || CastField<FNameProperty>(Property);
}

bool IsEditableWidgetValue(const FProperty* Property)
{
    return Property && Property->HasAnyPropertyFlags(CPF_Edit) &&
        !Property->HasAnyPropertyFlags(CPF_EditConst | CPF_DisableEditOnTemplate | CPF_Transient) &&
        Property->GetFName() != TEXT("bIsVariable") && IsWidgetValueType(Property);
}

bool IsFiniteWidgetValue(const FProperty* Property, const void* Value)
{
    if (const FNumericProperty* Number = CastField<FNumericProperty>(Property))
        if (Number->IsFloatingPoint()) return FMath::IsFinite(Number->GetFloatingPointPropertyValue(Value));
    if (const FStructProperty* Struct = CastField<FStructProperty>(Property))
        for (TFieldIterator<FProperty> It(Struct->Struct); It; ++It)
            if (!IsFiniteWidgetValue(*It, It->ContainerPtrToValuePtr<void>(Value))) return false;
    return true;
}

struct FWidgetImportErrors : FOutputDevice
{
    FString Message;
    virtual void Serialize(const TCHAR* Text, ELogVerbosity::Type Verbosity, const FName& Category) override
    {
        if (!Message.IsEmpty()) Message += TEXT("; ");
        Message += Text;
    }
};

FString WidgetPropertyOperation(const FString& Asset, const FString& Widget, const FString& Target,
    const FString& PropertyName, const FString* Input, bool bApply)
{
    if (!IsInGameThread() || !GEditor || (Input && (!GEditor->CanTransact() || GEditor->IsPlaySessionInProgress() || GEditor->IsTransactionActive())))
        return TEXT("error: Property writes and previews require an idle editor outside PIE and transactions. Nothing changed.");
    FTurboMCPAssetResolver::FResolved Resolved;
    const FString Error = FTurboMCPAssetResolver::Resolve(Asset, Resolved);
    if (!Error.IsEmpty()) return Error;
    UWidgetBlueprint* Blueprint = LoadObject<UWidgetBlueprint>(nullptr, *Resolved.ObjectPath);
    if (!Blueprint || !Blueprint->WidgetTree) return TEXT("error: Asset must be a Widget Blueprint with a widget tree.");
    UWidget* Item = Blueprint->WidgetTree->FindWidget(FName(*Widget.TrimStartAndEnd()));
    if (!Item) return TEXT("error: Widget not found. Use ReadTurboMCPWidgetTree for its exact designer object name.");
    const FString TargetName = Target.TrimStartAndEnd().ToLower();
    if (TargetName != TEXT("widget") && TargetName != TEXT("slot")) return TEXT("error: Target must be widget or slot.");
    UObject* Object = TargetName == TEXT("slot") ? static_cast<UObject*>(Item->Slot.Get()) : Item;
    if (!Object) return TEXT("error: This widget has no parent slot. Select Target=widget or choose a child widget.");
    const FString Requested = PropertyName.TrimStartAndEnd();
    if (Requested.IsEmpty() && Input) return TEXT("error: Property is required for a write. Read properties first; empty never means all properties.");
    auto Export = [Object](const FProperty* Property, const void* Value)
    {
        FString Text;
        Property->ExportTextItem_Direct(Text, Value, nullptr, Object, PPF_None);
        return Text;
    };
    const FString Identity = FString::Printf(TEXT("asset: %s\nwidget: %s\ntarget: %s\nclass: %s"), *Resolved.ObjectPath, *Item->GetName(), *TargetName, *Object->GetClass()->GetPathName());
    if (Requested.IsEmpty())
    {
        TArray<FString> Lines;
        for (TFieldIterator<FProperty> It(Object->GetClass()); It; ++It)
            if (IsEditableWidgetValue(*It))
                Lines.Add(FString::Printf(TEXT("%s [%s] = %s"), *It->GetName(), *It->GetCPPType(), *Export(*It, It->ContainerPtrToValuePtr<void>(Object))));
        Lines.Sort();
        return TEXT("read: Supported editable value properties.\n") + Identity + TEXT("\n") + FString::Join(Lines, TEXT("\n")) + TEXT("\nValues use Unreal property text, not JSON. Objects, containers, delegates and localized text are omitted; this is not a full object export.");
    }
    FProperty* Property = FindFProperty<FProperty>(Object->GetClass(), FName(*Requested));
    if (!IsEditableWidgetValue(Property))
        return TEXT("error: Property is missing, read-only or outside supported value types. Use ReadTurboMCPWidgetProperty with empty Property to list supported names. Nothing changed.");
    void* Live = Property->ContainerPtrToValuePtr<void>(Object);
    const FString Before = Export(Property, Live);
    const FString Header = Identity + FString::Printf(TEXT("\nproperty: %s\ntype: %s"), *Property->GetName(), *Property->GetCPPType());
    if (!Input) return TEXT("read: Widget property.\n") + Header + TEXT("\nvalue: ") + Before;
    if (Input->Len() > 65536) return TEXT("error: Value exceeds 65536 characters. Nothing changed.");
    // Import into an initialized copy. Incomplete structs retain unspecified fields.
    void* Temporary = FMemory::Malloc(Property->GetSize(), Property->GetMinAlignment());
    Property->InitializeValue(Temporary);
    ON_SCOPE_EXIT { Property->DestroyValue(Temporary); FMemory::Free(Temporary); };
    Property->CopyCompleteValue(Temporary, Live);
    FWidgetImportErrors Errors;
    const TCHAR* End = Property->ImportText_Direct(**Input, Temporary, Object, PPF_None, &Errors);
    if (!End || !FString(End).TrimStartAndEnd().IsEmpty() || !Errors.Message.IsEmpty() || !IsFiniteWidgetValue(Property, Temporary))
        return TEXT("error: Value could not be read completely as the property's Unreal text type, or contains a non-finite number. Nothing changed. Read the current value for its format. ") + Errors.Message;
    const FString Proposed = Export(Property, Temporary);
    if (!bApply) return TEXT("preview: Property value only; nothing changed.\n") + Header + TEXT("\nbefore: ") + Before + TEXT("\nafter: ") + Proposed + TEXT("\nEdit reparses against current state. Compile/save are separate.");
    if (Property->Identical(Live, Temporary)) return TEXT("unchanged: Property already has the requested value.\n") + Header + TEXT("\nvalue: ") + Before;
    const FScopedTransaction Transaction(NSLOCTEXT("TurboMCP", "WidgetProperty", "Set widget property"));
    Blueprint->Modify();
    Object->SetFlags(RF_Transactional);
    Object->Modify();
    Object->PreEditChange(Property);
    Property->CopyCompleteValue(Live, Temporary);
    FPropertyChangedEvent Changed(Property, EPropertyChangeType::ValueSet);
    Object->PostEditChangeProperty(Changed);
    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
    const FString After = Export(Property, Property->ContainerPtrToValuePtr<void>(Object));
    return TEXT("edited: Property updated; current value read back after editor notification.\n") + Header + TEXT("\nbefore: ") + Before + TEXT("\nafter: ") + After + TEXT("\nNative property-change hooks may run. This does not prove rendered layout or runtime behavior. No compile or save was requested.");
}
}

FString UTurboMCPReadToolset::ReadTurboMCPWidgetProperty(const FString& Asset, const FString& Widget, const FString& Target, const FString& Property)
{
    return WidgetPropertyOperation(Asset, Widget, Target, Property, nullptr, false);
}
FString UTurboMCPPreviewToolset::PreviewTurboMCPWidgetProperty(const FString& Asset, const FString& Widget, const FString& Target, const FString& Property, const FString& Value)
{
    return WidgetPropertyOperation(Asset, Widget, Target, Property, &Value, false);
}
FString UTurboMCPEditToolset::EditTurboMCPWidgetProperty(const FString& Asset, const FString& Widget, const FString& Target, const FString& Property, const FString& Value)
{
    return WidgetPropertyOperation(Asset, Widget, Target, Property, &Value, true);
}
