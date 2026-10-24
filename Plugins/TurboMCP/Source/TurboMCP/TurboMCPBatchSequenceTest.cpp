// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Tools/TurboMCPToolsets.h"
#include "TurboMCPNodeId.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_CallFunction.h"
#include "Kismet/KismetSystemLibrary.h"
#include "GameFramework/Actor.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "UObject/Package.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTurboMCPBatchSequenceTest,
    "TurboMCP.Batch.Sequential",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTurboMCPBatchSequenceTest::RunTest(const FString& Parameters)
{
    const FString PackageName = TEXT("/Game/Tests/BatchSequence_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
    UPackage* Package = CreatePackage(*PackageName);
    UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(AActor::StaticClass(), Package, TEXT("BatchFixture"),
        BPTYPE_Normal, UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass(), TEXT("TurboMCPBatchTest"));
    if (!TestNotNull(TEXT("Blueprint factory succeeds"), Blueprint)) return false;
    FAssetRegistryModule::AssetCreated(Blueprint);
    ON_SCOPE_EXIT
    {
        FAssetRegistryModule::AssetDeleted(Blueprint);
        Blueprint->ClearFlags(RF_Public | RF_Standalone);
        Blueprint->MarkAsGarbage();
        Package->SetDirtyFlag(false); // Disposable, unsaved fixture only.
        Package->MarkAsGarbage();
    };
    UEdGraph* Graph = NewObject<UEdGraph>(Blueprint, TEXT("FailureGraph"), RF_Transactional);
    Graph->Schema = UEdGraphSchema_K2::StaticClass();
    Blueprint->UbergraphPages.Add(Graph);
    auto MakeCall = [&](int32 X)
    {
        UK2Node_CallFunction* Node = NewObject<UK2Node_CallFunction>(Graph, NAME_None, RF_Transactional);
        Node->SetFromFunction(UKismetSystemLibrary::StaticClass()->FindFunctionByName(TEXT("PrintString")));
        Graph->AddNode(Node, false, false);
        Node->CreateNewGuid();
        Node->AllocateDefaultPins();
        Node->NodePosX = X;
        return Node;
    };
    UK2Node_CallFunction* Source = MakeCall(0);
    UK2Node_CallFunction* Consumer = MakeCall(300);
    UK2Node_CallFunction* Target = MakeCall(600);
    Source->FindPinChecked(TEXT("then"))->MakeLinkTo(Consumer->FindPinChecked(TEXT("execute")));
    Consumer->FindPinChecked(TEXT("InString"))->DefaultValue = TEXT("preserve-me");
    Package->SetDirtyFlag(false);

    UK2Node_CallFunction* FinalTarget = MakeCall(900);
    auto Operation = [&](const TCHAR* Id, UEdGraphNode* From, UEdGraphNode* To, const TCHAR* Pin)
    {
        return FString::Printf(TEXT("{\"id\":\"%s\",\"asset\":\"%s\",\"graphName\":\"%s\",\"sourceNodeId\":\"%s\",\"sourcePin\":\"then\",\"targetNodeId\":\"%s\",\"targetPin\":\"%s\",\"keepSource\":true}"),
            Id, *Blueprint->GetPathName(), *Graph->GetName(), *TurboMCPNodeId::MakeForNode(From, true), *TurboMCPNodeId::MakeForNode(To, true), Pin);
    };
    const FString First = Operation(TEXT("first"), Source, Target, TEXT("then"));
    const FString Invalid = TEXT("[") + First + TEXT(",") + Operation(TEXT("second"), Target, FinalTarget, TEXT("MissingPin")) + TEXT("]");
    auto OriginalEdge = [&]()
    {
        UEdGraphPin* Input = Consumer->FindPinChecked(TEXT("execute"));
        return Input->LinkedTo.Num() == 1 && Input->LinkedTo.Contains(Source->FindPinChecked(TEXT("then")))
            && Source->FindPinChecked(TEXT("then"))->LinkedTo.Contains(Input)
            && Target->FindPinChecked(TEXT("then"))->LinkedTo.IsEmpty()
            && FinalTarget->FindPinChecked(TEXT("then"))->LinkedTo.IsEmpty();
    };
    UTurboMCPPreviewToolset* Preview = GetMutableDefault<UTurboMCPPreviewToolset>();
    UTurboMCPEditToolset* Edit = GetMutableDefault<UTurboMCPEditToolset>();
    FString Result = Preview->PreviewTurboMCPBatchEdit(Invalid);
    TestTrue(TEXT("Invalid later operation refused in preview"), Result.Contains(TEXT("refused")) && Result.Contains(TEXT("MissingPin")));
    TestTrue(TEXT("Failed preview preserves original edge"), OriginalEdge());
    Result = Edit->EditTurboMCPBatchEdit(Invalid);
    TestTrue(TEXT("Invalid later operation refused before live write"), Result.StartsWith(TEXT("unchanged")) && Result.Contains(TEXT("MissingPin")));
    TestTrue(TEXT("Refused batch preserves original edge"), OriginalEdge());
    const FString Valid = TEXT("[") + First + TEXT(",") + Operation(TEXT("second"), Target, FinalTarget, TEXT("then")) + TEXT("]");
    Result = Preview->PreviewTurboMCPBatchEdit(Valid);
    TestTrue(TEXT("Sequential preview accepts moved readers"), Result.Contains(TEXT("planned")) && !Result.Contains(TEXT("refused:")));
    TestTrue(TEXT("Successful preview preserves original edge"), OriginalEdge());
    Result = Edit->EditTurboMCPBatchEdit(Valid);
    TestTrue(TEXT("Both sequential operations apply"), Result.StartsWith(TEXT("changed")) && Result.Contains(TEXT("applied=first,second")));
    TestTrue(TEXT("Compile/save not requested"), Result.Contains(TEXT("compile=not_run")) && Result.Contains(TEXT("save=not_requested")));
    UEdGraphPin* Input = Consumer->FindPinChecked(TEXT("execute"));
    UEdGraphPin* Output = FinalTarget->FindPinChecked(TEXT("then"));
    TestTrue(TEXT("Second operation consumes first operation result"), Input->LinkedTo.Num() == 1 && Input->LinkedTo.Contains(Output) && Output->LinkedTo.Contains(Input));
    TestTrue(TEXT("Old outputs disconnected"), Source->FindPinChecked(TEXT("then"))->LinkedTo.IsEmpty() && Target->FindPinChecked(TEXT("then"))->LinkedTo.IsEmpty());
    TestEqual(TEXT("All nodes retained"), Graph->Nodes.Num(), 4);
    TestEqual(TEXT("Unrelated default retained"), Consumer->FindPinChecked(TEXT("InString"))->DefaultValue, FString(TEXT("preserve-me")));
    TestEqual(TEXT("Unrelated position retained"), Consumer->NodePosX, 300);
    return !HasAnyErrors();
}
#endif
