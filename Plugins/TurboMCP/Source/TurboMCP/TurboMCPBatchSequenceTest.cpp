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
#include "UObject/ObjectSaveContext.h"
#include "BlueprintEditorSettings.h"
#include "Misc/PackageName.h"
#include "HAL/FileManager.h"
#include "Editor.h"
#include "Editor/Transactor.h"

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
        Node->PostPlacedNewNode();
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
    const int32 UndoBeforePreview = GEditor->Trans->GetQueueLength();
    const bool DirtyBeforePreview = Package->IsDirty();
    const TArray<FString> RefusedPayloads = {
        TEXT("not JSON"), TEXT("[]"),
        TEXT("[") + First.Replace(TEXT("\"id\":\"first\""), TEXT("\"kind\":\"unsupported\"")) + TEXT("]"),
        TEXT("[") + First.Replace(TEXT("\"id\":\"first\""), TEXT("\"unknownField\":\"value\"")) + TEXT("]"),
        TEXT("[") + First + TEXT(",") + First + TEXT("]"),
        TEXT("[") + First.Replace(TEXT("\"graphName\":"), TEXT("\"graph\":\"FailureGraph\",\"graphName\":")) + TEXT("]")
    };
    for (const FString& Payload : RefusedPayloads)
    {
        const FString PreviewResult = Preview->PreviewTurboMCPBatchEdit(Payload);
        const FString EditResult = Edit->EditTurboMCPBatchEdit(Payload);
        TestTrue(TEXT("Invalid shape refused by preview"), PreviewResult.StartsWith(TEXT("unchanged\nrefused:")));
        TestTrue(TEXT("Invalid shape refused by edit"), EditResult.StartsWith(TEXT("unchanged\nrefused:")));
        TestTrue(TEXT("Invalid shape preserves original links"), OriginalEdge());
        TestEqual(TEXT("Invalid shape preserves dirty flag"), Package->IsDirty(), DirtyBeforePreview);
        TestEqual(TEXT("Invalid shape creates no undo"), GEditor->Trans->GetQueueLength(), UndoBeforePreview);
    }
    FString Result = Preview->PreviewTurboMCPBatchEdit(TEXT("[null]"));
    TestTrue(TEXT("First malformed operation uses one-based numbering"), Result.Contains(TEXT("Operation 1 must be a JSON object.")));
    TestTrue(TEXT("Malformed preview preserves original edge"), OriginalEdge());
    Result = Edit->EditTurboMCPBatchEdit(TEXT("[") + First + TEXT(",null]"));
    TestTrue(TEXT("Later malformed operation uses one-based numbering"), Result.StartsWith(TEXT("unchanged")) && Result.Contains(TEXT("Operation 2 must be a JSON object.")));
    TestTrue(TEXT("Malformed later operation preserves original edge"), OriginalEdge());
    Result = Preview->PreviewTurboMCPBatchEdit(Invalid);
    TestTrue(TEXT("Invalid later operation refused in preview"), Result.Contains(TEXT("refused")) && Result.Contains(TEXT("MissingPin")));
    TestTrue(TEXT("Failed preview preserves original edge"), OriginalEdge());
    Result = Edit->EditTurboMCPBatchEdit(Invalid);
    TestTrue(TEXT("Invalid later operation refused before live write"), Result.StartsWith(TEXT("unchanged")) && Result.Contains(TEXT("MissingPin")));
    TestTrue(TEXT("Refused batch preserves original edge"), OriginalEdge());
    const FString Valid = TEXT("[") + First + TEXT(",") + Operation(TEXT("second"), Target, FinalTarget, TEXT("then")) + TEXT("]");
    Result = Preview->PreviewTurboMCPBatchEdit(Valid);
    TestTrue(TEXT("Sequential preview accepts moved readers"), Result.Contains(TEXT("planned")) && !Result.Contains(TEXT("refused:")));
    TestTrue(TEXT("Successful preview preserves original edge"), OriginalEdge());
    TestEqual(TEXT("Preview preserves dirty flag"), Package->IsDirty(), DirtyBeforePreview);
    TestEqual(TEXT("Preview creates no undo"), GEditor->Trans->GetQueueLength(), UndoBeforePreview);
    Result = Edit->EditTurboMCPBatchEdit(Valid);
    AddInfo(TEXT("Live batch result: ") + Result);
    TestTrue(TEXT("Both sequential operations apply"), Result.StartsWith(TEXT("changed")) && Result.Contains(TEXT("applied=first,second")));
    TestTrue(TEXT("Compile/save not requested"), Result.Contains(TEXT("compile=not_run")) && Result.Contains(TEXT("save=not_requested")));
    UEdGraphPin* Input = Consumer->FindPinChecked(TEXT("execute"));
    UEdGraphPin* Output = FinalTarget->FindPinChecked(TEXT("then"));
    TestTrue(TEXT("Second operation consumes first operation result"), Input->LinkedTo.Num() == 1 && Input->LinkedTo.Contains(Output) && Output->LinkedTo.Contains(Input));
    TestTrue(TEXT("Old outputs disconnected"), Source->FindPinChecked(TEXT("then"))->LinkedTo.IsEmpty() && Target->FindPinChecked(TEXT("then"))->LinkedTo.IsEmpty());
    TestEqual(TEXT("All nodes retained"), Graph->Nodes.Num(), 4);
    TestEqual(TEXT("Unrelated default retained"), Consumer->FindPinChecked(TEXT("InString"))->DefaultValue, FString(TEXT("preserve-me")));
    TestEqual(TEXT("Unrelated position retained"), Consumer->NodePosX, 300);
    TestEqual(TEXT("Successful batch creates exactly one undo record"), GEditor->Trans->GetQueueLength(), UndoBeforePreview + 1);
    TestTrue(TEXT("One user undo succeeds"), GEditor->UndoTransaction(false));
    TestTrue(TEXT("One user undo restores original links"), OriginalEdge());
    TestEqual(TEXT("User undo preserves unrelated default"), Consumer->FindPinChecked(TEXT("InString"))->DefaultValue, FString(TEXT("preserve-me")));

    // Explicit compilation must not inherit Save on Compile from user settings.
    // Use only this unsaved fixture and restore the preference even on failure.
    UBlueprintEditorSettings* Settings = GetMutableDefault<UBlueprintEditorSettings>();
    const ESaveOnCompile PreviousSaveMode = Settings->SaveOnCompile;
    Settings->SaveOnCompile = SoC_Always;
    int32 SaveAttempts = 0;
    const FDelegateHandle SaveHandle = UPackage::PreSavePackageWithContextEvent.AddLambda(
        [&](UPackage* SavingPackage, FObjectPreSaveContext)
        {
            if (SavingPackage == Package) ++SaveAttempts;
        });
    ON_SCOPE_EXIT
    {
        UPackage::PreSavePackageWithContextEvent.Remove(SaveHandle);
        Settings->SaveOnCompile = PreviousSaveMode;
    };
    const FString Filename = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());
    TestFalse(TEXT("Fixture has no file before compile"), IFileManager::Get().FileExists(*Filename));
    Result = Edit->EditTurboMCPCompile(Blueprint->GetPathName());
    AddInfo(TEXT("Explicit compile with Save on Compile Always: ") + Result);
    TestTrue(TEXT("Explicit fixture compile succeeds"), Blueprint->Status == BS_UpToDate || Blueprint->Status == BS_UpToDateWithWarnings);
    TestEqual(TEXT("Compile does not attempt to save the fixture"), SaveAttempts, 0);
    TestFalse(TEXT("Compile leaves the fixture off disk"), IFileManager::Get().FileExists(*Filename));
    return !HasAnyErrors();
}
#endif
