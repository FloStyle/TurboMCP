// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
#include "Tools/TurboMCPToolsets.h"
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Editor.h"
#include "Engine/World.h"
#include "Engine/GameViewportClient.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformTime.h"
#include "SlateInspectorToolset.h"
#include "Widgets/Input/SButton.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"

namespace PlayMenuFixture
{
TSharedPtr<SWidget> Root;
FString Page;
FString Observer;
int32 Purchases = 0;
FString Evidence;
FTextBlockStyle MenuText;
void Record(const FString& Text)
{
    Evidence += Text + TEXT("\n");
    FFileHelper::SaveStringToFile(Evidence, *FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("TurboMCP/Workflow/play-menu.txt")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}
bool ClickAndCheck(FAutomationTestBase& Test, const FString& Label, const FString& Expected)
{
    const FString Snapshot = USlateInspectorToolset::Snapshot(TEXT(""), 50, true);
    Record(Snapshot);
    TArray<FString> Lines; Snapshot.ParseIntoArrayLines(Lines);
    FString Ref;
    for (const FString& Line : Lines)
    {
        if (Line.Contains(TEXT("button")) && Line.Contains(Label))
        {
            const int32 Start = Line.Find(TEXT("[ref="));
            const int32 End = Line.Find(TEXT("]"), ESearchCase::CaseSensitive, ESearchDir::FromStart, Start);
            if (Start >= 0 && End > Start) { if (!Ref.IsEmpty()) return false; Ref = Line.Mid(Start+5, End-Start-5); }
        }
    }
    if (!Test.TestFalse(TEXT("Visible button ref discovered"), Ref.IsEmpty())) return false;
    const bool Delivered = USlateInspectorToolset::Click(Ref);
    Test.TestTrue(TEXT("Native pointer click delivered"), Delivered);
    Test.TestEqual(TEXT("Independent callback state matches action"), Page, Expected);
    Record(Label + TEXT(" -> ") + Page);
    return Delivered && Page == Expected;
}
}
// One latent test owns the full session: starting another automation test ends PIE.
class FPlayMenuLoop : public IAutomationLatentCommand
{
    FAutomationTestBase* Test;
    double Next = FPlatformTime::Seconds()+2.0;
    double Deadline = FPlatformTime::Seconds()+65.0;
    int32 Step = 0;
public:
    explicit FPlayMenuLoop(FAutomationTestBase* InTest): Test(InTest) {}
    virtual bool Update() override
    {
        using namespace PlayMenuFixture;
        const double Now=FPlatformTime::Seconds();
        if (Now < Next) return false;
        Next=Now+1.0;
        if (Now > Deadline) { Test->AddError(TEXT("Owned menu workflow timed out.")); USlateInspectorToolset::Unobserve(Observer); Root.Reset(); return true; }
        if (Step==0)
        {
            if (!GEditor || !GEditor->PlayWorld || !GEditor->PlayWorld->GetGameViewport()) return false;
    Page=TEXT("Main menu"); Purchases=0; Evidence.Empty();
    MenuText=FCoreStyle::Get().GetWidgetStyle<FTextBlockStyle>("NormalText");
    MenuText.SetFont(FCoreStyle::GetDefaultFontStyle("Regular",24));
    Root = SNew(SBorder).Padding(40).HAlign(HAlign_Center).VAlign(VAlign_Center)
    [SNew(SVerticalBox)
     +SVerticalBox::Slot().AutoHeight().Padding(12)[SNew(STextBlock).TextStyle(&MenuText).Text_Lambda([]{return FText::FromString(PlayMenuFixture::Page);})]
     +SVerticalBox::Slot().AutoHeight().Padding(12)[SNew(SButton).TextStyle(&MenuText).ContentPadding(FMargin(20,12)).Visibility_Lambda([]{return PlayMenuFixture::Page==TEXT("Main menu")?EVisibility::Visible:EVisibility::Collapsed;}).Text(FText::FromString(TEXT("Vision Settings"))).OnClicked_Lambda([]{PlayMenuFixture::Page=TEXT("Settings menu");return FReply::Handled();})]
     +SVerticalBox::Slot().AutoHeight().Padding(12)[SNew(SButton).TextStyle(&MenuText).ContentPadding(FMargin(20,12)).Visibility_Lambda([]{return PlayMenuFixture::Page==TEXT("Settings menu")?EVisibility::Visible:EVisibility::Collapsed;}).Text(FText::FromString(TEXT("Vision Back"))).OnClicked_Lambda([]{PlayMenuFixture::Page=TEXT("Main menu");return FReply::Handled();})]
     +SVerticalBox::Slot().AutoHeight().Padding(12)[SNew(SButton).TextStyle(&MenuText).ContentPadding(FMargin(20,12)).Visibility_Lambda([]{return PlayMenuFixture::Page==TEXT("Main menu")?EVisibility::Visible:EVisibility::Collapsed;}).Text(FText::FromString(TEXT("Vision Purchase"))).OnClicked_Lambda([]{++PlayMenuFixture::Purchases;PlayMenuFixture::Page=TEXT("Purchase complete");return FReply::Handled();})]];
    GEditor->PlayWorld->GetGameViewport()->AddViewportWidgetContent(Root.ToSharedRef(),100);
    Observer=USlateInspectorToolset::Observe(TEXT(""),50);
    Record(TEXT("Menu fixture installed; callbacks must be reached through native Slate pointer routing."));

            ++Step; return false;
        }
        if (Step>=1 && Step<=4)
        {
            const FString Shot=UTurboMCPLookToolset::LookTurboMCPCapture(TEXT(""),TEXT(""),TEXT("viewport"),TEXT("pie"),TEXT(""));
            Record(Page+TEXT("\n")+Shot);
            Test->TestTrue(TEXT("PIE game layer captured"),Shot.Contains(TEXT("image:")) && Shot.Contains(TEXT("game layer including menu UI")));
            if (Step==1) ClickAndCheck(*Test,TEXT("Vision Settings"),TEXT("Settings menu"));
            if (Step==2) ClickAndCheck(*Test,TEXT("Vision Back"),TEXT("Main menu"));
            if (Step==3) ClickAndCheck(*Test,TEXT("Vision Purchase"),TEXT("Purchase complete"));
            if (Step==4)
            {
                Test->TestEqual(TEXT("Exactly one purchase callback"),Purchases,1);
                Test->TestEqual(TEXT("Final menu state"),Page,FString(TEXT("Purchase complete")));
                Test->TestTrue(TEXT("Viewport refuses resizing instead of changing input coordinates"),UTurboMCPLookToolset::LookTurboMCPCapture(TEXT(""),TEXT(""),TEXT("viewport"),TEXT("pie"),TEXT("1280x720")).StartsWith(TEXT("error:")));
                USlateInspectorToolset::Unobserve(Observer); Observer.Empty();
                if (GEditor && GEditor->PlayWorld && Root.IsValid()) GEditor->PlayWorld->GetGameViewport()->RemoveViewportWidgetContent(Root.ToSharedRef());
                Root.Reset(); Record(TEXT("Menu removed; observer released."));
            }
            ++Step; return false;
        }
        const FString Result=UTurboMCPReadToolset::ReadTurboMCPPlayCheck(TEXT(""),TEXT(""));
        if (Result.Contains(TEXT("played for")))
        {
            Record(Result); Test->TestTrue(TEXT("No runtime warnings or errors"),Result.Contains(TEXT("0 warning(s) and 0 error(s)")));
            Test->TestTrue(TEXT("No-play capture refuses"),UTurboMCPLookToolset::LookTurboMCPCapture(TEXT(""),TEXT(""),TEXT("viewport"),TEXT("pie"),TEXT("" )).StartsWith(TEXT("error:")));
            return true;
        }
        return false;
    }
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTurboMCPMenuLoop, "TurboMCP.Workflow.PlayMenu", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTurboMCPMenuLoop::RunTest(const FString&)
{
    const FString Start=UTurboMCPReadToolset::ReadTurboMCPPlayCheck(TEXT("15"),TEXT(""));
    if (Start.StartsWith(TEXT("error:")) || Start.Contains(TEXT("already playing"))) { AddError(Start); return false; }
    ADD_LATENT_AUTOMATION_COMMAND(FPlayMenuLoop(this));
    return true;
}
#endif
