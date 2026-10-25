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

namespace
{
    // Report the verified pin, not only the caller's requested text. These quoted
    // fields are display values; localized text is not serialized edit syntax.
    FString DescribePinDefault(const UEdGraphPin& Pin)
    {
        const FString DisplayValue = Pin.DefaultObject ? Pin.DefaultObject->GetPathName()
            : !Pin.DefaultTextValue.IsEmpty() ? Pin.DefaultTextValue.ToString() : Pin.DefaultValue;
        return FString::Printf(TEXT("\nnode: \"%s\"\npin: \"%s\"\nvalue: \"%s\""),
            *TurboMCPNodeId::MakeForNode(Pin.GetOwningNode(), true).ReplaceCharWithEscapedChar(),
            *Pin.PinName.ToString().ReplaceCharWithEscapedChar(), *DisplayValue.ReplaceCharWithEscapedChar());
    }
}

FString UTurboMCPEditToolset::EditTurboMCPPinDefault(const FString& Asset,
    const FString& GraphName, const FString& NodeId, const FString& PinName, const FString& Value)
{
    if (!IsInGameThread() || !GEditor || GEditor->IsPlaySessionInProgress() || GEditor->IsTransactionActive())
        return TEXT("unchanged: refused; requires an idle editor outside an active transaction");
    FTurboMCPAssetResolver::FResolved Resolved;
    const FString Error = FTurboMCPAssetResolver::Resolve(Asset, Resolved);
    if (!Error.IsEmpty()) return Error;
    UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *Resolved.ObjectPath);
    if (!Blueprint) return TEXT("unchanged: refused; Blueprint could not be loaded");
    int32 GraphMatches = 0;
    UEdGraph* Graph = Blueprint ? TurboMCPNodeId::FindGraphByName(Blueprint, GraphName, GraphMatches) : nullptr;
    if (!Graph || GraphMatches != 1) return TEXT("unchanged: refused; Blueprint/graph not found or ambiguous");
    const UEdGraphSchema_K2* Schema = Cast<UEdGraphSchema_K2>(Graph->GetSchema());
    if (!Schema) return TEXT("unchanged: refused; only K2 Blueprint graphs are supported");
    UEdGraphPin* Pin = nullptr;
    for (UEdGraphNode* Node : Graph->Nodes)
    {
        if (!Node || (TurboMCPNodeId::MakeForNode(Node, true) != NodeId.TrimStartAndEnd()
            && TurboMCPNodeId::MakeForNode(Node, false) != NodeId.TrimStartAndEnd())) continue;
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
    if (!Pin)
        return FString::Printf(TEXT("unchanged: refused; input pin '%s' was not found on node '%s'; use the exact reader node ID and pin name, not a display label or numeric index"), *PinName, *NodeId);
    if (Pin->bOrphanedPin)
        return TEXT("unchanged: refused; pin is orphaned; repair or reconstruct the node before setting its default");
    if (Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec)
        return TEXT("unchanged: refused; execution pins accept connections, not literal defaults");
    if (Pin->LinkedTo.Num())
        return FString::Printf(TEXT("unchanged: refused; pin has %d connection(s); a literal default is inactive while connected"), Pin->LinkedTo.Num());
    if (Pin->bDefaultValueIsReadOnly)
        return TEXT("unchanged: refused; the node marks this pin's default read-only; use a supported connection or the node's own editing operation");
    if (Pin->bDefaultValueIsIgnored)
        return TEXT("unchanged: refused; the node ignores this pin's default; supply its value through a supported connection");
    FString Parsed;
    TObjectPtr<UObject> Object = nullptr;
    FText Text;
    Schema->GetPinDefaultValuesFromString(Pin->PinType, Pin->GetOwningNode(), Value, Parsed, Object, Text, false);
    const FString Invalid = Schema->IsPinDefaultValid(Pin, Parsed, Object, Text);
    if (!Invalid.IsEmpty())
        return FString::Printf(TEXT("unchanged: refused; pin '%s' (category=%s, subcategory=%s) rejected the literal: %s"),
            *Pin->PinName.ToString(), *Pin->PinType.PinCategory.ToString(), *Pin->PinType.PinSubCategory.ToString(), *Invalid);
    if (Pin->DefaultValue == Parsed && Pin->DefaultObject == Object && Pin->DefaultTextValue.EqualTo(Text))
        return FString(TEXT("unchanged: pin already has the requested default; compile=not_run; save=not_requested")) + DescribePinDefault(*Pin);
    const FGuid EditedNodeGuid = Pin->GetOwningNode()->NodeGuid;
    TUniquePtr<FScopedTransaction> Transaction = MakeUnique<FScopedTransaction>(NSLOCTEXT("TurboMCP", "PinDefault", "TurboMCP pin default"));
    Blueprint->Modify(); Graph->Modify(); Pin->GetOwningNode()->Modify();
    Schema->TrySetDefaultValue(*Pin, Value, true);
    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
    // Schema callbacks may reconstruct pins or change display-derived node identities.
    int32 Matches = 0;
    bool ValueMatches = false;
    FString VerifiedDescription;
    for (UEdGraphNode* Node : Graph->Nodes)
        if (Node && Node->NodeGuid == EditedNodeGuid)
            for (UEdGraphPin* Current : Node->Pins)
                if (Current && Current->Direction == EGPD_Input && Current->PinName.ToString() == PinName)
                {
                    ++Matches;
                    ValueMatches = !Current->bOrphanedPin && Current->LinkedTo.IsEmpty()
                        && Current->DefaultValue == Parsed && Current->DefaultObject == Object
                        && Current->DefaultTextValue.EqualTo(Text);
                    if (ValueMatches) VerifiedDescription = DescribePinDefault(*Current);
                }
    if (Matches == 1 && ValueMatches)
        return FString(TEXT("changed: pin default verified; undo=one; compile=not_run; save=not_requested")) + VerifiedDescription;

    UTransBuffer* Buffer = Cast<UTransBuffer>(GEditor->Trans);
    const FGuid OwnedId = Buffer && Buffer->ActiveCount == 1 && !Buffer->UndoBuffer.IsEmpty()
        ? Buffer->UndoBuffer.Last()->GetId() : FGuid();
    Transaction.Reset(); // Finalize once, then undo only the record created by this edit.
    const int32 Index = Buffer ? Buffer->GetQueueLength() - Buffer->GetUndoCount() - 1 : -1;
    const FTransaction* Record = Index >= 0 ? Buffer->GetTransaction(Index) : nullptr;
    const bool UndoSucceeded = OwnedId.IsValid() && Record && Record->GetId() == OwnedId && GEditor->UndoTransaction(false);
    return UndoSucceeded
        ? TEXT("partial: pin default read-back failed; undo succeeded but restoration was not verified; inspect the Blueprint before saving or retrying; rollback=not_confirmed; save=not_requested")
        : TEXT("partial: pin default read-back failed; rollback=not_confirmed; inspect before retrying; save=not_requested");
}
