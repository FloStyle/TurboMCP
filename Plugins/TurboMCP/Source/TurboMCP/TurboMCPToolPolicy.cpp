// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
#include "TurboMCPToolPolicy.h"
#include "TurboMCPSettings.h"
namespace
{
    TMap<FString,FString> Denials;
    TSet<FString> Known;
    TArray<FString> ActiveDomains;
    TMap<FString,bool> ActiveOverrides;
    bool Enabled=false;
}
FString TurboMCPToolPolicy::Domain(const FString& Name)
{
    if (Name.StartsWith(TEXT("LookTurboMCP"))) return TEXT("Vision");
    if (Name.Contains(TEXT("Widget")) || Name.Contains(TEXT("Common"))) return TEXT("Widget");
    if (Name.Contains(TEXT("Play")) || Name.Contains(TEXT("Menu")) || Name.Contains(TEXT("Input"))) return TEXT("Play");
    if (Name.Contains(TEXT("Graph")) || Name.Contains(TEXT("Node")) || Name.Contains(TEXT("Function")) || Name.Contains(TEXT("Blueprint")) || Name.Contains(TEXT("Pin")) || Name.Contains(TEXT("Comment")) || Name.Contains(TEXT("LocalVar"))) return TEXT("Blueprint");
    if (Name.Contains(TEXT("Log")) || Name.Contains(TEXT("Status")) || Name.Contains(TEXT("Trace")) || Name.Contains(TEXT("Diagnostic"))) return TEXT("Diagnostics");
    if (Name.Contains(TEXT("Asset")) || Name.Contains(TEXT("DataTable")) || Name.Contains(TEXT("Struct")) || Name.Contains(TEXT("Save")) || Name.Contains(TEXT("Dirty")) || Name.Contains(TEXT("Material"))) return TEXT("Assets");
    return TEXT("Other");
}
void TurboMCPToolPolicy::Initialize(const UTurboMCPSettings& Settings,const TArray<FString>& Names)
{
    Denials.Reset();Known.Reset();Enabled=Settings.bEnableTurboMCP;
    ActiveDomains=Settings.DisabledToolDomains;ActiveOverrides=Settings.ToolOverrides;
    for (const FString& Name:Names)
    {
        Known.Add(Name);
        bool Allow=!ActiveDomains.Contains(Domain(Name));
        if (const bool* Override=ActiveOverrides.Find(Name)) Allow=*Override;
        if (Name==TEXT("ReadTurboMCPStatus")) Allow=true;
        if (!Enabled) Denials.Add(Name,TEXT("TurboMCP is disabled for this editor session."));
        else if (!Allow) Denials.Add(Name,TEXT("Disabled by your tool preferences for this editor session."));
    }
    for (const FString& Name:Names) if (Name.StartsWith(TEXT("EditTurboMCP")))
    {
        const FString Preview=TEXT("PreviewTurboMCP")+Name.Mid(12);
        if (Known.Contains(Preview) && Denials.Contains(Preview))
            Denials.Add(Name,TEXT("Its matching preview tool is disabled. Enable the preview before enabling this edit."));
    }
}
bool TurboMCPToolPolicy::IsAllowed(const FString& Name,FString& Reason)
{
    if (!Known.Contains(Name)) {Reason=TEXT("Tool policy is unavailable for this tool; restart the editor.");return false;}
    if (const FString* Why=Denials.Find(Name)) {Reason=*Why;return false;}
    Reason.Empty();return true;
}
TArray<FString> TurboMCPToolPolicy::DisabledNames()
{
    TArray<FString> Names;Denials.GetKeys(Names);Names.Sort();return Names;
}
FString TurboMCPToolPolicy::Describe(const UTurboMCPSettings& Settings)
{
    const bool Pending=Enabled!=Settings.bEnableTurboMCP || ActiveDomains!=Settings.DisabledToolDomains || !ActiveOverrides.OrderIndependentCompareEqual(Settings.ToolOverrides);
    FString Text=FString::Printf(TEXT("tool policy: %d enabled, %d disabled; preference changes require restart: %s\n"),Known.Num()-Denials.Num(),Denials.Num(),Pending?TEXT("yes"):TEXT("no"));
    for (const FString& Name:DisabledNames()) Text+=Name+TEXT(" [")+Domain(Name)+TEXT("]: ")+Denials[Name]+TEXT("\n");
    for (const auto& Pair:Settings.ToolOverrides) if (!Known.Contains(Pair.Key)) Text+=TEXT("Unknown tool override: ")+Pair.Key+TEXT(". Use an exact bare tool name.\n");
    for (const FString& Value:Settings.DisabledToolDomains)
        if (Value!=TEXT("Blueprint") && Value!=TEXT("Widget") && Value!=TEXT("Assets") && Value!=TEXT("Play") && Value!=TEXT("Vision") && Value!=TEXT("Diagnostics") && Value!=TEXT("Other"))
            Text+=TEXT("Unknown tool domain: ")+Value+TEXT(". This entry disables nothing.\n");
    return Text;
}
