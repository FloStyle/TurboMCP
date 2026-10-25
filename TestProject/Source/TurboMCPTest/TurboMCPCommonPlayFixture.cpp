// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
#include "TurboMCPCommonPlayFixture.h"
#include "UObject/UObjectIterator.h"
#include "Blueprint/WidgetTree.h"
#include "Components/VerticalBox.h"
#include "Components/HorizontalBox.h"
#include "Components/TextBlock.h"
#include "Components/SizeBox.h"
#include "Components/Border.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "CommonAnimatedSwitcher.h"
#include "Widgets/CommonActivatableWidgetContainer.h"
#include "Input/CommonUIInputTypes.h"
#include "Input/CommonUIActionRouterBase.h"
#include "CommonInputSubsystem.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"
#include "InputMappingContext.h"

UTurboMCPCommonInputData::UTurboMCPCommonInputData()
{
    EnhancedInputBackAction=CreateDefaultSubobject<UInputAction>(TEXT("MenuBack"));
    EnhancedInputBackAction->bTriggerWhenPaused=true;
    NextTab=CreateDefaultSubobject<UInputAction>(TEXT("NextTab"));NextTab->bTriggerWhenPaused=true;
    PreviousTab=CreateDefaultSubobject<UInputAction>(TEXT("PreviousTab"));PreviousTab->bTriggerWhenPaused=true;
}
void UTurboMCPCommonPlayButton::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    SetIsFocusable(true);
    auto* Size=WidgetTree->ConstructWidget<USizeBox>();Size->SetWidthOverride(480);Size->SetHeightOverride(65);
    Label=WidgetTree->ConstructWidget<UTextBlock>();Label->SetColorAndOpacity(FSlateColor(FLinearColor::Black));
    auto Font=Label->GetFont();Font.Size=26;Label->SetFont(Font);
    Label->SetText(FText::FromString(TEXT("Button")));Size->AddChild(Label);WidgetTree->RootWidget=Size;
}
void UTurboMCPCommonPlayButton::SetLabel(const FString& Value) {Label->SetText(FText::FromString(Value));}
void UTurboMCPCommonPlayTabs::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    Row=WidgetTree->ConstructWidget<UHorizontalBox>();WidgetTree->RootWidget=Row;
    const auto* Data=GetDefault<UTurboMCPCommonInputData>();
    NextTabEnhancedInputAction=Data->NextTab;PreviousTabEnhancedInputAction=Data->PreviousTab;
    bAutoListenForInput=true;
}
void UTurboMCPCommonPlayTabs::HandleTabCreation_Implementation(FName Name,UCommonButtonBase* Button)
{
    Row->AddChild(Button);CastChecked<UTurboMCPCommonPlayButton>(Button)->SetLabel(Name.ToString());
}
UTurboMCPCommonPlayScreen::UTurboMCPCommonPlayScreen() {bIsBackHandler=true;}
TOptional<FUIInputConfig> UTurboMCPCommonPlayScreen::GetDesiredInputConfig() const {return FUIInputConfig(ECommonInputMode::Menu,EMouseCaptureMode::NoCapture);}
void UTurboMCPCommonPlayScreen::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    auto* Column=WidgetTree->ConstructWidget<UVerticalBox>();
    auto* Panel=WidgetTree->ConstructWidget<UBorder>();Panel->SetBrushColor(FLinearColor(0.04f,0.04f,0.04f,1));Panel->SetPadding(FMargin(24));Panel->AddChild(Column);WidgetTree->RootWidget=Panel;
    FirstButton=CreateWidget<UTurboMCPCommonPlayButton>(GetOwningPlayer());FirstButton->SetLabel(TEXT("Settings"));Column->AddChild(FirstButton);
    FirstButton->OnClicked().AddLambda([this]{OpenSettings.ExecuteIfBound();});
    auto* Resume=CreateWidget<UTurboMCPCommonPlayButton>(GetOwningPlayer());Resume->SetLabel(TEXT("Resume"));Column->AddChild(Resume);
    Resume->OnClicked().AddLambda([this]{DeactivateWidget();});
}
UWidget* UTurboMCPCommonPlayScreen::NativeGetDesiredFocusTarget() const {return FirstButton;}
UTurboMCPCommonSettingsScreen::UTurboMCPCommonSettingsScreen() {bIsBackHandler=true;}
TOptional<FUIInputConfig> UTurboMCPCommonSettingsScreen::GetDesiredInputConfig() const {return FUIInputConfig(ECommonInputMode::Menu,EMouseCaptureMode::NoCapture);}
void UTurboMCPCommonSettingsScreen::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    auto* Column=WidgetTree->ConstructWidget<UVerticalBox>();
    auto* Panel=WidgetTree->ConstructWidget<UBorder>();Panel->SetBrushColor(FLinearColor(0.04f,0.04f,0.04f,1));Panel->SetPadding(FMargin(24));Panel->AddChild(Column);WidgetTree->RootWidget=Panel;
    Tabs=CreateWidget<UTurboMCPCommonPlayTabs>(GetOwningPlayer());Column->AddChild(Tabs);
    auto* Switcher=WidgetTree->ConstructWidget<UCommonAnimatedSwitcher>();Switcher->SetDisableTransitionAnimation(true);Column->AddChild(Switcher);Tabs->SetLinkedSwitcher(Switcher);
    for (const FString& Name:{FString(TEXT("Audio")),FString(TEXT("Video"))})
    {
        auto* Text=WidgetTree->ConstructWidget<UTextBlock>();Text->SetText(FText::FromString(Name+TEXT(" settings content")));
        Switcher->AddChild(Text);
        Tabs->RegisterTab(FName(*Name),UTurboMCPCommonPlayButton::StaticClass(),Text);
    }
}
UWidget* UTurboMCPCommonSettingsScreen::NativeGetDesiredFocusTarget() const {return Tabs?Tabs->GetTabButtonBaseByID(TEXT("Audio")):nullptr;}
void UTurboMCPCommonPlayRoot::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    Stack=WidgetTree->ConstructWidget<UCommonActivatableWidgetStack>();Stack->SetTransitionDuration(0);
    auto* Overlay=WidgetTree->ConstructWidget<UOverlay>();WidgetTree->RootWidget=Overlay;
    auto* Size=WidgetTree->ConstructWidget<USizeBox>();Size->SetWidthOverride(1000);Size->SetHeightOverride(360);Size->AddChild(Stack);
    auto* OverlaySlot=Overlay->AddChildToOverlay(Size);OverlaySlot->SetHorizontalAlignment(HAlign_Center);OverlaySlot->SetVerticalAlignment(VAlign_Center);
}
TOptional<FUIInputConfig> UTurboMCPCommonPlayRoot::GetDesiredInputConfig() const {return FUIInputConfig(ECommonInputMode::Game,EMouseCaptureMode::CapturePermanently);}
ATurboMCPCommonPlayFixture::ATurboMCPCommonPlayFixture()
{
    PrimaryActorTick.bCanEverTick=true;PrimaryActorTick.bTickEvenWhenPaused=true;
}
void ATurboMCPCommonPlayFixture::BeginPlay()
{
    Super::BeginPlay();
    auto* PC=GetWorld()->GetFirstPlayerController();
    Root=CreateWidget<UTurboMCPCommonPlayRoot>(PC);Root->AddToViewport(100);Root->ActivateWidget();
    Mapping=NewObject<UInputMappingContext>(this);
    auto* Pause=NewObject<UInputAction>(this);Pause->bTriggerWhenPaused=true;
    auto* Look=NewObject<UInputAction>(this);Look->ValueType=EInputActionValueType::Axis1D;
    const auto* Data=GetDefault<UTurboMCPCommonInputData>();
    Mapping->MapKey(Pause,EKeys::Gamepad_Special_Right);Mapping->MapKey(Pause,EKeys::P);
    Mapping->MapKey(Data->EnhancedInputBackAction,EKeys::Gamepad_FaceButton_Right);
    Mapping->MapKey(Data->EnhancedInputBackAction,EKeys::BackSpace);
    Mapping->MapKey(Data->NextTab,EKeys::Gamepad_RightShoulder);Mapping->MapKey(Data->PreviousTab,EKeys::Gamepad_LeftShoulder);
    Mapping->MapKey(Look,EKeys::Gamepad_RightX);
    Input=NewObject<UEnhancedInputComponent>(this);Input->RegisterComponent();Input->Priority=2000;
    Input->BindActionValueLambda(Pause,ETriggerEvent::Started,[this](const FInputActionValue&)
    {
        if (Root->Stack->GetActiveWidget()) return;
        ++PauseCount;
        auto* Menu=Root->Stack->AddWidget<UTurboMCPCommonPlayScreen>(UTurboMCPCommonPlayScreen::StaticClass());
        Menu->OpenSettings.BindLambda([this]{Root->Stack->AddWidget<UTurboMCPCommonSettingsScreen>(UTurboMCPCommonSettingsScreen::StaticClass());});
        GetWorld()->GetFirstPlayerController()->SetPause(true);
    });
    Input->BindActionValueLambda(Look,ETriggerEvent::Triggered,[this](const FInputActionValue& Value)
    {
        ++LookCount;LookYaw+=Value.Get<float>();
        auto* Controller=GetWorld()->GetFirstPlayerController();auto Rotation=Controller->GetControlRotation();Rotation.Yaw+=Value.Get<float>();Controller->SetControlRotation(Rotation);
    });
    PC->PushInputComponent(Input);
    ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer())->AddMappingContext(Mapping,2000);
}
void ATurboMCPCommonPlayFixture::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (Root && !Root->Stack->GetActiveWidget() && GetWorld()->IsPaused()) GetWorld()->GetFirstPlayerController()->SetPause(false);
}
FString UTurboMCPCommonPlayButton::GetLabel() const {return Label?Label->GetText().ToString():TEXT("none");}
FString ATurboMCPCommonPlayFixture::Describe() const
{
    auto* PC=GetWorld()->GetFirstPlayerController();
    auto* Active=Root?Root->Stack->GetActiveWidget():nullptr;
    auto* Settings=Cast<UTurboMCPCommonSettingsScreen>(Active);
    auto* Common=ULocalPlayer::GetSubsystem<UCommonInputSubsystem>(PC->GetLocalPlayer());
    auto* Router=ULocalPlayer::GetSubsystem<UCommonUIActionRouterBase>(PC->GetLocalPlayer());
    const auto* Content=Settings?Cast<UTextBlock>(Settings->Tabs->GetLinkedSwitcher()->GetActiveWidget()):nullptr;
    const FString ContentText=Content?Content->GetText().ToString():TEXT("none");
    FString FocusLabel=TEXT("none");
    for (TObjectIterator<UTurboMCPCommonPlayButton> It;It;++It)
        if (It->GetWorld()==GetWorld() && (It->HasUserFocus(PC)||It->HasUserFocusedDescendants(PC))) FocusLabel=It->GetLabel();
    return TEXT("focused button: ")+FocusLabel+TEXT("; content: ")+ContentText+TEXT("; ")+FString::Printf(TEXT("screen: %s; tab: %s; paused: %s; pauses: %d; look calls: %d; look yaw: %.1f; input type: %d; routing mode: %d"),Settings?TEXT("Settings"):Active?TEXT("Pause"):TEXT("Gameplay"),Settings?*Settings->Tabs->GetActiveTab().ToString():TEXT("none"),GetWorld()->IsPaused()?TEXT("yes"):TEXT("no"),PauseCount,LookCount,LookYaw,Common?int32(Common->GetCurrentInputType()):-1,Router?int32(Router->GetActiveInputMode()):-1);
}
void ATurboMCPCommonPlayFixture::EndPlay(const EEndPlayReason::Type Reason)
{
    if (auto* PC=GetWorld()->GetFirstPlayerController())
    {
        if (auto* Subsystem=ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer())) Subsystem->RemoveMappingContext(Mapping);
        PC->PopInputComponent(Input);
    }
    if (Root) Root->RemoveFromParent();
    Super::EndPlay(Reason);
}
