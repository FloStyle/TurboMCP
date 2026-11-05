// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
#include "CoreMinimal.h"
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWindow.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "ToolsetRegistry/ToolsetRegistrySubsystem.h"
#include "ToolsetRegistry/Toolset.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTurboMCPSlateVisibilityTest,"TurboMCP.Workflow.SlateVisibility",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTurboMCPSlateVisibilityTest::RunTest(const FString&)
{
    if (!FSlateApplication::IsInitialized()) { AddError(TEXT("Rendering Slate is required.")); return false; }
    auto Registry=UToolsetRegistrySubsystem::Get();
    if (Registry.HasError()) { AddError(Registry.GetError()); return false; }
    auto Toolset=Registry.GetValue()->ToolsetRegistry.Find(TEXT("SlateInspectorToolset.SlateInspectorToolset"));
    if (!Toolset) { AddError(TEXT("Stock SlateInspector toolset is not registered.")); return false; }
    FString Evidence;
    auto Call=[&](const TCHAR* Name,const FString& Json)
    {
        auto Future=Toolset->ExecuteTool(Name,Json);
        if (!Future.IsReady()) { AddError(TEXT("Expected synchronous stock Slate call.")); return FString(); }
        auto Result=Future.Get();
        if (Result.HasError()) { AddError(Result.GetError()); return FString(); }
        Evidence+=FString(Name)+TEXT("\n")+Result.GetValue()+TEXT("\n");
        return Result.GetValue();
    };
    const FString Prefix=TEXT("TMCPVisibility_")+FGuid::NewGuid().ToString(EGuidFormats::Digits);
    const FString Visible=Prefix+TEXT("_Visible"), Collapsed=Prefix+TEXT("_Collapsed"), Nested=Prefix+TEXT("_Nested");
    TSharedPtr<STextBlock> CollapsedText;
    TSharedPtr<SVerticalBox> HiddenParent;
    const TSharedRef<SWindow> Window=SNew(SWindow).Title(FText::FromString(TEXT("TurboMCP visibility reproduction"))).ClientSize(FVector2D(500,150));
    Window->SetContent(SNew(SVerticalBox)
        +SVerticalBox::Slot()[SNew(STextBlock).Text(FText::FromString(Visible))]
        +SVerticalBox::Slot()[SAssignNew(CollapsedText,STextBlock).Text(FText::FromString(Collapsed)).Visibility(EVisibility::Collapsed)]
        +SVerticalBox::Slot()[SAssignNew(HiddenParent,SVerticalBox).Visibility(EVisibility::Collapsed)
            +SVerticalBox::Slot()[SNew(STextBlock).Text(FText::FromString(Nested))]]);
    FSlateApplication::Get().AddWindow(Window,false);
    Window->ShowWindow();
    auto Wait=[&](const FString& Label)
    {
        const FString Result=Call(TEXT("WaitFor"),FString::Printf(TEXT("{\"Text\":\"%s\",\"TextGone\":\"\"}"),*Label));
        TestTrue(TEXT("Stock text wait reports presence"),Result.Contains(TEXT("true")));
    };
    for (const FString& Label:{Visible,Collapsed,Nested}) Wait(Label);
    const FString Before=Call(TEXT("Snapshot"),TEXT("{\"Ref\":\"\",\"MaxDepth\":30,\"bIncludeSourceLocations\":false}"));
    TestTrue(TEXT("Visible positive control appears in snapshot"),Before.Contains(Visible));
    TestFalse(TEXT("Collapsed label omitted from snapshot"),Before.Contains(Collapsed));
    TestFalse(TEXT("Collapsed ancestor hides visible descendant in snapshot"),Before.Contains(Nested));
    CollapsedText->SetVisibility(EVisibility::Visible);
    HiddenParent->SetVisibility(EVisibility::Visible);
    for (const FString& Label:{Visible,Collapsed,Nested}) Wait(Label);
    const FString After=Call(TEXT("Snapshot"),TEXT("{\"Ref\":\"\",\"MaxDepth\":30,\"bIncludeSourceLocations\":false}"));
    TestTrue(TEXT("Revealed direct label appears"),After.Contains(Collapsed));
    TestTrue(TEXT("Revealed nested label appears"),After.Contains(Nested));
    Window->RequestDestroyWindow();
    TestTrue(TEXT("Reproduction transcript saved"),FFileHelper::SaveStringToFile(Evidence,*FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("TurboMCP/Workflow/slate-visibility.txt")),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM));
    return true;
}
#endif
