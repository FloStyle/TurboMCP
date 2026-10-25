// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
#include "Tools/TurboMCPToolsets.h"
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "K2Node_Event.h"
#include "K2Node_CallFunction.h"
#include "K2Node_VariableGet.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "HAL/FileManager.h"
#include "Modules/ModuleManager.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"
#include "WidgetBlueprint.h"
#include "WidgetBlueprintOperationUtils.h"
#include "TurboMCPNodeId.h"

// Fixture setup uses engine APIs. Only recorded tool calls count as tool evidence.
// Common UI is enabled in TestProject, not made a TurboMCP dependency.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTurboMCPOverrideWorkflowTest,
    "TurboMCP.Workflow.Override", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTurboMCPOverrideWorkflowTest::RunTest(const FString& Parameters)
{
    FModuleManager::Get().LoadModule(TEXT("CommonUI"));
    UClass* ButtonClass = LoadClass<UUserWidget>(nullptr, TEXT("/Script/CommonUI.CommonButtonBase"));
    UClass* StyleClass = LoadClass<UObject>(nullptr, TEXT("/Script/CommonUI.CommonButtonStyle"));
    if (!TestNotNull(TEXT("Stock Common Button class available"), ButtonClass) || !TestNotNull(TEXT("Stock style class available"), StyleClass)) return false;
    const FString Suffix = FGuid::NewGuid().ToString(EGuidFormats::Digits);
    UPackage* Package = CreatePackage(*(TEXT("/Game/Tests/CommonButton_") + Suffix));
    FString Transcript;
    ON_SCOPE_EXIT
    {
        const FString Folder = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("TurboMCP/Workflow"), Suffix);
        IFileManager::Get().MakeDirectory(*Folder, true);
        FFileHelper::SaveStringToFile(Transcript, *FPaths::Combine(Folder, TEXT("override-review.txt")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
        Package->SetDirtyFlag(false); // Only the unique, never-saved fixture package.
    };
    auto Record = [&](const TCHAR* Name, const FString& Value) { Transcript += FString::Printf(TEXT("\n=== %s ===\n%s\n"), Name, *Value); return Value; };
    UBlueprint* Style = FKismetEditorUtilities::CreateBlueprint(StyleClass, Package, TEXT("BS_Review"), BPTYPE_Normal, UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass(), TEXT("CommonButtonReview"));
    if (!TestNotNull(TEXT("Style fixture created"), Style)) return false;
    FKismetEditorUtilities::CompileBlueprint(Style);
    UWidgetBlueprint* BP = FWidgetBlueprintOperationUtils::CreateWidgetBlueprint(Package, TEXT("WBP_CommonReview"), BPTYPE_Normal, ButtonClass, UCanvasPanel::StaticClass(), TEXT("CommonButtonReview"), false);
    if (!TestNotNull(TEXT("Button fixture created"), BP)) return false;
    UTextBlock* Label = BP->WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Label"));
    Label->bIsVariable = true; BP->WidgetVariableNameToGuidMap.Add(Label->GetFName(), FGuid::NewGuid()); Label->SetText(FText::FromString(TEXT("Purchase")));
    UCanvasPanelSlot* Slot = Cast<UCanvasPanel>(BP->WidgetTree->RootWidget)->AddChildToCanvas(Label);
    Slot->SetPosition(FVector2D(40,40)); Slot->SetSize(FVector2D(300,80));
    FKismetEditorUtilities::CompileBlueprint(BP);
    // Remove only the disposable template's unused default events before the scenario.
    // This is fixture preparation, not evidence for a removal tool.
    for(UEdGraph* G:BP->UbergraphPages) {
        const TArray<TObjectPtr<UEdGraphNode>> Defaults=G->Nodes;
        for(UEdGraphNode* N:Defaults)G->RemoveNode(N);
    }
    const FString OverrideAsset=BP->GetPathName();
    auto Snapshot=[&]() {
        TArray<FString> Rows;
        for(UEdGraph* G:BP->UbergraphPages)for(UEdGraphNode* N:G->Nodes) {
            FString Row=N->NodeGuid.ToString()+FString::Printf(TEXT(":%d,%d"),N->NodePosX,N->NodePosY);
            for(UEdGraphPin* P:N->Pins)if(P){Row+=P->PinName.ToString()+P->DefaultValue+P->DefaultTextValue.ToString();for(UEdGraphPin* L:P->LinkedTo)Row+=L->PinId.ToString();}
            Rows.Add(Row);
        }
        Rows.Sort();return FString::Join(Rows,TEXT("\n"));
    };
    const FString Available=Record(TEXT("ReadTurboMCPOverrides available"),UTurboMCPReadToolset::ReadTurboMCPOverrides(OverrideAsset,TEXT("available")));
    TestTrue(TEXT("Hover events discovered"),Available.Contains(TEXT("BP_OnHovered"))&&Available.Contains(TEXT("BP_OnUnhovered")));
    TestTrue(TEXT("Invalid filter refused"),UTurboMCPReadToolset::ReadTurboMCPOverrides(OverrideAsset,TEXT("wrong")).StartsWith(TEXT("error:")));
    UEdGraph* Graph=nullptr;
    for (const auto& Pair : {TPair<FName,FString>(TEXT("BP_OnHovered"),TEXT("Ready to purchase")), {TEXT("BP_OnUnhovered"),TEXT("Purchase")}})
    {
        const FString BeforeOverride=Snapshot();const bool BeforeDirty=Package->IsDirty();
        Record(TEXT("PreviewTurboMCPOverrideAdd"),UTurboMCPPreviewToolset::PreviewTurboMCPOverrideAdd(OverrideAsset,Pair.Key.ToString()));
        TestEqual(TEXT("Preview preserves graph"),Snapshot(),BeforeOverride);TestEqual(TEXT("Preview preserves dirty flag"),Package->IsDirty(),BeforeDirty);
        for(bool Edit:{false,true}) {
            const FString Refusal=Record(TEXT("Override invalid name"),Edit?UTurboMCPEditToolset::EditTurboMCPOverrideAdd(OverrideAsset,TEXT("MissingOverride")):UTurboMCPPreviewToolset::PreviewTurboMCPOverrideAdd(OverrideAsset,TEXT("MissingOverride")));
            TestTrue(TEXT("Invalid override refused"),Refusal.Contains(TEXT("not something")));
            TestEqual(TEXT("Refusal preserves graph"),Snapshot(),BeforeOverride);
        }
        const FString Created=Record(TEXT("EditTurboMCPOverrideAdd"),UTurboMCPEditToolset::EditTurboMCPOverrideAdd(OverrideAsset,Pair.Key.ToString()));
        UK2Node_Event* Event=nullptr;
        for(UEdGraph* G:BP->UbergraphPages)for(UEdGraphNode* N:G->Nodes)if(UK2Node_Event* E=Cast<UK2Node_Event>(N))if(E->GetFunctionName()==Pair.Key){Event=E;Graph=G;}
        if(!TestNotNull(TEXT("Override event created"),Event))return false;
        TestTrue(TEXT("Result provides live node identity"),Created.Contains(TurboMCPNodeId::MakeForNode(Event,true)));
        const FString AfterOverride=Snapshot();
        TestTrue(TEXT("Duplicate preview explains unchanged edit"),Record(TEXT("Duplicate preview"),UTurboMCPPreviewToolset::PreviewTurboMCPOverrideAdd(OverrideAsset,Pair.Key.ToString())).Contains(TEXT("Edit will return unchanged")));
        TestEqual(TEXT("Duplicate preview preserves graph"),Snapshot(),AfterOverride);
        TestTrue(TEXT("Duplicate returns unchanged"),Record(TEXT("Override duplicate"),UTurboMCPEditToolset::EditTurboMCPOverrideAdd(OverrideAsset,Pair.Key.ToString())).StartsWith(TEXT("unchanged:")));
        TestEqual(TEXT("Duplicate preserves graph"),Snapshot(),AfterOverride);
        UK2Node_VariableGet* Get=NewObject<UK2Node_VariableGet>(Graph);Get->VariableReference.SetSelfMember(TEXT("Label"));Graph->AddNode(Get,false,false);Get->CreateNewGuid();Get->AllocateDefaultPins();
        UK2Node_CallFunction* Call=NewObject<UK2Node_CallFunction>(Graph);Call->SetFromFunction(UTextBlock::StaticClass()->FindFunctionByName(TEXT("SetText")));Graph->AddNode(Call,false,false);Call->CreateNewGuid();Call->AllocateDefaultPins();
        Call->FindPinChecked(TEXT("InText"))->DefaultTextValue=FText::FromString(Pair.Value);
        const FString EventId=TurboMCPNodeId::MakeForNode(Event,true),CallId=TurboMCPNodeId::MakeForNode(Call,true),GetId=TurboMCPNodeId::MakeForNode(Get,true);
        Record(TEXT("Connect body"),UTurboMCPEditToolset::EditTurboMCPGraphConnect(OverrideAsset,Graph->GetName(),EventId,TEXT("then"),CallId,TEXT("execute")));
        Record(TEXT("Connect label"),UTurboMCPEditToolset::EditTurboMCPGraphConnect(OverrideAsset,Graph->GetName(),GetId,TEXT("Label"),CallId,TEXT("self")));
    }
    const FString Implemented=Record(TEXT("ReadTurboMCPOverrides overridden"),UTurboMCPReadToolset::ReadTurboMCPOverrides(OverrideAsset,TEXT("overridden")));
    TestTrue(TEXT("Reader confirms both overrides"),Implemented.Contains(TEXT("BP_OnHovered"))&&Implemented.Contains(TEXT("BP_OnUnhovered")));
    const FString Remaining=UTurboMCPReadToolset::ReadTurboMCPOverrides(OverrideAsset,TEXT("available"));
    TestFalse(TEXT("Implemented hover no longer available"),Remaining.Contains(TEXT("BP_OnHovered :")));
    const FString Tidy=Record(TEXT("Tidy overrides"),UTurboMCPEditToolset::EditTurboMCPGraphTidy(OverrideAsset,Graph->GetName(),false,{}));
    TestTrue(TEXT("Tidy applies"),Tidy.StartsWith(TEXT("changed:")));
    Record(TEXT("Compile overrides"),UTurboMCPEditToolset::EditTurboMCPCompile(OverrideAsset));
    if (!TestTrue(TEXT("Overrides compile"),BP->Status!=BS_Error)) return false;
    const FString Capture=Record(TEXT("Capture overrides"),UTurboMCPLookToolset::LookTurboMCPCapture(OverrideAsset,Graph->GetName(),TEXT("graph"),TEXT(""),TEXT("1200x700")));
    TestTrue(TEXT("Graph capture succeeds"),Capture.Contains(TEXT("image: ")));
    UUserWidget* Live=NewObject<UUserWidget>(GetTransientPackage(),BP->GeneratedClass);
    Live->Initialize();Live->TakeWidget();
    UTextBlock* LiveLabel=Cast<UTextBlock>(Live->WidgetTree->FindWidget(TEXT("Label")));
    if(!TestNotNull(TEXT("Live label exists"),LiveLabel))return false;
    Live->ProcessEvent(Live->FindFunctionChecked(TEXT("BP_OnHovered")),nullptr);
    TestEqual(TEXT("Hover changes live label"),LiveLabel->GetText().ToString(),FString(TEXT("Ready to purchase")));
    Live->ProcessEvent(Live->FindFunctionChecked(TEXT("BP_OnUnhovered")),nullptr);
    TestEqual(TEXT("Unhover restores same live label"),LiveLabel->GetText().ToString(),FString(TEXT("Purchase")));
    FClassProperty* StyleProperty=FindFProperty<FClassProperty>(ButtonClass,TEXT("Style"));
    if (!TestNotNull(TEXT("Style reference reflected"),StyleProperty)) return false;
    StyleProperty->SetObjectPropertyValue_InContainer(BP->GeneratedClass->GetDefaultObject(),Style->GeneratedClass);
    const FString Asset=BP->GetPathName();
    const FString Before=Record(TEXT("ReadTurboMCPWidgetTree"),UTurboMCPReadToolset::ReadTurboMCPWidgetTree(Asset));
    TestTrue(TEXT("Tree names label"),Before.Contains(TEXT("Label")));
    TestTrue(TEXT("Tree exposes assigned style"),Before.Contains(Style->GeneratedClass->GetPathName()));
    const bool Dirty=Package->IsDirty();
    Record(TEXT("ReadTurboMCPBlueprintLoad"),UTurboMCPReadToolset::ReadTurboMCPBlueprintLoad(Asset,TEXT("signatures"),TEXT(""),TEXT("read"),TEXT("60000")));
    Record(TEXT("ReadTurboMCPGraphLoad"),UTurboMCPReadToolset::ReadTurboMCPGraphLoad(Asset,Graph->GetName(),TEXT("read"),TEXT("full")));
    auto Preview=[&](const FString& Function,const FString& Execution=TEXT("allow"),const FString& Size=TEXT("640x240"))
    { return Record(TEXT("LookTurboMCPWidgetPreview"),UTurboMCPLookToolset::LookTurboMCPWidgetPreview(Asset,TEXT(""),Function,Size,Execution)); };
    TestTrue(TEXT("Default execution refused"),Preview(TEXT(""),TEXT("deny")).StartsWith(TEXT("error:")));
    TestTrue(TEXT("Invalid size refused"),Preview(TEXT(""),TEXT("allow"),TEXT("0x240")).StartsWith(TEXT("error:")));
    auto ImagePath=[](const FString& Result){FString Left,Right;Result.Split(TEXT("\nimage: "),&Left,&Right);FString Path,Tail;Right.Split(TEXT("\n"),&Path,&Tail);return Path;};
    const FString Normal=Preview(TEXT("")), Hover=Preview(TEXT("BP_OnHovered")), Unhover=Preview(TEXT("BP_OnUnhovered"));
    for(const FString& Result:{Normal,Hover,Unhover}) TestFalse(TEXT("Preview completes without partial result"),Result.StartsWith(TEXT("partial:"))||Result.StartsWith(TEXT("error:")));
    TArray<uint8> NormalBytes,HoverBytes,UnhoverBytes;
    TestTrue(TEXT("Normal image exists"),FFileHelper::LoadFileToArray(NormalBytes,*ImagePath(Normal)));
    TestTrue(TEXT("Hover image exists"),FFileHelper::LoadFileToArray(HoverBytes,*ImagePath(Hover)));
    TestTrue(TEXT("Unhover image exists"),FFileHelper::LoadFileToArray(UnhoverBytes,*ImagePath(Unhover)));
    TestTrue(TEXT("Hover callback changes rendered label"),NormalBytes!=HoverBytes);
    TestTrue(TEXT("Unhover callback restores rendered label"),NormalBytes==UnhoverBytes);
    TestTrue(TEXT("Missing callback explicitly partial"),Preview(TEXT("MissingCallback")).StartsWith(TEXT("partial:")));
    TestEqual(TEXT("Widget data unchanged"),UTurboMCPReadToolset::ReadTurboMCPWidgetTree(Asset),Before);
    TestEqual(TEXT("Dirty state preserved in this fixture"),Package->IsDirty(),Dirty);
    TestTrue(TEXT("Default style preserved"),StyleProperty->GetObjectPropertyValue_InContainer(BP->GeneratedClass->GetDefaultObject())==Style->GeneratedClass);
    return !HasAnyErrors();
}
#endif
