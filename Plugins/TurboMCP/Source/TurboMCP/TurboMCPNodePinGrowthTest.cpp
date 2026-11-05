// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
#include "Tools/TurboMCPToolsets.h"
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Editor.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "GameFramework/Actor.h"
#include "Components/Widget.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_MakeArray.h"
#include "K2Node_ExecutionSequence.h"
#include "K2Node_CommutativeAssociativeBinaryOperator.h"
#include "K2Node_VariableGet.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "TurboMCPNodeId.h"
#include "UObject/Package.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTurboMCPNodePinGrowthTest,"TurboMCP.GraphAppend.NodePinGrowth",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTurboMCPNodePinGrowthTest::RunTest(const FString&)
{
    UPackage* Package=CreatePackage(*(TEXT("/Game/Tests/PinGrowth_")+FGuid::NewGuid().ToString(EGuidFormats::Digits)));
    UBlueprint* BP=FKismetEditorUtilities::CreateBlueprint(AActor::StaticClass(),Package,TEXT("PinGrowth"),BPTYPE_Normal,UBlueprint::StaticClass(),UBlueprintGeneratedClass::StaticClass());
    if (!TestNotNull(TEXT("Fixture Blueprint"),BP)) return false;
    FEdGraphPinType WidgetType;WidgetType.PinCategory=UEdGraphSchema_K2::PC_Object;WidgetType.PinSubCategoryObject=UWidget::StaticClass();
    for (const TCHAR* Name:{TEXT("HeaderA"),TEXT("HeaderB"),TEXT("HeaderC")}) FBlueprintEditorUtils::AddMemberVariable(BP,Name,WidgetType);
    FKismetEditorUtilities::CompileBlueprint(BP);
    UEdGraph* Graph=BP->UbergraphPages[0];
    auto Add=[&](UEdGraphNode* Node){ Graph->AddNode(Node,false,false);Node->CreateNewGuid();Node->AllocateDefaultPins(); };
    auto* Array=NewObject<UK2Node_MakeArray>(Graph,NAME_None,RF_Transactional);Add(Array);
    TArray<UEdGraphPin*> Sources;
    for (const TCHAR* Name:{TEXT("HeaderA"),TEXT("HeaderB"),TEXT("HeaderC")})
    {
        auto* Getter=NewObject<UK2Node_VariableGet>(Graph,NAME_None,RF_Transactional);Getter->VariableReference.SetSelfMember(Name);Add(Getter);
        UEdGraphPin* Pin=Getter->FindPin(Name,EGPD_Output);
        if (!TestNotNull(TEXT("Widget getter output"),Pin)) return false;
        Sources.Add(Pin);
    }
    const UEdGraphSchema* Schema=Graph->GetSchema();
    UEdGraphPin* First=Array->FindPin(TEXT("[0]"));
    if (!TestNotNull(TEXT("Initial array input"),First)) return false;
    TestTrue(TEXT("Initial widget source connected"),Schema->TryCreateConnection(Sources[0],First));
    const FString Asset=BP->GetPathName(), GraphName=Graph->GetName();
    const FString ArrayId=TurboMCPNodeId::MakeForNode(Array,true);
    auto Edit=[&](const FString& Id,const TCHAR* Count){const FString R=UTurboMCPEditToolset::EditTurboMCPNodePinGrowth(Asset,GraphName,Id,Count);AddInfo(R);return R;};
    Package->SetDirtyFlag(false);
    const int32 Nodes=Graph->Nodes.Num();
    const FString Preview=UTurboMCPPreviewToolset::PreviewTurboMCPNodePinGrowth(Asset,GraphName,ArrayId,TEXT("3"));AddInfo(Preview);
    TestTrue(TEXT("Preview succeeds"),Preview.Contains(TEXT("preview (transient graph)")) && Preview.Contains(TEXT("[1], [2]")));
    TestFalse(TEXT("Preview clean"),Package->IsDirty());TestEqual(TEXT("Preview leaves count"),Array->NumInputs,1);
    TestTrue(TEXT("Preview retains original link"),First->LinkedTo.Contains(Sources[0]));
    TestTrue(TEXT("Grow array succeeds"),Edit(ArrayId,TEXT("3")).StartsWith(TEXT("changed\n")));
    TestEqual(TEXT("Three array inputs"),Array->NumInputs,3);TestEqual(TEXT("No new nodes"),Graph->Nodes.Num(),Nodes);
    TestTrue(TEXT("Original input identity retained"),First==Array->FindPin(TEXT("[0]")));
    for (int32 Index=0;Index<3;++Index)
    {
        UEdGraphPin* Pin=Array->FindPin(*FString::Printf(TEXT("[%d]"),Index));
        if (!TestNotNull(TEXT("Array input exists"),Pin)) return false;
        if (Index) TestTrue(TEXT("Additional widget source connected"),Schema->TryCreateConnection(Sources[Index],Pin));
        TestTrue(TEXT("Correct ordered widget source"),Pin->LinkedTo.Num()==1 && Pin->LinkedTo[0]==Sources[Index]);
        TestTrue(TEXT("Widget type propagated"),Pin->PinType.PinSubCategoryObject==UWidget::StaticClass());
    }
    Package->SetDirtyFlag(false);
    TestTrue(TEXT("Repeated growth is no-op"),Edit(ArrayId,TEXT("3")).StartsWith(TEXT("unchanged\nThe node already")));
    TestFalse(TEXT("No-op clean"),Package->IsDirty());
    for (const TCHAR* Count:{TEXT("2"),TEXT("0"),TEXT("33"),TEXT("3.5"),TEXT("-1")}) TestTrue(TEXT("Bad target refuses"),Edit(ArrayId,Count).Contains(TEXT("refused:")));
    TestFalse(TEXT("Refusals clean"),Package->IsDirty());
    // Disconnect test-only added wires before undoing the tool-owned transaction.
    for (int32 Index=1;Index<3;++Index) Array->FindPin(*FString::Printf(TEXT("[%d]"),Index))->BreakAllPinLinks();
    TestTrue(TEXT("Undo growth"),GEditor->UndoTransaction(false));
    TestEqual(TEXT("Undo restores initial array count"),Array->NumInputs,1);
    TestTrue(TEXT("Undo preserves initial source"),Array->FindPin(TEXT("[0]"))->LinkedTo.Contains(Sources[0]));
    auto* Sequence=NewObject<UK2Node_ExecutionSequence>(Graph,NAME_None,RF_Transactional);Add(Sequence);
    TestTrue(TEXT("Sequence third branch"),Edit(TurboMCPNodeId::MakeForNode(Sequence,true),TEXT("3")).StartsWith(TEXT("changed\n")));
    int32 Branches=0;for (const UEdGraphPin* Pin:Sequence->Pins) if (Pin && Pin->Direction==EGPD_Output) ++Branches;
    TestEqual(TEXT("Three execution branches"),Branches,3);
    auto* Operator=NewObject<UK2Node_CommutativeAssociativeBinaryOperator>(Graph,NAME_None,RF_Transactional);
    Operator->SetFromFunction(UKismetMathLibrary::StaticClass()->FindFunctionByName(TEXT("BooleanAND")));Add(Operator);
    TestTrue(TEXT("Operator third operand"),Edit(TurboMCPNodeId::MakeForNode(Operator,true),TEXT("3")).StartsWith(TEXT("changed\n")));
    TestEqual(TEXT("Operator count"),Operator->GetNumberOfAdditionalInputs(),1);
    TestTrue(TEXT("Unsupported getter refuses"),Edit(TurboMCPNodeId::MakeForNode(Sources[0]->GetOwningNode(),true),TEXT("3")).Contains(TEXT("only Make Array")));
    for (const TCHAR* Name:{TEXT("PreviewTurboMCPNodePinGrowth"),TEXT("EditTurboMCPNodePinGrowth")})
    {
        const FString Help=UTurboMCPReadToolset::ReadTurboMCPToolHelp(FString(Name).StartsWith(TEXT("Preview"))?TEXT("TurboMCP.TurboMCPPreviewToolset"):TEXT("TurboMCP.TurboMCPEditToolset"),Name);
        TestTrue(TEXT("Help explains desired count and no shrink"),Help.Contains(TEXT("Desired total")) && Help.Contains(TEXT("does not shrink")));
    }
    return true;
}
#endif
