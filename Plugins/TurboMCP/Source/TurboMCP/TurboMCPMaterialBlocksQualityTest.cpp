// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
#include "TurboMCPMaterialBlocks.h"
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionAdd.h"
#include "Materials/MaterialExpressionComment.h"
#include "Materials/MaterialExpressionNamedReroute.h"
#include "UObject/Package.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTurboMCPMaterialBlocksQualityTest,
    "TurboMCP.Material.BlocksQuality", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTurboMCPMaterialBlocksQualityTest::RunTest(const FString& Parameters)
{
    UPackage* Package = CreatePackage(*(TEXT("/Game/Tests/BlocksQuality_") + FGuid::NewGuid().ToString(EGuidFormats::Digits)));
    ON_SCOPE_EXIT { Package->SetDirtyFlag(false); };
    UMaterial* Material = NewObject<UMaterial>(Package, TEXT("M_Blocks"), RF_Public | RF_Standalone | RF_Transactional);
    auto* Value = NewObject<UMaterialExpressionVectorParameter>(Material, NAME_None, RF_Transactional);
    Value->MaterialExpressionGuid = FGuid::NewGuid();
    Value->ParameterName = TEXT("Channels");
    Value->DefaultValue = FLinearColor(0.2f, 0.7f, 0.9f);
    Material->GetExpressionCollection().AddExpression(Value);
    TArray<UMaterialExpressionAdd*> Readers;
    for (int32 Index = 0; Index < 4; ++Index)
    {
        auto* Box = NewObject<UMaterialExpressionComment>(Material, NAME_None, RF_Transactional);
        Box->MaterialExpressionEditorX = 400 + (Index / 2) * 600;
        Box->MaterialExpressionEditorY = 0; Box->SizeX = 500; Box->SizeY = 500;
        Box->Text = FString::Printf(TEXT("Consumer %d"), Index);
        Material->GetExpressionCollection().AddComment(Box);
        auto* Reader = NewObject<UMaterialExpressionAdd>(Material, NAME_None, RF_Transactional);
        Reader->MaterialExpressionGuid = FGuid::NewGuid();
        Reader->MaterialExpressionEditorX = Box->MaterialExpressionEditorX + 50;
        Reader->MaterialExpressionEditorY = 100;
        // Distinct outputs of one expression are distinct values, not one shared vector.
        Reader->A.Connect((Index % 2) + 1, Value);
        Reader->ConstB = 0.0f;
        Material->GetExpressionCollection().AddExpression(Reader);
        Readers.Add(Reader);
    }
    Material->GetEditorOnlyData()->BaseColor.Connect(0, Readers[0]);
    Material->GetEditorOnlyData()->Roughness.Connect(0, Readers[1]);
    Package->SetDirtyFlag(false);
    const FGuid BeforeState = Material->StateId;
    FTurboMCPMaterialBlocks::FPlan Plan;
    FString Error;
    if (!TestTrue(TEXT("Plan succeeds"), FTurboMCPMaterialBlocks::Plan(Material->GetPathName(), Plan, Error)))
    { AddError(Error); return false; }
    TestFalse(TEXT("Preview keeps package clean"), Package->IsDirty());
    TestTrue(TEXT("Preview preserves material state ID"), Material->StateId == BeforeState);
    FString Rollback;
    const FString Result = FTurboMCPMaterialBlocks::Apply(Plan, Rollback);
    AddInfo(Result + Rollback);
    TestFalse(TEXT("Edit does not fail"), Result.Contains(TEXT("FAILED")) || Result.StartsWith(TEXT("error:")));
    for (int32 Index = 0; Index < Readers.Num(); ++Index)
    {
        const FExpressionInput* Connection = &Readers[Index]->A;
        if (const auto* Usage = Cast<UMaterialExpressionNamedRerouteUsage>(Connection->Expression))
        {
            if (!TestNotNull(TEXT("Usage declaration"), Usage->Declaration.Get())) continue;
            Connection = &Usage->Declaration->Input;
        }
        TestTrue(TEXT("Original source preserved"), Connection->Expression == Value);
        TestEqual(TEXT("Original source output preserved"), Connection->OutputIndex, (Index % 2) + 1);
    }
    return true;
}
#endif
