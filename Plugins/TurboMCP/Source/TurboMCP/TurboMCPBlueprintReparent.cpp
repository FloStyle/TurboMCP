// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
#include "Tools/TurboMCPToolsets.h"
#include "BlueprintEditorLibrary.h"
#include "Blueprint/UserWidget.h"
#include "Editor.h"
#include "EdGraph/EdGraph.h"
#include "Engine/Blueprint.h"
#include "GameFramework/Actor.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "TurboMCPLogCapture.h"
#include "UObject/UnrealType.h"
#include "WidgetBlueprint.h"

namespace
{
const TCHAR* CompilerStatus(EBlueprintStatus Status)
{
    switch (Status)
    {
    case BS_UpToDate: return TEXT("up to date");
    case BS_UpToDateWithWarnings: return TEXT("up to date with warnings");
    case BS_Error: return TEXT("compile error");
    case BS_Dirty: return TEXT("requires compilation");
    case BS_BeingCreated: return TEXT("being created");
    default: return TEXT("unknown");
    }
}
// Reparenting includes compilation and reinstancing. It is not a normal undoable
// graph edit; do not wrap it in a transaction and imply reliable rollback.
FString Reparent(bool Edit, const FString& Asset, const FString& NewParent,
    const FString& ExpectedParent, const FString& Confirmation)
{
    auto Refuse=[](const FString& Reason) { return TEXT("unchanged\nrefused: ")+Reason; };
    if (!IsInGameThread() || !GEditor || GEditor->IsPlaySessionInProgress() || GEditor->IsTransactionActive())
        return Refuse(TEXT("Requires the idle editor game thread, no play session and no active transaction."));
    if (!Asset.StartsWith(TEXT("/")) || !NewParent.StartsWith(TEXT("/")))
        return Refuse(TEXT("Asset and NewParent must be exact object paths, not display names."));
    UBlueprint* BP=LoadObject<UBlueprint>(nullptr,*Asset);
    if (BP == nullptr)
    {
        return Refuse(TEXT("Could not load the Blueprint asset at the requested path."));
    }
    UClass* Parent=LoadObject<UClass>(nullptr,*NewParent);
    if (Parent == nullptr)
    {
        return Refuse(TEXT("Could not load the new parent class at the requested path."));
    }
    if (BP->GetPathName()!=Asset || Parent->GetPathName()!=NewParent)
        return Refuse(TEXT("Resolved paths differ from the requested asset or parent. Use their exact object paths."));
    if (BP->GetClass()!=UBlueprint::StaticClass() && BP->GetClass()!=UWidgetBlueprint::StaticClass())
        return Refuse(TEXT("Specialized Blueprint types, such as animation or level Blueprints, are unsupported."));
    if (BP->BlueprintType!=BPTYPE_Normal || !BP->ParentClass || BP->bBeingCompiled || BP->bQueuedForCompilation)
        return Refuse(TEXT("Requires a normal Blueprint with an existing parent that is not compiling."));
    if (Parent->HasAnyClassFlags(CLASS_Deprecated|CLASS_NewerVersionExists|CLASS_Interface)
        || !FKismetEditorUtilities::CanCreateBlueprintOfClass(Parent))
        return Refuse(TEXT("NewParent is not an eligible current Blueprint parent class."));
    if (Parent->ClassGeneratedBy==BP || (BP->GeneratedClass && Parent->IsChildOf(BP->GeneratedClass)))
        return Refuse(TEXT("NewParent would create an inheritance cycle."));
    const bool IsWidget=BP->IsA<UWidgetBlueprint>();
    if (IsWidget!=Parent->IsChildOf(UUserWidget::StaticClass())
        || BP->ParentClass->IsChildOf(AActor::StaticClass())!=Parent->IsChildOf(AActor::StaticClass()))
        return Refuse(TEXT("NewParent belongs to an incompatible Blueprint family (widget, actor or object)."));
    const FString OldParent=BP->ParentClass->GetPathName();
    if (Edit && ExpectedParent!=OldParent)
        return Refuse(TEXT("ExpectedParent differs from the current parent. Preview again before confirming."));
    if (BP->ParentClass==Parent) return TEXT("unchanged\nThe Blueprint already uses parent: ")+OldParent;
    // A collision must be resolved deliberately, not left to compiler renaming.
    for (const FBPVariableDescription& Variable:BP->NewVariables)
        if (FindFProperty<FProperty>(Parent,Variable.VarName))
            return Refuse(TEXT("Owned variable conflicts with a new inherited property: ")+Variable.VarName.ToString());
    for (const FBPInterfaceDescription& Interface:BP->ImplementedInterfaces)
        if (Interface.Interface && Parent->ImplementsInterface(Interface.Interface))
            return Refuse(TEXT("NewParent already implements an owned interface: ")+Interface.Interface->GetPathName());
    for (const UEdGraph* Graph:BP->FunctionGraphs)
        if (Graph && Parent->FindFunctionByName(Graph->GetFName()) && !BP->ParentClass->FindFunctionByName(Graph->GetFName()))
            return Refuse(TEXT("Owned function conflicts with a newly inherited function: ")+Graph->GetName());
    const FString Consent=TEXT("reparent ")+Asset+TEXT(" to ")+NewParent+TEXT(" and compile");
    const FString Details=TEXT("\nold parent: ")+OldParent+TEXT("\nnew parent: ")+NewParent
        +TEXT("\nhierarchy data-loss risk: ")+(Parent->IsChildOf(BP->ParentClass)?TEXT("lower; inherited values still require review"):TEXT("yes; old inherited members may be lost"));
    if (!Edit) return TEXT("unchanged\npreview: reparent Blueprint")+Details+TEXT("\nconfirmation: ")+Consent
        +TEXT("\nExecution refreshes graph nodes and compiles. Project code may execute and objects may be reinstanced. Undo and rollback are not guaranteed. No automatic save. Preview checks compatibility; it does not prove compilation will succeed.");
    if (Confirmation!=Consent) return Refuse(TEXT("Confirmation must exactly match the preview confirmation text, including compile consent."));
    FTurboMCPLogCapture Capture;
    UBlueprintEditorLibrary::ReparentBlueprint(BP,Parent);
    const bool ParentMatches=BP->ParentClass==Parent;
    const bool GeneratedMatches=BP->GeneratedClass && BP->GeneratedClass->GetSuperClass()==Parent;
    const bool Compiled=(BP->Status==BS_UpToDate || BP->Status==BS_UpToDateWithWarnings) && Capture.ErrorCount()==0;
    return TEXT("changed\nreparent result: ")+FString(ParentMatches && GeneratedMatches && Compiled?TEXT("compiled"):TEXT("failed or incomplete; inspect the asset before further edits"))
        +Details+TEXT("\nactual parent: ")+GetPathNameSafe(BP->ParentClass)
        +TEXT("\ngenerated parent: ")+GetPathNameSafe(BP->GeneratedClass?BP->GeneratedClass->GetSuperClass():nullptr)
        +FString::Printf(TEXT("\ncompiler status: %s; warnings: %d; errors: %d"),CompilerStatus(BP->Status),Capture.WarningCount(),Capture.ErrorCount())
        +TEXT("\n")+Capture.SummariseSince(0)+TEXT("\n")+Capture.SummariseErrorsSince(0)
        +TEXT("\nCompilation and reinstancing were requested. No rollback or save was performed. Inspect diagnostics and inherited defaults before explicitly saving.");
}
}
FString UTurboMCPPreviewToolset::PreviewTurboMCPBlueprintReparent(const FString& Asset,const FString& NewParent)
{ return Reparent(false,Asset,NewParent,FString(),FString()); }
FString UTurboMCPEditToolset::EditTurboMCPBlueprintReparent(const FString& Asset,const FString& NewParent,const FString& ExpectedParent,const FString& Confirmation)
{ return Reparent(true,Asset,NewParent,ExpectedParent,Confirmation); }
