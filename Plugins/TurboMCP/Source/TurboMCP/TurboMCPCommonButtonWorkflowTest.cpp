// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
#include "Tools/TurboMCPToolsets.h"
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Blueprint/WidgetTree.h"
#include "Blueprint/UserWidget.h"
#include "Slate/WidgetRenderer.h"
#include "Engine/TextureRenderTarget2D.h"
#include "ImageUtils.h"
#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "Widgets/SWidget.h"
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
    // Native Slate state rendering proves the style is consumed by CommonUI.
    // This does not claim OS pointer routing, focus navigation or PIE coverage.
    const TCHAR* States[] = {TEXT("NormalBase"), TEXT("NormalHovered"), TEXT("NormalPressed"), TEXT("Disabled")};
    const TCHAR* Colors[] = {TEXT("(R=0.05,G=0.1,B=0.3,A=1)"), TEXT("(R=0.1,G=0.6,B=0.2,A=1)"), TEXT("(R=0.7,G=0.2,B=0.05,A=1)"), TEXT("(R=0.3,G=0.3,B=0.3,A=1)")};
    for (int32 Index=0; Index<4; ++Index)
    {
        const FString Brush=States[Index];
        auto AcceptEdit=[&](const FString& Field,const FString& Value)
        {
            const FString Result=EditStyle(Field,Value);
            TestTrue(TEXT("Visual style edit accepted"),Result.StartsWith(TEXT("edited:")) || Result.StartsWith(TEXT("unchanged:")));
        };
        AcceptEdit(Brush+TEXT(".DrawAs"),TEXT("Box"));
        AcceptEdit(Brush+TEXT(".TintColor.ColorUseRule"),TEXT("UseColor_Specified"));
        AcceptEdit(Brush+TEXT(".TintColor.SpecifiedColor"),Colors[Index]);
    }
    FKismetEditorUtilities::CompileBlueprint(Style);
    StyleProperty->SetObjectPropertyValue_InContainer(BP->GeneratedClass->GetDefaultObject(),Style->GeneratedClass);
    UUserWidget* Visual=NewObject<UUserWidget>(GetTransientPackage(),BP->GeneratedClass);
    if (!TestNotNull(TEXT("Visual widget created"),Visual)) return false;
    Visual->Initialize();
    // The disposable generated widget initializes from its archetype. Assign the
    // style through CommonUI's runtime setter after initialization, as a consumer
    // of the tool-edited shared asset; this is fixture setup, not a tool call.
    UFunction* SetStyleFunction=Visual->FindFunction(TEXT("SetStyle"));
    if (!TestNotNull(TEXT("CommonUI style setter available"),SetStyleFunction)) return false;
    struct { UClass* InStyle; } StyleArgs{Style->GeneratedClass};
    Visual->ProcessEvent(SetStyleFunction,&StyleArgs);
    TSharedRef<SWidget> RootWidget=Visual->TakeWidget();
    Visual->SynchronizeProperties();
    TestTrue(TEXT("Visual instance uses edited style"),StyleProperty->GetObjectPropertyValue_InContainer(Visual)==Style->GeneratedClass);
    TSharedPtr<SWidget> NativeButton;
    TFunction<void(TSharedRef<SWidget>)> FindButton=[&](TSharedRef<SWidget> Widget)
    {
        if (Widget->GetType()==TEXT("SCommonButton")) { NativeButton=Widget; return; }
        FChildren* Children=Widget->GetChildren();
        for (int32 Index=0;Index<Children->Num() && !NativeButton; ++Index) FindButton(Children->GetChildAt(Index));
    };
    FindButton(RootWidget);
    if (!TestTrue(TEXT("Actual CommonUI Slate button found"),NativeButton.IsValid())) return false;
    FWidgetRenderer Renderer(false);
    UTextureRenderTarget2D* RenderTarget=FWidgetRenderer::CreateTargetFor(FVector2D(640,240),TF_Bilinear,false);
    if (!TestNotNull(TEXT("Visual render target created"),RenderTarget)) return false;
    const FString VisualFolder=FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("TurboMCP/Workflow"),Suffix);
    IFileManager::Get().MakeDirectory(*VisualFolder,true);
    auto CaptureState=[&](const TCHAR* State)
    {
        Renderer.DrawWidget(RenderTarget,RootWidget,FVector2D(640,240),1.0f,false);
        TArray<FColor> Pixels;
        FReadSurfaceDataFlags Flags(RCM_UNorm,CubeFace_MAX);Flags.SetLinearToGamma(false);
        TestTrue(TEXT("State pixels readable"),RenderTarget->GameThread_GetRenderTargetResource()->ReadPixels(Pixels,Flags));
        if (Pixels.Num()==640*240)
        {
            const FColor Sample=Pixels[200*640+500];
            Record(TEXT("background pixel"),Sample.ToString());
            const FString StateName=State;
            if (StateName==TEXT("style-normal")) TestTrue(TEXT("Normal brush is blue"),Sample.B>Sample.R && Sample.B>Sample.G);
            if (StateName==TEXT("style-hovered")) TestTrue(TEXT("Hovered brush is green"),Sample.G>Sample.R && Sample.G>Sample.B);
            if (StateName==TEXT("style-pressed")) TestTrue(TEXT("Pressed brush is orange"),Sample.R>Sample.G && Sample.G>Sample.B);
        }
        for (FColor& Pixel:Pixels) Pixel.A=255;
        TArray64<uint8> Bytes;FImageUtils::PNGCompressImageArray(640,240,Pixels,Bytes);
        const FString Path=FPaths::Combine(VisualFolder,FString(State)+TEXT(".png"));
        TestTrue(TEXT("State image saved"),FFileHelper::SaveArrayToFile(Bytes,*Path));
        Record(State,Path);
        return Bytes;
    };
    const auto BaseImage=CaptureState(TEXT("style-normal"));
    const FPointerEvent Pointer(0,FVector2D(50,50),FVector2D(50,50),TSet<FKey>{EKeys::LeftMouseButton},EKeys::LeftMouseButton,0,FModifierKeysState());
    NativeButton->OnMouseEnter(NativeButton->GetCachedGeometry(),Pointer);
    const auto HoverImage=CaptureState(TEXT("style-hovered"));
    TestTrue(TEXT("Hover style differs from normal"),BaseImage!=HoverImage);
    TestTrue(TEXT("Native press handled"),NativeButton->OnMouseButtonDown(NativeButton->GetCachedGeometry(),Pointer).IsEventHandled());
    const auto PressImage=CaptureState(TEXT("style-pressed"));
    TestTrue(TEXT("Pressed style differs from hover"),PressImage!=HoverImage);
    NativeButton->OnMouseButtonUp(NativeButton->GetCachedGeometry(),Pointer);
    NativeButton->OnMouseLeave(Pointer);
    Visual->SetIsEnabled(false);
    const auto DisabledImage=CaptureState(TEXT("style-disabled"));
    TestTrue(TEXT("Disabled style differs from normal"),DisabledImage!=BaseImage);
    Visual->ReleaseSlateResources(true);
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
