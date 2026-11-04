// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
#include "TurboMCPMaterialBlocks.h"
#include "TurboMCPMaterial.h"
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionAdd.h"
#include "Materials/MaterialExpressionReroute.h"
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
    // A separate in-block chain carries the green output to an expression and a material output.
    auto* ChainMaterial = NewObject<UMaterial>(Package, TEXT("M_Chain"), RF_Public | RF_Standalone | RF_Transactional);
    auto* Box = NewObject<UMaterialExpressionComment>(ChainMaterial, NAME_None, RF_Transactional);
    Box->MaterialExpressionEditorX = -100; Box->MaterialExpressionEditorY = -100;
    Box->SizeX = 1200; Box->SizeY = 600;
    ChainMaterial->GetExpressionCollection().AddComment(Box);
    auto* ChainValue = NewObject<UMaterialExpressionVectorParameter>(ChainMaterial, NAME_None, RF_Transactional);
    ChainValue->MaterialExpressionGuid = FGuid::NewGuid();
    ChainValue->ParameterName = TEXT("ChainChannels");
    ChainValue->DefaultValue = FLinearColor(0.2f, 0.7f, 0.9f);
    ChainMaterial->GetExpressionCollection().AddExpression(ChainValue);
    auto* First = NewObject<UMaterialExpressionReroute>(ChainMaterial, NAME_None, RF_Transactional);
    First->MaterialExpressionGuid = FGuid::NewGuid(); First->Input.Connect(2, ChainValue);
    ChainMaterial->GetExpressionCollection().AddExpression(First);
    auto* Second = NewObject<UMaterialExpressionReroute>(ChainMaterial, NAME_None, RF_Transactional);
    Second->MaterialExpressionGuid = FGuid::NewGuid(); Second->Input.Connect(0, First);
    ChainMaterial->GetExpressionCollection().AddExpression(Second);
    auto* Consumer = NewObject<UMaterialExpressionAdd>(ChainMaterial, NAME_None, RF_Transactional);
    Consumer->MaterialExpressionGuid = FGuid::NewGuid(); Consumer->A.Connect(0, Second);
    ChainMaterial->GetExpressionCollection().AddExpression(Consumer);
    ChainMaterial->GetEditorOnlyData()->BaseColor.Connect(0, Consumer);
    ChainMaterial->GetEditorOnlyData()->Roughness.Connect(0, First);
    FTurboMCPMaterialBlocks::FPlan ChainPlan;
    if (!TestTrue(TEXT("Chain plan succeeds"), FTurboMCPMaterialBlocks::Plan(ChainMaterial->GetPathName(), ChainPlan, Error))) return false;
    TestEqual(TEXT("Only the internal reroute is removed"), ChainPlan.InBlockReroutes.Num(), 1);
    const FString ChainResult = FTurboMCPMaterialBlocks::Apply(ChainPlan, Rollback);
    AddInfo(ChainResult + Rollback);
    TestTrue(TEXT("Chain cleanup changed"), ChainResult.StartsWith(TEXT("changed:")));
    TestTrue(TEXT("Chain final source preserved"), Consumer->A.Expression == ChainValue);
    TestEqual(TEXT("Chain selected output preserved"), Consumer->A.OutputIndex, 2);
    TestEqual(TEXT("Chain green mask preserved"), Consumer->A.MaskG, First->Input.MaskG);
    TestEqual(TEXT("Chain mask enable preserved"), Consumer->A.Mask, First->Input.Mask);
    TestTrue(TEXT("Material output stays connected"), ChainMaterial->GetEditorOnlyData()->Roughness.Expression == First);
    // A redundant label usage may feed a material property as well as expression inputs.
    auto* Declaration = NewObject<UMaterialExpressionNamedRerouteDeclaration>(ChainMaterial, NAME_None, RF_Transactional);
    Declaration->MaterialExpressionGuid = FGuid::NewGuid();
    Declaration->Input.Connect(2, ChainValue); Declaration->SetEditableName(TEXT("GreenChannel"));
    ChainMaterial->GetExpressionCollection().AddExpression(Declaration);
    auto* UsageA = NewObject<UMaterialExpressionNamedRerouteUsage>(ChainMaterial, NAME_None, RF_Transactional);
    auto* UsageB = NewObject<UMaterialExpressionNamedRerouteUsage>(ChainMaterial, NAME_None, RF_Transactional);
    for (auto* Usage : {UsageA, UsageB})
    {
        Usage->MaterialExpressionGuid = FGuid::NewGuid();
        Usage->Declaration = Declaration; Usage->DeclarationGuid = Declaration->VariableGuid;
        ChainMaterial->GetExpressionCollection().AddExpression(Usage);
    }
    auto* RemovedUsage = FTurboMCPMaterial::KeyOf(UsageA) < FTurboMCPMaterial::KeyOf(UsageB) ? UsageB : UsageA;
    auto* KeptUsage = RemovedUsage == UsageA ? UsageB : UsageA;
    ChainMaterial->GetEditorOnlyData()->Roughness.Connect(0, RemovedUsage);
    Consumer->A.Connect(0, RemovedUsage);
    FTurboMCPMaterialBlocks::FPlan MergePlan;
    if (!TestTrue(TEXT("Output merge plan succeeds"), FTurboMCPMaterialBlocks::Plan(ChainMaterial->GetPathName(), MergePlan, Error))) return false;
    TestEqual(TEXT("One usage merge planned"), MergePlan.Merges.Num(), 1);
    const FString MergeResult = FTurboMCPMaterialBlocks::Apply(MergePlan, Rollback);
    AddInfo(MergeResult + Rollback);
    TestTrue(TEXT("Usage merge changed"), MergeResult.StartsWith(TEXT("changed:")));
    TestTrue(TEXT("Material output retargeted to surviving usage"), ChainMaterial->GetEditorOnlyData()->Roughness.Expression == KeptUsage);
    TestTrue(TEXT("Expression retargeted to surviving usage"), Consumer->A.Expression == KeptUsage);
    return true;
}
#endif
