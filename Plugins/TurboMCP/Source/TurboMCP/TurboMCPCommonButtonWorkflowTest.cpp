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

// Fixture setup uses engine APIs. Only recorded tool calls count as tool evidence.
// Common UI is enabled in TestProject, not made a TurboMCP dependency.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTurboMCPCommonButtonWorkflowTest,
    "TurboMCP.Workflow.CommonButton", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTurboMCPCommonButtonWorkflowTest::RunTest(const FString& Parameters)
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
        FFileHelper::SaveStringToFile(Transcript, *FPaths::Combine(Folder, TEXT("common-button-review.txt")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
        Package->SetDirtyFlag(false); // Only the unique, never-saved fixture package.
    };
    auto Record = [&](const TCHAR* Name, const FString& Value) { Transcript += FString::Printf(TEXT("\n=== %s ===\n%s\n"), Name, *Value); return Value; };
    UBlueprint* Style = FKismetEditorUtilities::CreateBlueprint(StyleClass, Package, TEXT("BS_Review"), BPTYPE_Normal, UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass(), TEXT("CommonButtonReview"));
    if (!TestNotNull(TEXT("Style fixture created"), Style)) return false;
    FKismetEditorUtilities::CompileBlueprint(Style);
    // Exercise the public style tools against a disposable shared style asset.
    const FString StyleAsset = Style->GetPathName();
    auto ReadStyle = [&](const FString& Field) { return Record(TEXT("ReadTurboMCPCommonButtonStyle"), UTurboMCPReadToolset::ReadTurboMCPCommonButtonStyle(StyleAsset, Field)); };
    auto PreviewStyle = [&](const FString& Field, const FString& Value) { return Record(TEXT("PreviewTurboMCPCommonButtonStyle"), UTurboMCPPreviewToolset::PreviewTurboMCPCommonButtonStyle(StyleAsset, Field, Value)); };
    auto EditStyle = [&](const FString& Field, const FString& Value) { return Record(TEXT("EditTurboMCPCommonButtonStyle"), UTurboMCPEditToolset::EditTurboMCPCommonButtonStyle(StyleAsset, Field, Value)); };
    const FString PaddingBefore = ReadStyle(TEXT("ButtonPadding"));
    const bool StyleDirtyBefore = Package->IsDirty();
    const FString Padding = TEXT("(Left=12,Top=8,Right=12,Bottom=8)");
    TestTrue(TEXT("Style preview succeeds"), PreviewStyle(TEXT("ButtonPadding"), Padding).StartsWith(TEXT("preview:")));
    TestEqual(TEXT("Preview preserves style value"), ReadStyle(TEXT("ButtonPadding")), PaddingBefore);
    TestEqual(TEXT("Preview preserves dirty state"), Package->IsDirty(), StyleDirtyBefore);
    TestTrue(TEXT("Padding edit succeeds"), EditStyle(TEXT("ButtonPadding"), Padding).StartsWith(TEXT("edited:")));
    TestTrue(TEXT("Padding readback"), ReadStyle(TEXT("ButtonPadding")).Contains(TEXT("Left=12")));
    TestTrue(TEXT("Identical edit is a no-op"), EditStyle(TEXT("ButtonPadding"), Padding).StartsWith(TEXT("unchanged:")));
    const FString PaddingAfter = ReadStyle(TEXT("ButtonPadding"));
    TestTrue(TEXT("Malformed input refused"), EditStyle(TEXT("ButtonPadding"), TEXT("(Left=12) trailing")).StartsWith(TEXT("error:")));
    TestTrue(TEXT("Missing field refused"), EditStyle(TEXT("MissingField"), TEXT("1")).StartsWith(TEXT("error:")));
    TestTrue(TEXT("Reference edit refused"), EditStyle(TEXT("NormalTextStyle"), TEXT("None")).StartsWith(TEXT("error:")));
    TestEqual(TEXT("Refusals preserve padding"), ReadStyle(TEXT("ButtonPadding")), PaddingAfter);
    TestTrue(TEXT("Nested color edit succeeds"), EditStyle(TEXT("NormalHovered.TintColor.SpecifiedColor"), TEXT("(R=0.2,G=0.4,B=0.8,A=1)")).StartsWith(TEXT("edited:")));
    const FString ColorAfter = ReadStyle(TEXT("NormalHovered.TintColor.SpecifiedColor"));
    TestTrue(TEXT("Nested color readback"), ColorAfter.Contains(TEXT("B=0.800000")));
    FKismetEditorUtilities::CompileBlueprint(Style);
    TestTrue(TEXT("Edited style compiles"), Style->Status != BS_Error);
    TestEqual(TEXT("Compile preserves padding"), ReadStyle(TEXT("ButtonPadding")), PaddingAfter);
    TestEqual(TEXT("Compile preserves nested color"), ReadStyle(TEXT("NormalHovered.TintColor.SpecifiedColor")), ColorAfter);
    UWidgetBlueprint* BP = FWidgetBlueprintOperationUtils::CreateWidgetBlueprint(Package, TEXT("WBP_CommonReview"), BPTYPE_Normal, ButtonClass, UCanvasPanel::StaticClass(), TEXT("CommonButtonReview"), false);
    if (!TestNotNull(TEXT("Button fixture created"), BP)) return false;
    UTextBlock* Label = BP->WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Label"));
    Label->bIsVariable = true; BP->WidgetVariableNameToGuidMap.Add(Label->GetFName(), FGuid::NewGuid()); Label->SetText(FText::FromString(TEXT("Purchase")));
    UCanvasPanelSlot* Slot = Cast<UCanvasPanel>(BP->WidgetTree->RootWidget)->AddChildToCanvas(Label);
    Slot->SetPosition(FVector2D(40,40)); Slot->SetSize(FVector2D(300,80));
    FKismetEditorUtilities::CompileBlueprint(BP);
    UEdGraph* Graph = FBlueprintEditorUtils::CreateNewGraph(BP,TEXT("ButtonEvents"),UEdGraph::StaticClass(),UEdGraphSchema_K2::StaticClass());
    FBlueprintEditorUtils::AddUbergraphPage(BP,Graph);
    for (const auto& Pair : {TPair<FName,FString>(TEXT("BP_OnHovered"),TEXT("Ready to purchase")), {TEXT("BP_OnUnhovered"),TEXT("Purchase")}})
    {
        UK2Node_Event* Event=NewObject<UK2Node_Event>(Graph); Event->EventReference.SetExternalMember(Pair.Key,ButtonClass); Event->bOverrideFunction=true;
        Graph->AddNode(Event,false,false);Event->CreateNewGuid();Event->PostPlacedNewNode();Event->AllocateDefaultPins();
        UK2Node_VariableGet* Get=NewObject<UK2Node_VariableGet>(Graph);Get->VariableReference.SetSelfMember(TEXT("Label"));Graph->AddNode(Get,false,false);Get->CreateNewGuid();Get->AllocateDefaultPins();
        UK2Node_CallFunction* Call=NewObject<UK2Node_CallFunction>(Graph);Call->SetFromFunction(UTextBlock::StaticClass()->FindFunctionByName(TEXT("SetText")));Graph->AddNode(Call,false,false);Call->CreateNewGuid();Call->AllocateDefaultPins();
        Call->FindPinChecked(TEXT("InText"))->DefaultTextValue=FText::FromString(Pair.Value);
        TestTrue(TEXT("Fixture event connected"),Graph->GetSchema()->TryCreateConnection(Event->FindPinChecked(TEXT("then")),Call->FindPinChecked(TEXT("execute"))));
        TestTrue(TEXT("Fixture label connected"),Graph->GetSchema()->TryCreateConnection(Get->FindPinChecked(TEXT("Label")),Call->FindPinChecked(TEXT("self"))));
    }
    FKismetEditorUtilities::CompileBlueprint(BP);
    if (!TestTrue(TEXT("Fixture compiles"),BP->Status!=BS_Error)) return false;
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
