// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
#include "TurboMCPMaterialLabelsPromote.h"
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionAdd.h"
#include "Materials/MaterialExpressionNamedReroute.h"
#include "UObject/Package.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTurboMCPMaterialLabelPromoteQualityTest,
    "TurboMCP.Material.LabelPromoteQuality", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTurboMCPMaterialLabelPromoteQualityTest::RunTest(const FString& Parameters)
{
    UPackage* Package = CreatePackage(*(TEXT("/Game/Tests/LabelQuality_") + FGuid::NewGuid().ToString(EGuidFormats::Digits)));
    ON_SCOPE_EXIT { Package->SetDirtyFlag(false); };
    UMaterial* Material = NewObject<UMaterial>(Package, TEXT("M_Labels"), RF_Public | RF_Standalone | RF_Transactional);
    auto* Value = NewObject<UMaterialExpressionVectorParameter>(Material, NAME_None, RF_Transactional);
    Value->MaterialExpressionGuid = FGuid::NewGuid();
    Value->ParameterName = TEXT("Channels");
    Value->DefaultValue = FLinearColor(0.2f, 0.7f, 0.9f);
    Material->GetExpressionCollection().AddExpression(Value);
    TArray<UMaterialExpressionAdd*> Readers;
    for (int32 Index = 0; Index < 4; ++Index)
    {
        auto* Reader = NewObject<UMaterialExpressionAdd>(Material, NAME_None, RF_Transactional);
        Reader->MaterialExpressionGuid = FGuid::NewGuid();
        Reader->A.Connect(Index % 2 + 1, Value);
        Reader->ConstB = 0.0f;
        Reader->MaterialExpressionEditorX = 600;
        Reader->MaterialExpressionEditorY = Index * 240;
        Material->GetExpressionCollection().AddExpression(Reader);
        Readers.Add(Reader);
    }
    auto* Existing = NewObject<UMaterialExpressionNamedRerouteDeclaration>(Material, NAME_None, RF_Transactional);
    Existing->MaterialExpressionGuid = FGuid::NewGuid();
    Existing->VariableGuid = FGuid::NewGuid();
    Existing->Name = TEXT("ExistingRed");
    Existing->Input = Readers[0]->A;
    Material->GetExpressionCollection().AddExpression(Existing);
    Material->GetEditorOnlyData()->BaseColor.Connect(0, Readers[0]);
    Material->GetEditorOnlyData()->Roughness.Connect(0, Readers[1]);
    Package->SetDirtyFlag(false);
    const FGuid BeforeState = Material->StateId;
    FTurboMCPMaterialLabelsPromote::FPlan Plan;
    FString Error;
    if (!TestTrue(TEXT("Plan shared channels"), FTurboMCPMaterialLabelsPromote::Plan(Material->GetPathName(), Plan, Error))) return false;
    TestFalse(TEXT("Preview does not dirty the package"), Package->IsDirty());
    TestEqual(TEXT("Preview preserves compilation state"), Material->StateId, BeforeState);
    TestEqual(TEXT("Red and green have separate promotions"), Plan.Promotions.Num(), 2);
    const FString Result = FTurboMCPMaterialLabelsPromote::Apply(Plan);
    TestTrue(TEXT("Apply succeeds"), Result.StartsWith(TEXT("changed:")));
    for (int32 Index = 0; Index < Readers.Num(); ++Index)
    {
        auto* Usage = Cast<UMaterialExpressionNamedRerouteUsage>(Readers[Index]->A.Expression);
        if (!TestNotNull(TEXT("Reader uses a label"), Usage)) continue;
        auto* Declaration = Usage->Declaration.Get();
        if (!TestNotNull(TEXT("Usage has declaration"), Declaration)) continue;
        TestEqual(TEXT("Original output preserved"), Declaration->Input.OutputIndex, Index % 2 + 1);
        TestTrue(TEXT("Original source preserved"), Declaration->Input.Expression == Value);
        if (Index % 2 == 0) TestTrue(TEXT("Existing red declaration reused"), Declaration == Existing);
    }
    TestEqual(TEXT("Existing label name preserved"), Existing->GetEditableName(), FString(TEXT("ExistingRed")));
    FTurboMCPMaterialLabelsPromote::FPlan Repeat;
    TestTrue(TEXT("Repeat plan succeeds"), FTurboMCPMaterialLabelsPromote::Plan(Material->GetPathName(), Repeat, Error));
    TestTrue(TEXT("Repeat is unchanged"), FTurboMCPMaterialLabelsPromote::Apply(Repeat).StartsWith(TEXT("unchanged:")));
    return true;
}
#endif
