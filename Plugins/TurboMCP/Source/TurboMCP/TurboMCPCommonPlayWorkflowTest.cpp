// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
#include "Tools/TurboMCPToolsets.h"
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Editor.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformTime.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWindow.h"
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
    int32 DeactivationStep=0;
    bool SimulatedInactive=false;
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
    void Hold(const TCHAR* Key,const TCHAR* Value=TEXT(""),const TCHAR* Duration=TEXT("0.15"))
    {
        FString Out=UTurboMCPEditToolset::EditTurboMCPPlayInput(TEXT("pie:0"),TEXT("hold"),Key,Value,Duration);
        Record(Out);Test->TestFalse(TEXT("Input accepted"),Out.StartsWith(TEXT("error:")));
    }
public:
    explicit FCommonPlayLoop(FAutomationTestBase* InTest):Test(InTest) {}
    ~FCommonPlayLoop() {if (SimulatedInactive && FSlateApplication::IsInitialized()) FSlateApplication::Get().OnApplicationActivationChanged(true);if (FocusWindow.IsValid()) FocusWindow->RequestDestroyWindow();}
    virtual bool Update() override
    {
        double Now=FPlatformTime::Seconds();if (Now<Next) return false;Next=Now+1;
        if (Now>Deadline) {Test->AddError(TEXT("CommonUI workflow timed out."));if (Fixture.IsValid()) Fixture->Destroy();return true;}
        if (Step==0)
        {
            if (!GEditor || !GEditor->PlayWorld) return false;
            UClass* Class=LoadClass<AActor>(nullptr,TEXT("/Script/TurboMCPTest.TurboMCPCommonPlayFixture"));
            if (!Test->TestNotNull(TEXT("Project CommonUI fixture class"),Class)) return true;
            Fixture=GEditor->PlayWorld->SpawnActor<AActor>(Class);
            if (!Test->TestTrue(TEXT("Fixture spawned"),Fixture.IsValid())) return true;
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
                Hold(FocusStep==0?TEXT("Gamepad_DPad_Down"):TEXT("Gamepad_DPad_Up"));
                ++FocusStep;return false;
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
                    const FString Inactive=UTurboMCPReadToolset::ReadTurboMCPPlayInputState(TEXT("pie:0"));Record(Inactive);
                    Test->TestTrue(TEXT("Deactivation cancelled held input"),Inactive.Contains(TEXT("tool-held input: none")) && Inactive.Contains(TEXT("last release reason: application deactivated")));
                    Test->TestTrue(TEXT("Inactive input refused"),UTurboMCPEditToolset::EditTurboMCPPlayInput(TEXT("pie:0"),TEXT("hold"),TEXT("P"),TEXT(""),TEXT("0.15")).Contains(TEXT("application is inactive")));
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
            const FString Current=State();
            const FString InputState=UTurboMCPReadToolset::ReadTurboMCPPlayInputState(TEXT("pie:0"));Record(InputState);
            Test->TestTrue(TEXT("Hold automatically released"),InputState.Contains(TEXT("tool-held input: none")));
            const FString Capture=UTurboMCPLookToolset::LookTurboMCPCapture(TEXT(""),TEXT(""),TEXT("viewport"),TEXT("pie"),TEXT(""));Record(Capture);
            Test->TestTrue(TEXT("Workflow image captured"),Capture.Contains(TEXT("image:")));
            switch(Step)
            {
            case 1: Record(UTurboMCPEditToolset::EditTurboMCPPlayInput(TEXT("pie:0"),TEXT("focus"),TEXT(""),TEXT(""),TEXT("")));Hold(TEXT("Gamepad_Special_Right"));break;
            case 2: Test->TestTrue(TEXT("Disabled control did not activate"),Current.Contains(TEXT("disabled clicks: 0")));Test->TestTrue(TEXT("D-pad up restored Settings focus"),Current.Contains(TEXT("focused button: Settings")));Test->TestTrue(TEXT("CommonUI diagnostics present"),InputState.Contains(TEXT("CommonUI active widgets: 2")) && InputState.Contains(TEXT("Gamepad")));Test->TestTrue(TEXT("Pause screen active"),Current.Contains(TEXT("screen: Pause")));Hold(TEXT("Gamepad_FaceButton_Bottom"));break;
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
    FString Start=UTurboMCPReadToolset::ReadTurboMCPPlayCheck(TEXT("40"),TEXT(""));
    if (Start.StartsWith(TEXT("error:")) || Start.Contains(TEXT("already playing"))) {AddError(Start);return false;}
    ADD_LATENT_AUTOMATION_COMMAND(FCommonPlayLoop(this));return true;
}
#endif
