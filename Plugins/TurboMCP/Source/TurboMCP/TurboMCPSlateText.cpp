// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
#include "Tools/TurboMCPToolsets.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWindow.h"
#include "Widgets/Text/STextBlock.h"
#include "Serialization/JsonSerializer.h"

FString UTurboMCPReadToolset::ReadTurboMCPSlateText(const FString& Text, bool IncludeHidden, int32 MatchLimit, int32 MaxDepth)
{
    if (!IsInGameThread() || !FSlateApplication::IsInitialized())
        return TEXT("error: requires initialized Slate on the game thread; no input was sent.");
    if (Text.TrimStartAndEnd().IsEmpty() || Text.Len() > 256 || MatchLimit < 1 || MatchLimit > 100 || MaxDepth < 1 || MaxDepth > 128)
        return TEXT("error: Text must contain 1..256 characters, MatchLimit must be 1..100, and MaxDepth must be 1..128.");
    TArray<TSharedRef<SWindow>> Windows;
    FSlateApplication::Get().GetAllVisibleWindowsOrdered(Windows);
    TArray<TSharedPtr<FJsonValue>> Matches;
    int32 Visited = 0, Found = 0;
    bool TraversalTruncated = false;
    TFunction<void(TSharedRef<SWidget>, bool, int32)> Walk;
    Walk = [&](TSharedRef<SWidget> Widget, bool AncestorsVisible, int32 Depth)
    {
        if (Visited >= 20000) { TraversalTruncated = true; return; }
        ++Visited;
        const bool LocalVisible = Widget->GetVisibility().IsVisible();
        const bool EffectiveVisible = AncestorsVisible && LocalVisible;
        FString Value;
        if (Widget->GetType() == FName(TEXT("STextBlock")))
            Value = StaticCastSharedRef<STextBlock>(Widget)->GetText().ToString();
#if WITH_ACCESSIBILITY
        if (Value.IsEmpty()) Value = Widget->GetAccessibleText(EAccessibleType::Main).ToString();
#endif
        if ((IncludeHidden || EffectiveVisible) && Value.Contains(Text, ESearchCase::IgnoreCase))
        {
            ++Found;
            if (Matches.Num() < MatchLimit)
            {
                auto Row = MakeShared<FJsonObject>();
                Row->SetStringField(TEXT("text"), Value.Left(1024));
                Row->SetBoolField(TEXT("textTruncated"), Value.Len() > 1024);
                Row->SetStringField(TEXT("widgetType"), Widget->GetTypeAsString());
                Row->SetBoolField(TEXT("localVisible"), LocalVisible);
                Row->SetBoolField(TEXT("ancestorsVisible"), AncestorsVisible);
                Row->SetBoolField(TEXT("effectiveVisible"), EffectiveVisible);
                Matches.Add(MakeShared<FJsonValueObject>(Row));
            }
        }
        // Visit hidden descendants as well: local visibility alone is misleading.
        FChildren* Children = Widget->GetChildren();
        if (Depth >= MaxDepth) { if (Children->Num() > 0) TraversalTruncated = true; return; }
        for (int32 Index = 0; Index < Children->Num(); ++Index)
        {
            if (Visited >= 20000) { TraversalTruncated = true; break; }
            Walk(Children->GetChildAt(Index), EffectiveVisible, Depth + 1);
        }
    };
    for (const auto& Window : Windows) Walk(Window, true, 0);
    auto Result = MakeShared<FJsonObject>();
    Result->SetArrayField(TEXT("matches"), Matches);
    Result->SetBoolField(TEXT("includeHidden"), IncludeHidden);
    Result->SetNumberField(TEXT("visitedWidgets"), Visited);
    Result->SetNumberField(TEXT("matchedWidgets"), Found);
    Result->SetBoolField(TEXT("traversalTruncated"), TraversalTruncated);
    Result->SetBoolField(TEXT("resultsTruncated"), Found > Matches.Num());
    Result->SetStringField(TEXT("finding"), Found > 0 ? TEXT("matching text found") : TraversalTruncated ? TEXT("unknown: search incomplete") : (IncludeHidden ? TEXT("no matching supported text in searched windows") : TEXT("no effectively visible matching supported text; hidden matches excluded")));
    Result->SetStringField(TEXT("limits"), TEXT("Mounted STextBlock/accessibility text in visible top-level windows only. Effective visibility includes ancestors, not clipping, enabled input, hit testing or OS foreground. Presence is not clickability. No wait, focus or input was requested."));
    FString Output;
    FJsonSerializer::Serialize(Result, TJsonWriterFactory<>::Create(&Output));
    return Output;
}
