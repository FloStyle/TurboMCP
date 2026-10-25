// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
#include "Tools/TurboMCPToolsets.h"
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Editor.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "EnhancedPlayerInput.h"
#include "InputMappingContext.h"
#include "InputAction.h"
#include "UObject/StrongObjectPtr.h"
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
TSharedPtr<SButton> ResumeButton;
FString Page;
FString Observer;
int32 Purchases = 0;
FString Evidence;
FTextBlockStyle MenuText;
TStrongObjectPtr<UInputMappingContext> Mapping;
TStrongObjectPtr<UEnhancedInputComponent> InputComponent;
int32 PauseActions=0;
int32 AnalogActions=0;
void ResumeGame()
{
    if (GEditor && GEditor->PlayWorld)
    {
        if (APlayerController* PC=GEditor->PlayWorld->GetFirstPlayerController()) {PC->SetPause(false);PC->bShowMouseCursor=false;PC->SetInputMode(FInputModeGameOnly());}
    }
    Page=TEXT("Gameplay");
}

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
    Page=TEXT("Gameplay"); Purchases=0; PauseActions=0; AnalogActions=0; Evidence.Empty();
    APlayerController* PC=GEditor->PlayWorld->GetFirstPlayerController();
    if (!PC || !Cast<UEnhancedPlayerInput>(PC->PlayerInput)) {Test->AddError(TEXT("EnhancedPlayerInput is required in the test project."));return true;}
    UEnhancedInputLocalPlayerSubsystem* Subsystem=ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer());
    if (!Subsystem) {Test->AddError(TEXT("Enhanced Input subsystem missing."));return true;}
    Mapping.Reset(NewObject<UInputMappingContext>(PC,TEXT("VisionPauseMapping")));
    UInputAction* Pause=NewObject<UInputAction>(Mapping.Get(),TEXT("VisionPause"));Pause->ValueType=EInputActionValueType::Boolean;Pause->bTriggerWhenPaused=true;
    UInputAction* Move=NewObject<UInputAction>(Mapping.Get(),TEXT("VisionMove"));Move->ValueType=EInputActionValueType::Axis1D;
    Mapping->MapKey(Pause,EKeys::P);Mapping->MapKey(Pause,EKeys::Gamepad_Special_Right);Mapping->MapKey(Move,EKeys::Gamepad_LeftX);
    InputComponent.Reset(NewObject<UEnhancedInputComponent>(PC));InputComponent->RegisterComponent();InputComponent->Priority=1000;
    InputComponent->BindActionValueLambda(Pause,ETriggerEvent::Started,[](const FInputActionValue&){++PlayMenuFixture::PauseActions;PlayMenuFixture::Page=TEXT("Main menu");if (GEditor && GEditor->PlayWorld) {auto* Controller=GEditor->PlayWorld->GetFirstPlayerController();Controller->SetPause(true);Controller->bShowMouseCursor=true;FInputModeGameAndUI Mode;Mode.SetWidgetToFocus(PlayMenuFixture::ResumeButton);Mode.SetHideCursorDuringCapture(false);Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);Controller->SetInputMode(Mode);}});
    InputComponent->BindActionValueLambda(Move,ETriggerEvent::Triggered,[](const FInputActionValue& Value){if (Value.Get<float>()>0.5f) ++PlayMenuFixture::AnalogActions;});
    PC->PushInputComponent(InputComponent.Get());Subsystem->AddMappingContext(Mapping.Get(),1000);

    MenuText=FCoreStyle::Get().GetWidgetStyle<FTextBlockStyle>("NormalText");
    MenuText.SetFont(FCoreStyle::GetDefaultFontStyle("Regular",24));
    Root = SNew(SBorder).Visibility_Lambda([]{return PlayMenuFixture::Page==TEXT("Gameplay")?EVisibility::Collapsed:EVisibility::Visible;}).Padding(40).HAlign(HAlign_Center).VAlign(VAlign_Center)
    [SNew(SVerticalBox)
     +SVerticalBox::Slot().AutoHeight().Padding(12)[SNew(STextBlock).TextStyle(&MenuText).Text_Lambda([]{return FText::FromString(PlayMenuFixture::Page);})]
     +SVerticalBox::Slot().AutoHeight().Padding(12)[SNew(SButton).TextStyle(&MenuText).ContentPadding(FMargin(20,12)).Visibility_Lambda([]{return PlayMenuFixture::Page==TEXT("Main menu")?EVisibility::Visible:EVisibility::Collapsed;}).Text(FText::FromString(TEXT("Vision Settings"))).OnClicked_Lambda([]{PlayMenuFixture::Page=TEXT("Settings menu");return FReply::Handled();})]
     +SVerticalBox::Slot().AutoHeight().Padding(12)[SNew(SButton).TextStyle(&MenuText).ContentPadding(FMargin(20,12)).Visibility_Lambda([]{return PlayMenuFixture::Page==TEXT("Settings menu")?EVisibility::Visible:EVisibility::Collapsed;}).Text(FText::FromString(TEXT("Vision Back"))).OnClicked_Lambda([]{PlayMenuFixture::Page=TEXT("Main menu");return FReply::Handled();})]
     +SVerticalBox::Slot().AutoHeight().Padding(12)[SNew(SButton).TextStyle(&MenuText).ContentPadding(FMargin(20,12)).Visibility_Lambda([]{return PlayMenuFixture::Page==TEXT("Main menu")?EVisibility::Visible:EVisibility::Collapsed;}).Text(FText::FromString(TEXT("Vision Purchase"))).OnClicked_Lambda([]{++PlayMenuFixture::Purchases;PlayMenuFixture::Page=TEXT("Purchase complete");return FReply::Handled();})]
     +SVerticalBox::Slot().AutoHeight().Padding(12)[SAssignNew(ResumeButton,SButton).TextStyle(&MenuText).ContentPadding(FMargin(20,12)).Text(FText::FromString(TEXT("Vision Resume"))).OnClicked_Lambda([]{PlayMenuFixture::ResumeGame();return FReply::Handled();})]];
    GEditor->PlayWorld->GetGameViewport()->AddViewportWidgetContent(Root.ToSharedRef(),100);
    Observer=USlateInspectorToolset::Observe(TEXT(""),50);
    Record(TEXT("Menu fixture installed; callbacks must be reached through native Slate pointer routing."));

            ++Step; return false;
        }
        if (Step>=1 && Step<=9)
        {
            const FString Shot=UTurboMCPLookToolset::LookTurboMCPCapture(TEXT(""),TEXT(""),TEXT("viewport"),TEXT("pie"),TEXT(""));
            Record(Page+TEXT("\n")+Shot);
            Test->TestTrue(TEXT("PIE game layer captured"),Shot.Contains(TEXT("image:")));
            auto Input=[&](const TCHAR* Action,const TCHAR* Key=TEXT(""),const TCHAR* Value=TEXT(""),const TCHAR* Seconds=TEXT(""))
            {
                const FString Out=UTurboMCPEditToolset::EditTurboMCPPlayInput(TEXT("pie:0"),Action,Key,Value,Seconds);Record(Out);Test->TestFalse(TEXT("Input call accepted"),Out.StartsWith(TEXT("error:")));return Out;
            };
            const FString State=UTurboMCPReadToolset::ReadTurboMCPPlayInputState(TEXT("pie:0"));Record(State);
            Test->TestTrue(TEXT("Owned hold auto released"),State.Contains(TEXT("tool-held input: none")));
            Test->TestTrue(TEXT("Mapping is reported"),State.Contains(TEXT("VisionPauseMapping")));
            if (Step==1)
            {
                Test->TestTrue(TEXT("Invalid key refuses"),UTurboMCPEditToolset::EditTurboMCPPlayInput(TEXT("pie:0"),TEXT("hold"),TEXT("NoSuchVisionKey"),TEXT(""),TEXT("0.1")).StartsWith(TEXT("error:")));
                Test->TestTrue(TEXT("Invalid duration refuses"),UTurboMCPEditToolset::EditTurboMCPPlayInput(TEXT("pie:0"),TEXT("hold"),TEXT("P"),TEXT(""),TEXT("6")).StartsWith(TEXT("error:")));
                Input(TEXT("focus"));Input(TEXT("hold"),TEXT("P"),TEXT(""),TEXT("0.15"));
                Test->TestTrue(TEXT("Overlapping hold refuses"),UTurboMCPEditToolset::EditTurboMCPPlayInput(TEXT("pie:0"),TEXT("hold"),TEXT("P"),TEXT(""),TEXT("0.1")).StartsWith(TEXT("error:")));
            }
            if (Step==2) {Test->TestEqual(TEXT("Keyboard mapped pause fired once"),PauseActions,1);Test->TestTrue(TEXT("Gameplay paused"),GEditor->PlayWorld->IsPaused());ClickAndCheck(*Test,TEXT("Vision Settings"),TEXT("Settings menu"));}
            if (Step==3) ClickAndCheck(*Test,TEXT("Vision Back"),TEXT("Main menu"));
            if (Step==4) ClickAndCheck(*Test,TEXT("Vision Purchase"),TEXT("Purchase complete"));
            if (Step==5) {Test->TestEqual(TEXT("Exactly one purchase callback"),Purchases,1);ClickAndCheck(*Test,TEXT("Vision Resume"),TEXT("Gameplay"));}
            if (Step==6) {Test->TestFalse(TEXT("Gameplay resumed"),GEditor->PlayWorld->IsPaused());Input(TEXT("focus"));Input(TEXT("hold"),TEXT("Gamepad_Special_Right"),TEXT(""),TEXT("0.15"));}
            if (Step==7) {Test->TestEqual(TEXT("Gamepad mapped pause also fired"),PauseActions,2);Test->TestTrue(TEXT("Gamepad paused gameplay"),GEditor->PlayWorld->IsPaused());ClickAndCheck(*Test,TEXT("Vision Resume"),TEXT("Gameplay"));}
            if (Step==8) {Input(TEXT("focus"));Input(TEXT("hold"),TEXT("Gamepad_LeftX"),TEXT("0.75"),TEXT("0.4"));}
            if (Step==9)
            {
                Test->TestTrue(TEXT("Analog action evaluated after resume"),AnalogActions>0);
                Input(TEXT("release"));
                USlateInspectorToolset::Unobserve(Observer);Observer.Empty();
                if (GEditor && GEditor->PlayWorld)
                {
                    auto* PC=GEditor->PlayWorld->GetFirstPlayerController();
                    ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer())->RemoveMappingContext(Mapping.Get());
                    PC->PopInputComponent(InputComponent.Get());InputComponent->UnregisterComponent();
                    GEditor->PlayWorld->GetGameViewport()->RemoveViewportWidgetContent(Root.ToSharedRef());
                }
                Mapping.Reset();InputComponent.Reset();Root.Reset();Record(TEXT("Menu, input mapping and observer removed."));
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
    const FString Start=UTurboMCPReadToolset::ReadTurboMCPPlayCheck(TEXT("25"),TEXT(""));
    if (Start.StartsWith(TEXT("error:")) || Start.Contains(TEXT("already playing"))) { AddError(Start); return false; }
    ADD_LATENT_AUTOMATION_COMMAND(FPlayMenuLoop(this));
    return true;
}
#endif
