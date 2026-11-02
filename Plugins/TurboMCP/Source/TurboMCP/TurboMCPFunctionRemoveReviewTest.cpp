// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Tools/TurboMCPToolsets.h"
#include "Misc/AutomationTest.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "GameFramework/Actor.h"
#include "EdGraphSchema_K2.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "K2Node_CallFunction.h"
#include "ScopedTransaction.h"
#include "UObject/Package.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTurboMCPFunctionRemoveReviewTest,
    "TurboMCP.Function.RemoveReview", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTurboMCPFunctionRemoveReviewTest::RunTest(const FString& Parameters)
{
    const FString PackageName=TEXT("/Game/Tests/FunctionRemoveReview_")+FGuid::NewGuid().ToString(EGuidFormats::Digits);
    UPackage* Package=CreatePackage(*PackageName);
    UBlueprint* BP=FKismetEditorUtilities::CreateBlueprint(AActor::StaticClass(),Package,TEXT("FunctionRemoveReview"),BPTYPE_Normal,UBlueprint::StaticClass(),UBlueprintGeneratedClass::StaticClass());
    if (!BP) return false;
    auto Add=[&](const TCHAR* Name)
    {
        UEdGraph* Graph=FBlueprintEditorUtils::CreateNewGraph(BP,Name,UEdGraph::StaticClass(),UEdGraphSchema_K2::StaticClass());
        FBlueprintEditorUtils::AddFunctionGraph<UFunction>(BP,Graph,true,nullptr);
        return Graph;
    };
    UEdGraph* Removed=Add(TEXT("ObsoleteCalculation"));
    UEdGraph* Kept=Add(TEXT("KeepCalculation"));
    FKismetEditorUtilities::CompileBlueprint(BP);
    const FString Asset=BP->GetPathName();
    Package->SetDirtyFlag(false);
    auto Remove=[&](const FString& Name){return UTurboMCPEditToolset::EditTurboMCPFunctionRemove(Asset,Name);};
    TestTrue(TEXT("Empty name error"),Remove(TEXT(" ")).StartsWith(TEXT("error:")));
    const FString Missing=Remove(TEXT("MissingFunction")); AddInfo(Missing);
    TestTrue(TEXT("Missing unchanged and lists functions"),Missing.StartsWith(TEXT("unchanged")) && Missing.Contains(TEXT("KeepCalculation")));
    TestFalse(TEXT("Refusals preserve clean package"),Package->IsDirty());
    {
        FScopedTransaction Transaction(NSLOCTEXT("TurboMCPTest","RemoveGuard","Test removal guard"));
        TestTrue(TEXT("Active transaction refused"),Remove(TEXT("ObsoleteCalculation")).StartsWith(TEXT("unchanged")));
        TestTrue(TEXT("Transaction refusal preserves graph"),BP->FunctionGraphs.Contains(Removed));
        Transaction.Cancel();
    }
    const int32 KeptNodes=Kept->Nodes.Num();
    const FString Result=Remove(TEXT("  obsoletecalculation  ")); AddInfo(Result);
    TestTrue(TEXT("Case and whitespace normalized"),Result.StartsWith(TEXT("changed")));
    TestFalse(TEXT("Requested graph removed"),BP->FunctionGraphs.Contains(Removed));
    TestTrue(TEXT("Other function retained"),BP->FunctionGraphs.Contains(Kept));
    TestEqual(TEXT("Other function nodes retained"),Kept->Nodes.Num(),KeptNodes);
    TestTrue(TEXT("Removal dirty"),Package->IsDirty());
    TestTrue(TEXT("Output discloses no caller repair"),Result.Contains(TEXT("not repaired")));
    TestTrue(TEXT("Output discloses no dedicated undo"),Result.Contains(TEXT("dedicated undo")));
    Package->SetDirtyFlag(false);
    TestTrue(TEXT("Repeated remove unchanged"),Remove(TEXT("ObsoleteCalculation")).StartsWith(TEXT("unchanged")));
    TestFalse(TEXT("Repeated remove stays clean"),Package->IsDirty());
    FKismetEditorUtilities::CompileBlueprint(BP);
    TestTrue(TEXT("Remaining Blueprint compiles"),BP->Status!=BS_Error);
    return true;
}
#endif
