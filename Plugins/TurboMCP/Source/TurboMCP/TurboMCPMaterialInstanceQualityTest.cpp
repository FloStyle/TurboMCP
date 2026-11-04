// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
#include "TurboMCPMaterialInstance.h"
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Materials/Material.h"
#include "Materials/MaterialRenderProxy.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "MaterialEditingLibrary.h"
#include "Tools/TurboMCPToolsets.h"
#include "Materials/MaterialExpressionTextureSampleParameter2D.h"
#include "Engine/Texture.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Slate/WidgetRenderer.h"
#include "Widgets/Images/SImage.h"
#include "Styling/SlateBrush.h"
#include "ShaderCompiler.h"
#include "TextureCompiler.h"
#include "ImageUtils.h"
#include "Serialization/BufferArchive.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "UObject/Package.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTurboMCPMaterialInstanceQualityTest,
    "TurboMCP.Material.InstanceQuality", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTurboMCPMaterialInstanceQualityTest::RunTest(const FString& Parameters)
{
    UPackage* Package = CreatePackage(*(TEXT("/Game/Tests/MaterialQuality_") + FGuid::NewGuid().ToString(EGuidFormats::Digits)));
    ON_SCOPE_EXIT { Package->SetDirtyFlag(false); };
    UMaterial* Material = NewObject<UMaterial>(Package, TEXT("M_Base"), RF_Public | RF_Standalone);
    auto* Scalar = NewObject<UMaterialExpressionScalarParameter>(Material);
    Scalar->ParameterName = TEXT("Amount"); Scalar->DefaultValue = 0.25f;
    Material->GetExpressionCollection().AddExpression(Scalar);
    // A scalar and vector may share a name; neither may hide the other in the reader.
    auto* Vector = NewObject<UMaterialExpressionVectorParameter>(Material);
    Vector->ParameterName = TEXT("Amount");
    Vector->DefaultValue = FLinearColor::Green;
    Material->GetExpressionCollection().AddExpression(Vector);
    auto MakeTexture = [&](const TCHAR* Name, const FColor Color) {
        UTexture2D* NewTexture = NewObject<UTexture2D>(Package, Name, RF_Public | RF_Standalone);
        TArray<FColor> Pixels; Pixels.Init(Color, 16);
        NewTexture->Source.Init(4, 4, 1, 1, TSF_BGRA8, reinterpret_cast<const uint8*>(Pixels.GetData()));
        NewTexture->SRGB = true; NewTexture->UpdateResource(); return NewTexture;
    };
    UTexture* Texture = MakeTexture(TEXT("T_Red"), FColor::Red);
    UTexture* BlueTexture = MakeTexture(TEXT("T_Blue"), FColor::Blue);
    FTextureCompilingManager::Get().FinishAllCompilation();
    FlushRenderingCommands();
    if (!TestNotNull(TEXT("Stock texture loaded"), Texture)) return false;
    auto* TextureParameter = NewObject<UMaterialExpressionTextureSampleParameter2D>(Material);
    TextureParameter->ParameterName = TEXT("SurfaceTexture"); TextureParameter->Texture = BlueTexture;
    TextureParameter->SamplerType = SAMPLERTYPE_Color;
    Material->GetExpressionCollection().AddExpression(TextureParameter);
    Material->MaterialDomain = MD_UI;
    Material->GetEditorOnlyData()->EmissiveColor.Connect(0, TextureParameter);
    Material->PostEditChange();
    auto* Parent = NewObject<UMaterialInstanceConstant>(Package, TEXT("MI_Parent"), RF_Public | RF_Standalone);
    UMaterialEditingLibrary::SetMaterialInstanceParent(Parent, Material);
    Parent->SetScalarParameterValueEditorOnly(TEXT("Amount"), 0.75f);
    Parent->PostEditChange();
    auto* Child = NewObject<UMaterialInstanceConstant>(Package, TEXT("MI_Child"), RF_Public | RF_Standalone);
    UMaterialEditingLibrary::SetMaterialInstanceParent(Child, Parent);
    Package->SetDirtyFlag(false);
    FTurboMCPMaterialInstance::FResult Result; FString Error;
    if (!TestTrue(TEXT("Read child"), FTurboMCPMaterialInstance::Read(Child->GetPathName(), Result, Error))) { AddError(Error); return false; }
    AddInfo(FTurboMCPMaterialInstance::Render(Result));
    TestEqual(TEXT("Three-level chain"), Result.Chain.Num(), 3);
    TestEqual(TEXT("Three inherited parameters, independent of ancestor count"), Result.Inherited.Num(), 3);
    for (const auto& Value : Result.Inherited) if (Value.Name == TEXT("Amount") && Value.Type == TEXT("Scalar")) TestEqual(TEXT("Nearest parent's value"), Value.Value, FString(TEXT("0.75")));
    TestEqual(TEXT("No local overrides"), Result.Overridden.Num(), 0);
    TestFalse(TEXT("Reader does not dirty asset"), Package->IsDirty());
    Child->SetScalarParameterValueEditorOnly(TEXT("Amount"), 0.5f);
    TestTrue(TEXT("Read local override"), FTurboMCPMaterialInstance::Read(Child->GetPathName(), Result, Error));
    TestEqual(TEXT("One override"), Result.Overridden.Num(), 1);
    TestEqual(TEXT("Scalar override preserves inherited vector and texture"), Result.Inherited.Num(), 2);
    TestTrue(TEXT("Same-name vector remains inherited"), Result.Inherited.ContainsByPredicate([](const FTurboMCPMaterialInstance::FParameter& Value) {
        return Value.Name == TEXT("Amount") && Value.Type == TEXT("Vector");
    }));
    TestEqual(TEXT("Declared override not orphaned"), Result.Orphaned.Num(), 0);
    TestFalse(TEXT("Base material refused"), FTurboMCPMaterialInstance::Read(Material->GetPathName(), Result, Error));
    auto Capture = [&](const TCHAR* Name, bool Red, UObject* Resource = nullptr) {
        if (GShaderCompilingManager) GShaderCompilingManager->FinishAllCompilation();
        // Offscreen widget rendering does not run the window renderer's uniform-cache update.
        // Flush newly compiled material values before measuring the fixture.
        ENQUEUE_RENDER_COMMAND(PrepareMaterialQualityUniforms)([](FRHICommandListImmediate& RHICmdList) {
            FMaterialRenderProxy::UpdateDeferredCachedUniformExpressions(RHICmdList);
        });
        FlushRenderingCommands();
        FSlateBrush Brush; Brush.SetResourceObject(Resource ? Resource : Child); Brush.ImageSize = FVector2D(128, 128);
        TSharedRef<SWidget> Widget = SNew(SImage).Image(&Brush);
        FWidgetRenderer Renderer(false);
        UTextureRenderTarget2D* Target = FWidgetRenderer::CreateTargetFor(FVector2D(128,128), TF_Bilinear, false);
        if (!TestNotNull(TEXT("Real rendering target required"), Target)) return;
        // Slate may initialize a material resource during its first paint.
        // Render and flush one preparation frame before measuring the next frame.
        Renderer.DrawWidget(Target, Widget, FVector2D(128,128), 0.0f, false);
        FlushRenderingCommands();
        if (GShaderCompilingManager) GShaderCompilingManager->FinishAllCompilation();
        Renderer.DrawWidget(Target, Widget, FVector2D(128,128), 0.0f, false);
        FlushRenderingCommands();
        TArray<FColor> Pixels;
        TestTrue(TEXT("Capture pixels readable"), Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels));
        if (Pixels.Num() > 64*128+64) {
            const FColor Center = Pixels[64*128+64];
            TestTrue(FString::Printf(TEXT("Rendered color: %s"), Name), Red ? Center.R > 180 && Center.B < 50 : Center.B > 180 && Center.R < 50);
        }
        FBufferArchive Png;
        TestTrue(TEXT("PNG encoded"), FImageUtils::ExportRenderTarget2DAsPNG(Target, Png));
        const FString Folder = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("TurboMCP/MaterialInstanceQuality"), FPaths::GetCleanFilename(Package->GetName()));
        IFileManager::Get().MakeDirectory(*Folder, true);
        TestTrue(TEXT("PNG saved"), FFileHelper::SaveArrayToFile(Png, *FPaths::Combine(Folder, Name)));
    };
    // Finalize fixture-only parent and scalar changes before measuring appearance.
    UMaterialEditingLibrary::UpdateMaterialInstance(Child);
    UTexture* BeforeTexture = nullptr;
    TestTrue(TEXT("Inherited blue texture resolves before render"), Child->GetTextureParameterValue(FMaterialParameterInfo(TEXT("SurfaceTexture")), BeforeTexture) && BeforeTexture == BlueTexture);
    Capture(TEXT("texture-blue.png"), false, BlueTexture);
    Capture(TEXT("base-blue.png"), false, Material);
    Capture(TEXT("parent-blue.png"), false, Parent);
    Capture(TEXT("before-blue.png"), false);
    const FString Assignment = TEXT("SurfaceTexture = ") + Texture->GetPathName();
    const FString Mixed = UTurboMCPEditToolset::EditTurboMCPMaterialInstanceSet(Child->GetPathName(), {Assignment, TEXT("Missing = ") + Texture->GetPathName()});
    TestTrue(TEXT("Mixed invalid batch refused"), Mixed.StartsWith(TEXT("error:")));
    TestEqual(TEXT("Mixed refusal writes nothing"), Child->TextureParameterValues.Num(), 0);
    const FString Edit = UTurboMCPEditToolset::EditTurboMCPMaterialInstanceSet(Child->GetPathName(), {Assignment}); AddInfo(Edit);
    TestTrue(TEXT("Valid texture override changed"), Edit.StartsWith(TEXT("changed:")));
    TestEqual(TEXT("One local texture override"), Child->TextureParameterValues.Num(), 1);
    UTexture* Actual = nullptr;
    TestTrue(TEXT("Effective texture read-back"), Child->GetTextureParameterValue(FMaterialParameterInfo(TEXT("SurfaceTexture")), Actual) && Actual == Texture);
    Capture(TEXT("after-red.png"), true);
    Package->SetDirtyFlag(false);
    TestTrue(TEXT("Same override unchanged"), UTurboMCPEditToolset::EditTurboMCPMaterialInstanceSet(Child->GetPathName(), {Assignment}).StartsWith(TEXT("unchanged:")));
    TestFalse(TEXT("No-op keeps clean state"), Package->IsDirty());
    TestTrue(TEXT("Parent preserved"), Child->Parent == Parent && Parent->Parent == Material);
    return true;
}
#endif
