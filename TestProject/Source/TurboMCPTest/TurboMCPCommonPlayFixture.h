// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
#pragma once
#include "CommonInputBaseTypes.h"
#include "CommonActivatableWidget.h"
#include "CommonButtonBase.h"
#include "CommonTabListWidgetBase.h"
#include "GameFramework/Actor.h"
#include "TurboMCPCommonPlayFixture.generated.h"

class UCommonActivatableWidgetStack;
class UCommonAnimatedSwitcher;
class UHorizontalBox;
class UTextBlock;
class UInputMappingContext;
class UEnhancedInputComponent;

// Test-project-only classes. Production TurboMCP does not depend on CommonUI.
UCLASS()
class UTurboMCPCommonInputData : public UCommonUIInputData
{
    GENERATED_BODY()
public:
    UTurboMCPCommonInputData();
    UPROPERTY() TObjectPtr<UInputAction> NextTab;
    UPROPERTY() TObjectPtr<UInputAction> PreviousTab;
};
UCLASS()
class UTurboMCPCommonPlayButton : public UCommonButtonBase
{
    GENERATED_BODY()
public:
    void SetLabel(const FString& Value);
    FString GetLabel() const;
protected:
    virtual void NativeOnInitialized() override;
    UPROPERTY() TObjectPtr<UTextBlock> Label;
};
UCLASS()
class UTurboMCPCommonPlayTabs : public UCommonTabListWidgetBase
{
    GENERATED_BODY()
protected:
    virtual void NativeOnInitialized() override;
    virtual void HandleTabCreation_Implementation(FName Name,UCommonButtonBase* Button) override;
    UPROPERTY() TObjectPtr<UHorizontalBox> Row;
};
UCLASS()
class UTurboMCPCommonPlayScreen : public UCommonActivatableWidget
{
    GENERATED_BODY()
public:
    UTurboMCPCommonPlayScreen();
    FSimpleDelegate OpenSettings;
    int32 DisabledClickCount=0;
    virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;
protected:
    virtual void NativeOnInitialized() override;
    virtual UWidget* NativeGetDesiredFocusTarget() const override;
    UPROPERTY() TObjectPtr<UTurboMCPCommonPlayButton> FirstButton;
};
UCLASS()
class UTurboMCPCommonSettingsScreen : public UCommonActivatableWidget
{
    GENERATED_BODY()
public:
    UTurboMCPCommonSettingsScreen();
    virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;
    UPROPERTY() TObjectPtr<UTurboMCPCommonPlayTabs> Tabs;
protected:
    virtual void NativeOnInitialized() override;
    virtual UWidget* NativeGetDesiredFocusTarget() const override;
};
UCLASS()
class UTurboMCPCommonPlayRoot : public UCommonActivatableWidget
{
    GENERATED_BODY()
public:
    UPROPERTY() TObjectPtr<UCommonActivatableWidgetStack> Stack;
    virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;
protected:
    virtual void NativeOnInitialized() override;
};
UCLASS()
class ATurboMCPCommonPlayFixture : public AActor
{
    GENERATED_BODY()
public:
    ATurboMCPCommonPlayFixture();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    UFUNCTION() FString Describe() const;
private:
    UPROPERTY() TObjectPtr<UTurboMCPCommonPlayRoot> Root;
    UPROPERTY() TObjectPtr<UInputMappingContext> Mapping;
    UPROPERTY() TObjectPtr<UEnhancedInputComponent> Input;
    int32 PauseCount=0;
    int32 LookCount=0;
    float LookYaw=0;
};
