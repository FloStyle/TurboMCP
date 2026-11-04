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
#include "Materials/MaterialExpressionAppendVector.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialRenderProxy.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Slate/WidgetRenderer.h"
#include "Widgets/Images/SImage.h"
#include "Styling/SlateBrush.h"
#include "ShaderCompiler.h"
#include "ImageUtils.h"
#include "Serialization/BufferArchive.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Tools/TurboMCPToolsets.h"

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
        Reader->MaterialExpressionEditorY = 100 + (Index % 2) * 200;
        // Distinct outputs of one expression are distinct values, not one shared vector.
        Reader->A.Connect((Index % 2) + 1, Value);
        Reader->ConstB = 0.0f;
        Material->GetExpressionCollection().AddExpression(Reader);
        Readers.Add(Reader);
    }
    Material->GetEditorOnlyData()->BaseColor.Connect(0, Readers[0]);
    Material->GetEditorOnlyData()->Roughness.Connect(0, Readers[1]);
    Material->MaterialDomain = MD_UI;
    auto* RG = NewObject<UMaterialExpressionAppendVector>(Material, NAME_None, RF_Transactional);
    RG->MaterialExpressionGuid = FGuid::NewGuid(); RG->A.Connect(0, Readers[0]); RG->B.Connect(0, Readers[1]);
    RG->MaterialExpressionEditorX = 1800;
    Material->GetExpressionCollection().AddExpression(RG);
    auto* Blue = NewObject<UMaterialExpressionConstant>(Material, NAME_None, RF_Transactional);
    Blue->MaterialExpressionGuid = FGuid::NewGuid(); Blue->R = 0.0f; Blue->MaterialExpressionEditorX = 1800; Blue->MaterialExpressionEditorY = 200;
    Material->GetExpressionCollection().AddExpression(Blue);
    auto* RGB = NewObject<UMaterialExpressionAppendVector>(Material, NAME_None, RF_Transactional);
    RGB->MaterialExpressionGuid = FGuid::NewGuid(); RGB->A.Connect(0, RG); RGB->B.Connect(0, Blue);
    RGB->MaterialExpressionEditorX = 2100;
    Material->GetExpressionCollection().AddExpression(RGB);
    Material->GetEditorOnlyData()->EmissiveColor.Connect(0, RGB);
    Material->EditorX = 2400; Material->EditorY = 0;
    Material->PostEditChange();
    const FString Folder = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("TurboMCP/MaterialBlocksQuality"), FPaths::GetCleanFilename(Package->GetName()));
    IFileManager::Get().MakeDirectory(*Folder, true);
    auto Capture = [&](const TCHAR* Name) -> FColor
    {
        if (GShaderCompilingManager) GShaderCompilingManager->FinishAllCompilation();
        ENQUEUE_RENDER_COMMAND(BlocksUniforms)([](FRHICommandListImmediate& RHICmdList) {
            FMaterialRenderProxy::UpdateDeferredCachedUniformExpressions(RHICmdList);
        });
        FlushRenderingCommands();
        FSlateBrush Brush; Brush.SetResourceObject(Material); Brush.ImageSize = FVector2D(128,128);
        TSharedRef<SWidget> Widget = SNew(SImage).Image(&Brush);
        FWidgetRenderer Renderer(false);
        UTextureRenderTarget2D* Target = FWidgetRenderer::CreateTargetFor(FVector2D(128,128), TF_Bilinear, false);
        if (!TestNotNull(TEXT("Real render target"), Target)) return FColor::Black;
        Renderer.DrawWidget(Target, Widget, FVector2D(128,128), 0.0f, false);
        FlushRenderingCommands();
        if (GShaderCompilingManager) GShaderCompilingManager->FinishAllCompilation();
        Renderer.DrawWidget(Target, Widget, FVector2D(128,128), 0.0f, false);
        FlushRenderingCommands();
        TArray<FColor> Pixels;
        TestTrue(TEXT("Rendered pixels readable"), Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels));
        FBufferArchive PNG;
        TestTrue(TEXT("PNG encoded"), FImageUtils::ExportRenderTarget2DAsPNG(Target, PNG));
        TestTrue(TEXT("PNG saved"), FFileHelper::SaveArrayToFile(PNG, *FPaths::Combine(Folder, Name)));
        return Pixels.IsValidIndex(64*128+64) ? Pixels[64*128+64] : FColor::Black;
    };
    const FColor BeforeColor = Capture(TEXT("before.png"));
    TestTrue(TEXT("Fixture displays distinct red and green channels"), BeforeColor.G > BeforeColor.R && BeforeColor.R > 20 && BeforeColor.B < 10);
    Package->SetDirtyFlag(false);
    const FGuid BeforeState = Material->StateId;
    FTurboMCPMaterialBlocks::FPlan Plan;
    FString Error;
    if (!TestTrue(TEXT("Plan succeeds"), FTurboMCPMaterialBlocks::Plan(Material->GetPathName(), Plan, Error)))
    { AddError(Error); return false; }
    TestFalse(TEXT("Preview keeps package clean"), Package->IsDirty());
    TestTrue(TEXT("Preview preserves material state ID"), Material->StateId == BeforeState);
    FString Rollback;
    Readers[0]->A.Connect(3, Value);
    const FString StaleResult = FTurboMCPMaterialBlocks::Apply(Plan, Rollback);
    TestTrue(TEXT("Stale plan refused"), StaleResult.StartsWith(TEXT("error:")));
    TestEqual(TEXT("Stale refusal preserves new connection"), Readers[0]->A.OutputIndex, 3);
    Readers[0]->A.Connect(1, Value);
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
    const FColor AfterColor = Capture(TEXT("after.png"));
    TestEqual(TEXT("Rendered color preserved"), AfterColor, BeforeColor);
    const FString GraphCapture = UTurboMCPLookToolset::LookTurboMCPCapture(Material->GetPathName(), TEXT(""), TEXT("asset-editor"), TEXT("fit"), TEXT(""));
    TestFalse(TEXT("Material editor capture succeeds"), GraphCapture.StartsWith(TEXT("error:")));
    FFileHelper::SaveStringToFile(GraphCapture, *FPaths::Combine(Folder, TEXT("graph-capture.txt")));
    FTurboMCPMaterialBlocks::FPlan RepeatPlan;
    TestTrue(TEXT("Repeat plan succeeds"), FTurboMCPMaterialBlocks::Plan(Material->GetPathName(), RepeatPlan, Error));
    TestTrue(TEXT("Cleanup is stable after one edit"), RepeatPlan.bNothingToChange);
    TestTrue(TEXT("Repeat edit reports unchanged"), FTurboMCPMaterialBlocks::Apply(RepeatPlan, Rollback).StartsWith(TEXT("unchanged:")));
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
