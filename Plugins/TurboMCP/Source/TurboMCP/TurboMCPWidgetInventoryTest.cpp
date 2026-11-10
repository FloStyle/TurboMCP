// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Tools/TurboMCPToolsets.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/Image.h"
#include "Components/RichTextBlock.h"
#include "Components/RichTextBlockImageDecorator.h"
#include "Dom/JsonObject.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "Engine/DataTable.h"
#include "Engine/Texture2D.h"
#include "HAL/PlatformTime.h"
#include "InputAction.h"
#include "K2Node_CallFunction.h"
#include "K2Node_Event.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Modules/ModuleManager.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"
#include "WidgetBlueprint.h"
#include "WidgetBlueprintOperationUtils.h"

namespace
{
bool HasEvidence(const TSharedPtr<FJsonObject>& Page, const FString& Rule, const FString& Source,
    const FString& Path, const FString& Value, const FString& Origin = FString(), const FString& Row = FString())
{
    if (!Page)
    {
        return false;
    }
    for (const TSharedPtr<FJsonValue>& Match : Page->GetArrayField(TEXT("matches")))
    {
        for (const TSharedPtr<FJsonValue>& Entry : Match->AsObject()->GetArrayField(TEXT("evidence")))
        {
            const TSharedPtr<FJsonObject>& Evidence = Entry->AsObject();
            FString ActualOrigin;
            FString ActualRow;
            Evidence->TryGetStringField(TEXT("origin"), ActualOrigin);
            Evidence->TryGetStringField(TEXT("row"), ActualRow);
            if (Evidence->GetStringField(TEXT("rule")) == Rule &&
                Evidence->GetStringField(TEXT("source")) == Source &&
                Evidence->GetStringField(TEXT("path")) == Path &&
                Evidence->GetStringField(TEXT("value")) == Value &&
                ActualOrigin == Origin && ActualRow == Row)
            {
                return true;
            }
        }
    }
    return false;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTurboMCPWidgetInventoryTest, "TurboMCP.Widget.Inventory",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTurboMCPWidgetInventoryTest::RunTest(const FString& Parameters)
{
    const FString Folder = TEXT("/Game/Tests/TMCPWidgetInventory_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
    TArray<UPackage*> Packages;
    TArray<UWidgetBlueprint*> Widgets;
    TArray<UObject*> ReferenceAssets;
    ON_SCOPE_EXIT
    {
        for (UWidgetBlueprint* Widget : Widgets)
        {
            FAssetRegistryModule::AssetDeleted(Widget);
            Widget->ClearFlags(RF_Public | RF_Standalone);
            Widget->MarkAsGarbage();
        }
        for (UObject* Asset : ReferenceAssets)
        {
            Asset->ClearFlags(RF_Public | RF_Standalone);
            Asset->MarkAsGarbage();
        }
        for (UPackage* Package : Packages)
        {
            Package->SetDirtyFlag(false); // Retirement of unique never-saved fixture packages.
        }
    };
    auto MakePackage = [&](const FString& Path)
    {
        UPackage* Package = CreatePackage(*Path);
        Packages.Add(Package);
        return Package;
    };
    auto MakeWidget = [&](const FString& Parent, const TCHAR* Name)
    {
        UPackage* Package = MakePackage(Parent + TEXT("/") + Name);
        UWidgetBlueprint* Widget = FWidgetBlueprintOperationUtils::CreateWidgetBlueprint(Package, FName(Name),
            BPTYPE_Normal, UUserWidget::StaticClass(), UCanvasPanel::StaticClass(), TEXT("TurboMCPInventory"), false);
        if (Widget)
        {
            Widgets.Add(Widget);
        }
        return Widget;
    };
    UWidgetBlueprint* Menu = MakeWidget(Folder, TEXT("A_Menu"));
    UWidgetBlueprint* Card = MakeWidget(Folder + TEXT("/Child"), TEXT("B_Card"));
    UWidgetBlueprint* Sibling = MakeWidget(Folder + TEXT("_Sibling"), TEXT("C_Sibling"));
    if (!Menu || !Card || !Sibling || !Menu->WidgetTree || !Card->WidgetTree || !Sibling->WidgetTree)
    {
        AddError(TEXT("The three isolated Widget Blueprint fixtures were not created."));
        return false;
    }
    UCanvasPanel* Root = CastChecked<UCanvasPanel>(Menu->WidgetTree->RootWidget);
    UImage* Hero = Menu->WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("Hero"));
    Root->AddChildToCanvas(Hero);
    UTexture2D* Texture = NewObject<UTexture2D>(MakePackage(Folder + TEXT("/Icons/T_Icon")),
        TEXT("T_Icon"), RF_Public | RF_Standalone);
    ReferenceAssets.Add(Texture);
    Hero->SetBrushFromTexture(Texture);

    UEdGraph* Graph = FBlueprintEditorUtils::CreateNewGraph(Menu, TEXT("InventoryEvents"),
        UEdGraph::StaticClass(), UEdGraphSchema_K2::StaticClass());
    Menu->UbergraphPages.Add(Graph);
    UK2Node_Event* Event = NewObject<UK2Node_Event>(Graph, TEXT("ConstructEvent"));
    Event->bOverrideFunction = true;
    Event->EventReference.SetExternalMember(TEXT("Construct"), UUserWidget::StaticClass());
    Graph->AddNode(Event, false, false);
    UK2Node_CallFunction* Call = NewObject<UK2Node_CallFunction>(Graph, TEXT("SetBrushCall"));
    Call->SetFromFunction(UImage::StaticClass()->FindFunctionByName(TEXT("SetBrushFromTexture")));
    Graph->AddNode(Call, false, false);
    Call->AllocateDefaultPins();
    UEdGraphPin* TexturePin = Call->FindPinChecked(TEXT("Texture"));
    TexturePin->DefaultObject = Texture;

    // CommonUI is enabled in TestProject. The fixture uses its reflected fields through the same module boundary as production.
    FModuleManager::Get().LoadModule(TEXT("CommonUI"));
    UClass* ActionType = FindObject<UClass>(nullptr, TEXT("/Script/CommonUI.CommonActionWidget"));
    if (!TestNotNull(TEXT("The TestProject CommonActionWidget class is available"), ActionType))
    {
        return false;
    }
    UWidget* Action = Menu->WidgetTree->ConstructWidget<UWidget>(ActionType, TEXT("Action"));
    Root->AddChildToCanvas(Action);
    const FObjectPropertyBase* Enhanced = FindFProperty<FObjectPropertyBase>(ActionType, TEXT("EnhancedInputAction"));
    const FArrayProperty* Rows = FindFProperty<FArrayProperty>(ActionType, TEXT("InputActions"));
    if (!TestNotNull(TEXT("Enhanced Input reflection"), Enhanced) ||
        !TestNotNull(TEXT("Input action row reflection"), Rows))
    {
        return false;
    }
    UInputAction* Input = NewObject<UInputAction>(MakePackage(Folder + TEXT("/Inputs/IA_Select")),
        TEXT("IA_Select"), RF_Public | RF_Standalone);
    UDataTable* Table = NewObject<UDataTable>(MakePackage(Folder + TEXT("/Inputs/DT_Actions")),
        TEXT("DT_Actions"), RF_Public | RF_Standalone);
    ReferenceAssets.Add(Input);
    ReferenceAssets.Add(Table);
    Enhanced->SetObjectPropertyValue(Enhanced->ContainerPtrToValuePtr<void>(Action), Input);
    FScriptArrayHelper RowValues(Rows, Rows->ContainerPtrToValuePtr<void>(Action));
    auto& Row = *reinterpret_cast<FDataTableRowHandle*>(RowValues.GetRawPtr(RowValues.AddValue()));
    Row.DataTable = Table;
    Row.RowName = TEXT("Select");

    URichTextBlock* Rich = Menu->WidgetTree->ConstructWidget<URichTextBlock>(
        URichTextBlock::StaticClass(), TEXT("Rich"));
    Root->AddChildToCanvas(Rich);
    Rich->SetDecorators({URichTextBlockImageDecorator::StaticClass()});

    // Publish the memory-only assets after setup, then make each package clean for the no-mutation check.
    for (UWidgetBlueprint* Widget : Widgets)
    {
        FAssetRegistryModule::AssetCreated(Widget);
    }
    for (UPackage* Package : Packages)
    {
        Package->SetDirtyFlag(false);
    }

    const double Start = FPlatformTime::Seconds();
    int32 Calls = 0;
    auto Read = [&](const FString& Rules, bool bRecursive = true, int32 PageSize = 8,
        const FString& Cursor = TEXT("first"), const FString& Mode = TEXT("all"))
    {
        ++Calls;
        return UTurboMCPReadToolset::ReadTurboMCPWidgetInventory(Rules, Folder, bRecursive, Mode, PageSize, Cursor, false);
    };
    const FString ResultMarker = TEXT("\n; tmcp-result-v1: ");
    auto Page = [&](const FString& Answer)
    {
        const int32 End = Answer.Find(ResultMarker, ESearchCase::CaseSensitive, ESearchDir::FromEnd);
        TSharedPtr<FJsonObject> Json;
        if (End == INDEX_NONE || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Answer.Left(End)), Json))
        {
            AddError(TEXT("The inventory did not return a JSON body and result envelope: ") + Answer.Left(500));
        }
        return Json.IsValid() ? Json : MakeShared<FJsonObject>();
    };

    auto RefusedWith = [&](const FString& Answer, const TCHAR* Code)
    {
        const int32 Offset = Answer.Find(ResultMarker, ESearchCase::CaseSensitive, ESearchDir::FromEnd);
        TSharedPtr<FJsonObject> Envelope;
        FString Outcome;
        FString ActualCode;
        FString OperationId;
        return Offset != INDEX_NONE &&
            FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Answer.Mid(Offset + ResultMarker.Len())), Envelope) &&
            Envelope && Envelope->TryGetStringField(TEXT("outcome"), Outcome) && Outcome == TEXT("refused") &&
            Envelope->TryGetStringField(TEXT("code"), ActualCode) && ActualCode == Code &&
            Envelope->TryGetStringField(TEXT("operationId"), OperationId) && !OperationId.IsEmpty();
    };

    const FString Panels = TEXT("{\"class\":\"/Script/UMG.PanelWidget\"}");
    const TSharedPtr<FJsonObject> First = Page(Read(Panels, true, 1));
    if (!First || !First->HasField(TEXT("nextCursor")))
    {
        AddError(TEXT("The first page has no cursor field."));
        return false;
    }
    const FString Cursor = First->GetStringField(TEXT("nextCursor"));
    TestEqual(TEXT("Candidate count excludes the sibling folder"), First->GetIntegerField(TEXT("totalCandidates")), 2);
    TestEqual(TEXT("Only one candidate scanned"), First->GetIntegerField(TEXT("scanned")), 1);
    TestTrue(TEXT("The total is unknown before continuation"), First->HasTypedField<EJson::Null>(TEXT("totalMatches")));
    TestTrue(TEXT("A CanvasPanel matches its PanelWidget superclass"), HasEvidence(First, TEXT("class"),
        Root->GetName(), TEXT("class"), UCanvasPanel::StaticClass()->GetPathName()));
    const TSharedPtr<FJsonObject> Second = Page(Read(Panels, true, 1, Cursor));
    if (!Second || !Second->HasField(TEXT("totalMatches")))
    {
        AddError(TEXT("The second page has no total."));
        return false;
    }
    TestEqual(TEXT("The final page totals both matching candidates"), Second->GetIntegerField(TEXT("totalMatches")), 2);
    TestEqual(TEXT("The continuation ends on the last page"), Second->GetStringField(TEXT("nextCursor")), FString());
    TestEqual(TEXT("Shallow scan excludes Child"), Page(Read(Panels, false))->GetIntegerField(TEXT("totalCandidates")), 1);

    const FString ImageAndAsset = FString::Printf(TEXT("{\"class\":\"/Script/UMG.Image\",\"asset\":\"%s\"}"), *Texture->GetPathName());
    const TSharedPtr<FJsonObject> Images = Page(Read(ImageAndAsset));
    TestEqual(TEXT("All rules select just the menu"), Images->GetIntegerField(TEXT("totalMatches")), 1);
    const TArray<TSharedPtr<FJsonValue>>& ImageMatches = Images->GetArrayField(TEXT("matches"));
    if (ImageMatches.Num() != 1)
    {
        AddError(TEXT("Expected exactly one match record for the class and asset query."));
        return false;
    }
    TestEqual(TEXT("The menu uses a full object path"), ImageMatches[0]->AsObject()->GetStringField(TEXT("asset")), Menu->GetPathName());
    TestTrue(TEXT("Exact texture matches the designer brush override"), HasEvidence(Images, TEXT("asset"),
        TEXT("Hero"), TEXT("Brush.ResourceObject"), Texture->GetPathName(), TEXT("instance_value")));
    TestTrue(TEXT("Exact texture also matches an unlinked graph literal"), HasEvidence(Images, TEXT("asset"),
        Graph->GetName(), TEXT("SetBrushCall.Texture"), Texture->GetPathName(), TEXT("pin_literal")));

    const FString ActionAndFolder = FString::Printf(TEXT("{\"class\":\"/Script/CommonUI.CommonActionWidget\",\"assetFolder\":\"%s/Inputs\"}"), *Folder);
    const TSharedPtr<FJsonObject> Actions = Page(Read(ActionAndFolder));
    TestTrue(TEXT("Enhanced Input designer value is reported"), HasEvidence(Actions, TEXT("assetFolder"), TEXT("Action"),
        TEXT("EnhancedInputAction"), Input->GetPathName(), TEXT("instance_value")));
    TestTrue(TEXT("Input-action row retains its table and row name"), HasEvidence(Actions, TEXT("assetFolder"), TEXT("Action"),
        TEXT("InputActions[0]"), Table->GetPathName(), TEXT("instance_value"), TEXT("Select")));
    const TSharedPtr<FJsonObject> Decorators = Page(Read(TEXT("{\"class\":\"/Script/UMG.RichTextBlockDecorator\"}")));
    TestTrue(TEXT("Decorator subclasses match through the assigned class array"), HasEvidence(Decorators, TEXT("class"),
        TEXT("Rich"), TEXT("DecoratorClasses[0]"), URichTextBlockImageDecorator::StaticClass()->GetPathName(), TEXT("instance_value")));

    const FString GraphRules = TEXT("{\"override\":\"Construct\",\"call\":\"SetBrushFromTexture\",\"nodeClass\":\"/Script/BlueprintGraph.K2Node_CallFunction\"}");
    const TSharedPtr<FJsonObject> Graphs = Page(Read(GraphRules));
    TestEqual(TEXT("Event, node class and call match together"), Graphs->GetIntegerField(TEXT("totalMatches")), 1);
    TestTrue(TEXT("Construct is attributed to the actual event node"), HasEvidence(Graphs, TEXT("override"),
        Graph->GetName(), TEXT("ConstructEvent"), TEXT("Construct")));
    const FString AnyRules = TEXT("{\"override\":\"Tick\",\"call\":\"SetBrushFromTexture\"}");
    TestEqual(TEXT("Any accepts the matching call"), Page(Read(AnyRules, true, 8, TEXT("first"), TEXT("any")))->GetIntegerField(TEXT("totalMatches")), 1);
    TestEqual(TEXT("All requires Blueprint evidence for Tick as well"), Page(Read(AnyRules))->GetIntegerField(TEXT("totalMatches")), 0);

    TestTrue(TEXT("A different query rejects a continuation"), RefusedWith(Read(ImageAndAsset, true, 1, Cursor), TEXT("stale_inventory_cursor")));
    TestTrue(TEXT("Unknown rule refuses"), RefusedWith(Read(TEXT("{\"unsupported\":\"x\"}")), TEXT("invalid_inventory_rule")));
    TestTrue(TEXT("Unavailable optional class refuses"), RefusedWith(Read(TEXT("{\"class\":\"/Script/UnavailableUI.MissingWidget\"}")), TEXT("invalid_inventory_rule")));
    TestTrue(TEXT("Out-of-range page size refuses"), RefusedWith(Read(Panels, true, 17), TEXT("invalid_inventory_options")));
    TestEqual(TEXT("Loaded-only inspection attempted no package loads"), First->GetIntegerField(TEXT("loadAttempts")), 0);
    TestTrue(TEXT("All fixture packages remained clean"), !Packages.ContainsByPredicate([](const UPackage* Package) { return Package->IsDirty(); }));
    TestTrue(TEXT("The exact designer and graph objects survived inspection"),
        Menu->WidgetTree->FindWidget(TEXT("Hero")) == Hero && Hero->GetBrush().GetResourceObject() == Texture &&
        Graph->Nodes.Contains(Event) && Graph->Nodes.Contains(Call) && TexturePin->DefaultObject == Texture);
    AddInfo(FString::Printf(TEXT("Widget inventory fixture made %d tool calls in %.3f ms."), Calls,
        1000.0 * (FPlatformTime::Seconds() - Start)));
    return true;
}

#endif
