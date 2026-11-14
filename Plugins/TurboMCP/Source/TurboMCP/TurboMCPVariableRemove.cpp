// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
#include "Tools/TurboMCPToolsets.h"
#include "TurboMCPAssetResolver.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "UObject/UnrealType.h"
#include "K2Node.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "Kismet2/Kismet2NameValidators.h"
#include "K2Node_Variable.h"
#include "K2Node_GetClassDefaults.h"
#include "WidgetBlueprint.h"
#include "Modules/ModuleManager.h"
#include "UObject/UObjectIterator.h"
#include "Editor.h"
#include "ScopedTransaction.h"
#include "TurboMCPRollback.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Misc/SecureHash.h"
#include "UObject/Package.h"

namespace TurboMCPVariableRemoval
{
bool DerivesFrom(UClass* Class, const UBlueprint* Owner)
{
    for (UClass* Current=Class; Current; Current=Current->GetSuperClass())
        if (Current->ClassGeneratedBy==Owner) return true;
    return false;
}

// Discovery may load dependent Blueprints. It never edits or removes references.
// Incomplete discovery is a refusal, not evidence that a variable is unused.
FString InspectReferences(UBlueprint* Owner, const FBPVariableDescription& Variable)
{
    IAssetRegistry& Registry=FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
    if (Registry.IsLoadingAssets()) return TEXT("error: Asset discovery is still running; retry preview after it finishes. Nothing removed.");
    TArray<FName> Packages={Owner->GetOutermost()->GetFName()};
    TSet<FName> Seen; Seen.Add(Packages[0]);
    for (int32 Index=0; Index<Packages.Num(); ++Index)
    {
        TArray<FName> Referencers; Registry.GetReferencers(Packages[Index],Referencers);
        for (FName Package:Referencers) if (!Seen.Contains(Package))
        {
            if (Seen.Num()>=256) return TEXT("error: More than 256 dependency packages require inspection. Use the editor; nothing removed.");
            Seen.Add(Package); Packages.Add(Package);
        }
    }
    for (FName Package:Packages)
    {
        TArray<FAssetData> Assets; Registry.GetAssetsByPackageName(Package,Assets);
        for (const FAssetData& Data:Assets)
        {
            if (Data.AssetClassPath.GetAssetName()==FName(TEXT("World")))
                return TEXT("error: A dependent map requires manual reference inspection: ")+Data.GetObjectPathString();
            UClass* Class=Data.GetClass();
            if (!Class) return TEXT("error: Cannot classify dependent asset: ")+Data.GetObjectPathString();
            if (Class->IsChildOf(UBlueprint::StaticClass()) && !Cast<UBlueprint>(Data.GetAsset()))
                return TEXT("error: Cannot load dependent Blueprint: ")+Data.GetObjectPathString();
        }
    }
    for (TObjectIterator<UBlueprint> It; It; ++It)
    {
        UBlueprint* Blueprint=*It;
        if (!IsValid(Blueprint) || Blueprint->HasAnyFlags(RF_ClassDefaultObject)) continue;
        const bool Inherits=Blueprint==Owner || DerivesFrom(Blueprint->ParentClass,Owner);
        if (const auto* Widget=Cast<UWidgetBlueprint>(Blueprint); Widget && Inherits)
        {
            for (const FDelegateEditorBinding& Binding:Widget->Bindings)
            {
                bool References=Binding.SourceProperty==Variable.VarName || Binding.MemberGuid==Variable.VarGuid;
                for (const FEditorPropertyPathSegment& Segment:Binding.SourcePath.Segments)
                    References |= Segment.GetMemberName()==Variable.VarName;
                if (References) return TEXT("error: Variable is used by a widget binding in ")+Blueprint->GetPathName()+TEXT(". Nothing removed.");
            }
        }
        TArray<UEdGraph*> Graphs; Blueprint->GetAllGraphs(Graphs);
        for (UEdGraph* Graph:Graphs) if (Graph) for (UEdGraphNode* Node:Graph->Nodes)
        {
            if (!Node) continue;
            bool References=false;
            if (const auto* VariableNode=Cast<UK2Node_Variable>(Node))
            {
                const auto& Reference=VariableNode->VariableReference;
                UClass* Scope=Reference.IsSelfContext()?Blueprint->GeneratedClass.Get():Reference.GetMemberParentClass();
                References=Reference.GetMemberGuid()==Variable.VarGuid;
                // Missing GUIDs must not turn a name/owner match into an unused result.
                if (Reference.GetMemberName()==Variable.VarName && !Reference.IsLocalScope())
                    References |= (Reference.IsSelfContext() && Inherits) || DerivesFrom(Scope,Owner) || !Scope;
            }
            if (const auto* Defaults=Cast<UK2Node_GetClassDefaults>(Node))
                if (DerivesFrom(Defaults->GetInputClass(),Owner))
                    References |= Defaults->FindPin(Variable.VarName)!=nullptr;
            if (const auto* K2=Cast<UK2Node>(Node))
                References |= K2->ReferencesVariable(Variable.VarName,Owner->SkeletonGeneratedClass);
            if (References) return TEXT("error: Variable is referenced by node ")+Node->GetPathName()+TEXT(". Remove or rewire the reference explicitly first; no nodes or variables were removed.");
        }
    }
    return FString();
}
}

namespace TurboMCPVariableRemoval
{
FString Declarations(UBlueprint* Blueprint)
{
    FString Result;
    for (const FBPVariableDescription& Variable:Blueprint->NewVariables)
    {
        FString Item;
        FBPVariableDescription::StaticStruct()->ExportText(Item,&Variable,nullptr,Blueprint,PPF_None,nullptr);
        Result += FString::Printf(TEXT("%d:"), Item.Len()) + Item;
    }
    return Result;
}
FString GraphState(UBlueprint* Blueprint)
{
    TArray<FString> Rows;
    TArray<UEdGraph*> Graphs; Blueprint->GetAllGraphs(Graphs);
    for (UEdGraph* Graph:Graphs) if (Graph)
    {
        Rows.Add(TEXT("graph:")+Graph->GetPathName());
        for (UEdGraphNode* Node:Graph->Nodes) if (Node)
        {
            const FString Id=Graph->GetPathName()+TEXT(":")+Node->NodeGuid.ToString();
            Rows.Add(Id+FString::Printf(TEXT(":%d,%d:"),Node->NodePosX,Node->NodePosY)+Node->NodeComment);
            for (const UEdGraphPin* Pin:Node->Pins) if (Pin)
            {
                FString Row=Id+TEXT(":")+Pin->PinId.ToString()+TEXT(":")+Pin->PinName.ToString()+TEXT(":")+Pin->DefaultValue+TEXT(":")+Pin->DefaultTextValue.ToString();
                if (Pin->DefaultObject) Row+=Pin->DefaultObject->GetPathName();
                TArray<FString> Links;
                for (const UEdGraphPin* Link:Pin->LinkedTo) if (Link) Links.Add(Link->PinId.ToString());
                Links.Sort(); Rows.Add(Row+FString::Join(Links,TEXT(";")));
            }
        }
    }
    Rows.Sort(); return FString::Join(Rows,TEXT("\n"));
}
FString Remove(const FString& Asset,const FString& Name,const FString& Confirm,bool Apply)
{
    if (!IsInGameThread() || !GEditor || GEditor->IsPlaySessionInProgress() || GEditor->IsTransactionActive())
        return TEXT("error: Requires an idle editor outside PIE and active transactions. Nothing removed.");
    FTurboMCPAssetResolver::FResolved Resolved;
    const FString ResolveError=FTurboMCPAssetResolver::Resolve(Asset,Resolved);
    if (!ResolveError.IsEmpty()) return ResolveError;
    UBlueprint* Blueprint=LoadObject<UBlueprint>(nullptr,*Resolved.ObjectPath);
    if (!Blueprint || Name.IsEmpty() || FName(*Name).IsNone()) return TEXT("error: Specify a Blueprint and an owned member variable name. Nothing removed.");
    const int32 Index=Blueprint->NewVariables.IndexOfByPredicate([&](const FBPVariableDescription& V){return V.VarName.ToString()==Name;});
    if (Index==INDEX_NONE) return TEXT("error: Exact owned member variable not found. Inherited, local, component and widget-generated variables are not removable here.");
    const FBPVariableDescription Variable=Blueprint->NewVariables[Index];
    if (!Variable.VarGuid.IsValid() || !Blueprint->SkeletonGeneratedClass || Blueprint->Status==BS_Error)
        return TEXT("error: Variable identity or skeleton is unavailable. Compile successfully and preview again; nothing removed.");
    if (Blueprint->ParentClass && FindFProperty<FProperty>(Blueprint->ParentClass,Variable.VarName))
        return TEXT("error: This name also belongs to an inherited property. Nothing removed.");
    for (const FBPVariableDescription& V:Blueprint->NewVariables)
        if (V.HasMetaData(TEXT("FieldNotify"))) return TEXT("error: Field-notify metadata requires manual dependency review. Nothing removed.");
    const FString ReferenceError=InspectReferences(Blueprint,Variable);
    if (!ReferenceError.IsEmpty()) return ReferenceError;
    const FString BeforeDeclarations=Declarations(Blueprint);
    const FString BeforeGraphs=GraphState(Blueprint);
    const FString Token=FMD5::HashAnsiString(*(Blueprint->GetPathName()+Name+BeforeDeclarations+BeforeGraphs));
    const FString Limits=TEXT("\nInspected registered dependencies and loaded Blueprint graphs/bindings. Dynamic strings, native code and unavailable content are not proven safe. Inspection can load assets and run load callbacks. Structural refresh may compile dependencies; no save and no guaranteed undo across compilation.");
    if (!Apply) return TEXT("preview: Remove one unused owned variable; no declaration or graph edit performed.\nvariable: ")+Name+TEXT("\nconfirm: ")+Token+Limits;
    if (Confirm!=Token) return TEXT("error: Confirmation is missing or stale. Preview again and copy its confirm token; nothing removed.");
    const bool WasDirty=Blueprint->GetOutermost()->IsDirty();
    FScopedTransaction Transaction(FText::FromString(TEXT("TurboMCP remove unused variable")));
    Blueprint->Modify();
    // Do not use RemoveMemberVariable: it also deletes variable nodes implicitly.
    Blueprint->NewVariables.RemoveAt(Index);
    const FString ExpectedDeclarations=Declarations(Blueprint);
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    if (Declarations(Blueprint)!=ExpectedDeclarations || GraphState(Blueprint)!=BeforeGraphs)
    {
        const bool Undone=FTurboMCPRollback::EndAndUndoActiveTransaction();
        const bool Restored=Undone && Declarations(Blueprint)==BeforeDeclarations && GraphState(Blueprint)==BeforeGraphs;
        if (Restored) Blueprint->GetOutermost()->SetDirtyFlag(WasDirty);
        return Restored?TEXT("error: Removal verification failed; declaration and graph baseline restored. Nothing saved.")
            :TEXT("error: Removal verification failed and rollback could not be verified. Inspect the Blueprint before saving; changes may remain.");
    }
    return TEXT("edited: Removed unused member variable ")+Name+TEXT(". Other declarations and graph nodes/links/defaults/positions verified unchanged. Compile and inspect before explicitly saving.")+Limits;
}
}
FString UTurboMCPPreviewToolset::PreviewTurboMCPVariableRemove(const FString& Asset,const FString& Name)
{ return TurboMCPVariableRemoval::Remove(Asset,Name,TEXT(""),false); }
FString UTurboMCPEditToolset::EditTurboMCPVariableRemove(const FString& Asset,const FString& Name,const FString& Confirm)
{ return TurboMCPVariableRemoval::Remove(Asset,Name,Confirm,true); }


namespace TurboMCPVariableRemoval
{
FString LocalDeclarations(UBlueprint* Blueprint)
{
    FString Result;
    TArray<UEdGraph*> Graphs;
    Blueprint->GetAllGraphs(Graphs);
    for (UEdGraph* Graph : Graphs) if (Graph)
        for (UEdGraphNode* Node : Graph->Nodes)
            if (const auto* Entry = Cast<UK2Node_FunctionEntry>(Node))
            {
                Result += Entry->GetPathName();
                for (const FBPVariableDescription& Variable : Entry->LocalVariables)
                {
                    FString Item;
                    FBPVariableDescription::StaticStruct()->ExportText(Item, &Variable, nullptr, Blueprint, PPF_None, nullptr);
                    Result += FString::Printf(TEXT("%d:"), Item.Len()) + Item;
                }
            }
    return Result;
}

FString RemoveLocal(const FString& Asset, const FString& Function, const FString& Name, const FString& Confirm, bool Apply)
{
    if (!IsInGameThread() || !GEditor || GEditor->IsPlaySessionInProgress() || GEditor->IsTransactionActive())
        return TEXT("error: Requires an idle editor outside PIE and active transactions. Nothing removed.");
    FTurboMCPAssetResolver::FResolved Resolved;
    const FString ResolveError = FTurboMCPAssetResolver::Resolve(Asset, Resolved);
    if (!ResolveError.IsEmpty()) return ResolveError;
    UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *Resolved.ObjectPath);
    if (!Blueprint) return TEXT("error: Blueprint could not be loaded. Nothing removed.");
    if (Function.IsEmpty() || Name.IsEmpty())
        return TEXT("error: Specify an exact owned function and local variable name. Nothing removed.");
    UEdGraph* ScopeGraph = nullptr;
    for (UEdGraph* Graph : Blueprint->FunctionGraphs)
        if (Graph && Graph->GetName() == Function)
        {
            if (ScopeGraph) return TEXT("error: Function name is ambiguous. Nothing removed.");
            ScopeGraph = Graph;
        }
    if (!ScopeGraph) return TEXT("error: Exact owned function graph not found. Inherited functions, events and macros are not supported.");
    TArray<UK2Node_FunctionEntry*> Entries;
    ScopeGraph->GetNodesOfClass(Entries);
    if (Entries.Num() != 1 || !Entries[0])
        return TEXT("error: Function must have exactly one entry node. Nothing removed.");
    UK2Node_FunctionEntry* Entry = Entries[0];
    const int32 Index = Entry->LocalVariables.IndexOfByPredicate([&](const FBPVariableDescription& V) { return V.VarName.ToString() == Name; });
    if (Index == INDEX_NONE) return TEXT("error: Exact local variable declaration not found. Nothing removed.");
    const FBPVariableDescription Variable = Entry->LocalVariables[Index];
    if (!Variable.VarGuid.IsValid() || !Blueprint->SkeletonGeneratedClass || Blueprint->Status == BS_Error)
        return TEXT("error: Local identity or skeleton is unavailable. Compile successfully and preview again; nothing removed.");
    TArray<UEdGraph*> Graphs;
    Blueprint->GetAllGraphs(Graphs);
    TArray<FString> References;
    for (UEdGraph* Graph : Graphs) if (Graph)
        for (UEdGraphNode* Node : Graph->Nodes)
            if (const auto* VariableNode = Cast<UK2Node_Variable>(Node))
            {
                const auto& Reference = VariableNode->VariableReference;
                const bool SameGuid = Reference.GetMemberGuid() == Variable.VarGuid;
                const bool SameLocalName = Reference.IsLocalScope() && Reference.GetMemberName() == Variable.VarName;
                const FString ScopeName = Reference.GetMemberScopeName();
                if (SameGuid || (SameLocalName && (ScopeName == Function || ScopeName.IsEmpty())))
                    References.Add(Node->GetPathName());
            }
    if (!References.IsEmpty())
        return TEXT("error: Local variable is referenced by these nodes; remove or rewire them explicitly first. Nothing removed.\n") + FString::Join(References, TEXT("\n"));
    const FString BeforeLocals = LocalDeclarations(Blueprint);
    const FString BeforeMembers = Declarations(Blueprint);
    const FString BeforeGraphs = GraphState(Blueprint);
    const FString Token = FMD5::HashAnsiString(*(Blueprint->GetPathName() + Function + Name + BeforeLocals + BeforeMembers + BeforeGraphs));
    const FString Limits = TEXT("\nNo save or checkout. Structural refresh can compile and affect undo. Native or dynamic references are not proven safe.");
    if (!Apply)
    {
        FString Description;
        FBPVariableDescription::StaticStruct()->ExportText(Description, &Variable, nullptr, Blueprint, PPF_None, nullptr);
        return TEXT("preview: Remove one unused local declaration. No edits performed.\nfunction: ") + Function
            + TEXT("\nvariable: ") + Name + TEXT("\ndeclaration: ") + Description + TEXT("\nconfirm: ") + Token + Limits;
    }
    if (Confirm != Token) return TEXT("error: Confirmation is missing or stale. Preview again and copy its confirm token; nothing removed.");
    const bool WasDirty = Blueprint->GetOutermost()->IsDirty();
    FScopedTransaction Transaction(FText::FromString(TEXT("TurboMCP remove unused local variable")));
    Blueprint->Modify();
    Entry->Modify();
    // The engine removal helper also deletes variable nodes. Remove only the declaration.
    Entry->LocalVariables.RemoveAt(Index);
    const FString ExpectedLocals = LocalDeclarations(Blueprint);
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    if (LocalDeclarations(Blueprint) != ExpectedLocals || Declarations(Blueprint) != BeforeMembers || GraphState(Blueprint) != BeforeGraphs)
    {
        const bool Undone = FTurboMCPRollback::EndAndUndoActiveTransaction();
        const bool Restored = Undone && LocalDeclarations(Blueprint) == BeforeLocals && Declarations(Blueprint) == BeforeMembers && GraphState(Blueprint) == BeforeGraphs;
        if (Restored) Blueprint->GetOutermost()->SetDirtyFlag(WasDirty);
        return Restored ? TEXT("error: Removal verification failed; declaration and graph baseline restored. Nothing saved.")
            : TEXT("error: Removal verification failed and rollback could not be verified. Inspect the Blueprint before saving; changes may remain.");
    }
    return TEXT("edited: Removed unused local variable ") + Function + TEXT(".") + Name
        + TEXT(". Other declarations and graph state verified unchanged. Compile and inspect before saving.") + Limits;
}
}

FString UTurboMCPPreviewToolset::PreviewTurboMCPLocalVariableRemove(const FString& Asset, const FString& Function, const FString& Name)
{ return TurboMCPVariableRemoval::RemoveLocal(Asset, Function, Name, TEXT(""), false); }
FString UTurboMCPEditToolset::EditTurboMCPLocalVariableRemove(const FString& Asset, const FString& Function, const FString& Name, const FString& Confirm)
{ return TurboMCPVariableRemoval::RemoveLocal(Asset, Function, Name, Confirm, true); }


namespace TurboMCPVariableRemoval
{
// Include member-reference identity: graph pin snapshots alone cannot verify a rename.
FString RenameState(UBlueprint* Blueprint)
{
    TArray<FString> References;
    TArray<UEdGraph*> Graphs; Blueprint->GetAllGraphs(Graphs);
    for (UEdGraph* Graph : Graphs) if (Graph)
        for (UEdGraphNode* Node : Graph->Nodes)
            if (const auto* VariableNode = Cast<UK2Node_Variable>(Node))
            {
                const auto& Ref = VariableNode->VariableReference;
                References.Add(Node->GetPathName() + TEXT(":") + Ref.GetMemberName().ToString()
                    + TEXT(":") + Ref.GetMemberScopeName() + TEXT(":") + Ref.GetMemberGuid().ToString()
                    + FString::Printf(TEXT(":%d:%d"), Ref.IsLocalScope(), Ref.IsSelfContext()));
            }
    References.Sort();
    return LocalDeclarations(Blueprint) + Declarations(Blueprint) + GraphState(Blueprint) + FString::Join(References, TEXT("\n"));
}

FString RenameLocal(const FString& Asset, const FString& Function, const FString& Name, const FString& NewName, const FString& Confirm, bool Apply)
{
    if (!IsInGameThread() || !GEditor || GEditor->IsPlaySessionInProgress() || GEditor->IsTransactionActive())
        return TEXT("error: Requires an idle editor outside PIE and active transactions. Nothing renamed.");
    FTurboMCPAssetResolver::FResolved Resolved;
    const FString ResolveError = FTurboMCPAssetResolver::Resolve(Asset, Resolved);
    if (!ResolveError.IsEmpty()) return ResolveError;
    UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *Resolved.ObjectPath);
    if (!Blueprint) return TEXT("error: Blueprint could not be loaded. Nothing renamed.");
    UEdGraph* ScopeGraph = nullptr;
    for (UEdGraph* Graph : Blueprint->FunctionGraphs) if (Graph && Graph->GetName() == Function)
    {
        if (ScopeGraph) return TEXT("error: Function name is ambiguous. Nothing renamed.");
        ScopeGraph = Graph;
    }
    if (!ScopeGraph) return TEXT("error: Exact owned function graph not found. Nothing renamed.");
    TArray<UK2Node_FunctionEntry*> Entries; ScopeGraph->GetNodesOfClass(Entries);
    if (Entries.Num() != 1 || !Entries[0]) return TEXT("error: Function must have exactly one entry. Nothing renamed.");
    UK2Node_FunctionEntry* Entry = Entries[0];
    const int32 Index = Entry->LocalVariables.IndexOfByPredicate([&](const FBPVariableDescription& V) { return V.VarName.ToString() == Name; });
    if (Index == INDEX_NONE) return TEXT("error: Exact local declaration not found. Nothing renamed.");
    const FBPVariableDescription Variable = Entry->LocalVariables[Index];
    if (!Variable.VarGuid.IsValid() || !Blueprint->SkeletonGeneratedClass || Blueprint->Status == BS_Error)
        return TEXT("error: Compile the Blueprint successfully before previewing this rename.");
    const UFunction* Scope = Blueprint->SkeletonGeneratedClass->FindFunctionByName(ScopeGraph->GetFName());
    if (!Scope) return TEXT("error: Compiled function scope is unavailable. Compile and preview again.");
    if (Name == NewName) return TEXT("unchanged: The local variable already has that name.");
    if (NewName.IsEmpty() || NewName.Len() > 100 || FName(*NewName).IsNone())
        return TEXT("error: NewName must be a valid identifier of 1 to 100 characters.");
    FKismetNameValidator Validator(Blueprint, Variable.VarName, Scope);
    if (Validator.IsValid(NewName) != EValidatorResult::Ok)
        return TEXT("error: NewName is invalid or conflicts with a function parameter or local variable.");
    const FName NewVariableName(*NewName);
    for (int32 I = 0; I < Entry->LocalVariables.Num(); ++I)
        if (I != Index && (Entry->LocalVariables[I].VarName == NewVariableName || Entry->LocalVariables[I].VarGuid == Variable.VarGuid))
            return TEXT("error: Local declaration identity or new name is ambiguous. Nothing renamed.");

    TArray<UK2Node_Variable*> Affected;
    TArray<FString> Paths;
    TArray<UEdGraph*> Graphs; Blueprint->GetAllGraphs(Graphs);
    for (UEdGraph* Graph : Graphs) if (Graph) for (UEdGraphNode* Node : Graph->Nodes)
    {
        const bool InScope = FBlueprintEditorUtils::GetTopLevelGraph(Graph) == ScopeGraph;
        if (auto* VariableNode = Cast<UK2Node_Variable>(Node))
        {
            const auto& Ref = VariableNode->VariableReference;
            const bool SameGuid = Ref.GetMemberGuid() == Variable.VarGuid;
            const bool SameScope = Ref.IsLocalScope() && Ref.GetMemberScopeName() == Function;
            const bool SameName = Ref.GetMemberName() == Variable.VarName;
            if (SameGuid || (SameScope && SameName))
            {
                if (!InScope || !SameScope || !SameName || (Ref.GetMemberGuid().IsValid() && !SameGuid))
                    return TEXT("error: Inconsistent local reference identity at ") + Node->GetPathName() + TEXT(". Nothing renamed.");
                if (Node->GetClass() != UK2Node_VariableGet::StaticClass() && Node->GetClass() != UK2Node_VariableSet::StaticClass())
                    return TEXT("error: Custom local reference node is not supported: ") + Node->GetPathName();
                for (const UEdGraphPin* Pin : Node->Pins)
                    if (Pin && Pin->PinName == NewVariableName && Pin->PinName != Variable.VarName)
                        return TEXT("error: NewName conflicts with an existing node pin at ") + Node->GetPathName();
                Affected.Add(VariableNode); Paths.Add(Node->GetPathName());
            }
            else if (InScope && SameName && Ref.IsLocalScope())
                return TEXT("error: Local reference scope is inconsistent at ") + Node->GetPathName();
        }
        else if (const auto* K2 = Cast<UK2Node>(Node); InScope && K2 && K2->ReferencesVariable(Variable.VarName, Scope))
            return TEXT("error: Unsupported local reference node at ") + Node->GetPathName() + TEXT(". Nothing renamed.");
    }
    const FString Before = RenameState(Blueprint);
    const FString Token = FMD5::HashAnsiString(*(Blueprint->GetPathName() + Function + Name + NewName + Before));
    const FString Limits = TEXT("\nNo save or checkout. Structural refresh can compile and affect undo. Dynamic/native references are not verified.");
    if (!Apply) return FString::Printf(TEXT("preview: Rename local %s.%s to %s; %d reference node(s). No edits performed.\n"), *Function, *Name, *NewName, Affected.Num())
        + FString::Join(Paths, TEXT("\n")) + TEXT("\nconfirm: ") + Token + Limits;
    if (Confirm != Token) return TEXT("error: Confirmation is missing or stale. Preview again and copy its confirm token; nothing renamed.");
    const bool WasDirty = Blueprint->GetOutermost()->IsDirty();
    FScopedTransaction Transaction(FText::FromString(TEXT("TurboMCP rename local variable")));
    Blueprint->Modify(); Entry->Modify();
    Entry->LocalVariables[Index].VarName = NewVariableName;
    Entry->LocalVariables[Index].FriendlyName = FName::NameToDisplayString(NewName, Variable.VarType.PinCategory == FName(TEXT("bool")));
    for (UK2Node_Variable* Node : Affected)
    {
        Node->Modify();
        Node->VariableReference.SetLocalMember(NewVariableName, Function, Variable.VarGuid);
        // Pin names change in place; no reconstruction or broad name-only engine dispatch.
        for (UEdGraphPin* Pin : Node->Pins) if (Pin && Pin->PinName == Variable.VarName) Pin->PinName = NewVariableName;
    }
    const FString Expected = RenameState(Blueprint);
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    if (RenameState(Blueprint) != Expected)
    {
        const bool Undone = FTurboMCPRollback::EndAndUndoActiveTransaction();
        const bool Restored = Undone && RenameState(Blueprint) == Before;
        if (Restored) Blueprint->GetOutermost()->SetDirtyFlag(WasDirty);
        return Restored ? TEXT("error: Rename verification failed; original declarations and graph state restored.")
            : TEXT("error: Rename verification failed and rollback is unverified. Changes may remain; inspect before saving.");
    }
    return FString::Printf(TEXT("edited: Renamed local %s.%s to %s and updated %d reference node(s). Compile and inspect before saving."), *Function, *Name, *NewName, Affected.Num()) + Limits;
}
}
FString UTurboMCPPreviewToolset::PreviewTurboMCPLocalVariableRename(const FString& Asset, const FString& Function, const FString& Name, const FString& NewName)
{ return TurboMCPVariableRemoval::RenameLocal(Asset, Function, Name, NewName, TEXT(""), false); }
FString UTurboMCPEditToolset::EditTurboMCPLocalVariableRename(const FString& Asset, const FString& Function, const FString& Name, const FString& NewName, const FString& Confirm)
{ return TurboMCPVariableRemoval::RenameLocal(Asset, Function, Name, NewName, Confirm, true); }
