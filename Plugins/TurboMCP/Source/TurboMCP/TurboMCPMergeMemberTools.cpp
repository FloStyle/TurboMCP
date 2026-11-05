// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
#include "Tools/TurboMCPToolsets.h"
#include "TurboMCPAssetResolver.h"
#include "TurboMCPTypeParser.h"
#include "TurboMCPNodeId.h"
#include "Editor.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "UObject/UnrealType.h"

namespace TurboMCPMergeMembers
{
static FString ResolveTarget(const FString& Asset, UBlueprint*& BP)
{
    if (!IsInGameThread() || !GEditor || GEditor->IsPlaySessionInProgress() || GEditor->IsTransactionActive())
        return TEXT("error: Requires an idle editor on the game thread, outside PIE and active transactions. Nothing changed.");
    FTurboMCPAssetResolver::FResolved Resolved;
    const FString Error = FTurboMCPAssetResolver::Resolve(Asset, Resolved);
    if (!Error.IsEmpty()) return Error;
    BP = LoadObject<UBlueprint>(nullptr, *Resolved.ObjectPath);
    if (!BP || BP->BlueprintType == BPTYPE_Interface || BP->BlueprintType == BPTYPE_MacroLibrary)
        return TEXT("error: Target must be a Blueprint or Widget Blueprint with executable functions. Nothing changed.");
    return FString();
}

static FString InterfaceAdd(const FString& Asset, const FString& Interface, bool bApply)
{
    UBlueprint* BP = nullptr;
    FString Error = ResolveTarget(Asset, BP);
    if (!Error.IsEmpty()) return Error;
    UClass* InterfaceClass = nullptr;
    if (Interface.StartsWith(TEXT("/Script/")))
        InterfaceClass = LoadObject<UClass>(nullptr, *Interface);
    else
    {
        FTurboMCPAssetResolver::FResolved Resolved;
        Error = FTurboMCPAssetResolver::Resolve(Interface, Resolved);
        if (!Error.IsEmpty()) return Error;
        UBlueprint* InterfaceBP = LoadObject<UBlueprint>(nullptr, *Resolved.ObjectPath);
        if (!InterfaceBP) return TEXT("error: Interface asset is not a Blueprint. Nothing changed.");
        if (InterfaceBP->BlueprintType == BPTYPE_Interface && InterfaceBP->Status != BS_Error)
            InterfaceClass = InterfaceBP->GeneratedClass;
    }
    if (!InterfaceClass || !InterfaceClass->HasAnyClassFlags(CLASS_Interface) || InterfaceClass->HasMetaData(TEXT("CannotImplementInterfaceInBlueprint")))
        return TEXT("error: Interface must resolve to a compiled Blueprint interface or a Blueprint-implementable native interface class. Nothing changed.");
    for (const FBPInterfaceDescription& Existing : BP->ImplementedInterfaces)
        if (Existing.Interface == InterfaceClass)
            return TEXT("unchanged: This Blueprint already implements the interface. Nothing saved.");
    if (BP->ParentClass && BP->ParentClass->ImplementsInterface(InterfaceClass))
        return TEXT("unchanged: The parent class already implements the interface. Nothing saved.");
    // Stock ImplementNewInterface can create some graphs before discovering a
    // later conflict. Preflight every signature before entering that API.
    TArray<UEdGraph*> Graphs;
    BP->GetAllGraphs(Graphs);
    for (TFieldIterator<UFunction> It(InterfaceClass, EFieldIteratorFlags::IncludeSuper); It; ++It)
    {
        if (It->HasMetaData(TEXT("AnimBlueprintFunction")))
            return TEXT("error: Animation-layer interfaces are not supported by this tool. Nothing changed.");
        if (!UEdGraphSchema_K2::CanKismetOverrideFunction(*It)) continue;
        for (const UEdGraph* Graph : Graphs)
            if (Graph && Graph->GetFName() == It->GetFName())
                return FString::Printf(TEXT("error: Existing graph '%s' conflicts with an interface function. Rename or migrate it explicitly first. Nothing changed."), *Graph->GetName());
        if (BP->SkeletonGeneratedClass && BP->SkeletonGeneratedClass->FindFunctionByName(It->GetFName()))
            return FString::Printf(TEXT("error: Existing function or event '%s' conflicts with the interface. Nothing changed."), *It->GetName());
    }
    const FString Summary = FString::Printf(TEXT("asset: %s\ninterface: %s\n"), *BP->GetPathName(), *InterfaceClass->GetPathName());
    if (!bApply) return TEXT("preview: Add interface. No changes made.\n") + Summary + TEXT("Unreal will create required function graphs; event-style signatures do not receive event bodies. Existing implementations are not copied. Structural refresh may compile dependent Blueprints. Nothing will be saved.");
    BP->Modify();
    if (!FBlueprintEditorUtils::ImplementNewInterface(BP, InterfaceClass->GetClassPathName()))
        return TEXT("error: Unreal could not add the interface. Inspect the target before retrying; no save was requested.");
    return TEXT("edited: Interface added.\n") + Summary + TEXT("Required function graphs were created by Unreal; event bodies and source logic were not copied. Structural refresh requested; this is not compile-success evidence. No save requested. Undo is not guaranteed across structural compilation. Read the target and compile it before continuing the merge.");
}

static FString LocalAdd(const FString& Asset, const FString& Function, const FString& Name, const FString& Type, const FString& Default, bool bApply)
{
    if (Name.IsEmpty() || Name.Len() >= NAME_SIZE || Name.Equals(TEXT("None"), ESearchCase::IgnoreCase) || (!FChar::IsAlpha(Name[0]) && Name[0] != TEXT('_')))
        return TEXT("error: Name must be a nonempty identifier starting with a letter or underscore; 'None' is reserved. Nothing changed.");
    for (TCHAR C : Name) if (!FChar::IsAlnum(C) && C != TEXT('_'))
        return TEXT("error: Name may contain only letters, digits and underscores. Nothing changed.");
    UBlueprint* BP = nullptr;
    FString Error = ResolveTarget(Asset, BP);
    if (!Error.IsEmpty()) return Error;
    int32 Matches = 0;
    UEdGraph* Graph = TurboMCPNodeId::FindGraphByName(BP, Function, Matches);
    if (Graph == nullptr) return TEXT("error: No owned function matches Function. Read the Blueprint graph names and use an implemented function. Nothing changed.");
    if (Matches > 1) return TEXT("error: Function name is ambiguous. Nothing changed.");
    // Interface implementations live outside FunctionGraphs. Admit only graphs
    // registered as functions on this Blueprint, never inherited or event graphs.
    bool bOwnedFunction = Graph && BP->FunctionGraphs.Contains(Graph);
    for (const FBPInterfaceDescription& Implementation : BP->ImplementedInterfaces)
        bOwnedFunction |= Graph && Implementation.Graphs.Contains(Graph);
    if (!bOwnedFunction || !Graph->GetSchema() || Graph->GetSchema()->GetGraphType(Graph) != GT_Function)
        return TEXT("error: Function must name an owned function or implemented interface function. Event graphs and inherited functions are not accepted. Nothing changed.");
    TArray<UK2Node_FunctionEntry*> Entries;
    Graph->GetNodesOfClass(Entries);
    if (Entries.Num() != 1) return TEXT("error: Function must have exactly one entry node. Nothing changed.");
    FEdGraphPinType PinType;
    if (!FTurboMCPTypeParser::Resolve(Type, PinType, Error)) return TEXT("error: ") + Error + TEXT(" Nothing changed.");
    // Empty means Unreal's type default. A nonempty value is validated using
    // a detached pin, so preview never adds temporary nodes to the asset.
    if (!Default.IsEmpty())
    {
        if (PinType.IsContainer() || PinType.PinCategory == UEdGraphSchema_K2::PC_Object || PinType.PinCategory == UEdGraphSchema_K2::PC_Class || PinType.PinCategory == UEdGraphSchema_K2::PC_SoftObject || PinType.PinCategory == UEdGraphSchema_K2::PC_SoftClass)
            return TEXT("error: Explicit container and object/class defaults are not supported here. Use an empty Default for the type default, then assign the value in the function. Nothing changed.");
        UK2Node_FunctionEntry* Probe = NewObject<UK2Node_FunctionEntry>(GetTransientPackage());
        UEdGraphPin* Pin = Probe->CreatePin(EGPD_Input, PinType, TEXT("Value"));
        Error = GetDefault<UEdGraphSchema_K2>()->IsPinDefaultValid(Pin, Default, nullptr, FText::GetEmpty());
        if (!Error.IsEmpty()) return TEXT("error: Invalid local default: ") + Error + TEXT(" Nothing changed.");
    }
    for (const FBPVariableDescription& Local : Entries[0]->LocalVariables)
        if (Local.VarName == FName(*Name))
            return Local.VarType == PinType && Local.DefaultValue == Default
                ? TEXT("unchanged: The function already declares this local with the same type and default. Nothing saved.")
                : TEXT("error: A local with this name has a different type or default. Nothing changed.");
    for (UEdGraphNode* Node : Graph->Nodes)
        if (Node->IsA<UK2Node_FunctionEntry>() || Node->IsA<UK2Node_FunctionResult>())
            for (const UEdGraphPin* Pin : Node->Pins)
                if (Pin && Pin->PinName == FName(*Name)) return TEXT("error: Name conflicts with a function parameter. Nothing changed.");
    if (BP->SkeletonGeneratedClass && FindFProperty<FProperty>(BP->SkeletonGeneratedClass, FName(*Name)))
        return TEXT("error: Name conflicts with a member variable. Choose a distinct local name. Nothing changed.");
    const FString Summary = FString::Printf(TEXT("asset: %s\nfunction: %s\nlocal: %s\ntype: %s\ndefault: %s\n"), *BP->GetPathName(), *Graph->GetName(), *Name, *Type, Default.IsEmpty() ? TEXT("Unreal type default") : *Default);
    if (!bApply) return TEXT("preview: Declare function local. No changes made.\n") + Summary + TEXT("This declares storage only; it does not add getter/setter nodes or copy source logic. Structural refresh may compile dependent Blueprints. Nothing will be saved.");
    if (!FBlueprintEditorUtils::AddLocalVariable(BP, Graph, FName(*Name), PinType, Default))
        return TEXT("error: Unreal refused the local declaration. No save requested.");
    return TEXT("edited: Function local declared.\n") + Summary + TEXT("No getter/setter nodes or source logic were added. Structural refresh requested; this is not compile-success evidence. No save requested. Undo is not guaranteed across structural compilation. Read the function and compile the target before continuing the merge.");
}
}

FString UTurboMCPPreviewToolset::PreviewTurboMCPInterfaceAdd(const FString& Asset, const FString& Interface) { return TurboMCPMergeMembers::InterfaceAdd(Asset, Interface, false); }
FString UTurboMCPEditToolset::EditTurboMCPInterfaceAdd(const FString& Asset, const FString& Interface) { return TurboMCPMergeMembers::InterfaceAdd(Asset, Interface, true); }
FString UTurboMCPPreviewToolset::PreviewTurboMCPLocalVariableAdd(const FString& Asset, const FString& Function, const FString& Name, const FString& Type, const FString& Default) { return TurboMCPMergeMembers::LocalAdd(Asset, Function, Name, Type, Default, false); }
FString UTurboMCPEditToolset::EditTurboMCPLocalVariableAdd(const FString& Asset, const FString& Function, const FString& Name, const FString& Type, const FString& Default) { return TurboMCPMergeMembers::LocalAdd(Asset, Function, Name, Type, Default, true); }

namespace TurboMCPMergeMembers
{
static FString InterfaceRemove(const FString& Asset,const FString& Interface,const FString& Confirm,bool Apply)
{
    UBlueprint* BP=nullptr;FString Error=ResolveTarget(Asset,BP);if(!Error.IsEmpty())return Error;
    UClass* Found=nullptr;
    for(const auto& I:BP->ImplementedInterfaces)
    {
        UClass* C=I.Interface;if(!C)continue;
        const UObject* Origin=C->ClassGeneratedBy;
        if(C->GetPathName()==Interface || C->GetName()==Interface || (Origin && (Origin->GetPathName()==Interface || Origin->GetName()==Interface)))
        {if(Found)return TEXT("error: Interface name is ambiguous; use its full class path. Nothing changed.");Found=C;}
    }
    if(!Found)return TEXT("error: No directly implemented interface matches. Inherited interfaces cannot be removed here. Nothing changed.");
    const FString Token=BP->GetPathName()+TEXT("|")+Found->GetPathName();
    if(!Apply)return TEXT("preview: Remove interface contract while preserving its function implementations as ordinary functions. Calls requiring the interface may stop working. No changes made.\nconfirmation: ")+Token;
    if(Confirm!=Token)return TEXT("error: Confirm must equal the confirmation returned by InterfaceRemove preview. Nothing changed.");
    BP->Modify();FBlueprintEditorUtils::RemoveInterface(BP,Found->GetClassPathName(),true);
    for(const auto& I:BP->ImplementedInterfaces)if(I.Interface==Found)return TEXT("error: Interface remains implemented. Inspect target; no save requested.");
    return TEXT("edited: Removed the interface contract and preserved its function implementations. Structural refresh may compile dependencies. No save requested. Compile and inspect interface callers before saving; undo is not guaranteed across compilation.");
}
}
FString UTurboMCPPreviewToolset::PreviewTurboMCPInterfaceRemove(const FString& Asset,const FString& Interface){return TurboMCPMergeMembers::InterfaceRemove(Asset,Interface,TEXT(""),false);}
FString UTurboMCPEditToolset::EditTurboMCPInterfaceRemove(const FString& Asset,const FString& Interface,const FString& Confirm){return TurboMCPMergeMembers::InterfaceRemove(Asset,Interface,Confirm,true);}
