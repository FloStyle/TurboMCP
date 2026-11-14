// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Tools/TurboMCPToolsets.h"
#include "TurboMCPWidgetTreeSync.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetNavigation.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "UObject/Package.h"
#include "WidgetBlueprint.h"
#include "WidgetBlueprintOperationUtils.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTurboMCPWidgetNavigationReadTest, "TurboMCP.Widget.NavigationRead",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTurboMCPWidgetNavigationReadTest::RunTest(const FString& Parameters)
{
    const FString Name = TEXT("WBP_NavRead_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
    const FString PackageName = TEXT("/Game/Tests/") + Name;
    UPackage* Package = CreatePackage(*PackageName);
    UWidgetBlueprint* Blueprint = FWidgetBlueprintOperationUtils::CreateWidgetBlueprint(Package, FName(*Name),
        BPTYPE_Normal, UUserWidget::StaticClass(), UCanvasPanel::StaticClass(), TEXT("TurboMCPNavigationRead"), false);
    if (!Blueprint || !Blueprint->WidgetTree || !Blueprint->WidgetTree->RootWidget)
    {
        AddError(TEXT("Could not create the isolated Widget Blueprint fixture."));
        return false;
    }
    ON_SCOPE_EXIT
    {
        FAssetRegistryModule::AssetDeleted(Blueprint);
        Blueprint->ClearFlags(RF_Public | RF_Standalone);
        Package->SetDirtyFlag(false);
    };

    UCanvasPanel* Root = CastChecked<UCanvasPanel>(Blueprint->WidgetTree->RootWidget);
    UButton* Button = Blueprint->WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("ConfirmButton"));
    Root->AddChildToCanvas(Button);
    Button->Navigation = NewObject<UWidgetNavigation>(Button);
    Button->Navigation->Up.Rule = EUINavigationRule::Explicit;
    Button->Navigation->Up.WidgetToFocus = TEXT("PreviousButton");
    Button->Navigation->Down.Rule = EUINavigationRule::Custom;
    Button->Navigation->Down.WidgetToFocus = TEXT("ChooseNextButton");
    Button->Navigation->Left.Rule = EUINavigationRule::Stop;
    Button->SetClipping(EWidgetClipping::ClipToBounds);
    FWidgetTransform Transform;
    Transform.Translation = FVector2D(12.0, 8.0);
    Button->SetRenderTransform(Transform);
    FAssetRegistryModule::AssetCreated(Blueprint);
    Package->SetDirtyFlag(false);

    const FString Navigation = UTurboMCPReadToolset::ReadTurboMCPWidgetProperty(PackageName,
        TEXT("ConfirmButton"), TEXT("widget"), TEXT("Navigation"));
    TestTrue(TEXT("Explicit destination"), Navigation.Contains(TEXT("Up: Explicit; target=PreviousButton")));
    TestTrue(TEXT("Custom function"), Navigation.Contains(TEXT("Down: Custom; function=ChooseNextButton")));
    TestTrue(TEXT("Default direction"), Navigation.Contains(TEXT("Previous: Escape")));
    TestTrue(TEXT("Stored Designer qualification"), Navigation.Contains(TEXT("not observed runtime focus")));

    const FString Tree = UTurboMCPReadToolset::ReadTurboMCPWidgetTree(PackageName);
    TestTrue(TEXT("Tree navigation summary"), Tree.Contains(TEXT("Up=Explicit(PreviousButton)")));
    TestTrue(TEXT("Tree clipping"), Tree.Contains(TEXT("clipping: ClipToBounds")));
    TestTrue(TEXT("Tree transform"), Tree.Contains(TEXT("render transform: translation=")));
    TestTrue(TEXT("Tree focusability"), Tree.Contains(TEXT("focusable (Designer):")));
    TestTrue(TEXT("Absent navigation is explicit"), FTurboMCPWidgetTreeSync::ReadDesignerNavigation(Root).Contains(TEXT("absent")));
    TestTrue(TEXT("Default tree context stays compact"), FTurboMCPWidgetTreeSync::ReadDesignerContext(Root).IsEmpty());
    TestTrue(TEXT("Navigation read stays read-only"), UTurboMCPPreviewToolset::PreviewTurboMCPWidgetProperty(
        PackageName, TEXT("ConfirmButton"), TEXT("widget"), TEXT("Navigation"), TEXT("x")).Contains(TEXT("read-only")));
    TestFalse(TEXT("Package remains clean"), Package->IsDirty());
    TestEqual(TEXT("Navigation target unchanged"), Button->Navigation->Up.WidgetToFocus, FName(TEXT("PreviousButton")));
    return !HasAnyErrors();
}

#endif
