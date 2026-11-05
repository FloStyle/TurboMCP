// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
#include "Tools/TurboMCPToolsets.h"
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Modules/ModuleManager.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"
#include "Components/CanvasPanel.h"
#include "WidgetBlueprint.h"
#include "WidgetBlueprintOperationUtils.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "UObject/Package.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTurboMCPBlueprintReparentTest,"TurboMCP.GraphAppend.BlueprintReparent",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTurboMCPBlueprintReparentTest::RunTest(const FString&)
{
    FModuleManager::Get().LoadModule(TEXT("CommonUI"));
    UClass* Parent=LoadClass<UUserWidget>(nullptr,TEXT("/Script/CommonUI.CommonActivatableWidget"));
    if (!TestNotNull(TEXT("CommonUI activatable parent"),Parent)) return false;
    UPackage* Package=CreatePackage(*(TEXT("/Game/Tests/Reparent_")+FGuid::NewGuid().ToString(EGuidFormats::Digits)));
    auto* BP=FWidgetBlueprintOperationUtils::CreateWidgetBlueprint(Package,TEXT("WBP_Reparent"),BPTYPE_Normal,UUserWidget::StaticClass(),UCanvasPanel::StaticClass(),TEXT("ReparentTest"),false);
    if (!TestNotNull(TEXT("Widget fixture"),BP)) return false;
    auto* Label=BP->WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),TEXT("PurchaseLabel"));
    Label->bIsVariable=true;Label->SetText(FText::FromString(TEXT("Purchase")));
    BP->WidgetVariableNameToGuidMap.Add(Label->GetFName(),FGuid::NewGuid());
    CastChecked<UCanvasPanel>(BP->WidgetTree->RootWidget)->AddChildToCanvas(Label);
    FEdGraphPinType Type;Type.PinCategory=UEdGraphSchema_K2::PC_Int;
    FBlueprintEditorUtils::AddMemberVariable(BP,TEXT("Quantity"),Type,TEXT("7"));
    FKismetEditorUtilities::CompileBlueprint(BP);
    if (BP->UbergraphPages.IsEmpty())
    {
        auto* NewGraph=FBlueprintEditorUtils::CreateNewGraph(BP,TEXT("EventGraph"),UEdGraph::StaticClass(),UEdGraphSchema_K2::StaticClass());
        FBlueprintEditorUtils::AddUbergraphPage(BP,NewGraph);
    }
    UEdGraph* Graph=BP->UbergraphPages[0];
    auto* Getter=NewObject<UK2Node_VariableGet>(Graph);
    Getter->VariableReference.SetSelfMember(TEXT("Quantity"));Graph->AddNode(Getter,false,false);Getter->CreateNewGuid();Getter->AllocateDefaultPins();
    auto* Setter=NewObject<UK2Node_VariableSet>(Graph);
    Setter->VariableReference.SetSelfMember(TEXT("Quantity"));Graph->AddNode(Setter,false,false);Setter->CreateNewGuid();Setter->AllocateDefaultPins();Setter->NodePosX=400;
    TestTrue(TEXT("Fixture data link"),Graph->GetSchema()->TryCreateConnection(Getter->FindPinChecked(TEXT("Quantity")),Setter->FindPinChecked(TEXT("Quantity"))));
    // Compare identities and semantic links, not pointers invalidated by refresh.
    auto GraphSnapshot=[&]()
    {
        TArray<FString> Rows;
        for (const UEdGraphNode* Node:Graph->Nodes) if (Node)
        {
            Rows.Add(Node->NodeGuid.ToString()+FString::Printf(TEXT(":%d:%d"),Node->NodePosX,Node->NodePosY));
            for (const UEdGraphPin* Pin:Node->Pins) if (Pin)
            {
                Rows.Add(Node->NodeGuid.ToString()+Pin->PinName.ToString()+Pin->DefaultValue);
                for (const UEdGraphPin* Linked:Pin->LinkedTo) if (Linked)
                    Rows.Add(Node->NodeGuid.ToString()+Pin->PinName.ToString()+Linked->GetOwningNode()->NodeGuid.ToString()+Linked->PinName.ToString());
            }
        }
        Rows.Sort();return FString::Join(Rows,TEXT("\n"));
    };
    const FString GraphBefore=GraphSnapshot();
    const FString Asset=BP->GetPathName(),NewParent=Parent->GetPathName(),OldParent=BP->ParentClass->GetPathName();
    const FString Tree=UTurboMCPReadToolset::ReadTurboMCPWidgetTree(Asset);
    const FBPVariableDescription Before=BP->NewVariables[0];
    Package->SetDirtyFlag(false);
    const FString Preview=UTurboMCPPreviewToolset::PreviewTurboMCPBlueprintReparent(Asset,NewParent);AddInfo(Preview);
    TestTrue(TEXT("Preview explains compilation"),Preview.Contains(TEXT("preview: reparent Blueprint")) && Preview.Contains(TEXT("compiles")));
    TestFalse(TEXT("Preview clean"),Package->IsDirty());
    TestEqual(TEXT("Preview retains parent"),BP->ParentClass->GetPathName(),OldParent);
    const FString Consent=TEXT("reparent ")+Asset+TEXT(" to ")+NewParent+TEXT(" and compile");
    TestTrue(TEXT("Missing consent refuses"),UTurboMCPEditToolset::EditTurboMCPBlueprintReparent(Asset,NewParent,OldParent,TEXT("")).Contains(TEXT("refused:")));
    TestTrue(TEXT("Stale parent refuses"),UTurboMCPEditToolset::EditTurboMCPBlueprintReparent(Asset,NewParent,TEXT("/Script/Engine.Actor"),Consent).Contains(TEXT("refused:")));
    TestTrue(TEXT("Incompatible family refuses"),UTurboMCPPreviewToolset::PreviewTurboMCPBlueprintReparent(Asset,TEXT("/Script/Engine.Actor")).Contains(TEXT("refused:")));
    TestTrue(TEXT("Self parent refuses"),UTurboMCPPreviewToolset::PreviewTurboMCPBlueprintReparent(Asset,BP->GeneratedClass->GetPathName()).Contains(TEXT("refused:")));
    TestFalse(TEXT("Refusals clean"),Package->IsDirty());
    const FString Result=UTurboMCPEditToolset::EditTurboMCPBlueprintReparent(Asset,NewParent,OldParent,Consent);AddInfo(Result);
    TestTrue(TEXT("Reparent compiled"),Result.StartsWith(TEXT("changed\nreparent result: compiled")));
    TestTrue(TEXT("Actual parent changed"),BP->ParentClass==Parent);
    TestTrue(TEXT("Generated parent changed"),BP->GeneratedClass && BP->GeneratedClass->GetSuperClass()==Parent);
    TestEqual(TEXT("Graph identities, positions, defaults and links retained"),GraphSnapshot(),GraphBefore);
    TestEqual(TEXT("Widget tree retained"),UTurboMCPReadToolset::ReadTurboMCPWidgetTree(Asset),Tree);
    TestEqual(TEXT("Owned default retained"),BP->NewVariables[0].DefaultValue,Before.DefaultValue);
    TestTrue(TEXT("Owned variable identity retained"),BP->NewVariables[0].VarGuid==Before.VarGuid);
    Package->SetDirtyFlag(false);
    TestTrue(TEXT("Same parent no-op"),UTurboMCPEditToolset::EditTurboMCPBlueprintReparent(Asset,NewParent,NewParent,Consent).StartsWith(TEXT("unchanged")));
    TestFalse(TEXT("No-op clean"),Package->IsDirty());
    for (const TCHAR* Tool:{TEXT("PreviewTurboMCPBlueprintReparent"),TEXT("EditTurboMCPBlueprintReparent")})
    {
        const FString Help=UTurboMCPReadToolset::ReadTurboMCPToolHelp(FString(Tool).StartsWith(TEXT("Preview"))?TEXT("TurboMCP.TurboMCPPreviewToolset"):TEXT("TurboMCP.TurboMCPEditToolset"),Tool);AddInfo(Help);
        TestTrue(TEXT("Help discloses compile and reinstancing"),Help.Contains(TEXT("compil")) && Help.Contains(TEXT("reinstancing")));
    }
    return true;
}
#endif
