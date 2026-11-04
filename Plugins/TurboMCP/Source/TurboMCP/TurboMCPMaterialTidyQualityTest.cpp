// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
#include "TurboMCPMaterialTidy.h"
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionAdd.h"
#include "Materials/MaterialExpressionNamedReroute.h"
#include "UObject/Package.h"
#include "Tools/TurboMCPToolsets.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"

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


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTurboMCPMaterialTidyQualityTest,
    "TurboMCP.Material.TidyQuality", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTurboMCPMaterialTidyQualityTest::RunTest(const FString& Parameters)
{
    UPackage* Package = CreatePackage(*(TEXT("/Game/Tests/TidyQuality_") + FGuid::NewGuid().ToString(EGuidFormats::Digits)));
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
    Existing->MaterialExpressionEditorY = 300;
    Material->EditorX = 1100; Material->EditorY = 0;
    Material->GetEditorOnlyData()->BaseColor.Connect(0, Readers[0]);
    Material->GetEditorOnlyData()->Roughness.Connect(0, Readers[1]);
    Material->MaterialDomain = MD_UI;
    auto* RG = NewObject<UMaterialExpressionAppendVector>(Material, NAME_None, RF_Transactional);
    RG->MaterialExpressionGuid = FGuid::NewGuid(); RG->A.Connect(0, Readers[0]); RG->B.Connect(0, Readers[1]);
    RG->MaterialExpressionEditorX = 800;
    Material->GetExpressionCollection().AddExpression(RG);
    auto* Blue = NewObject<UMaterialExpressionConstant>(Material, NAME_None, RF_Transactional);
    Blue->MaterialExpressionGuid = FGuid::NewGuid(); Blue->R = 0.0f; Blue->MaterialExpressionEditorX = 800; Blue->MaterialExpressionEditorY = 200;
    Material->GetExpressionCollection().AddExpression(Blue);
    auto* RGB = NewObject<UMaterialExpressionAppendVector>(Material, NAME_None, RF_Transactional);
    RGB->MaterialExpressionGuid = FGuid::NewGuid(); RGB->A.Connect(0, RG); RGB->B.Connect(0, Blue);
    RGB->MaterialExpressionEditorX = 1000;
    Material->GetExpressionCollection().AddExpression(RGB);
    Material->GetEditorOnlyData()->EmissiveColor.Connect(0, RGB);
    Material->PostEditChange();
    Package->SetDirtyFlag(false);
    const FString Folder = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("TurboMCP/MaterialTidyQuality"), FPaths::GetCleanFilename(Package->GetName()));
    IFileManager::Get().MakeDirectory(*Folder, true);
    auto Capture = [&](const TCHAR* Name, UMaterial* Resource = nullptr) -> FColor
    {
        if (GShaderCompilingManager) GShaderCompilingManager->FinishAllCompilation();
        ENQUEUE_RENDER_COMMAND(LabelUniforms)([](FRHICommandListImmediate& RHICmdList) {
            FMaterialRenderProxy::UpdateDeferredCachedUniformExpressions(RHICmdList);
        });
        FlushRenderingCommands();
        FSlateBrush Brush; Brush.SetResourceObject(Resource ? Resource : Material); Brush.ImageSize = FVector2D(128,128);
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
    TestTrue(TEXT("Nontrivial red and green reference"), BeforeColor.G > BeforeColor.R && BeforeColor.R > 20 && BeforeColor.B < 10);
    const FGuid BeforeState = Material->StateId;
    FTurboMCPMaterialTidy::FPlan Plan;
    FString Error;
    if (!TestTrue(TEXT("Plan tidy"), FTurboMCPMaterialTidy::Plan(Material->GetPathName(), false, Plan, Error))) return false;
    TestFalse(TEXT("Preview does not dirty"), Package->IsDirty());
    TestEqual(TEXT("Preview preserves compilation state"), Material->StateId, BeforeState);
    TMap<const UMaterialExpression*, double> PlannedX;
    for (const auto& Node : Plan.Nodes) PlannedX.Add(Node.Expression, Node.PlannedX);
    for (const auto& Node : Plan.Nodes)
    {
        for (int32 Index = 0; Index < Node.Expression->CountInputs(); ++Index)
            if (const FExpressionInput* Input = Node.Expression->GetInput(Index))
                if (Input->Expression && PlannedX.Contains(Input->Expression))
                    TestTrue(TEXT("Producer precedes consumer"), PlannedX[Input->Expression] < Node.PlannedX);
        if (Node.Expression == RGB) TestFalse(TEXT("Material output source is reachable"), Node.bUnreached);
    }
    Readers[0]->A.OutputIndex = 3;
    TestTrue(TEXT("Stale plan refuses"), FTurboMCPMaterialTidy::Apply(Plan).StartsWith(TEXT("error:")));
    TestEqual(TEXT("Refusal preserves intervening edit"), Readers[0]->A.OutputIndex, 3);
    Readers[0]->A.OutputIndex = 1;
    TestTrue(TEXT("Tidy applies"), FTurboMCPMaterialTidy::Apply(Plan).StartsWith(TEXT("changed:")));
    for (int32 Index = 0; Index < Readers.Num(); ++Index)
    {
        TestTrue(TEXT("Reader source preserved"), Readers[Index]->A.Expression == Value);
        TestEqual(TEXT("Reader output preserved"), Readers[Index]->A.OutputIndex, Index % 2 + 1);
    }
    FTurboMCPMaterialTidy::FPlan Repeat;
    TestTrue(TEXT("Repeat plan succeeds"), FTurboMCPMaterialTidy::Plan(Material->GetPathName(), false, Repeat, Error));
    TestTrue(TEXT("Repeat unchanged"), FTurboMCPMaterialTidy::Apply(Repeat).StartsWith(TEXT("unchanged:")));
    FTurboMCPMaterialTidy::FPlan ForcedRepeat;
    TestTrue(TEXT("Legacy Force plan succeeds"), FTurboMCPMaterialTidy::Plan(Material->GetPathName(), true, ForcedRepeat, Error));
    const FGuid RepeatState = Material->StateId;
    TestTrue(TEXT("Legacy Force does not rewrite matching layout"), FTurboMCPMaterialTidy::Apply(ForcedRepeat).StartsWith(TEXT("unchanged:")));
    TestEqual(TEXT("No-op does not compile"), Material->StateId, RepeatState);
    TestEqual(TEXT("Rendered output preserved"), Capture(TEXT("after.png")), BeforeColor);
    const FString GraphCapture = UTurboMCPLookToolset::LookTurboMCPCapture(Material->GetPathName(), TEXT(""), TEXT("asset-editor"), TEXT("fit"), TEXT(""));
    TestFalse(TEXT("Graph capture succeeds"), GraphCapture.StartsWith(TEXT("error:")));
    FFileHelper::SaveStringToFile(GraphCapture, *FPaths::Combine(Folder, TEXT("graph-capture.txt")));
    return true;
}
#endif
