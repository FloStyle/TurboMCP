// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
#include "TurboMCPPlayViewport.h"
#include "Editor.h"
#include "Engine/World.h"
#include "Engine/GameViewportClient.h"
#include "Slate/SGameLayerManager.h"
#include "Framework/Application/SlateApplication.h"
#include "ImageUtils.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

FString TurboMCPPlayViewport::Capture()
{
    if (!GEditor || !FSlateApplication::IsInitialized()) return TEXT("error: Live PIE viewport capture requires a rendering editor.");
    UWorld* World = nullptr;
    int32 Count = 0;
    for (const FWorldContext& Context : GEditor->GetWorldContexts())
    {
        if (Context.WorldType == EWorldType::PIE && Context.World()) { World = Context.World(); ++Count; }
    }
    if (Count != 1) return FString::Printf(TEXT("error: Target=pie requires exactly one live in-process PIE world; found %d. Start one play session, then retry. No editor viewport was substituted."), Count);
    UGameViewportClient* Client = World->GetGameViewport();
    if (!Client || !Client->GetGameLayerManager().IsValid()) return TEXT("error: The PIE world has no available game UI layer. Wait for play startup, then retry.");
    const TSharedRef<SWidget> Layer = Client->GetGameLayerManager()->AsWidget();
    const FVector2D Geometry = Layer->GetTickSpaceGeometry().GetAbsoluteSize();
    if (Geometry.X <= 0 || Geometry.Y <= 0 || Geometry.X > 4096 || Geometry.Y > 4096)
        return TEXT("error: PIE viewport must have visible geometry from 1 to 4096 pixels per edge. Resize or restore its window, then retry.");
    TArray<FColor> Pixels;
    FIntVector Size;
    // Use Slate's existing live window renderer, never reparent a live game layer.
    if (!FSlateApplication::Get().TakeScreenshot(Layer, Pixels, Size) || Size.X <= 0 || Size.Y <= 0 || Size.X > 4096 || Size.Y > 4096 || int64(Pixels.Num()) != int64(Size.X) * Size.Y)
        return TEXT("error: Slate could not capture the complete PIE viewport within the 4096-pixel edge limit. No image was written.");
    for (FColor& Pixel : Pixels) Pixel.A = 255;
    TArray64<uint8> Png;
    FImageUtils::PNGCompressImageArray(Size.X, Size.Y, Pixels, Png);
    const FString Folder = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("TurboMCP/Captures"));
    IFileManager::Get().MakeDirectory(*Folder, true);
    const FString Path = FPaths::ConvertRelativePathToFull(FPaths::Combine(Folder, TEXT("pie_") + FGuid::NewGuid().ToString(EGuidFormats::Digits) + TEXT(".png")));
    if (Png.IsEmpty() || !FFileHelper::SaveArrayToFile(Png, *Path)) return TEXT("error: Could not write the PIE viewport image.");
    return FString::Printf(TEXT("look: live PIE viewport\nworld: %s\ntarget: pie; single in-process world\nframed: game layer including menu UI; excludes editor chrome\nsize: %dx%d; native pixels, no rescaling\nimage: %s\nFrame freshness: latest rendered frame, not a guarantee that a previous action has finished. Allow normal game ticks and inspect the next capture.\nInteraction: ReadTurboMCPPlayInputState lists runtime buttons. Use EditTurboMCPMenuActivate for a named button or EditTurboMCPPlayInput for game keys, then wait for frames and inspect state/capture. Slate clicks can produce hover without activation; delivery alone is not success. Capture does not start, stop or send input to play."), *World->GetPathName(), Size.X, Size.Y, *Path);
}
