// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
#include "CoreMinimal.h"
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Serialization/JsonSerializer.h"
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
    auto Plugin = Registry.GetValue()->ToolsetRegistry.Find(TEXT("TurboMCP.TurboMCPReadToolset"));
    if (!Plugin) { Window->RequestDestroyWindow(); AddError(TEXT("TurboMCP reader not registered.")); return false; }
    auto Query = [&](const FString& Needle, bool IncludeHidden, int32 Limit, int32 Depth)
    {
        const FString Args = FString::Printf(TEXT("{\"text\":\"%s\",\"includeHidden\":%s,\"matchLimit\":%d,\"maxDepth\":%d}"), *Needle, IncludeHidden ? TEXT("true") : TEXT("false"), Limit, Depth);
        auto Future = Plugin->ExecuteTool(TEXT("ReadTurboMCPSlateText"), Args);
        TSharedPtr<FJsonObject> Parsed;
        if (!Future.IsReady()) { AddError(TEXT("Text inspection must be synchronous.")); return Parsed; }
        auto Response = Future.Get();
        if (Response.HasError()) { AddError(Response.GetError()); return Parsed; }
        Evidence += TEXT("Plugin text query\n") + Response.GetValue() + TEXT("\n");
        // Registry output wraps reflected return values.
        TSharedPtr<FJsonObject> Envelope;
        if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Response.GetValue()), Envelope) || !Envelope.IsValid())
        { AddError(TEXT("Invalid registered output JSON.")); return Parsed; }
        FString Body;
        if (!Envelope->TryGetStringField(TEXT("returnValue"), Body)) Body = Response.GetValue();
        if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Body), Parsed)) AddError(TEXT("Invalid inspection JSON."));
        return Parsed;
    };
    auto CheckVisibility = [&](const FString& Label, bool Expected)
    {
        auto Result = Query(Label, true, 10, 128);
        if (!Result.IsValid()) return;
        const auto& Rows = Result->GetArrayField(TEXT("matches"));
        TestTrue(TEXT("Exact unique label found"), Rows.Num() > 0);
        for (const auto& Row : Rows) TestEqual(TEXT("Ancestor-effective visibility"), Row->AsObject()->GetBoolField(TEXT("effectiveVisible")), Expected);
    };
    CheckVisibility(Visible, true);
    CheckVisibility(Collapsed, false);
    CheckVisibility(Nested, false);
    auto VisibleOnly = Query(Collapsed, false, 10, 128);
    if (VisibleOnly.IsValid()) TestEqual(TEXT("Hidden match filtered"), VisibleOnly->GetArrayField(TEXT("matches")).Num(), 0);
    if (VisibleOnly.IsValid())
    {
        TestFalse(TEXT("Visibility filter echoed"), VisibleOnly->GetBoolField(TEXT("includeHidden")));
        TestTrue(TEXT("Hidden exclusion is explicit"), VisibleOnly->GetStringField(TEXT("finding")).Contains(TEXT("hidden matches excluded")));
    }
    auto Limited = Query(Prefix, true, 1, 128);
    if (Limited.IsValid())
    {
        TestEqual(TEXT("Output limited"), Limited->GetArrayField(TEXT("matches")).Num(), 1);
        TestTrue(TEXT("More matches disclosed"), Limited->GetBoolField(TEXT("resultsTruncated")));
    }
    auto Shallow = Query(Prefix + TEXT("_Absent"), true, 10, 1);
    if (Shallow.IsValid())
    {
        TestTrue(TEXT("Depth truncation disclosed"), Shallow->GetBoolField(TEXT("traversalTruncated")));
        TestTrue(TEXT("Incomplete search cannot prove absence"), Shallow->GetStringField(TEXT("finding")).Contains(TEXT("unknown")));
    }
    TestTrue(TEXT("Query preserves collapsed child"), CollapsedText->GetVisibility() == EVisibility::Collapsed);
    TestTrue(TEXT("Query preserves collapsed parent"), HiddenParent->GetVisibility() == EVisibility::Collapsed);
    const FString Before=Call(TEXT("Snapshot"),TEXT("{\"Ref\":\"\",\"MaxDepth\":30,\"bIncludeSourceLocations\":false}"));
    TestTrue(TEXT("Visible positive control appears in snapshot"),Before.Contains(Visible));
    TestFalse(TEXT("Collapsed label omitted from snapshot"),Before.Contains(Collapsed));
    TestFalse(TEXT("Collapsed ancestor hides visible descendant in snapshot"),Before.Contains(Nested));
    CollapsedText->SetVisibility(EVisibility::Visible);
    HiddenParent->SetVisibility(EVisibility::Visible);
    for (const FString& Label:{Visible,Collapsed,Nested}) Wait(Label);
    CheckVisibility(Collapsed, true);
    CheckVisibility(Nested, true);
    const FString After=Call(TEXT("Snapshot"),TEXT("{\"Ref\":\"\",\"MaxDepth\":30,\"bIncludeSourceLocations\":false}"));
    TestTrue(TEXT("Revealed direct label appears"),After.Contains(Collapsed));
    TestTrue(TEXT("Revealed nested label appears"),After.Contains(Nested));
    Window->RequestDestroyWindow();
    TestTrue(TEXT("Reproduction transcript saved"),FFileHelper::SaveStringToFile(Evidence,*FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("TurboMCP/Workflow/slate-visibility.txt")),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM));
    return true;
}
#endif
