// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
#include "TurboMCPPlayInput.h"
#include "TurboMCPFocusGuard.h"
#include "Tools/TurboMCPToolsets.h"
#include "Containers/Ticker.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Editor.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "Engine/World.h"
#include "Engine/LocalPlayer.h"
#include "GenericPlatform/GenericPlatformInputDeviceMapper.h"
#include "Misc/Guid.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "GameFramework/PlayerController.h"
#include "EnhancedPlayerInput.h"
#include "GameFramework/PlayerInput.h"
#include "InputKeyEventArgs.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "UObject/StructOnScope.h"
#include "UObject/UnrealType.h"
#include "EnhancedInputSubsystems.h"
#include "UObject/UObjectIterator.h"
#include "InputMappingContext.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Application/SlateUser.h"
#include "Layout/WidgetPath.h"
#include "Widgets/SViewport.h"
#include "Widgets/SWindow.h"
#include "GenericPlatform/GenericWindow.h"
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
    FInputDeviceId Device=INPUTDEVICEID_NONE;
    int32 PlayerIndex=0;
};
struct FHeldPlayInput
{
    FKey Key;
    float Value=0;
    uint32 User=0;
    FInputDeviceId Device=INPUTDEVICEID_NONE;
    double Deadline=0;
    TWeakObjectPtr<UWorld> World;
    TWeakObjectPtr<UPlayerInput> PlayerInput;
    TWeakPtr<SViewport> Viewport;
};
FHeldPlayInput Held;
struct FTimedPlayEvent
{
    FHeldPlayInput Input;
    double At=0;
    double Duration=0;
};
TArray<FTimedPlayEvent> PendingInputs;
TArray<FHeldPlayInput> SequenceInputs;
bool SequenceActive=false;
double SequenceStart=0;
uint64 SequenceGeneration=0;
FString SequenceStatus=TEXT("none");
FTSTicker::FDelegateHandle SequenceTick;
FTSTicker::FDelegateHandle TickHandle;
FDelegateHandle EndHandle;
FString LastInput=TEXT("none");
FString LastReleaseReason=TEXT("none");

// Opaque tokens bind object identity, so a restarted PIE instance cannot reuse
// an old target accidentally. Discovery does not start PIE or load a world.
struct FDiscoveredPlayTarget
{
    TWeakObjectPtr<UWorld> World;
    TWeakObjectPtr<ULocalPlayer> Player;
    FString Token;
};
TArray<FDiscoveredPlayTarget> DiscoveredTargets;
FString DiscoverPlayTargets()
{
    if (!GEditor) return TEXT("error: An editor is required to list local PIE clients.");
    DiscoveredTargets.RemoveAll([](const FDiscoveredPlayTarget& Entry)
        { return !Entry.World.IsValid() || !Entry.Player.IsValid(); });
    FString Result=TEXT("local PIE targets (session-bound; external processes are not listed)");
    int32 Count=0;
    for (const FWorldContext& Context:GEditor->GetWorldContexts())
    {
        UWorld* World=Context.World();
        if (Context.WorldType!=EWorldType::PIE || !World) continue;
        UGameInstance* Instance=World->GetGameInstance();
        if (!Instance) continue;
        for (int32 Index=0;Index<Instance->GetLocalPlayers().Num();++Index)
        {
            ULocalPlayer* Player=Instance->GetLocalPlayers()[Index];
            if (!Player) continue;
            FDiscoveredPlayTarget* Entry=DiscoveredTargets.FindByPredicate([&](const FDiscoveredPlayTarget& Item)
                { return Item.World.Get()==World && Item.Player.Get()==Player; });
            if (!Entry)
            {
                FDiscoveredPlayTarget NewEntry;
                NewEntry.World=World; NewEntry.Player=Player;
                NewEntry.Token=TEXT("pie:")+FGuid::NewGuid().ToString(EGuidFormats::Digits);
                Entry=&DiscoveredTargets.Add_GetRef(MoveTemp(NewEntry));
            }
            UGameViewportClient* Client=World->GetGameViewport();
            Result+=FString::Printf(TEXT("\ntarget: %s; world: %s; PIE instance: %d; player: %d; viewport: %s"),
                *Entry->Token,*World->GetPathName(),Context.PIEInstance,Index,
                Client && Client->GetGameViewportWidget().IsValid()?TEXT("ready"):TEXT("not ready"));
            ++Count;
        }
    }
    return Result+FString::Printf(TEXT("\ntotal: %d. Copy an exact target into input/state calls. Tokens expire with their world/player."),Count);
}
FString ResolvePlayInput(const FString& Target, FPlayInputTarget& Out)
{
    if (!GEditor || !FSlateApplication::IsInitialized()) return TEXT("error: A rendering editor and live PIE session are required.");
    ULocalPlayer* Player=nullptr;
    if (Target==TEXT("pie:0"))
    {
        int32 Worlds=0;
        for (const FWorldContext& Context:GEditor->GetWorldContexts())
            if (Context.WorldType==EWorldType::PIE && Context.World()) {Out.World=Context.World();++Worlds;}
        if (Worlds!=1) return TEXT("error: pie:0 requires exactly one live PIE world; no input was sent. ReadTurboMCPPlayInputState with Target=list lists explicit clients.");
        UGameInstance* Instance=Out.World->GetGameInstance();
        if (!Instance || Instance->GetLocalPlayers().Num()!=1) return TEXT("error: pie:0 requires exactly one local player; no input was sent. Use Target=list to discover explicit clients.");
        Player=Instance->GetLocalPlayers()[0];
    }
    else
    {
        const FDiscoveredPlayTarget* Entry=DiscoveredTargets.FindByPredicate([&](const FDiscoveredPlayTarget& Item){return Item.Token==Target;});
        if (!Entry || !Entry->World.IsValid() || !Entry->Player.IsValid())
            return TEXT("error: Unknown or expired PIE target. Use ReadTurboMCPPlayInputState with Target=list; no input was sent.");
        Out.World=Entry->World.Get(); Player=Entry->Player.Get();
        bool Live=false;
        for (const FWorldContext& Context:GEditor->GetWorldContexts())
            if (Context.WorldType==EWorldType::PIE && Context.World()==Out.World) Live=true;
        UGameInstance* Instance=Out.World->GetGameInstance();
        Out.PlayerIndex=Instance?Instance->GetLocalPlayers().IndexOfByKey(Player):INDEX_NONE;
        if (!Live || Out.PlayerIndex==INDEX_NONE)
            return TEXT("error: PIE target is no longer a live local client; no input was sent. Discover targets again.");
    }
    Out.Controller=Player ? Player->GetPlayerController(Out.World):nullptr;
    const TSharedPtr<FSlateUser> User=Player ? Player->GetSlateUser():nullptr;
    UGameViewportClient* Client=Out.World->GetGameViewport();
    Out.Viewport=Client ? Client->GetGameViewportWidget():nullptr;
    if (!Out.Controller || !User.IsValid() || !Out.Viewport.IsValid()) return TEXT("error: Player, Slate user or game viewport is not ready; retry after normal editor ticks.");
    Out.User=User->GetUserIndex();
    Out.Device=IPlatformInputDeviceMapper::Get().GetPrimaryInputDeviceForUser(Player->GetPlatformUserId());
    return FString();
}

bool HasPlayFocus(const FPlayInputTarget& Target)
{
    for (TSharedPtr<SWidget> Widget=FSlateApplication::Get().GetUserFocusedWidget(Target.User);Widget.IsValid();Widget=Widget->GetParentWidget())
        if (Widget.Get()==Target.Viewport.Get()) return true;
    return false;
}
// Stock PIE may forward gamepad input to a different window after Slate focus
// routing. Refuse that configuration rather than changing the user's preferences.
bool WouldForwardPlayInput(UWorld* World, const FKey& Key)
{
    if (!World || !Key.IsGamepadKey() || !GEngine) return false;
    const ULevelEditorPlaySettings* Settings=GetDefault<ULevelEditorPlaySettings>();
    bool Forward=false, SingleProcess=false;
    Settings->GetRouteGamepadToSecondWindow(Forward);
    Settings->GetRunUnderOneProcess(SingleProcess);
    UGameViewportClient* Client=World->GetGameViewport();
    return Forward && SingleProcess && Client && GEngine->GetNextPIEViewport(Client)!=nullptr;
}
FString PlayRoutingRefusal(const FPlayInputTarget& Target, const FKey& Key)
{
    if (!Target.Device.IsValid())
        return TEXT("error: The selected player has no mapped input device. No input was sent.");
    if (WouldForwardPlayInput(Target.World,Key))
        return TEXT("error: Unreal's Route Gamepad to Second Window setting would redirect this input to another client. Disable that setting explicitly before targeting this player. No settings were changed and no input was sent.");
    if (GEngine->GetLocalPlayerFromInputDevice(Target.World->GetGameViewport(),Target.Device)!=Target.Controller->GetLocalPlayer())
        return TEXT("error: The mapped input device does not resolve to the selected local player. No input was sent.");
    return FString();
}
bool SendPlayEvent(const FHeldPlayInput& Input, bool Release)
{
    if (!FSlateApplication::IsInitialized() || WouldForwardPlayInput(Input.World.Get(),Input.Key)) return false;
    auto& App=FSlateApplication::Get();
    if (Input.Key.IsAnalog()) return App.ProcessAnalogInputEvent(FAnalogInputEvent(Input.Key,FModifierKeysState(),Input.Device,false,0,0,Release?0.f:Input.Value,static_cast<int32>(Input.User)));
    const FKeyEvent Event(Input.Key,FModifierKeysState(),Input.Device,false,0,0,static_cast<int32>(Input.User));
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
bool InputTargetHasFocus(const FHeldPlayInput& Input)
{
    if (!FSlateApplication::IsInitialized() || !FSlateApplication::Get().IsActive() || WouldForwardPlayInput(Input.World.Get(),Input.Key)) return false;
    const TSharedPtr<SViewport> Viewport=Input.Viewport.Pin();
    if (!Viewport.IsValid()) return false;
    for (TSharedPtr<SWidget> Widget=FSlateApplication::Get().GetUserFocusedWidget(Input.User);Widget.IsValid();Widget=Widget->GetParentWidget())
        if (Widget==Viewport) return true;
    return false;
}
void ReleaseOwnedInput(FHeldPlayInput Input)
{
    if (!Input.Key.IsValid()) return;
    // Analog InputKey adds samples within a frame. Sending zero alone after a
    // press in that same frame leaves the earlier sample latched. Reset only
    // the owned component, including its paired axis, before the release event.
    if (Input.Key.IsAnalog())
    {
        if (UPlayerInput* OriginalInput=Input.PlayerInput.Get())
        {
            if (FKeyState* State=OriginalInput->GetKeyState(Input.Key))
            {
                State->RawValueAccumulator.X=0.f;
                // Public readback uses the last processed Value. Clear the owned
                // component immediately; queued zero alone waits for another frame.
                State->RawValue.X=0.f;
                State->Value.X=0.f;
            }
            const EPairedAxis Component=Input.Key.GetPairedAxis();
            const int32 Index=Component==EPairedAxis::X?0:Component==EPairedAxis::Y?1:Component==EPairedAxis::Z?2:INDEX_NONE;
            if (Index!=INDEX_NONE)
                if (FKeyState* Pair=OriginalInput->GetKeyState(Input.Key.GetPairedAxisKey()))
                {
                    Pair->RawValueAccumulator[Index]=0.f;
                    Pair->RawValue[Index]=0.f;
                    Pair->Value[Index]=0.f;
                }
        }
    }
    // Never route cleanup into an editor control after focus leaves this viewport.
    if (InputTargetHasFocus(Input)) SendPlayEvent(Input,true);
    // A UI handler can consume Slate's release. Clear only our original player's
    // owned key as well; never flush unrelated input or inject an action/press.
    if (UPlayerInput* OriginalInput=Input.PlayerInput.Get())
        OriginalInput->InputKey(FInputKeyEventArgs::CreateSimulated(Input.Key,Input.Key.IsAnalog()?IE_Axis:IE_Released,0.f));
}
void ReleasePlayInput(const TCHAR* Reason)
{
    const FHeldPlayInput Input=Held;
    Held=FHeldPlayInput();
    LastReleaseReason=Reason;
    ReleaseOwnedInput(Input);
}
void CancelSequence(const TCHAR* Reason)
{
    ++SequenceGeneration;
    if (SequenceTick.IsValid()) { FTSTicker::GetCoreTicker().RemoveTicker(SequenceTick); SequenceTick.Reset(); }
    if (!SequenceActive) return;
    SequenceActive=false;
    PendingInputs.Reset();
    // Detach ownership before routing callbacks, which may end PIE or reenter cleanup.
    TArray<FHeldPlayInput> Released=MoveTemp(SequenceInputs);
    SequenceInputs.Reset();
    SequenceStatus=FString(TEXT("stopped: "))+Reason;
    LastReleaseReason=Reason;
    for (const FHeldPlayInput& Input:Released) ReleaseOwnedInput(Input);
}
void CancelPlayHold(const TCHAR* Reason=TEXT("explicit release"))
{
    if (TickHandle.IsValid()) {FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);TickHandle.Reset();}
    ReleasePlayInput(Reason);
    CancelSequence(Reason);
}
bool TickPlayHold(float)
{
    const TCHAR* Reason=nullptr;
    if (!Held.World.IsValid()) Reason=TEXT("PIE world ended");
    else if (!FSlateApplication::IsInitialized() || !FSlateApplication::Get().IsActive()) Reason=TEXT("application deactivated");
    else if (!InputTargetHasFocus(Held)) Reason=TEXT("focus left the PIE viewport");
    else if (FPlatformTime::Seconds()>=Held.Deadline) Reason=TEXT("hold duration elapsed");
    if (Reason) {ReleasePlayInput(Reason);TickHandle.Reset();return false;}
    // Wall-clock checks still run while the gameplay world is paused.
    if (Held.Key.IsAnalog()) SendPlayEvent(Held,false);
    return true;
}
// All scheduling runs on editor ticks. Never replay expired taps after a stall.
bool TickSequence(float)
{
    if (!SequenceActive) return false;
    const uint64 Generation=SequenceGeneration;
    const double Now=FPlatformTime::Seconds();
    const FHeldPlayInput Target=SequenceInputs.Num()?SequenceInputs[0]:PendingInputs[0].Input;
    if (!Target.World.IsValid() || !Target.PlayerInput.IsValid() || !InputTargetHasFocus(Target))
    { CancelSequence(TEXT("world, player or PIE focus unavailable")); return false; }
    for (int32 Index=SequenceInputs.Num()-1;Index>=0;--Index)
    {
        const FHeldPlayInput Input=SequenceInputs[Index];
        if (Now>=Input.Deadline)
        {
            SequenceInputs.RemoveAt(Index);
            ReleaseOwnedInput(Input);
            if (Generation!=SequenceGeneration) return false;
        }
    }
    TArray<FKey> StartedThisTick;
    while (PendingInputs.Num() && Now>=SequenceStart+PendingInputs[0].At)
    {
        const FTimedPlayEvent Event=PendingInputs[0];
        if (!InputTargetHasFocus(Event.Input))
        { CancelSequence(TEXT("focus left PIE during input dispatch")); return false; }
        if (FPlatformTime::Seconds()>=SequenceStart+Event.At+Event.Duration)
        { CancelSequence(TEXT("scheduled input expired before dispatch; remaining events cancelled")); return false; }
        UPlayerInput* Player=Event.Input.PlayerInput.Get();
        const FKeyState* State=Player?Player->GetKeyState(Event.Input.Key):nullptr;
        if (!Player || (State && ((Event.Input.Key.IsDigital() && State->bDown) || !FMath::IsNearlyZero(State->RawValue.X))))
        { CancelSequence(TEXT("scheduled key has existing player input; remaining events cancelled")); return false; }
        PendingInputs.RemoveAt(0);
        FHeldPlayInput Input=Event.Input;
        Input.Deadline=SequenceStart+Event.At+Event.Duration;
        SequenceInputs.Add(Input);
        StartedThisTick.Add(Input.Key);
        LastInput=Input.Key.GetFName().ToString();
        SendPlayEvent(Input,false);
        if (Generation!=SequenceGeneration) return false;
        if (!InputTargetHasFocus(Input))
        { CancelSequence(TEXT("focus left PIE during input dispatch")); return false; }
    }
    // Copy inputs before callbacks: project routing may terminate the sequence.
    const TArray<FHeldPlayInput> Inputs=SequenceInputs;
    for (const FHeldPlayInput& Input:Inputs)
    {
        if (Input.Key.IsAnalog() && !StartedThisTick.Contains(Input.Key)) SendPlayEvent(Input,false);
        if (Generation!=SequenceGeneration) return false;
    }
    if (PendingInputs.IsEmpty() && SequenceInputs.IsEmpty())
    {
        SequenceActive=false;
        SequenceStatus=TEXT("completed: all scheduled inputs released; gameplay outcome not verified");
        SequenceTick.Reset();
        return false;
    }
    return true;
}
FString StartSequence(const FPlayInputTarget& Target,const FString& Json)
{
    if (Held.Key.IsValid() || SequenceActive) return TEXT("error: Release the active tool input before starting a sequence. No sequence was started.");
    TArray<TSharedPtr<FJsonValue>> Values;
    if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json),Values) || Values.IsEmpty() || Values.Num()>32)
        return TEXT("error: Key must contain a JSON array of 1..32 input events. No input was sent.");
    TArray<FTimedPlayEvent> Events;
    double PreviousAt=-1;
    for (const TSharedPtr<FJsonValue>& Value:Values)
    {
        if (!Value.IsValid() || Value->Type!=EJson::Object) return TEXT("error: Each sequence event must be an object. No input was sent.");
        const TSharedPtr<FJsonObject> Object=Value->AsObject();
        for (const auto& Field:Object->Values)
            if (Field.Key!=TEXT("at") && Field.Key!=TEXT("key") && Field.Key!=TEXT("value") && Field.Key!=TEXT("duration"))
                return TEXT("error: Sequence event fields are at, key, duration and analog-only value. No input was sent.");
        FTimedPlayEvent Event;
        FString Key;
        if (!Object->TryGetNumberField(TEXT("at"),Event.At) || !Object->TryGetNumberField(TEXT("duration"),Event.Duration) || !Object->TryGetStringField(TEXT("key"),Key)
            || !FMath::IsFinite(Event.At) || !FMath::IsFinite(Event.Duration) || Event.At<0 || Event.At<PreviousAt || Event.Duration<0.05 || Event.Duration>5 || Event.At+Event.Duration>5)
            return TEXT("error: Events require nondecreasing finite at >= 0, duration 0.05..5 and completion by 5 seconds. No input was sent.");
        PreviousAt=Event.At;
        Event.Input.Key=FKey(*Key);
        const FKey InputKey=Event.Input.Key;
        const FString RoutingError=PlayRoutingRefusal(Target,InputKey);
        if (!RoutingError.IsEmpty()) return RoutingError;
        if (!InputKey.IsValid() || InputKey.IsMouseButton() || (InputKey.IsAnalog() && (!InputKey.IsGamepadKey() || !InputKey.IsAxis1D()))
            || FInputBindingManager::Get().GetCommandInfoFromInputChord(TEXT("PlayWorld"),FInputChord(InputKey),false).IsValid())
            return TEXT("error: Invalid or editor-command sequence key. Use keyboard/gamepad buttons or 1D gamepad axes. No input was sent.");
        double Amount=1;
        if (InputKey.IsAnalog())
        {
            if (!Object->TryGetNumberField(TEXT("value"),Amount) || !FMath::IsFinite(Amount) || Amount < -1 || Amount > 1)
                return TEXT("error: Axis events require a finite value from -1 to 1. No input was sent.");
        }
        else if (Object->HasField(TEXT("value"))) return TEXT("error: Button events must omit value. No input was sent.");
        for (const FTimedPlayEvent& Existing:Events)
            if (Existing.Input.Key==InputKey && Event.At<Existing.At+Existing.Duration)
                return TEXT("error: Events for the same key must not overlap. No input was sent.");
        if ((InputKey.IsDigital() && Target.Controller->IsInputKeyDown(InputKey)) || !FMath::IsNearlyZero(Target.Controller->GetInputAnalogKeyState(InputKey)))
            return TEXT("error: A sequence key already has player input. No input was sent.");
        Event.Input.Value=static_cast<float>(Amount);
        Event.Input.User=Target.User; Event.Input.Device=Target.Device; Event.Input.World=Target.World;
        Event.Input.PlayerInput=Target.Controller->PlayerInput; Event.Input.Viewport=Target.Viewport;
        Events.Add(Event);
    }
    if (!FSlateApplication::Get().IsActive() || !HasPlayFocus(Target)) return TEXT("error: Active PIE viewport or menu focus is required. No input was sent.");
    if (!EndHandle.IsValid()) EndHandle=FEditorDelegates::EndPIE.AddLambda([](bool){CancelPlayHold(TEXT("PIE ended"));});
    PendingInputs=MoveTemp(Events);
    SequenceActive=true; ++SequenceGeneration; SequenceStart=FPlatformTime::Seconds();
    SequenceStatus=TEXT("pending: scheduled inputs require normal editor ticks");
    SequenceTick=FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateStatic(&TickSequence));
    return FString::Printf(TEXT("play input sequence: admitted %d events; completion within 5 wall-clock seconds. Pending normal editor ticks. Distinct keys may overlap. Release cancels all owned inputs. Late events, focus loss or PIE end cancel remaining events. Routing does not prove a gameplay action occurred; inspect PlayInputState and the game/UI. No asset edit, save or undo was performed."),PendingInputs.Num());
}

}
void TurboMCPPlayInput::Shutdown()
{
    CancelPlayHold(TEXT("plugin shutdown"));
    if (EndHandle.IsValid()) {FEditorDelegates::EndPIE.Remove(EndHandle);EndHandle.Reset();}
}

FString UTurboMCPEditToolset::EditTurboMCPPlayInput(const FString& Target,const FString& Action,const FString& Key,const FString& Value,const FString& Seconds)
{
    if (Action==TEXT("release"))
    {
        if (!Key.IsEmpty() || !Value.IsEmpty() || !Seconds.IsEmpty()) return TEXT("error: release requires an exact target and empty Key, Value and Seconds.");
        FPlayInputTarget ReleaseTarget;
        const FString ReleaseError=ResolvePlayInput(Target,ReleaseTarget);
        if (!ReleaseError.IsEmpty()) return ReleaseError;
        const auto Matches=[&](const FHeldPlayInput& Input)
            {return Input.World.Get()==ReleaseTarget.World && Input.PlayerInput.Get()==ReleaseTarget.Controller->PlayerInput;};
        if ((Held.Key.IsValid() && !Matches(Held))
            || SequenceInputs.ContainsByPredicate([&](const FHeldPlayInput& Input){return !Matches(Input);})
            || PendingInputs.ContainsByPredicate([&](const FTimedPlayEvent& Event){return !Matches(Event.Input);}))
            return TEXT("error: Tool-owned input belongs to another PIE target; nothing was released.");
        const bool HadInput=Held.Key.IsValid() || SequenceActive; CancelPlayHold();
        return HadInput?TEXT("play input: released the tool's held input and cancelled pending events. Inspect the resulting game/UI state."):TEXT("play input: no tool-held input; nothing changed.");
    }
    FPlayInputTarget Resolved;
    const FString Error=ResolvePlayInput(Target,Resolved);
    if (!Error.IsEmpty()) return Error;
    if (Action==TEXT("sequence"))
    {
        if (!Value.IsEmpty() || !Seconds.IsEmpty()) return TEXT("error: sequence requires empty Value and Seconds; put events in Key. No input was sent.");
        return StartSequence(Resolved,Key);
    }
    if (SequenceActive) return TEXT("error: A tool input sequence is active. Wait for completion or call release first.");
    if (Action==TEXT("activate") || Action==TEXT("activate_gameplay"))
    {
        if (!Key.IsEmpty() || !Value.IsEmpty() || !Seconds.IsEmpty()) return TEXT("error: activation actions require empty Key, Value and Seconds. No activation or focus change was requested.");
        if (Held.Key.IsValid()) return TEXT("error: Release the tool-held input before requesting activation.");
        if (FTurboMCPFocusGuard::IsActive()) return TEXT("error: Native activation is disabled by the unattended focus guard. No activation was requested and no input was sent. Use an attended editor session; this tool does not disable the guard.");
        const TSharedPtr<SWindow> Window = FSlateApplication::Get().FindWidgetWindow(Resolved.Viewport.ToSharedRef());
        if (!Window.IsValid() || !Window->GetNativeWindow().IsValid()) return TEXT("error: The PIE viewport has no native owning window. No activation was requested.");
        // Use the stock window request, without bypassing OS foreground policy or
        // synthesizing a game key. Activation callbacks may arrive on later ticks.
        Window->BringToFront();
        // On Windows BringToFront only uses SetActiveWindow for regular windows.
        // This stock API requests SetForegroundWindow; OS restrictions still apply.
        Window->HACK_ForceToFront();
        FString GameplayFocus;
        if (Action == TEXT("activate_gameplay"))
        {
            // This explicit action may leave a menu control. Plain activate
            // continues to preserve Slate focus for menu navigation.
            const TSharedPtr<SWidget> Previous = FSlateApplication::Get().GetUserFocusedWidget(Resolved.User);
            const bool DisplacedMenu = Previous.IsValid() && Previous.Get() != Resolved.Viewport.Get();
            FSlateApplication::Get().SetUserFocus(Resolved.User, Resolved.Viewport, EFocusCause::SetDirectly);
            GameplayFocus = FString::Printf(TEXT("\nGameplay viewport focus now: %s\nPrevious focus widget: %s; non-viewport focus displaced: %s\nNo key was sent. Activation was requested; OS foreground permission is not guaranteed. Read PlayInputState after normal ticks before sending input."),
                HasPlayFocus(Resolved) ? TEXT("yes") : TEXT("no"), Previous.IsValid() ? *Previous->GetTypeAsString() : TEXT("none"), DisplacedMenu ? TEXT("yes") : TEXT("no"));
        }
        const bool Foreground = Window->GetNativeWindow()->IsForegroundWindow();
        const FString WindowDescription = FString::Printf(TEXT("window: %s; regular: %s; virtual: %s; visible: %s; parent: %s"),
            *Window->GetTitle().ToString(), Window->IsRegularWindow() ? TEXT("yes") : TEXT("no"),
            Window->IsVirtualWindow() ? TEXT("yes") : TEXT("no"), Window->GetNativeWindow()->IsVisible() ? TEXT("yes") : TEXT("no"),
            Window->GetParentWindow().IsValid() ? *Window->GetParentWindow()->GetTitle().ToString() : TEXT("none"));
        const bool Active = FSlateApplication::Get().IsActive();
        const FString ActivationReport = FString::Printf(TEXT("play input: requested activation of the PIE viewport's owning window. This may restore or raise the editor window.\n%s\nwindow foreground now: %s\napplication active now: %s\nNo key was sent and no explicit Slate focus change was requested. Activation can be delayed or refused by the OS. Read PlayInputState after normal editor ticks; if the owning window is still not foreground or the application is inactive, select the editor window manually. Use focus separately only when gameplay viewport focus is intended; it can leave a menu control."),
            *WindowDescription, Foreground ? TEXT("yes") : TEXT("no"), Active ? TEXT("yes") : TEXT("no"));
        if (Action == TEXT("activate_gameplay"))
            return FString::Printf(TEXT("play input: requested activation and gameplay focus.\n%s\nwindow foreground now: %s\napplication active now: %s"), *WindowDescription, Foreground ? TEXT("yes") : TEXT("no"), Active ? TEXT("yes") : TEXT("no")) + GameplayFocus;
        return ActivationReport;
    }
    if (Action==TEXT("focus"))
    {
        if (!Key.IsEmpty() || !Value.IsEmpty() || !Seconds.IsEmpty()) return TEXT("error: focus requires empty Key, Value and Seconds.");
        if (Held.Key.IsValid()) return TEXT("error: Release the tool-held input before changing focus.");
        const TSharedPtr<SWidget> PreviousFocus = FSlateApplication::Get().GetUserFocusedWidget(Resolved.User);
        const FString PreviousType = PreviousFocus.IsValid() ? PreviousFocus->GetTypeAsString() : TEXT("none");
        const bool DisplacedChild = HasPlayFocus(Resolved) && PreviousFocus != Resolved.Viewport;
        FSlateApplication::Get().SetUserFocus(Resolved.User,Resolved.Viewport,EFocusCause::SetDirectly);
        const bool Focused=HasPlayFocus(Resolved);
        if (!Focused) return TEXT("error: The game viewport did not accept Slate focus. No key was sent.");
        const TSharedPtr<SWidget> CurrentFocus = FSlateApplication::Get().GetUserFocusedWidget(Resolved.User);
        return FString::Printf(TEXT("play input: set Slate user focus to the game viewport. OS foreground activation was not requested.\nprevious focus widget type: %s\ncurrent focus widget type: %s\nPIE child/menu focus displaced: %s\napplication active: %s\nNo game key was sent. To preserve an already focused PIE menu, skip focus and use hold directly; it routes through the current Slate focus. To restore menu focus, use the intended control through the native Slate tools. EditTurboMCPMenuActivate also presses a button; it is not a focus-only action. If inactive, request activate separately and read PlayInputState."),
            *PreviousType, CurrentFocus.IsValid() ? *CurrentFocus->GetTypeAsString() : TEXT("none"),
            DisplacedChild ? TEXT("yes") : TEXT("no"), FSlateApplication::Get().IsActive() ? TEXT("yes") : TEXT("no"));
    }
    if (Action!=TEXT("hold")) return TEXT("error: Action must be activate, activate_gameplay, focus, hold, sequence or release. A short bounded hold is a tap; no indefinite key-down is supported.");
    const FKey InputKey(*Key);
    const FString RoutingError=PlayRoutingRefusal(Resolved,InputKey);
    if (!RoutingError.IsEmpty()) return RoutingError;
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
    Held.Key=InputKey;Held.Value=Amount;Held.User=Resolved.User;Held.Device=Resolved.Device;Held.World=Resolved.World;Held.PlayerInput=Resolved.Controller->PlayerInput;Held.Viewport=Resolved.Viewport;Held.Deadline=FPlatformTime::Seconds()+Duration;
    const bool Handled=SendPlayEvent(Held,false);
    LastInput=Key;
    TickHandle=FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateStatic(&TickPlayHold));
    return FString::Printf(TEXT("play input: routed %s through Slate for player %d, Slate user %u\nvalue: %.3f; automatic release after %.3f wall-clock seconds\nSlate handled initial event: %s\nThis is synthetic key/axis input, not Enhanced Input action injection or physical-device emulation. Normal frames are required. Delivery does not prove the action fired; inspect PlayInputState, the menu and the next capture. No asset edit, save or undo was performed."),*Key,Resolved.PlayerIndex,Resolved.User,Amount,Duration,Handled?TEXT("yes"):TEXT("no"));
}

// A visible child can still be hidden or disabled by an ancestor. Runtime
// activation must validate the mounted Slate path before moving focus.
static bool IsReachableMenuButton(const UWidget* Widget)
{
    if (!Widget || !Widget->GetIsEnabled() || !Widget->IsVisible()) return false;
    const TSharedPtr<SWidget> Cached = Widget->GetCachedWidget();
    if (!Cached.IsValid()) return false;
    FWidgetPath Path;
    if (!FSlateApplication::Get().GeneratePathToWidgetUnchecked(Cached.ToSharedRef(), Path)) return false;
    for (int32 Index = 0; Index < Path.Widgets.Num(); ++Index)
        if (!Path.Widgets[Index].Widget->IsEnabled()) return false;
    return true;
}

FString UTurboMCPEditToolset::EditTurboMCPMenuActivate(const FString& Target,const FString& Widget)
{
    FPlayInputTarget Resolved;const FString Error=ResolvePlayInput(Target,Resolved);if(!Error.IsEmpty())return Error;
    if(Widget.IsEmpty())return TEXT("error: Widget must be an exact runtime object path or unique name from ReadTurboMCPPlayInputState.");
    if(Held.Key.IsValid() || SequenceActive)return TEXT("error: Wait for the current input hold to finish before activating a menu button.");
    if(!FSlateApplication::Get().IsActive())return TEXT("error: The editor application is inactive. No input was sent.");
    UClass* CommonButton=FindObject<UClass>(nullptr,TEXT("/Script/CommonUI.CommonButtonBase"));
    UWidget* Match=nullptr;
    for(TObjectIterator<UWidget> It;It;++It)
    {
        if(It->GetWorld()!=Resolved.World || It->GetOwningPlayer()!=Resolved.Controller || (!It->IsA<UButton>() && (!CommonButton || !It->IsA(CommonButton))))continue;
        if(It->GetPathName()!=Widget && It->GetName()!=Widget)continue;
        if(Match)return TEXT("error: More than one runtime button matches. Use its full path from ReadTurboMCPPlayInputState. No input was sent.");
        Match=*It;
    }
    if(!Match)return TEXT("error: No runtime button matches this name/path in the PIE world. Read PlayInputState again. No input was sent.");
    if(!IsReachableMenuButton(Match))return TEXT("error: Button or an ancestor is disabled, hidden or not mounted in the live UI. No input was sent.");
    if(CommonButton && Match->IsA(CommonButton) && NativeGetter(Match,CommonButton,TEXT("IsInteractionEnabled"))!=TEXT("True"))return TEXT("error: CommonUI button interaction is disabled. No input was sent.");
    // Follow the widget's normal focus forwarding, including CommonButton's
    // internal Slate button; never call its click delegates directly.
    Match->SetUserFocus(Resolved.Controller);
    const UUserWidget* UserButton=Cast<UUserWidget>(Match);
    if(!Match->HasUserFocus(Resolved.Controller) && (!UserButton || !UserButton->HasUserFocusedDescendants(Resolved.Controller)))return TEXT("error: Button did not accept user focus. No input was sent.");
    const FString Input=EditTurboMCPPlayInput(Target,TEXT("hold"),TEXT("Gamepad_FaceButton_Bottom"),TEXT(""),TEXT("0.1"));
    if(Input.StartsWith(TEXT("error:")))return Input+TEXT(" Focus was moved to the requested button.");
    return TEXT("menu activation requested for: ")+Match->GetPathName()+TEXT("\n")+Input+
        TEXT("\nActivation is pending normal frames. Read PlayInputState for active screens after release; do not infer success from delivery.");
}

FString UTurboMCPReadToolset::ReadTurboMCPPlayInputState(const FString& Target,const FString& ButtonFilter,int32 ButtonLimit)
{
    if (ButtonLimit<0 || ButtonLimit>64) return TEXT("error: ButtonLimit must be 0..64. Use 0 for diagnostics without button paths. No input was sent.");
    if (Target==TEXT("list")) return DiscoverPlayTargets();
    FPlayInputTarget Resolved;
    const FString Error=ResolvePlayInput(Target,Resolved);
    if (!Error.IsEmpty()) return Error;
    const TSharedPtr<SWidget> Focus=FSlateApplication::Get().GetUserFocusedWidget(Resolved.User);
    FString Out=FString::Printf(TEXT("play input state\nworld: %s\nplayer: %d; Slate user: %u\ngame paused: %s\nfocus widget type: %s\nfocus inside PIE viewport: %s\ntool-held input: %s\nlast synthetic key: %s"),*Resolved.World->GetPathName(),Resolved.PlayerIndex,Resolved.User,Resolved.World->IsPaused()?TEXT("yes"):TEXT("no"),Focus.IsValid()?*Focus->GetTypeAsString():TEXT("none"),HasPlayFocus(Resolved)?TEXT("yes"):TEXT("no"),(Held.Key.IsValid() && Held.World.Get()==Resolved.World && Held.PlayerInput.Get()==Resolved.Controller->PlayerInput)?*Held.Key.GetFName().ToString():TEXT("none"),*LastInput);
    Out+=TEXT("\ntarget: ")+Target;
    Out+=TEXT("\ninput sequence: ")+SequenceStatus;
    Out+=TEXT("\nInput scheduling is shared across targets; held and pending counts describe global tool ownership.");
    Out+=FString::Printf(TEXT("\nsequence held: %d; pending: %d"),SequenceInputs.Num(),PendingInputs.Num());
    Out+=TEXT("\nlast release reason: ")+LastReleaseReason;
    Out+=FSlateApplication::Get().IsActive()?TEXT("\napplication active: yes"):TEXT("\napplication active: no");
    Out += FTurboMCPFocusGuard::IsActive() ? TEXT("\nunattended focus guard: active; activation requests refuse") : TEXT("\nunattended focus guard: inactive");
    const TSharedPtr<SWindow> Window = FSlateApplication::Get().FindWidgetWindow(Resolved.Viewport.ToSharedRef());
    Out += !Window.IsValid() || !Window->GetNativeWindow().IsValid() ? TEXT("\nPIE owning window foreground: unavailable")
        : Window->GetNativeWindow()->IsForegroundWindow() ? TEXT("\nPIE owning window foreground: yes") : TEXT("\nPIE owning window foreground: no");
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
    // Local visibility is a candidate filter, not proof of effective visibility:
    // a collapsed ancestor or an inactive screen may still hide the control.
    UClass* ButtonClass=FindObject<UClass>(nullptr,TEXT("/Script/CommonUI.CommonButtonBase"));
    TArray<FString> Buttons;
    int32 Candidates=0;
    for (TObjectIterator<UWidget> It;It;++It)
    {
        if (It->GetWorld()!=Resolved.World || It->GetOwningPlayer()!=Resolved.Controller || !(It->IsA<UButton>() || (ButtonClass && It->IsA(ButtonClass))) || !It->IsVisible() || !It->GetCachedWidget().IsValid()) continue;
        ++Candidates;
        const FString Path=It->GetPathName();
        if (!ButtonFilter.IsEmpty() && !Path.Contains(ButtonFilter,ESearchCase::IgnoreCase)) continue;
        Buttons.Add(Path+(It->GetIsEnabled()?TEXT("; enabled"):TEXT("; disabled")));
    }
    Buttons.Sort();
    const int32 Shown=FMath::Min(Buttons.Num(),ButtonLimit);
    Out+=FString::Printf(TEXT("\nruntime buttons: %d candidates; %d matching; %d shown; %d omitted (exact object paths, not display labels)"),Candidates,Buttons.Num(),Shown,Buttons.Num()-Shown);
    Out+=TEXT("\nCandidates have local widget visibility and a cached Slate widget; ancestor/screen visibility and input routing eligibility are not verified.");
    for (int32 Index=0;Index<Shown;++Index) Out+=TEXT("\n  ")+Buttons[Index];
    if (Buttons.Num()>Shown) Out+=TEXT("\nNarrow ButtonFilter or increase ButtonLimit (maximum 64) to inspect matching controls. ButtonLimit=0 returns diagnostics only.");
    Out+=TEXT("\nCommonUI routing policy: not inspected. Use Slate Snapshot for visible controls. CommonInput reports its current logical device type, not proof of a physical device. Context presence does not prove an action triggered.");
    return Out;
}
