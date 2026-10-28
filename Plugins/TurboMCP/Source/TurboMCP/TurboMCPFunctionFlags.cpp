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

namespace TurboMCPFunctionFlags
{
// The plan owns no mutation. Dependency inspection and caller reconstruction
// must complete before this plan can be applied by the public edit operation.
struct FPlan
{
    UBlueprint* Blueprint = nullptr;
    UEdGraph* Graph = nullptr;
    UK2Node_FunctionEntry* Entry = nullptr;
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
} // namespace TurboMCPFunctionFlags
