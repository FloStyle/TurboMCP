// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
#include "TurboMCPPlayInput.h"
#include "Tools/TurboMCPToolsets.h"
#include "Containers/Ticker.h"
#include "Editor.h"
#include "Engine/World.h"
#include "Engine/LocalPlayer.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "GameFramework/PlayerController.h"
#include "EnhancedPlayerInput.h"
#include "GameFramework/PlayerInput.h"
#include "InputKeyEventArgs.h"
#include "Blueprint/UserWidget.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "UObject/StructOnScope.h"
#include "UObject/UnrealType.h"
#include "EnhancedInputSubsystems.h"
#include "UObject/UObjectIterator.h"
#include "InputMappingContext.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Application/SlateUser.h"
#include "Widgets/SViewport.h"
#include "HAL/PlatformTime.h"
#include "Containers/UnrealString.h"
#include "Framework/Commands/InputBindingManager.h"
#include "Framework/Commands/UICommandInfo.h"

namespace
{
struct FPlayInputTarget
{
    UWorld* World=nullptr;
    APlayerController* Controller=nullptr;
    TSharedPtr<SViewport> Viewport;
    uint32 User=0;
};
struct FHeldPlayInput
{
    FKey Key;
    float Value=0;
    uint32 User=0;
    double Deadline=0;
    TWeakObjectPtr<UWorld> World;
    TWeakObjectPtr<UPlayerInput> PlayerInput;
    TWeakPtr<SViewport> Viewport;
};
FHeldPlayInput Held;
FTSTicker::FDelegateHandle TickHandle;
FDelegateHandle EndHandle;
FString LastInput=TEXT("none");
FString LastReleaseReason=TEXT("none");

FString ResolvePlayInput(const FString& Target, FPlayInputTarget& Out)
{
    if (Target!=TEXT("pie:0")) return TEXT("error: Target must be pie:0. This version supports one local player in one in-process PIE world.");
    if (!GEditor || !FSlateApplication::IsInitialized()) return TEXT("error: A rendering editor and live PIE session are required.");
    int32 Worlds=0;
    for (const FWorldContext& Context:GEditor->GetWorldContexts())
        if (Context.WorldType==EWorldType::PIE && Context.World()) {Out.World=Context.World();++Worlds;}
    if (Worlds!=1) return TEXT("error: Exactly one live PIE world is required; no input was sent.");
    UGameInstance* Instance=Out.World->GetGameInstance();
    if (!Instance || Instance->GetLocalPlayers().Num()!=1) return TEXT("error: Exactly one local player is required; no input was sent.");
    ULocalPlayer* Player=Instance->GetLocalPlayers()[0];
    Out.Controller=Player ? Player->GetPlayerController(Out.World):nullptr;
    const TSharedPtr<FSlateUser> User=Player ? Player->GetSlateUser():nullptr;
    UGameViewportClient* Client=Out.World->GetGameViewport();
    Out.Viewport=Client ? Client->GetGameViewportWidget():nullptr;
    if (!Out.Controller || !User.IsValid() || !Out.Viewport.IsValid()) return TEXT("error: Player, Slate user or game viewport is not ready; retry after normal editor ticks.");
    Out.User=User->GetUserIndex();
    return FString();
}
bool HasPlayFocus(const FPlayInputTarget& Target)
{
    for (TSharedPtr<SWidget> Widget=FSlateApplication::Get().GetUserFocusedWidget(Target.User);Widget.IsValid();Widget=Widget->GetParentWidget())
        if (Widget.Get()==Target.Viewport.Get()) return true;
    return false;
}
bool SendPlayEvent(const FHeldPlayInput& Input, bool Release)
{
    if (!FSlateApplication::IsInitialized()) return false;
    auto& App=FSlateApplication::Get();
    if (Input.Key.IsAnalog()) return App.ProcessAnalogInputEvent(FAnalogInputEvent(Input.Key,FModifierKeysState(),Input.User,false,0,0,Release?0.f:Input.Value));
    const FKeyEvent Event(Input.Key,FModifierKeysState(),Input.User,false,0,0);
    return Release ? App.ProcessKeyUpEvent(Event):App.ProcessKeyDownEvent(Event);
}
// Call only a named native getter on its loaded base class. Never resolve a
// Blueprint override or load optional plugins/assets merely to inspect state.
FString NativeGetter(UObject* Object,UClass* Base,const TCHAR* Name)
{
    UFunction* Function=Base?Base->FindFunctionByName(Name):nullptr;
    if (!Object || !Function || !Function->HasAnyFunctionFlags(FUNC_Native) || Function->NumParms!=1) return TEXT("unavailable");
    FProperty* Result=Function->GetReturnProperty();
    if (!Result) return TEXT("unavailable");
    FStructOnScope Parameters(Function);
    Object->ProcessEvent(Function,Parameters.GetStructMemory());
    FString Value;
    Result->ExportTextItem_Direct(Value,Result->ContainerPtrToValuePtr<void>(Parameters.GetStructMemory()),nullptr,Object,PPF_None);
    return Value;
}
FString CommonInputState(const FPlayInputTarget& Target)
{
    UClass* InputClass=FindObject<UClass>(nullptr,TEXT("/Script/CommonInput.CommonInputSubsystem"));
    ULocalPlayerSubsystem* Input=InputClass && InputClass->IsChildOf(ULocalPlayerSubsystem::StaticClass())
        ? Target.Controller->GetLocalPlayer()->GetSubsystemBase(InputClass):nullptr;
    FString Out=TEXT("\nCommonInput device type: ")+NativeGetter(Input,InputClass,TEXT("GetCurrentInputType"));
    Out+=TEXT("\nCommonInput configured gamepad name: ")+NativeGetter(Input,InputClass,TEXT("GetCurrentGamepadName"));
    UClass* ActiveClass=FindObject<UClass>(nullptr,TEXT("/Script/CommonUI.CommonActivatableWidget"));
    if (!ActiveClass) return Out+TEXT("\nCommonUI active widgets: unavailable (CommonUI is not loaded).");
    TArray<FString> Active;
    for (TObjectIterator<UUserWidget> It;It;++It)
    {
        if (It->GetWorld()!=Target.World || It->GetOwningPlayer()!=Target.Controller || !It->IsA(ActiveClass)) continue;
        if (NativeGetter(*It,ActiveClass,TEXT("IsActivated"))!=TEXT("True")) continue;
        Active.Add(It->GetPathName()+((It->HasUserFocus(Target.Controller)||It->HasUserFocusedDescendants(Target.Controller))?TEXT("; contains user focus"):TEXT("; no user focus")));
    }
    Active.Sort();
    Out+=FString::Printf(TEXT("\nCommonUI active widgets: %d (unordered; not the activation stack or routing owner)"),Active.Num());
    for (int32 Index=0;Index<FMath::Min(Active.Num(),16);++Index) Out+=TEXT("\n  ")+Active[Index];
    if (Active.Num()>16) Out+=TEXT("\nAdditional active widgets omitted after 16 entries.");
    return Out;
}
bool HeldTargetHasFocus()
{
    if (!FSlateApplication::IsInitialized() || !FSlateApplication::Get().IsActive()) return false;
    const TSharedPtr<SViewport> Viewport=Held.Viewport.Pin();
    if (!Viewport.IsValid()) return false;
    for (TSharedPtr<SWidget> Widget=FSlateApplication::Get().GetUserFocusedWidget(Held.User);Widget.IsValid();Widget=Widget->GetParentWidget())
        if (Widget==Viewport) return true;
    return false;
}
void ReleasePlayInput(const TCHAR* Reason)
{
    if (!Held.Key.IsValid()) return;
    // Analog InputKey adds samples within a frame. Sending zero alone after a
    // press in that same frame leaves the earlier sample latched. Reset only
    // the owned component, including its paired axis, before the release event.
    if (Held.Key.IsAnalog())
    {
        if (UPlayerInput* OriginalInput=Held.PlayerInput.Get())
        {
            if (FKeyState* State=OriginalInput->GetKeyState(Held.Key))
                State->RawValueAccumulator.X=0.f;
            const EPairedAxis Component=Held.Key.GetPairedAxis();
            const int32 Index=Component==EPairedAxis::X?0:Component==EPairedAxis::Y?1:Component==EPairedAxis::Z?2:INDEX_NONE;
            if (Index!=INDEX_NONE)
                if (FKeyState* Pair=OriginalInput->GetKeyState(Held.Key.GetPairedAxisKey()))
                    Pair->RawValueAccumulator[Index]=0.f;
        }
    }
    // Never route cleanup into an editor control after focus leaves this viewport.
    if (HeldTargetHasFocus()) SendPlayEvent(Held,true);
    // A UI handler can consume Slate's release. Clear only our original player's
    // owned key as well; never flush unrelated input or inject an action/press.
    if (UPlayerInput* OriginalInput=Held.PlayerInput.Get())
        OriginalInput->InputKey(FInputKeyEventArgs::CreateSimulated(Held.Key,Held.Key.IsAnalog()?IE_Axis:IE_Released,0.f));
    LastReleaseReason=Reason;
    Held=FHeldPlayInput();
}
void CancelPlayHold(const TCHAR* Reason=TEXT("explicit release"))
{
    if (TickHandle.IsValid()) {FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);TickHandle.Reset();}
    ReleasePlayInput(Reason);
}
bool TickPlayHold(float)
{
    const TCHAR* Reason=nullptr;
    if (!Held.World.IsValid()) Reason=TEXT("PIE world ended");
    else if (!FSlateApplication::IsInitialized() || !FSlateApplication::Get().IsActive()) Reason=TEXT("application deactivated");
    else if (!HeldTargetHasFocus()) Reason=TEXT("focus left the PIE viewport");
    else if (FPlatformTime::Seconds()>=Held.Deadline) Reason=TEXT("hold duration elapsed");
    if (Reason) {ReleasePlayInput(Reason);TickHandle.Reset();return false;}
    // Wall-clock checks still run while the gameplay world is paused.
    if (Held.Key.IsAnalog()) SendPlayEvent(Held,false);
    return true;
}
}
void TurboMCPPlayInput::Shutdown()
{
    CancelPlayHold(TEXT("plugin shutdown"));
    if (EndHandle.IsValid()) {FEditorDelegates::EndPIE.Remove(EndHandle);EndHandle.Reset();}
}

FString UTurboMCPEditToolset::EditTurboMCPPlayInput(const FString& Target,const FString& Action,const FString& Key,const FString& Value,const FString& Seconds)
{
    FPlayInputTarget Resolved;
    const FString Error=ResolvePlayInput(Target,Resolved);
    if (!Error.IsEmpty()) return Error;
    if (Action==TEXT("release"))
    {
        if (!Key.IsEmpty() || !Value.IsEmpty() || !Seconds.IsEmpty()) return TEXT("error: release requires empty Key, Value and Seconds.");
        const bool HadInput=Held.Key.IsValid(); CancelPlayHold();
        return HadInput?TEXT("play input: released the tool's held input. Inspect the resulting game/UI state."):TEXT("play input: no tool-held input; nothing changed.");
    }
    if (Action==TEXT("focus"))
    {
        if (!Key.IsEmpty() || !Value.IsEmpty() || !Seconds.IsEmpty()) return TEXT("error: focus requires empty Key, Value and Seconds.");
        if (Held.Key.IsValid()) return TEXT("error: Release the tool-held input before changing focus.");
        FSlateApplication::Get().SetUserFocus(Resolved.User,Resolved.Viewport,EFocusCause::SetDirectly);
        const bool Focused=HasPlayFocus(Resolved);
        return Focused?TEXT("play input: focused the game viewport. This can leave a focused menu control; use Slate refs to focus/interact with menus. No game key was sent."):TEXT("error: The game viewport did not accept focus.");
    }
    if (Action!=TEXT("hold")) return TEXT("error: Action must be focus, hold or release. A short bounded hold is a tap; no indefinite key-down is supported.");
    const FKey InputKey(*Key);
    if (!InputKey.IsValid() || InputKey.IsMouseButton() || (InputKey.IsAnalog() && (!InputKey.IsGamepadKey() || !InputKey.IsAxis1D())))
        return TEXT("error: Key must be a keyboard/gamepad button or a one-dimensional gamepad axis. Use native Slate tools for mouse clicks and text entry.");
    // PIE handles editor shortcuts before normal game/UI routing. Never report
    // a gameplay key as delivered when it would stop or control the editor session.
    if (const auto Command=FInputBindingManager::Get().GetCommandInfoFromInputChord(TEXT("PlayWorld"),FInputChord(InputKey),false))
        return FString::Printf(TEXT("error: Key %s is bound to the editor play command %s. No input was sent. Use another gameplay key or change that editor shortcut explicitly; this tool does not change editor bindings."),*Key,*Command->GetCommandName().ToString());
    double Duration=0; float Amount=1;
    if (!Seconds.IsNumeric() || !LexTryParseString(Duration,*Seconds) || !FMath::IsFinite(Duration) || Duration<0.05 || Duration>5)
        return TEXT("error: Seconds must be a finite duration from 0.05 to 5. Use 0.1 for a short tap.");
    if (InputKey.IsAnalog())
    {
        if (!Value.IsNumeric() || !LexTryParseString(Amount,*Value) || !FMath::IsFinite(Amount) || Amount < -1 || Amount > 1)
            return TEXT("error: Analog Value must be finite and between -1 and 1; release returns it to zero.");
    }
    else if (!Value.IsEmpty()) return TEXT("error: Digital keys require empty Value.");
    if (Held.Key.IsValid()) return TEXT("error: A tool-held input is still active. Wait for its release or call release before sending another.");
    if (!FSlateApplication::Get().IsActive()) return TEXT("error: The editor application is inactive. No input was sent.");
    // Analog activity is its value; the digital down flag can remain set after
    // same-frame axis press/release events even when the axis has returned to zero.
    if ((InputKey.IsDigital() && Resolved.Controller->IsInputKeyDown(InputKey)) || !FMath::IsNearlyZero(Resolved.Controller->GetInputAnalogKeyState(InputKey))) return TEXT("error: This key already has player input. Release it before starting a synthetic hold; the tool will not take ownership of existing input.");
    if (!HasPlayFocus(Resolved)) return TEXT("error: This player's focus is outside the PIE viewport. Inspect the UI; explicitly use focus for gameplay or a Slate control ref for menu input. No key was sent.");
    if (!EndHandle.IsValid()) EndHandle=FEditorDelegates::EndPIE.AddLambda([](bool){CancelPlayHold(TEXT("PIE ended"));});
    Held.Key=InputKey;Held.Value=Amount;Held.User=Resolved.User;Held.World=Resolved.World;Held.PlayerInput=Resolved.Controller->PlayerInput;Held.Viewport=Resolved.Viewport;Held.Deadline=FPlatformTime::Seconds()+Duration;
    const bool Handled=SendPlayEvent(Held,false);
    LastInput=Key;
    TickHandle=FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateStatic(&TickPlayHold));
    return FString::Printf(TEXT("play input: routed %s through Slate for player 0, Slate user %u\nvalue: %.3f; automatic release after %.3f wall-clock seconds\nSlate handled initial event: %s\nThis is synthetic key/axis input, not Enhanced Input action injection or physical-device emulation. Normal frames are required. Delivery does not prove the action fired; inspect PlayInputState, the menu and the next capture. No asset edit, save or undo was performed."),*Key,Resolved.User,Amount,Duration,Handled?TEXT("yes"):TEXT("no"));
}

FString UTurboMCPReadToolset::ReadTurboMCPPlayInputState(const FString& Target)
{
    FPlayInputTarget Resolved;
    const FString Error=ResolvePlayInput(Target,Resolved);
    if (!Error.IsEmpty()) return Error;
    const TSharedPtr<SWidget> Focus=FSlateApplication::Get().GetUserFocusedWidget(Resolved.User);
    FString Out=FString::Printf(TEXT("play input state\nworld: %s\nplayer: 0; Slate user: %u\ngame paused: %s\nfocus widget type: %s\nfocus inside PIE viewport: %s\ntool-held input: %s\nlast synthetic key: %s"),*Resolved.World->GetPathName(),Resolved.User,Resolved.World->IsPaused()?TEXT("yes"):TEXT("no"),Focus.IsValid()?*Focus->GetTypeAsString():TEXT("none"),HasPlayFocus(Resolved)?TEXT("yes"):TEXT("no"),Held.Key.IsValid()?*Held.Key.GetFName().ToString():TEXT("none"),*LastInput);
    Out+=TEXT("\nlast release reason: ")+LastReleaseReason;
    Out+=FSlateApplication::Get().IsActive()?TEXT("\napplication active: yes"):TEXT("\napplication active: no");
    if (Cast<UEnhancedPlayerInput>(Resolved.Controller->PlayerInput))
    {
        TArray<FString> Contexts;
        // Query the public subsystem API; applied context storage is engine-private.
        const auto* Subsystem=ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(Resolved.Controller->GetLocalPlayer());
        if (Subsystem)
        {
            for (TObjectIterator<UInputMappingContext> It;It;++It)
            {
                int32 Priority=0;
                if (Subsystem->HasMappingContext(*It,Priority))
                    Contexts.Add(FString::Printf(TEXT("  %s; priority %d"),*It->GetPathName(),Priority));
            }
        }
        else Out+=TEXT("\nEnhanced Input subsystem unavailable; mapping contexts could not be inspected.");
        if (Subsystem) {
        Contexts.Sort();Out+=FString::Printf(TEXT("\nEnhanced Input mapping contexts: %d"),Contexts.Num());
        for (int32 Index=0;Index<FMath::Min(Contexts.Num(),64);++Index) Out+=TEXT("\n")+Contexts[Index];
        if (Contexts.Num()>64) Out+=TEXT("\nAdditional contexts omitted after 64 entries.");
        }
    }
    else Out+=TEXT("\nEnhanced Input: this controller does not use UEnhancedPlayerInput.");
    Out+=CommonInputState(Resolved);
    Out+=TEXT("\nCommonUI routing policy: not inspected. Use Slate Snapshot for visible controls. CommonInput reports its current logical device type, not proof of a physical device. Context presence does not prove an action triggered.");
    return Out;
}
