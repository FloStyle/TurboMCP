// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
#include "Tools/TurboMCPToolsets.h"
#include "TurboMCPAssetResolver.h"
#include "Editor.h"
#include "Engine/Blueprint.h"
#include "EdGraph/EdGraph.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_CallFunction.h"
#include "K2Node_VariableSet.h"
#include "K2Node_MacroInstance.h"
#include "EdGraphSchema_K2.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "K2Node_CreateDelegate.h"
#include "Modules/ModuleManager.h"
#include "UObject/UObjectIterator.h"

namespace TurboMCPFunctionFlags
{
// The plan owns no mutation. Dependency inspection and caller reconstruction
// must complete before this plan can be applied by the public edit operation.
struct FPlan
{
    UBlueprint* Blueprint = nullptr;
    UEdGraph* Graph = nullptr;
    UK2Node_FunctionEntry* Entry = nullptr;
    TArray<UK2Node_CallFunction*> Calls;
    TArray<UBlueprint*> Affected;
    bool WasPure = false;
    bool WasConst = false;
    bool Pure = false;
    bool Const = false;
};

bool ParseChoice(const FString& Text, bool Current, bool& Result)
{
    if (Text == TEXT("keep")) { Result = Current; return true; }
    if (Text == TEXT("true")) { Result = true; return true; }
    if (Text == TEXT("false")) { Result = false; return true; }
    return false;
}

FString PrepareDefinition(const FString& Asset, const FString& Function,
    const FString& PureChoice, const FString& ConstChoice, FPlan& Plan)
{
    if (!IsInGameThread() || !GEditor || GEditor->IsPlaySessionInProgress() || GEditor->IsTransactionActive())
        return TEXT("error: Function flag changes require an idle editor outside play and active transactions. Nothing changed.");
    FTurboMCPAssetResolver::FResolved Resolved;
    FString Error = FTurboMCPAssetResolver::Resolve(Asset, Resolved);
    if (!Error.IsEmpty()) return Error;
    Plan.Blueprint = LoadObject<UBlueprint>(nullptr, *Resolved.ObjectPath);
    if (!Plan.Blueprint || Plan.Blueprint->BlueprintType == BPTYPE_Interface)
        return TEXT("error: Choose a Blueprint-owned function, not an interface declaration. Nothing changed.");
    for (UEdGraph* Candidate : Plan.Blueprint->FunctionGraphs)
    {
        if (!Candidate || Candidate->GetFName() != FName(*Function)) continue;
        if (Plan.Graph) return TEXT("error: The function name is ambiguous. Nothing changed.");
        Plan.Graph = Candidate;
    }
    if (!Plan.Graph) return TEXT("error: The named function is not owned by this Blueprint. Nothing changed.");
    if (Plan.Blueprint->ParentClass && Plan.Blueprint->ParentClass->FindFunctionByName(Plan.Graph->GetFName()))
        return TEXT("error: Inherited overrides must retain their defining function contract. Nothing changed.");
    for (const FBPInterfaceDescription& Interface : Plan.Blueprint->ImplementedInterfaces)
        if (Interface.Interface && Interface.Interface->FindFunctionByName(Plan.Graph->GetFName()))
            return TEXT("error: Interface implementations must retain their interface contract. Nothing changed.");
    for (UEdGraphNode* Node : Plan.Graph->Nodes)
        if (UK2Node_FunctionEntry* Entry = Cast<UK2Node_FunctionEntry>(Node))
        {
            if (Plan.Entry) return TEXT("error: The function has multiple entry nodes. Repair it in the editor first.");
            Plan.Entry = Entry;
        }
    if (!Plan.Entry) return TEXT("error: The function has no entry node. Nothing changed.");
    const int32 Flags = Plan.Entry->GetFunctionFlags();
    Plan.WasPure = (Flags & FUNC_BlueprintPure) != 0;
    Plan.WasConst = (Flags & FUNC_Const) != 0;
    if (!ParseChoice(PureChoice, Plan.WasPure, Plan.Pure) || !ParseChoice(ConstChoice, Plan.WasConst, Plan.Const))
        return TEXT("error: Pure and Const each require keep, true or false. Nothing changed.");
    if (Plan.Const && (Flags & FUNC_Static))
        return TEXT("error: Static functions have no receiver and cannot be made const. Nothing changed.");
    return FString();
}

// Class identity must be checked as well as the name: unrelated Blueprints may
// legitimately declare functions with identical names.
bool DerivesFrom(UClass* Class, const UBlueprint* Owner)
{
    for (UClass* Current = Class; Current; Current = Current->GetSuperClass())
        if (Current->ClassGeneratedBy == Owner) return true;
    return false;
}

FString InspectCallers(FPlan& Plan)
{
    IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
    if (Registry.IsLoadingAssets())
        return TEXT("error: Asset discovery is still running. Wait for it to finish, then preview again. Nothing changed.");
    TArray<FName> Packages;
    Packages.Add(Plan.Blueprint->GetOutermost()->GetFName());
    TSet<FName> Seen;
    Seen.Add(Packages[0]);
    for (int32 Index = 0; Index < Packages.Num(); ++Index)
    {
        TArray<FName> Referencers;
        Registry.GetReferencers(Packages[Index], Referencers);
        for (FName Package : Referencers)
        {
            if (Seen.Contains(Package)) continue;
            if (Seen.Num() >= 256)
                return TEXT("error: More than 256 dependent packages require inspection. Use the editor for this project-wide change. Nothing changed.");
            Seen.Add(Package);
            Packages.Add(Package);
        }
    }
    for (FName Package : Packages)
    {
        TArray<FAssetData> Assets;
        Registry.GetAssetsByPackageName(Package, Assets);
        for (const FAssetData& Data : Assets)
        {
            if (Data.AssetClassPath.GetAssetName() == FName(TEXT("World")))
                return TEXT("error: A dependent map may contain level-script callers: ") + Data.GetObjectPathString() + TEXT(". Map callers are not supported. Nothing changed.");
            UClass* Class = Data.GetClass();
            if (!Class)
                return TEXT("error: Cannot classify dependent asset: ") + Data.GetObjectPathString() + TEXT(". Nothing changed.");
            if (Class->IsChildOf(UBlueprint::StaticClass()) && !Cast<UBlueprint>(Data.GetAsset()))
                return TEXT("error: Cannot load dependent Blueprint: ") + Data.GetObjectPathString() + TEXT(". Nothing changed.");
        }
    }
    Plan.Affected.Add(Plan.Blueprint);
    for (TObjectIterator<UBlueprint> It; It; ++It)
    {
        UBlueprint* Blueprint = *It;
        if (!IsValid(Blueprint) || Blueprint->HasAnyFlags(RF_ClassDefaultObject)) continue;
        if (Blueprint != Plan.Blueprint && DerivesFrom(Blueprint->ParentClass, Plan.Blueprint))
            for (UEdGraph* Graph : Blueprint->FunctionGraphs)
                if (Graph && Graph->GetFName() == Plan.Graph->GetFName())
                    return TEXT("error: A child Blueprint overrides this function: ") + Blueprint->GetPathName() + TEXT(". Change the inheritance contract in the editor. Nothing changed.");
        TArray<UEdGraph*> Graphs;
        Blueprint->GetAllGraphs(Graphs);
        for (UEdGraph* Graph : Graphs)
        {
            if (!Graph) continue;
            for (UEdGraphNode* Node : Graph->Nodes)
            {
                if (UK2Node_CreateDelegate* Delegate = Cast<UK2Node_CreateDelegate>(Node))
                    if (Delegate->GetFunctionName() == Plan.Graph->GetFName() && DerivesFrom(Delegate->GetScopeClass(), Plan.Blueprint))
                        return TEXT("error: A delegate references this function: ") + Delegate->GetPathName() + TEXT(". Delegate-contract conversion is not supported. Nothing changed.");
                UK2Node_CallFunction* Call = Cast<UK2Node_CallFunction>(Node);
                if (!Call || Call->FunctionReference.GetMemberName() != Plan.Graph->GetFName()) continue;
                UClass* Scope = Call->FunctionReference.IsSelfContext() ? Blueprint->GeneratedClass.Get() : Call->FunctionReference.GetMemberParentClass();
                if (!DerivesFrom(Scope, Plan.Blueprint)) continue;
                if (Plan.Pure != Plan.WasPure)
                    for (UEdGraphPin* Pin : Call->Pins)
                    {
                        if (!Pin || Pin->LinkedTo.IsEmpty()) continue;
                        const bool Exec = Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec;
                        if ((Plan.Pure && Exec) || (!Plan.Pure && !Exec))
                            return TEXT("error: Purity conversion would remove connected execution flow or introduce an unexecuted data producer at ") + Call->GetPathName() + TEXT(". Rewire the caller explicitly first. Nothing changed.");
                    }
                Plan.Calls.Add(Call);
                Plan.Affected.AddUnique(Blueprint);
            }
        }
    }
    return FString();
}

} // namespace TurboMCPFunctionFlags
