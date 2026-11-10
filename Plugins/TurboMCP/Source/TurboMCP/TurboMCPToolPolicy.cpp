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
    FString RecoveryAsset;
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
void TurboMCPToolPolicy::RequireRecovery(const FString& Asset)
{
    if (RecoveryAsset.IsEmpty()) RecoveryAsset = Asset;
}
bool TurboMCPToolPolicy::IsRecoveryRequired(FString& Reason)
{
    if (RecoveryAsset.IsEmpty()) { Reason.Empty(); return false; }
    Reason = TEXT("TurboMCP edit calls are blocked for this editor session because batch restoration could not be verified for ")
        + RecoveryAsset + TEXT(". Inspect and recover the affected asset using Unreal or source control before restarting the editor. Do not save unverified changes. Read and status tools remain available if enabled.");
    return true;
}
#if WITH_DEV_AUTOMATION_TESTS
void TurboMCPToolPolicy::ResetRecoveryForTest() { RecoveryAsset.Empty(); }
#endif
bool TurboMCPToolPolicy::IsAllowed(const FString& Name,FString& Reason)
{
    if (!Known.Contains(Name))
    {
        Reason = TEXT("No policy entry exists for this tool. Check the exact registered name with ReadTurboMCPToolHelp.");
        return false;
    }
    if (Name.StartsWith(TEXT("EditTurboMCP")) && IsRecoveryRequired(Reason))
    {
        return false;
    }
    if (const FString* Why = Denials.Find(Name))
    {
        Reason = *Why;
        Reason += Enabled
            ? TEXT(" Review Tool controls in Editor Preferences > Plugins > TurboMCP and restart after changing them.")
            : TEXT(" Enable TurboMCP in Editor Preferences > Plugins > TurboMCP and restart the editor.");
        return false;
    }
    Reason.Reset();
    return true;
}
TArray<FString> TurboMCPToolPolicy::DisabledNames()
{
    TArray<FString> Names;Denials.GetKeys(Names);Names.Sort();return Names;
}
FString TurboMCPToolPolicy::Describe(const UTurboMCPSettings& Settings)
{
    const bool Pending=Enabled!=Settings.bEnableTurboMCP || ActiveDomains!=Settings.DisabledToolDomains || !ActiveOverrides.OrderIndependentCompareEqual(Settings.ToolOverrides);
    FString Text=FString::Printf(TEXT("tool policy: %d enabled, %d disabled; preference changes require restart: %s\n"),Known.Num()-Denials.Num(),Denials.Num(),Pending?TEXT("yes"):TEXT("no"));
    FString RecoveryReason;
    if (IsRecoveryRequired(RecoveryReason)) Text += TEXT("recovery required: ") + RecoveryReason + TEXT("\n");
    TMap<FString,int32> DomainTotals, DomainDisabled;
    for (const FString& Name:Known)
    {
        ++DomainTotals.FindOrAdd(Domain(Name));
        if (Denials.Contains(Name)) ++DomainDisabled.FindOrAdd(Domain(Name));
    }
    TArray<FString> DomainNames;DomainTotals.GetKeys(DomainNames);DomainNames.Sort();
    for (const FString& Name:DomainNames)
        Text+=FString::Printf(TEXT("domain %s: %d enabled, %d disabled\n"),*Name,DomainTotals[Name]-DomainDisabled.FindRef(Name),DomainDisabled.FindRef(Name));
    for (const FString& Name:DisabledNames()) Text+=Name+TEXT(" [")+Domain(Name)+TEXT("]: ")+Denials[Name]+TEXT("\n");
    for (const auto& Pair:Settings.ToolOverrides) if (!Known.Contains(Pair.Key)) Text+=TEXT("Unknown tool override: ")+Pair.Key+TEXT(". Use an exact bare tool name.\n");
    for (const FString& Value:Settings.DisabledToolDomains)
        if (Value!=TEXT("Blueprint") && Value!=TEXT("Widget") && Value!=TEXT("Assets") && Value!=TEXT("Play") && Value!=TEXT("Vision") && Value!=TEXT("Diagnostics") && Value!=TEXT("Other"))
            Text+=TEXT("Unknown tool domain: ")+Value+TEXT(". This entry disables nothing.\n");
    return Text;
}
