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
#include "K2Node_CustomEvent.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
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
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Widgets/SWidget.h"
#include "Slate/WidgetRenderer.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Components/Button.h"
#include "WidgetBlueprintOperationUtils.h"
#include "TurboMCPNodeId.h"

// Designer widgets and getter function bodies are fixture prerequisites.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTurboMCPWidgetBindingWorkflowTest,
    "TurboMCP.Workflow.WidgetBinding",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTurboMCPWidgetBindingWorkflowTest::RunTest(const FString& Parameters)
{
    const FString Suffix=FGuid::NewGuid().ToString(EGuidFormats::Digits);
    UPackage* Package=CreatePackage(*(TEXT("/Game/Tests/WidgetBinding_")+Suffix));FString Transcript;
    ON_SCOPE_EXIT {const FString Folder=FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("TurboMCP/Workflow"),Suffix);IFileManager::Get().MakeDirectory(*Folder,true);FFileHelper::SaveStringToFile(Transcript,*FPaths::Combine(Folder,TEXT("widget-binding-review.txt")),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);Package->SetDirtyFlag(false);};
    auto Record=[&](const TCHAR* Name,const FString& Value){Transcript+=FString::Printf(TEXT("\n=== %s ===\n%s\n"),Name,*Value);return Value;};
    UWidgetBlueprint* BP=FWidgetBlueprintOperationUtils::CreateWidgetBlueprint(Package,TEXT("WBP_Binding"),BPTYPE_Normal,UUserWidget::StaticClass(),UCanvasPanel::StaticClass(),TEXT("WidgetBindingReview"),false);if(!BP)return false;
    UTextBlock* Label=BP->WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),TEXT("PurchaseLabel"));Label->bIsVariable=true;BP->WidgetVariableNameToGuidMap.Add(Label->GetFName(),FGuid::NewGuid());Label->SetText(FText::FromString(TEXT("Original")));
    UCanvasPanelSlot* Slot=Cast<UCanvasPanel>(BP->WidgetTree->RootWidget)->AddChildToCanvas(Label);Slot->SetPosition(FVector2D(40,40));Slot->SetSize(FVector2D(300,80));
    UButton* Button=BP->WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(),TEXT("PurchaseButton"));
    BP->WidgetVariableNameToGuidMap.Add(Button->GetFName(),FGuid::NewGuid());
    Cast<UCanvasPanel>(BP->WidgetTree->RootWidget)->AddChildToCanvas(Button)->SetPosition(FVector2D(400,40));
    const FString Asset=BP->GetPathName();
    auto Getter=[&](const FString& Name,const FString& Type,const FString& Value,bool Pure) {
        Record(TEXT("Fixture function"),UTurboMCPEditToolset::EditTurboMCPFunctionAdd(Asset,Name,TEXT(""),TEXT("ReturnValue: ")+Type));
        for(UEdGraph* G:BP->FunctionGraphs)if(G->GetName()==Name)for(UEdGraphNode* N:G->Nodes) {
            if(UK2Node_FunctionEntry* E=Cast<UK2Node_FunctionEntry>(N))if(Pure)E->AddExtraFlags(FUNC_BlueprintPure|FUNC_Const);
            if(UK2Node_FunctionResult* R=Cast<UK2Node_FunctionResult>(N)) {UEdGraphPin* P=R->FindPinChecked(TEXT("ReturnValue"));if(Type==TEXT("text"))P->DefaultTextValue=FText::FromString(Value);else P->DefaultValue=Value;}
        }
    };
    Getter(TEXT("GetPurchaseText"),TEXT("text"),TEXT("Purchase ready"),true);
    Getter(TEXT("GetSoldText"),TEXT("text"),TEXT("Sold out"),true);
    Getter(TEXT("GetWrongType"),TEXT("int"),TEXT("7"),true);
    Getter(TEXT("GetImpureText"),TEXT("text"),TEXT("Unsafe"),false);
    Record(TEXT("Compile getters"),UTurboMCPEditToolset::EditTurboMCPCompile(Asset));if(!TestTrue(TEXT("Fixture functions compile"),BP->Status!=BS_Error))return false;
    auto Bind=[&](const FString& W,const FString& P,const FString& F){return Record(TEXT("EditTurboMCPWidgetBindAdd"),UTurboMCPEditToolset::EditTurboMCPWidgetBindAdd(Asset,W,P,F));};
    TestTrue(TEXT("Valid binding created"),Bind(TEXT("PurchaseLabel"),TEXT("Text"),TEXT("GetPurchaseText")).StartsWith(TEXT("changed:")));
    TestEqual(TEXT("One binding"),BP->Bindings.Num(),1);
    Package->SetDirtyFlag(false); // Prove no-op does not dirty a clean fixture.
    const bool Dirty=Package->IsDirty();
    TestTrue(TEXT("Alias and case request unchanged"),Bind(TEXT("purchaselabel"),TEXT("TextDelegate"),TEXT("GetPurchaseText")).StartsWith(TEXT("unchanged:")));
    TestEqual(TEXT("No-op dirty preserved"),Package->IsDirty(),Dirty);
    for(const auto& Case:{TPair<FString,FString>(TEXT("Text"),TEXT("MissingGetter")),{TEXT("MissingProperty"),TEXT("GetPurchaseText")},{TEXT("Text"),TEXT("GetWrongType")},{TEXT("Text"),TEXT("GetImpureText")},{TEXT("Text"),TEXT("")}}) {
        TestTrue(TEXT("Invalid request unchanged"),Bind(TEXT("PurchaseLabel"),Case.Key,Case.Value).StartsWith(TEXT("unchanged:")));
        TestEqual(TEXT("Invalid request retains single binding"),BP->Bindings.Num(),1);TestEqual(TEXT("Valid function retained"),BP->Bindings[0].FunctionName,FName(TEXT("GetPurchaseText")));
    }
    TestTrue(TEXT("Multicast event refused"),Bind(TEXT("PurchaseButton"),TEXT("OnClicked"),TEXT("GetPurchaseText")).StartsWith(TEXT("unchanged:")));
    TestTrue(TEXT("Missing widget refused"),Bind(TEXT("MissingWidget"),TEXT("Text"),TEXT("GetPurchaseText")).StartsWith(TEXT("error:")));
    auto Display=[&](const FString& Expected) {
        Record(TEXT("Compile binding"),UTurboMCPEditToolset::EditTurboMCPCompile(Asset));if(!TestTrue(TEXT("Binding compiles"),BP->Status!=BS_Error))return;
        const FString Image=Record(TEXT("Widget preview"),UTurboMCPLookToolset::LookTurboMCPWidgetPreview(Asset,TEXT(""),TEXT(""),TEXT("640x240"),TEXT("allow")));
        TestTrue(TEXT("Preview image rendered"),Image.Contains(TEXT("image: "))&&!Image.StartsWith(TEXT("partial:")));
        TestEqual(TEXT("Compiled binding count"),CastChecked<UWidgetBlueprintGeneratedClass>(BP->GeneratedClass)->Bindings.Num(),1);
        UUserWidget* Live=NewObject<UUserWidget>(GetTransientPackage(),BP->GeneratedClass);Live->Initialize();
        // Render a frame so Slate evaluates the bound text attribute before read-back.
        FWidgetRenderer Renderer(false);
        UTextureRenderTarget2D* Target=FWidgetRenderer::CreateTargetFor(FVector2D(640,240),TF_Bilinear,false);
        if(!TestNotNull(TEXT("Render target exists"),Target))return;
        // Retain Slate ownership through read-back; destruction releases child Slate resources.
        TSharedRef<SWidget> LiveSlate=Live->TakeWidget();
        Renderer.DrawWidget(Target,LiveSlate,FVector2D(640,240),1.0f,false);
        UTextBlock* LiveLabel=Cast<UTextBlock>(Live->WidgetTree->FindWidget(TEXT("PurchaseLabel")));if(!TestNotNull(TEXT("Live label exists"),LiveLabel))return;
        TestTrue(TEXT("Text delegate bound"),LiveLabel->TextDelegate.IsBound());
        TestEqual(TEXT("Displayed bound text"),LiveLabel->GetText().ToString(),Expected);

    };
    Display(TEXT("Purchase ready"));
    const FString Replaced=Bind(TEXT("PurchaseLabel"),TEXT("TextDelegate"),TEXT("GetSoldText"));
    TestTrue(TEXT("Replacement reports prior function"),Replaced.StartsWith(TEXT("changed:"))&&Replaced.Contains(TEXT("GetPurchaseText")));
    TestEqual(TEXT("Replacement is not duplicate"),BP->Bindings.Num(),1);Display(TEXT("Sold out"));
    TestEqual(TEXT("Designer text preserved"),Label->GetText().ToString(),FString(TEXT("Original")));
    TestTrue(TEXT("Designer widget retained"),BP->WidgetTree->FindWidget(TEXT("PurchaseLabel"))==Label);
    TestTrue(TEXT("Slot preserved"),Slot->GetPosition()==FVector2D(40,40)&&Slot->GetSize()==FVector2D(300,80));
    return !HasAnyErrors();
}
#endif
