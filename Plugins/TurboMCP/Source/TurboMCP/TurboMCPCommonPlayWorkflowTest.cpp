#include "TurboMCPFocusGuard.h"
// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
#include "Tools/TurboMCPToolsets.h"
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Editor.h"
#include "ToolsetRegistry/ToolsetRegistrySubsystem.h"
#include "ToolsetRegistry/Toolset.h"
#include "Blueprint/UserWidget.h"
#include "UObject/UObjectIterator.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformTime.h"
#include "HAL/PlatformProcess.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWindow.h"
#include "Widgets/SViewport.h"
#include "GenericPlatform/GenericWindow.h"
#include "Engine/GameViewportClient.h"
#include "Widgets/Input/SButton.h"
#include "GameFramework/PlayerController.h"

// The project owns the CommonUI fixture. Reflection avoids a production plugin
// dependency on either the project module or CommonUI.
class FCommonPlayLoop : public IAutomationLatentCommand
{
    FAutomationTestBase* Test;
    TWeakObjectPtr<AActor> Fixture;
    FString Evidence;
    TSharedPtr<SWindow> FocusWindow;
    TSharedPtr<SButton> OutsideButton;
    int32 FocusStep=0;
    int32 AnalogMenuStep=0;
    int32 DeactivationStep=0;
    int32 SequenceStep=0;
    bool SimulatedInactive=false;
    bool RestoreFocusGuard=false;
    int32 Step=0;
    double Next=FPlatformTime::Seconds()+2;
    double Deadline=FPlatformTime::Seconds()+65;
    void Record(const FString& Text)
    {
        Evidence+=Text+TEXT("\n");
        FFileHelper::SaveStringToFile(Evidence,*FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("TurboMCP/Workflow/common-play.txt")),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    }
    FString State()
    {
        struct {FString ReturnValue;} Args;
        if (!Fixture.IsValid()) return TEXT("fixture missing");
        UFunction* Function=Fixture->FindFunction(TEXT("Describe"));
        if (!Function) return TEXT("fixture Describe missing");
        Fixture->ProcessEvent(Function,&Args);Record(Args.ReturnValue);return Args.ReturnValue;
    }
    FString Dispatch(const FString& Name,const FString& Json)
    {
        auto Registry=UToolsetRegistrySubsystem::Get();
        if(Registry.HasError()){Test->AddError(Registry.GetError());return TEXT("error: registry unavailable");}
        auto Toolset=Registry.GetValue()->ToolsetRegistry.Find(TEXT("TurboMCP.TurboMCPEditToolset"));
        if(!Toolset){Test->AddError(TEXT("Registered edit toolset unavailable"));return TEXT("error: edit toolset unavailable");}
        auto Future=Toolset->ExecuteTool(Name,Json);
        if(!Future.IsReady()){Test->AddError(TEXT("Synchronous input dispatch unexpectedly pending"));return TEXT("error: input dispatch pending");}
        auto Result=Future.Get();return Result.HasError()?TEXT("error: ")+Result.GetError():Result.GetValue();
    }
    void Hold(const TCHAR* Key,const TCHAR* Value=TEXT(""),const TCHAR* Duration=TEXT("0.15"))
    {
        FString Out=Dispatch(TEXT("EditTurboMCPPlayInput"),FString::Printf(TEXT("{\"Target\":\"pie:0\",\"Action\":\"hold\",\"Key\":\"%s\",\"Value\":\"%s\",\"Seconds\":\"%s\"}"),Key,Value,Duration));
        Record(Out);Test->TestTrue(TEXT("Input route accepted inside registry response"),Out.Contains(TEXT("play input: routed ")) && !Out.Contains(TEXT("error:")));
    }
public:
    explicit FCommonPlayLoop(FAutomationTestBase* InTest):Test(InTest) {}
    ~FCommonPlayLoop() {if (RestoreFocusGuard) FTurboMCPFocusGuard::Install();if (SimulatedInactive && FSlateApplication::IsInitialized()) FSlateApplication::Get().OnApplicationActivationChanged(true);if (FocusWindow.IsValid()) FocusWindow->RequestDestroyWindow();}
    virtual bool Update() override
    {
        double Now=FPlatformTime::Seconds();if (Now<Next) return false;Next=Now+1;
        if (Now>Deadline) {Test->AddError(TEXT("CommonUI workflow timed out."));if (Fixture.IsValid()) Fixture->Destroy();return true;}
        if (Step==1 && RestoreFocusGuard)
        {
            const FString SettledState = UTurboMCPReadToolset::ReadTurboMCPPlayInputState(TEXT("pie:0"));
            Record(TEXT("Native activation state after normal ticks (guard suspended):\n") + SettledState);
            const auto Viewport = GEditor->PlayWorld->GetGameViewport()->GetGameViewportWidget();
            const auto Window = FSlateApplication::Get().FindWidgetWindow(Viewport.ToSharedRef());
            const bool NativeForeground = Window.IsValid() && Window->GetNativeWindow().IsValid()
                && Window->GetNativeWindow()->IsForegroundWindow();
            Test->TestTrue(TEXT("Activation readback matches actual native foreground result"),
                SettledState.Contains(NativeForeground ? TEXT("PIE owning window foreground: yes") : TEXT("PIE owning window foreground: no")));
            Record(NativeForeground ? TEXT("OS granted native foreground activation.")
                : TEXT("OS did not grant foreground activation in this run; manual fallback is required. This is not a successful activation."));
            FTurboMCPFocusGuard::Install();
            RestoreFocusGuard = false;
        }
        if (Step==0)
        {
            if (!GEditor || !GEditor->PlayWorld) return false;
            UClass* Class=LoadClass<AActor>(nullptr,TEXT("/Script/TurboMCPTest.TurboMCPCommonPlayFixture"));
            if (!Test->TestNotNull(TEXT("Project CommonUI fixture class"),Class)) return true;
            Fixture=GEditor->PlayWorld->SpawnActor<AActor>(Class);
            if (!Test->TestTrue(TEXT("Fixture spawned"),Fixture.IsValid())) return true;
            // Request actual window foreground; do not disable the input tool's
            // inactive-application refusal to make the test pass.
            RestoreFocusGuard = FTurboMCPFocusGuard::IsActive();
            if (RestoreFocusGuard)
            {
                Test->TestTrue(TEXT("Unattended guard refuses activation explicitly"),
                    UTurboMCPEditToolset::EditTurboMCPPlayInput(TEXT("pie:0"),TEXT("activate"),TEXT(""),TEXT(""),TEXT("")).Contains(TEXT("unattended focus guard")));
                FTurboMCPFocusGuard::Remove();
            }
            const FString Activation = UTurboMCPEditToolset::EditTurboMCPPlayInput(TEXT("pie:0"),TEXT("activate"),TEXT(""),TEXT(""),TEXT(""));
            Record(Activation);

            Test->TestTrue(TEXT("Activation reports observed state without promising delivery"), Activation.Contains(TEXT("requested activation"))
                && Activation.Contains(TEXT("window foreground now:")) && Activation.Contains(TEXT("No key was sent")) && Activation.Contains(TEXT("owning window is still not foreground"))
                && Activation.Contains(TEXT("select the editor window manually")));
            Test->TestTrue(TEXT("Activation rejects unexpected key"), UTurboMCPEditToolset::EditTurboMCPPlayInput(TEXT("pie:0"),TEXT("activate"),TEXT("P"),TEXT(""),TEXT("")).StartsWith(TEXT("error:")));
            const FString ActivationState = UTurboMCPReadToolset::ReadTurboMCPPlayInputState(TEXT("pie:0"));
            Record(ActivationState);
            Test->TestTrue(TEXT("State separates foreground from application activation"), ActivationState.Contains(TEXT("PIE owning window foreground:"))
                && ActivationState.Contains(TEXT("application active:")) && ActivationState.Contains(TEXT("tool-held input: none")));
            const FString InputHelp = UTurboMCPReadToolset::ReadTurboMCPToolHelp(TEXT("TurboMCP.TurboMCPEditToolset"),TEXT("EditTurboMCPPlayInput"));
            Test->TestTrue(TEXT("Help explains activation and separate focus"), InputHelp.Contains(TEXT("activate")) && InputHelp.Contains(TEXT("Slate focus")));
            Record(TEXT("Registered PlayInput help:\n") + InputHelp);
            Test->TestTrue(TEXT("Help gives analog menu example and limits"), InputHelp.Contains(TEXT("Gamepad_LeftY=-1")) && InputHelp.Contains(TEXT("Seconds=0.3")) && InputHelp.Contains(TEXT("exactly one row")) && InputHelp.Contains(TEXT("UI routing")));

            Record(Dispatch(TEXT("EditTurboMCPPlayInput"),TEXT("{\"Target\":\"pie:0\",\"Action\":\"focus\",\"Key\":\"\",\"Value\":\"\",\"Seconds\":\"\"}")));
            const FString Refused=Dispatch(TEXT("EditTurboMCPFunctionAdd"),TEXT("{\"Asset\":\"NotAnAsset\",\"Name\":\"MustNotEditInPIE\",\"Inputs\":\"\",\"Outputs\":\"\"}"));
            Test->TestTrue(TEXT("Asset edits still refused by PIE gate"),Refused.Contains(TEXT("no play session")));
            for (const TCHAR* Tool : {TEXT("EditTurboMCPCompile"), TEXT("EditTurboMCPSave"), TEXT("EditTurboMCPSaveWithoutThumbnail")})
            {
                const FString Result=Dispatch(Tool,TEXT("{\"Assets\":\"NotAnAsset\"}"));
                Record(Result);
                Test->TestTrue(TEXT("PIE refusal precedes asset lookup"),Result.Contains(TEXT("no play session")));
                Test->TestTrue(TEXT("Earlier edits are not reported as rolled back"),Result.Contains(TEXT("does not undo them")));
                Test->TestTrue(TEXT("Refused call is not queued"),Result.Contains(TEXT("not queued")));
                Test->TestTrue(TEXT("PIE refusal gives dirty-asset recovery step"),Result.Contains(TEXT("ReadTurboMCPDirtyList")));
            }
            ++Step;return false;
        }
        if (Step<=10 && (!Fixture.IsValid() || !GEditor || !GEditor->PlayWorld))
        {
            Test->AddError(TEXT("PIE or fixture disappeared before the workflow completed."));
            return true;
        }
        if (Step<=10)
        {
            // Pause before the existing confirm step to prove real directional
            // navigation, independently of the tool's delivery message.
            if (Step==2 && FocusStep<2)
            {
                const FString CurrentFocus=State();
                Test->TestTrue(TEXT("Directional focus reached expected button"),CurrentFocus.Contains(FocusStep==0?TEXT("focused button: Settings"):TEXT("focused button: Resume")));
                if (FocusStep == 0)
                {
                    const TSharedPtr<SWidget> MenuFocus = FSlateApplication::Get().GetUserFocusedWidget(0);
                    Test->TestTrue(TEXT("Menu has a focused widget before input"), MenuFocus.IsValid());
                    Hold(TEXT("LeftShift"));
                    Test->TestTrue(TEXT("Hold preserves menu focus identity"),
                        FSlateApplication::Get().GetUserFocusedWidget(0) == MenuFocus);
                    Record(UTurboMCPEditToolset::EditTurboMCPPlayInput(TEXT("pie:0"), TEXT("release"), TEXT(""), TEXT(""), TEXT("")));
                    const FString Transfer = UTurboMCPEditToolset::EditTurboMCPPlayInput(TEXT("pie:0"), TEXT("focus"), TEXT(""), TEXT(""), TEXT(""));
                    Record(Transfer);
                    Test->TestTrue(TEXT("Explicit focus reports displaced menu"),
                        Transfer.Contains(TEXT("PIE child/menu focus displaced: yes"))
                        && Transfer.Contains(TEXT("previous focus widget type:"))
                        && Transfer.Contains(TEXT("current focus widget type:"))
                        && Transfer.Contains(TEXT("skip focus and use hold directly"))
                        && Transfer.Contains(TEXT("not a focus-only action"))
                        && !Transfer.Contains(TEXT("PlayWidgetFocus")));
                    // Restore only this fixture's captured focus, then prove normal
                    // menu navigation handles the next input without a focus call.
                    if (MenuFocus.IsValid()) FSlateApplication::Get().SetUserFocus(0, MenuFocus, EFocusCause::SetDirectly);
                }
                Hold(FocusStep==0?TEXT("Gamepad_DPad_Down"):TEXT("Gamepad_DPad_Up"));
                ++FocusStep;return false;
            }
            if (Step==2 && AnalogMenuStep<3)
            {
                const FString MenuState = State();
                if (AnalogMenuStep==0)
                {
                    Test->TestTrue(TEXT("Analog control starts on Settings"), MenuState.Contains(TEXT("focused button: Settings")));
                    Hold(TEXT("Gamepad_LeftY"),TEXT("-1"),TEXT("0.3"));
                }
                else if (AnalogMenuStep==1)
                {
                    Test->TestTrue(TEXT("Analog down moves actual menu focus to Resume"), MenuState.Contains(TEXT("focused button: Resume")));
                    const FString Released = UTurboMCPReadToolset::ReadTurboMCPPlayInputState(TEXT("pie:0"));
                    Record(Released);
                    Test->TestTrue(TEXT("Analog menu hold released"), Released.Contains(TEXT("tool-held input: none")));
                    Hold(TEXT("Gamepad_DPad_Up"));
                }
                else Test->TestTrue(TEXT("D-pad control restores Settings after analog"), MenuState.Contains(TEXT("focused button: Settings")));
                ++AnalogMenuStep;return false;
            }
            if (Step==10 && FocusStep<6)
            {
                if (FocusStep==2)
                {
                    Record(UTurboMCPEditToolset::EditTurboMCPPlayInput(TEXT("pie:0"),TEXT("focus"),TEXT(""),TEXT(""),TEXT("")));
                    Hold(TEXT("Gamepad_RightX"),TEXT("0.5"),TEXT("5"));
                }
                else if (FocusStep==3)
                {
                    FocusWindow=SNew(SWindow).Title(FText::FromString(TEXT("TurboMCP owned focus-loss fixture"))).ClientSize(FVector2D(320,100));
                    FocusWindow->SetContent(SAssignNew(OutsideButton,SButton).Text(FText::FromString(TEXT("Outside PIE"))));
                    FSlateApplication::Get().AddWindow(FocusWindow.ToSharedRef());
                    FSlateApplication::Get().SetUserFocus(0,OutsideButton,EFocusCause::SetDirectly);
                }
                else if (FocusStep==4)
                {
                    const FString Cancelled=UTurboMCPReadToolset::ReadTurboMCPPlayInputState(TEXT("pie:0"));Record(Cancelled);
                    Test->TestTrue(TEXT("Focus loss cancelled long hold before expiry"),Cancelled.Contains(TEXT("tool-held input: none")) && Cancelled.Contains(TEXT("last release reason: focus left the PIE viewport")));
                    Test->TestEqual(TEXT("Original player axis cleared"),GEditor->PlayWorld->GetFirstPlayerController()->GetInputAnalogKeyState(EKeys::Gamepad_RightX),0.f);
                    Test->TestTrue(TEXT("Input outside PIE refused"),UTurboMCPEditToolset::EditTurboMCPPlayInput(TEXT("pie:0"),TEXT("hold"),TEXT("P"),TEXT(""),TEXT("0.15")).StartsWith(TEXT("error:")));
                    FocusWindow->RequestDestroyWindow();FocusWindow.Reset();OutsideButton.Reset();
                }
                else
                {
                    Record(UTurboMCPEditToolset::EditTurboMCPPlayInput(TEXT("pie:0"),TEXT("focus"),TEXT(""),TEXT(""),TEXT("")));
                    Hold(TEXT("Gamepad_RightX"),TEXT("0.5"),TEXT("5"));
                    Record(UTurboMCPEditToolset::EditTurboMCPPlayInput(TEXT("pie:0"),TEXT("release"),TEXT(""),TEXT(""),TEXT("")));
                    Test->TestTrue(TEXT("Explicit release reported"),UTurboMCPReadToolset::ReadTurboMCPPlayInputState(TEXT("pie:0")).Contains(TEXT("last release reason: explicit release")));
                }
                ++FocusStep;return false;
            }
            if (Step==10 && DeactivationStep<4)
            {
                // Exercise the native callback, not OS focus switching. Restore
                // activation on every exit so this fixture cannot strand Slate.
                if (DeactivationStep==0) Hold(TEXT("Gamepad_RightX"),TEXT("0.5"),TEXT("5"));
                else if (DeactivationStep==1)
                {
                    Record(TEXT("Simulating application deactivation callback; this is not an OS Alt-Tab test."));
                    SimulatedInactive=true;FSlateApplication::Get().OnApplicationActivationChanged(false);
                }
                else if (DeactivationStep==2)
                {
                    // Keep this callback-level assertion in the same tick as the
                    // simulated deactivation; real OS events can reactivate between ticks.
                    FSlateApplication::Get().OnApplicationActivationChanged(false);
                    const FString Inactive=UTurboMCPReadToolset::ReadTurboMCPPlayInputState(TEXT("pie:0"));Record(Inactive);
                    Test->TestTrue(TEXT("Deactivation cancelled held input"),Inactive.Contains(TEXT("tool-held input: none")) && Inactive.Contains(TEXT("last release reason: application deactivated")));
                    Test->TestTrue(TEXT("Inactive input refused"),UTurboMCPEditToolset::EditTurboMCPPlayInput(TEXT("pie:0"),TEXT("hold"),TEXT("P"),TEXT(""),TEXT("0.15")).Contains(TEXT("application is inactive")));
                    const FString InactiveFocus = UTurboMCPEditToolset::EditTurboMCPPlayInput(TEXT("pie:0"),TEXT("focus"),TEXT(""),TEXT(""),TEXT(""));
                    Record(InactiveFocus);
                    Test->TestTrue(TEXT("Slate focus does not claim OS activation"), InactiveFocus.Contains(TEXT("OS foreground activation was not requested"))
                        && InactiveFocus.Contains(TEXT("application active: no")));
                    FSlateApplication::Get().OnApplicationActivationChanged(true);SimulatedInactive=false;
                    Record(UTurboMCPEditToolset::EditTurboMCPPlayInput(TEXT("pie:0"),TEXT("focus"),TEXT(""),TEXT(""),TEXT("")));
                }
                else
                {
                    Test->TestEqual(TEXT("No stale analog input after reactivation"),GEditor->PlayWorld->GetFirstPlayerController()->GetInputAnalogKeyState(EKeys::Gamepad_RightX),0.f);
                    Hold(TEXT("Gamepad_RightX"),TEXT("0.25"));
                }
                ++DeactivationStep;return false;
            }
            if (Step==10 && SequenceStep<9)
            {
                auto Sequence=[&](const TCHAR* Events)
                {
                    const FString Result=UTurboMCPEditToolset::EditTurboMCPPlayInput(TEXT("pie:0"),TEXT("sequence"),Events,TEXT(""),TEXT(""));
                    Record(Result);return Result;
                };
                APlayerController* Player=GEditor->PlayWorld->GetFirstPlayerController();
                if (SequenceStep==0)
                {
                    Test->TestTrue(TEXT("Malformed schedule refuses"),Sequence(TEXT("[{\"at\":0,\"key\":\"P\",\"duration\":0.2,\"typo\":1}]")).StartsWith(TEXT("error:")));
                    Test->TestTrue(TEXT("Overlapping same key refuses"),Sequence(TEXT("[{\"at\":0,\"key\":\"P\",\"duration\":1},{\"at\":0.1,\"key\":\"P\",\"duration\":0.2}]")).Contains(TEXT("must not overlap")));
                    Test->TestTrue(TEXT("Refused schedules preserve gameplay"),State().Contains(TEXT("screen: Gameplay")));
                    Test->TestTrue(TEXT("Overlapping axis and taps admitted"),Sequence(TEXT("[{\"at\":0,\"key\":\"Gamepad_RightX\",\"value\":0.5,\"duration\":2.5},{\"at\":0.2,\"key\":\"P\",\"duration\":0.2},{\"at\":1.2,\"key\":\"BackSpace\",\"duration\":0.2}]")).Contains(TEXT("admitted 3 events")));
                }
                else if (SequenceStep==1)
                {
                    Test->TestTrue(TEXT("First scheduled tap opened pause while axis held"),State().Contains(TEXT("screen: Pause")));
                    Test->TestTrue(TEXT("Axis ownership overlaps scheduled tap"),UTurboMCPReadToolset::ReadTurboMCPPlayInputState(TEXT("pie:0")).Contains(TEXT("sequence held: 1; pending: 1")));
                }
                else if (SequenceStep==2)
                {
                    Test->TestTrue(TEXT("Second scheduled tap resumed game"),State().Contains(TEXT("screen: Gameplay")));
                    Test->TestTrue(TEXT("Concurrent ordinary hold refuses"),UTurboMCPEditToolset::EditTurboMCPPlayInput(TEXT("pie:0"),TEXT("hold"),TEXT("P"),TEXT(""),TEXT("0.1")).Contains(TEXT("sequence is active")));
                }
                else if (SequenceStep==3)
                {
                    const FString Completed=UTurboMCPReadToolset::ReadTurboMCPPlayInputState(TEXT("pie:0"));Record(Completed);
                    Test->TestTrue(TEXT("Sequence completes with no retained ownership"),Completed.Contains(TEXT("completed: all scheduled inputs released")) && Completed.Contains(TEXT("sequence held: 0; pending: 0")));
                    Test->TestEqual(TEXT("Sequence axis released"),Player->GetInputAnalogKeyState(EKeys::Gamepad_RightX),0.f);
                    Test->TestFalse(TEXT("Sequence keyboard keys released"),Player->IsInputKeyDown(EKeys::P) || Player->IsInputKeyDown(EKeys::BackSpace));
                    Test->TestTrue(TEXT("Cancellation sequence accepted"),Sequence(TEXT("[{\"at\":0,\"key\":\"Gamepad_RightX\",\"value\":0.5,\"duration\":4},{\"at\":3,\"key\":\"P\",\"duration\":0.2}]")).Contains(TEXT("admitted 2 events")));
                }
                else if (SequenceStep==4)
                {
                    Record(UTurboMCPEditToolset::EditTurboMCPPlayInput(TEXT("pie:0"),TEXT("release"),TEXT(""),TEXT(""),TEXT("")));
                    const FString Cancelled=UTurboMCPReadToolset::ReadTurboMCPPlayInputState(TEXT("pie:0"));Record(Cancelled);
                    Test->TestTrue(TEXT("Explicit cancellation clears owned and pending sequence"),Cancelled.Contains(TEXT("stopped: explicit release")) && Cancelled.Contains(TEXT("sequence held: 0; pending: 0")));
                    Test->TestEqual(TEXT("Cancellation clears original player axis"),Player->GetInputAnalogKeyState(EKeys::Gamepad_RightX),0.f);
                    Test->TestTrue(TEXT("Pending pause tap did not run"),State().Contains(TEXT("screen: Gameplay")));
                }
                else if (SequenceStep==5)
                {
                    Test->TestTrue(TEXT("Focus-loss sequence accepted"),Sequence(TEXT("[{\"at\":0,\"key\":\"Gamepad_RightX\",\"value\":0.5,\"duration\":4},{\"at\":3,\"key\":\"P\",\"duration\":0.2}]")).Contains(TEXT("admitted 2 events")));
                }
                else if (SequenceStep==6)
                {
                    FocusWindow=SNew(SWindow).Title(FText::FromString(TEXT("Sequence focus-loss fixture"))).ClientSize(FVector2D(320,100));
                    FocusWindow->SetContent(SAssignNew(OutsideButton,SButton).Text(FText::FromString(TEXT("Outside PIE"))));
                    FSlateApplication::Get().AddWindow(FocusWindow.ToSharedRef());
                    FSlateApplication::Get().SetUserFocus(0,OutsideButton,EFocusCause::SetDirectly);
                }
                else if (SequenceStep==7)
                {
                    const FString Cancelled=UTurboMCPReadToolset::ReadTurboMCPPlayInputState(TEXT("pie:0"));Record(Cancelled);
                    Test->TestTrue(TEXT("Focus loss cancels entire sequence"),Cancelled.Contains(TEXT("stopped: world, player or PIE focus unavailable")) && Cancelled.Contains(TEXT("sequence held: 0; pending: 0")));
                    Test->TestEqual(TEXT("Focus loss clears original axis"),Player->GetInputAnalogKeyState(EKeys::Gamepad_RightX),0.f);
                    FocusWindow->RequestDestroyWindow();FocusWindow.Reset();OutsideButton.Reset();
                    Record(UTurboMCPEditToolset::EditTurboMCPPlayInput(TEXT("pie:0"),TEXT("focus"),TEXT(""),TEXT(""),TEXT("")));
                    Test->TestTrue(TEXT("Late-event fixture accepted"),Sequence(TEXT("[{\"at\":0,\"key\":\"P\",\"duration\":0.05}]")).Contains(TEXT("admitted 1 events")));
                    // Deliberately block this one editor tick past the entire tap.
                    // The scheduler must refuse replay, not open Pause later.
                    FPlatformProcess::Sleep(0.1f);
                }
                else
                {
                    const FString Late=UTurboMCPReadToolset::ReadTurboMCPPlayInputState(TEXT("pie:0"));Record(Late);
                    Test->TestTrue(TEXT("Expired tap aborts without replay"),Late.Contains(TEXT("scheduled input expired before dispatch")) && Late.Contains(TEXT("sequence held: 0; pending: 0")));
                    Test->TestTrue(TEXT("Expired pause tap never opens menu"),State().Contains(TEXT("screen: Gameplay")));
                }
                ++SequenceStep;return false;
            }
            const FString Current=State();
            const FString InputState=UTurboMCPReadToolset::ReadTurboMCPPlayInputState(TEXT("pie:0"));Record(InputState);
            Test->TestTrue(TEXT("Hold automatically released"),InputState.Contains(TEXT("tool-held input: none")));
            const FString Capture=UTurboMCPLookToolset::LookTurboMCPCapture(TEXT(""),TEXT(""),TEXT("viewport"),TEXT("pie"),TEXT(""));Record(Capture);
            Test->TestTrue(TEXT("Workflow image captured"),Capture.Contains(TEXT("image:")));
            switch(Step)
            {
            case 1: Record(UTurboMCPEditToolset::EditTurboMCPPlayInput(TEXT("pie:0"),TEXT("focus"),TEXT(""),TEXT(""),TEXT("")));Hold(TEXT("Gamepad_Special_Right"));break;
            case 2: Test->TestTrue(TEXT("Disabled control did not activate"),Current.Contains(TEXT("disabled clicks: 0")));Test->TestTrue(TEXT("D-pad up restored Settings focus"),Current.Contains(TEXT("focused button: Settings")));Test->TestTrue(TEXT("CommonUI diagnostics present"),InputState.Contains(TEXT("CommonUI active widgets: 2")) && InputState.Contains(TEXT("Gamepad")));Test->TestTrue(TEXT("Pause screen active"),Current.Contains(TEXT("screen: Pause")));{
                UClass* ButtonClass=FindObject<UClass>(nullptr,TEXT("/Script/CommonUI.CommonButtonBase"));FString ButtonPath;
                for(TObjectIterator<UUserWidget> It;It;++It)if(ButtonClass && It->IsA(ButtonClass) && It->GetWorld()==GEditor->PlayWorld && (It->HasUserFocus(GEditor->PlayWorld->GetFirstPlayerController()) || It->HasUserFocusedDescendants(GEditor->PlayWorld->GetFirstPlayerController())))ButtonPath=It->GetPathName();
                const FString BeforeRead=State();
                const FString Compact=UTurboMCPReadToolset::ReadTurboMCPPlayInputState(TEXT("pie:0"),TEXT(""),0);Record(Compact);
                Test->TestTrue(TEXT("Summary-only reports no paths"),Compact.Contains(TEXT("0 shown;")));
                Test->TestTrue(TEXT("Diagnostics precede controls"),Compact.Find(TEXT("CommonUI active widgets:"))<Compact.Find(TEXT("runtime buttons:")));
                const FString Filtered=UTurboMCPReadToolset::ReadTurboMCPPlayInputState(TEXT("pie:0"),ButtonPath.ToLower(),1);Record(Filtered);
                Test->TestTrue(TEXT("Known button matches case-insensitively"),!ButtonPath.IsEmpty() && Filtered.Contains(ButtonPath) && Filtered.Contains(TEXT("1 shown;")));
                const FString Missing=UTurboMCPReadToolset::ReadTurboMCPPlayInputState(TEXT("pie:0"),TEXT("__no_such_runtime_button_879345__"),16);
                Test->TestTrue(TEXT("Unmatched filter is explicit"),Missing.Contains(TEXT("0 matching; 0 shown; 0 omitted")));
                for (int32 Limit:{-1,65}) Test->TestTrue(TEXT("Invalid limits refuse"),UTurboMCPReadToolset::ReadTurboMCPPlayInputState(TEXT("pie:0"),TEXT(""),Limit).StartsWith(TEXT("error:")));
                Test->TestEqual(TEXT("State reads do not change fixture"),State(),BeforeRead);
                const FString Help=UTurboMCPReadToolset::ReadTurboMCPToolHelp(TEXT("TurboMCP.TurboMCPReadToolset"),TEXT("ReadTurboMCPPlayInputState"));
                Test->TestTrue(TEXT("Help explains compact filter"),Help.Contains(TEXT("ButtonFilter")) && Help.Contains(TEXT("0..64")) && Help.Contains(TEXT("default 16")));
                const FString Activate=Dispatch(TEXT("EditTurboMCPMenuActivate"),FString::Printf(TEXT("{\"Target\":\"pie:0\",\"Widget\":\"%s\"}"),*ButtonPath));Record(Activate);Test->TestFalse(TEXT("Named button dispatch accepted"),Activate.StartsWith(TEXT("error:")));
            }break;
            case 3: Test->TestTrue(TEXT("Settings is modal"),Current.Contains(TEXT("modal: yes")));Test->TestTrue(TEXT("CommonButton opens Settings"),Current.Contains(TEXT("screen: Settings")));Test->TestTrue(TEXT("Audio initially selected"),Current.Contains(TEXT("tab: Audio")) && Current.Contains(TEXT("content: Audio settings content")));Hold(TEXT("Gamepad_RightShoulder"));break;
            case 4: Test->TestTrue(TEXT("Next tab selects Video"),Current.Contains(TEXT("tab: Video")) && Current.Contains(TEXT("content: Video settings content")));Hold(TEXT("Gamepad_LeftShoulder"));break;
            case 5: Test->TestTrue(TEXT("Previous tab selects Audio"),Current.Contains(TEXT("tab: Audio")) && Current.Contains(TEXT("content: Audio settings content")));Hold(TEXT("Gamepad_FaceButton_Right"));break;
            case 6: Test->TestTrue(TEXT("Modal Back preserves paused parent and disabled control"),Current.Contains(TEXT("paused: yes")) && Current.Contains(TEXT("disabled clicks: 0")));Test->TestTrue(TEXT("Back restores parent screen"),Current.Contains(TEXT("screen: Pause")));Hold(TEXT("Gamepad_FaceButton_Right"));break;
            case 7: Test->TestTrue(TEXT("Back resumes gameplay"),Current.Contains(TEXT("screen: Gameplay")) && Current.Contains(TEXT("paused: no")));Hold(TEXT("Gamepad_RightX"),TEXT("0.75"));break;
            case 8: Test->TestFalse(TEXT("Camera action evaluated"),Current.Contains(TEXT("look calls: 0")));Test->TestFalse(TEXT("Camera rotation changed"),Current.Contains(TEXT("look yaw: 0.0")));Hold(TEXT("P"));break;
            case 9: Test->TestTrue(TEXT("Editor Escape refuses without stopping PIE"),UTurboMCPEditToolset::EditTurboMCPPlayInput(TEXT("pie:0"),TEXT("hold"),TEXT("Escape"),TEXT(""),TEXT("0.15")).Contains(TEXT("editor play command")));Test->TestTrue(TEXT("Keyboard reopens Pause"),Current.Contains(TEXT("screen: Pause")));Hold(TEXT("BackSpace"));break;
            case 10: Test->TestTrue(TEXT("Keyboard Back resumes gameplay"),Current.Contains(TEXT("screen: Gameplay")) && Current.Contains(TEXT("paused: no")));Fixture->Destroy();Fixture.Reset();break;
            }
            ++Step;return false;
        }
        const FString Result=UTurboMCPReadToolset::ReadTurboMCPPlayCheck(TEXT(""),TEXT(""));
        if (!Result.Contains(TEXT("played for"))) return false;
        Record(Result);Test->TestTrue(TEXT("No runtime warnings/errors"),Result.Contains(TEXT("0 warning(s) and 0 error(s)")));return true;
    }
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTurboMCPCommonPlayTest,"TurboMCP.Workflow.CommonPlay",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTurboMCPCommonPlayTest::RunTest(const FString&)
{
    FString Start=UTurboMCPReadToolset::ReadTurboMCPPlayCheck(TEXT("55"),TEXT(""));
    if (Start.StartsWith(TEXT("error:")) || Start.Contains(TEXT("already playing"))) {AddError(Start);return false;}
    ADD_LATENT_AUTOMATION_COMMAND(FCommonPlayLoop(this));return true;
}
#endif
