// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
#include "Tools/TurboMCPToolsets.h"
#include "TurboMCPAssetResolver.h"
#include "TurboMCPNodeId.h"
#include "Editor.h"
#include "Editor/TransBuffer.h"
#include "Engine/Blueprint.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "ScopedTransaction.h"

FString UTurboMCPEditToolset::EditTurboMCPPinDefault(const FString& Asset,
    const FString& GraphName, const FString& NodeId, const FString& PinName, const FString& Value)
{
    if (!IsInGameThread() || !GEditor || GEditor->IsPlaySessionInProgress() || GEditor->IsTransactionActive())
        return TEXT("unchanged: refused; requires an idle editor outside an active transaction");
    FTurboMCPAssetResolver::FResolved Resolved;
    const FString Error = FTurboMCPAssetResolver::Resolve(Asset, Resolved);
    if (!Error.IsEmpty()) return Error;
    UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *Resolved.ObjectPath);
    int32 GraphMatches = 0;
    UEdGraph* Graph = Blueprint ? TurboMCPNodeId::FindGraphByName(Blueprint, GraphName, GraphMatches) : nullptr;
    if (!Graph || GraphMatches != 1) return TEXT("unchanged: refused; Blueprint/graph not found or ambiguous");
    const UEdGraphSchema_K2* Schema = Cast<UEdGraphSchema_K2>(Graph->GetSchema());
    if (!Schema) return TEXT("unchanged: refused; only K2 Blueprint graphs are supported");
    UEdGraphPin* Pin = nullptr;
    for (UEdGraphNode* Node : Graph->Nodes)
    {
        if (!Node || TurboMCPNodeId::MakeForNode(Node, true) != NodeId) continue;
        for (UEdGraphPin* Candidate : Node->Pins)
        {
            if (Candidate && Candidate->Direction == EGPD_Input
                && Candidate->PinName.ToString() == PinName)
            {
                if (Pin) return TEXT("unchanged: refused; ambiguous pin identity");
                Pin = Candidate;
            }
        }
    }
    if (!Pin || Pin->bOrphanedPin || Pin->bDefaultValueIsReadOnly || Pin->bDefaultValueIsIgnored
        || Pin->LinkedTo.Num() || Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec)
        return TEXT("unchanged: refused; pin is missing, connected or cannot accept a default");
    FString Parsed;
    TObjectPtr<UObject> Object = nullptr;
    FText Text;
    Schema->GetPinDefaultValuesFromString(Pin->PinType, Pin->GetOwningNode(), Value, Parsed, Object, Text, false);
    const FString Invalid = Schema->IsPinDefaultValid(Pin, Parsed, Object, Text);
    if (!Invalid.IsEmpty()) return FString::Printf(TEXT("unchanged: refused; %s"), *Invalid);
    if (Pin->DefaultValue == Parsed && Pin->DefaultObject == Object && Pin->DefaultTextValue.EqualTo(Text))
        return TEXT("unchanged: pin already has the requested default");
    const FGuid EditedNodeGuid = Pin->GetOwningNode()->NodeGuid;
    TUniquePtr<FScopedTransaction> Transaction = MakeUnique<FScopedTransaction>(NSLOCTEXT("TurboMCP", "PinDefault", "TurboMCP pin default"));
    Blueprint->Modify(); Graph->Modify(); Pin->GetOwningNode()->Modify();
    Schema->TrySetDefaultValue(*Pin, Value, true);
    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
    // Schema callbacks may reconstruct pins or change display-derived node identities.
    int32 Matches = 0;
    bool ValueMatches = false;
    for (UEdGraphNode* Node : Graph->Nodes)
        if (Node && Node->NodeGuid == EditedNodeGuid)
            for (UEdGraphPin* Current : Node->Pins)
                if (Current && Current->Direction == EGPD_Input && Current->PinName.ToString() == PinName)
                {
                    ++Matches;
                    ValueMatches = !Current->bOrphanedPin && Current->LinkedTo.IsEmpty()
                        && Current->DefaultValue == Parsed && Current->DefaultObject == Object
                        && Current->DefaultTextValue.EqualTo(Text);
                }
    if (Matches == 1 && ValueMatches)
        return TEXT("changed: pin default verified; undo=one; compile=not_run; save=not_requested");

    UTransBuffer* Buffer = Cast<UTransBuffer>(GEditor->Trans);
    const FGuid OwnedId = Buffer && Buffer->ActiveCount == 1 && !Buffer->UndoBuffer.IsEmpty()
        ? Buffer->UndoBuffer.Last()->GetId() : FGuid();
    Transaction.Reset(); // Finalize once, then undo only the record created by this edit.
    const int32 Index = Buffer ? Buffer->GetQueueLength() - Buffer->GetUndoCount() - 1 : -1;
    const FTransaction* Record = Index >= 0 ? Buffer->GetTransaction(Index) : nullptr;
    const bool Restored = OwnedId.IsValid() && Record && Record->GetId() == OwnedId && GEditor->UndoTransaction(false);
    return Restored
        ? TEXT("unchanged: pin default read-back failed; rollback=complete; save=not_requested")
        : TEXT("partial: pin default read-back failed; rollback=not_confirmed; inspect before retrying; save=not_requested");
}
