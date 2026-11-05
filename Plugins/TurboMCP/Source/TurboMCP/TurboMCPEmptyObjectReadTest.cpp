// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
#include "Tools/TurboMCPToolsets.h"
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"
#include "Subsystems/SubsystemBlueprintLibrary.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "K2Node_CallFunction.h"
#include "K2Node_VariableGet.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "BlueprintEditorLibrary.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "UObject/Package.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTurboMCPEmptyObjectReadTest, "TurboMCP.GraphAppend.EmptyObjectRead", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTurboMCPEmptyObjectReadTest::RunTest(const FString& Parameters)
{
    UPackage* Package = CreatePackage(*(TEXT("/Game/Tests/EmptyObject_") + FGuid::NewGuid().ToString(EGuidFormats::Digits)));
    ON_SCOPE_EXIT { Package->SetDirtyFlag(false); };
    UBlueprint* BP = FKismetEditorUtilities::CreateBlueprint(AActor::StaticClass(), Package, TEXT("BP_EmptyObject"), BPTYPE_Normal, UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass());
    if (!BP) { AddError(TEXT("Fixture creation failed.")); return false; }
    UEdGraph* Graph = UBlueprintEditorLibrary::AddFunctionGraph(BP, TEXT("ObjectInputs"));
    auto AddCall = [&](const TCHAR* Name)
    {
        UK2Node_CallFunction* Node = NewObject<UK2Node_CallFunction>(Graph);
        Node->SetFromFunction(USubsystemBlueprintLibrary::StaticClass()->FindFunctionByName(Name));
        Node->CreateNewGuid(); Graph->AddNode(Node, false, false); Node->AllocateDefaultPins(); return Node;
    };
    UK2Node_CallFunction* Player = AddCall(TEXT("GetLocalPlayerSubSystemFromPlayerController"));
    UK2Node_CallFunction* Context = AddCall(TEXT("GetLocalPlayerSubsystem"));
    TArray<UK2Node_FunctionEntry*> Entries; Graph->GetNodesOfClass(Entries);
    if (Entries.Num() != 1) { AddError(TEXT("Function entry missing.")); return false; }
    UK2Node_FunctionResult* Result = FBlueprintEditorUtils::FindOrCreateFunctionResultNode(Entries[0]);
    FEdGraphPinType SubsystemType = Player->GetReturnValuePin()->PinType;
    UEdGraphPin* PlayerResult = Result->CreateUserDefinedPin(TEXT("PlayerSubsystem"), SubsystemType, EGPD_Input, false);
    UEdGraphPin* ContextResult = Result->CreateUserDefinedPin(TEXT("ContextSubsystem"), SubsystemType, EGPD_Input, false);
    if (!PlayerResult || !ContextResult) { AddError(TEXT("Function outputs missing.")); return false; }
    Player->GetReturnValuePin()->MakeLinkTo(PlayerResult);
    Context->GetReturnValuePin()->MakeLinkTo(ContextResult);
    Entries[0]->FindPin(UEdGraphSchema_K2::PN_Then)->MakeLinkTo(Result->FindPin(UEdGraphSchema_K2::PN_Execute));
    const int32 OriginalNodes = Graph->Nodes.Num();
    Package->SetDirtyFlag(false);
    for (const FString& Detail : {FString(TEXT("bodies")), FString(TEXT("full"))})
    {
        const FString Read = UTurboMCPReadToolset::ReadTurboMCPGraphLoad(BP->GetPathName(), Graph->GetName(), TEXT("read"), Detail, 0, 24000, TEXT(""), TEXT(""));
        AddInfo(Read);
        TestTrue(TEXT("PlayerController absence visible"), Read.Contains(TEXT("object input PlayerController: unconnected with no stored default")));
        TestTrue(TEXT("Known context fallback distinguished"), Read.Contains(TEXT("object input ContextObject: unconnected; function metadata supplies self/context")));
        TestFalse(TEXT("Reader does not dirty fixture"), Package->IsDirty());
        TestEqual(TEXT("Reader preserves nodes"), Graph->Nodes.Num(), OriginalNodes);
    }
    FEdGraphPinType Type; Type.PinCategory = UEdGraphSchema_K2::PC_Object; Type.PinSubCategoryObject = APlayerController::StaticClass();
    FBlueprintEditorUtils::AddMemberVariable(BP, TEXT("Controller"), Type);
    UK2Node_VariableGet* Getter = NewObject<UK2Node_VariableGet>(Graph); Getter->VariableReference.SetSelfMember(TEXT("Controller"));
    Getter->CreateNewGuid(); Graph->AddNode(Getter, false, false); Getter->AllocateDefaultPins();
    UEdGraphPin* Output = Getter->FindPin(TEXT("Controller")); UEdGraphPin* Input = Player->FindPin(TEXT("PlayerController"));
    if (!Output || !Input) { AddError(TEXT("Fixture pins unavailable.")); return false; }
    Output->MakeLinkTo(Input); Package->SetDirtyFlag(false);
    const FString Connected = UTurboMCPReadToolset::ReadTurboMCPGraphLoad(BP->GetPathName(), Graph->GetName(), TEXT("read"), TEXT("bodies"), 0, 24000, TEXT(""), TEXT(""));
    TestFalse(TEXT("Connected input not described as empty"), Connected.Contains(TEXT("object input PlayerController: unconnected")));
    TestTrue(TEXT("Read preserves link"), Input->LinkedTo.Contains(Output));
    TestFalse(TEXT("Connected read preserves clean state"), Package->IsDirty());
    return !HasAnyErrors();
}
#endif
