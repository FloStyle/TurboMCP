// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
#include "TurboMCPBlueprintRead.h"
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "GameFramework/Actor.h"
#include "Kismet/KismetArrayLibrary.h"
#include "Kismet/BlueprintMapLibrary.h"
#include "Kismet/BlueprintSetLibrary.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "BlueprintEditorLibrary.h"
#include "K2Node_CallFunction.h"
#include "K2Node_VariableGet.h"
#include "K2Node_Knot.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "UObject/Package.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTurboMCPReferenceWritesTest, "TurboMCP.Read.ReferenceWrites", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTurboMCPReferenceWritesTest::RunTest(const FString& Parameters)
{
    UPackage* Package = CreatePackage(*(TEXT("/Game/Tests/ReferenceWrites_") + FGuid::NewGuid().ToString(EGuidFormats::Digits)));
    ON_SCOPE_EXIT { Package->SetDirtyFlag(false); };
    UBlueprint* BP = FKismetEditorUtilities::CreateBlueprint(AActor::StaticClass(), Package, TEXT("BP_ReferenceWrites"), BPTYPE_Normal, UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass());
    if (!BP) return false;
    auto CheckOperation = [&](UClass* Library, const TCHAR* FunctionName, const TCHAR* TargetName, EPinContainerType Container, bool bMutation, bool bReroute)
    {
        const FName MemberName(*FString::Printf(TEXT("Value_%s"), FunctionName));
        FEdGraphPinType Type; Type.PinCategory = UEdGraphSchema_K2::PC_Int; Type.ContainerType = Container;
        if (Container == EPinContainerType::Map) Type.PinValueType.TerminalCategory = UEdGraphSchema_K2::PC_Int;
        FBlueprintEditorUtils::AddMemberVariable(BP, MemberName, Type);
        UEdGraph* Graph = UBlueprintEditorLibrary::AddFunctionGraph(BP, FunctionName);
        UK2Node_VariableGet* Getter = NewObject<UK2Node_VariableGet>(Graph);
        Getter->VariableReference.SetSelfMember(MemberName); Getter->CreateNewGuid(); Graph->AddNode(Getter, false, false); Getter->AllocateDefaultPins();
        UK2Node_CallFunction* Call = NewObject<UK2Node_CallFunction>(Graph);
        Call->SetFromFunction(Library->FindFunctionByName(FunctionName)); Call->CreateNewGuid(); Graph->AddNode(Call, false, false); Call->AllocateDefaultPins();
        UEdGraphPin* Source = Getter->FindPin(MemberName); UEdGraphPin* Target = Call->FindPin(TargetName);
        if (!Source || !Target) { AddError(TEXT("Fixture pins missing")); return; }
        if (bReroute)
        {
            UK2Node_Knot* Knot = NewObject<UK2Node_Knot>(Graph); Knot->CreateNewGuid(); Graph->AddNode(Knot, false, false); Knot->AllocateDefaultPins();
            Source->MakeLinkTo(Knot->GetInputPin()); Knot->GetOutputPin()->MakeLinkTo(Target);
        }
        else Source->MakeLinkTo(Target);
        const int32 NodesBefore = Graph->Nodes.Num();
        Package->SetDirtyFlag(false);
        FTurboMCPBlueprintRead::FRequest Request; Request.Detail = FTurboMCPBlueprintRead::EDetail::Signatures; Request.GraphFilter.Add(Graph->GetName());
        const auto Result = FTurboMCPBlueprintRead::Read(BP, Request);
        TestEqual(FString(FunctionName) + TEXT(" mutation classification"), Result.Text.Contains(TEXT("container mutation sites:")), bMutation);
        TestFalse(TEXT("Stock read is not an unknown write"), Result.Text.Contains(TEXT("possible reference writes:")));
        TestEqual(TEXT("No graph mutation"), Graph->Nodes.Num(), NodesBefore);
        TestFalse(TEXT("Read does not dirty package"), Package->IsDirty());
        if (bMutation) TestFalse(TEXT("No contradictory no-write claim"), Result.Text.Contains(TEXT("no direct write found")));
    };
    CheckOperation(UKismetArrayLibrary::StaticClass(), TEXT("Array_Add"), TEXT("TargetArray"), EPinContainerType::Array, true, false);
    CheckOperation(UKismetArrayLibrary::StaticClass(), TEXT("Array_Remove"), TEXT("TargetArray"), EPinContainerType::Array, true, true);
    CheckOperation(UKismetArrayLibrary::StaticClass(), TEXT("Array_Length"), TEXT("TargetArray"), EPinContainerType::Array, false, false);
    CheckOperation(UBlueprintMapLibrary::StaticClass(), TEXT("Map_Add"), TEXT("TargetMap"), EPinContainerType::Map, true, false);
    CheckOperation(UBlueprintMapLibrary::StaticClass(), TEXT("Map_Length"), TEXT("TargetMap"), EPinContainerType::Map, false, false);
    CheckOperation(UBlueprintSetLibrary::StaticClass(), TEXT("Set_Remove"), TEXT("TargetSet"), EPinContainerType::Set, true, false);
    CheckOperation(UBlueprintSetLibrary::StaticClass(), TEXT("Set_Length"), TEXT("TargetSet"), EPinContainerType::Set, false, false);
    return !HasAnyErrors();
}
#endif
