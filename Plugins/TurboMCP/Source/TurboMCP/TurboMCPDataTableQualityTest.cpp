// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Misc/Guid.h"
#include "TurboMCPDataTableRows.h"
#include "Tools/TurboMCPToolsets.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "TurboMCPKitRow.h"
#include "Engine/DataTable.h"
#include "UObject/Package.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTurboMCPDataTableQualityTest,
    "TurboMCP.DataTable.OwnedRows",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTurboMCPDataTableQualityTest::RunTest(const FString& Parameters)
{
    UPackage* Package = CreatePackage(*(TEXT("/Temp/TurboMCP/DT_") + FGuid::NewGuid().ToString(EGuidFormats::Digits)));
    Package->SetFlags(RF_Transient);
    UDataTable* Table = NewObject<UDataTable>(Package, TEXT("OwnedRows"), RF_Transient | RF_Transactional);
    Table->RowStruct = FTurboMCPKitRow::StaticStruct();
    ON_SCOPE_EXIT
    {
        Table->MarkAsGarbage();
        Package->SetDirtyFlag(false);
        Package->MarkAsGarbage();
    };
    FTurboMCPKitRow Seed;
    Seed.Count = 3; Seed.Weight = 2.5f; Seed.Label = TEXT("Preserve");
    Table->AddRow(TEXT("Existing"), Seed);
    Table->AddRow(TEXT("RemoveMe"), Seed);
    using namespace FTurboMCPDataTableRows;
    TArray<FRowOp> Ops;
    TestTrue(TEXT("Parse ordered edits"), Parse({TEXT("Existing = Count=7"), TEXT("New = Count=9"), TEXT("- RemoveMe")}, Ops).IsEmpty());
    FPlan Prepared;
    if (!TestTrue(TEXT("Plan accepted"), Plan(Table, Ops, Prepared).IsEmpty() && Prepared.bAcceptable())) return false;
    const FTurboMCPKitRow* Before = Table->FindRow<FTurboMCPKitRow>(TEXT("Existing"), TEXT("quality"), false);
    if (!TestNotNull(TEXT("Existing row before apply"), Before)) return false;
    TestEqual(TEXT("Plan preserves count"), Before->Count, 3);
    TestEqual(TEXT("Plan preserves row count"), Table->GetRowMap().Num(), 2);
    FString Report;
    const FString Error = Apply(Table, Prepared, Report);
    if (!TestTrue(TEXT("Apply succeeds"), Error.IsEmpty())) { AddError(Error + Report); return false; }
    const FTurboMCPKitRow* Existing = Table->FindRow<FTurboMCPKitRow>(TEXT("Existing"), TEXT("quality"), false);
    const FTurboMCPKitRow* Added = Table->FindRow<FTurboMCPKitRow>(TEXT("New"), TEXT("quality"), false);
    if (TestNotNull(TEXT("Updated row exists"), Existing))
    {
        TestEqual(TEXT("Requested field changed"), Existing->Count, 7);
        TestEqual(TEXT("Unspecified weight preserved"), Existing->Weight, 2.5f);
        TestEqual(TEXT("Unspecified label preserved"), Existing->Label, FName(TEXT("Preserve")));
    }
    if (TestNotNull(TEXT("New row exists"), Added)) TestEqual(TEXT("New row value"), Added->Count, 9);
    TestFalse(TEXT("Named row removed"), Table->GetRowMap().Contains(TEXT("RemoveMe")));
    TestEqual(TEXT("Final row count"), Table->GetRowMap().Num(), 2);
    Ops.Reset();
    TestTrue(TEXT("Unknown field syntax parses"), Parse({TEXT("Existing = NoSuchField=1")}, Ops).IsEmpty());
    FPlan Refused;
    Plan(Table, Ops, Refused);
    TestFalse(TEXT("Unknown field plan refused"), Refused.bAcceptable());
    TestFalse(TEXT("Refused plan cannot apply"), Apply(Table, Refused, Report).IsEmpty());
    Existing = Table->FindRow<FTurboMCPKitRow>(TEXT("Existing"), TEXT("quality"), false);
    if (TestNotNull(TEXT("Row retained after refusal"), Existing)) TestEqual(TEXT("Refusal preserves value"), Existing->Count, 7);
    FString Transcript;
    auto Record=[&](const TCHAR* Name,const FString& Result) {Transcript+=FString::Printf(TEXT("\n=== %s ===\n%s\n"),Name,*Result);return Result;};
    const FString Asset=Table->GetPathName();
    auto Read=[&]() {return Record(TEXT("ReadTurboMCPDataTable"),UTurboMCPReadToolset::ReadTurboMCPDataTable(Asset,TEXT("schema"),TEXT("Count,Weight,Label")));};
    const FString Baseline=Read();
    TestTrue(TEXT("Public reader identifies table"),Baseline.Contains(Asset));
    const bool DirtyBeforePreview=Package->IsDirty();
    const TArray<FString> Request={TEXT("Existing = Count=11"),TEXT("Added = Count=5"),TEXT("- New")};
    const FString Preview=Record(TEXT("PreviewTurboMCPDataTableRows"),UTurboMCPPreviewToolset::PreviewTurboMCPDataTableRows(Asset,Request));
    TestTrue(TEXT("Public preview explicitly reports no write"),Preview.StartsWith(TEXT("preview:")));
    TestEqual(TEXT("Public preview preserves table"),Read(),Baseline);
    TestEqual(TEXT("Public preview preserves dirty state"),Package->IsDirty(),DirtyBeforePreview);
    const FString Edited=Record(TEXT("EditTurboMCPDataTableRows"),UTurboMCPEditToolset::EditTurboMCPDataTableRows(Asset,Request));
    TestTrue(TEXT("Public edit reports actual write"),Edited.StartsWith(TEXT("written:")));
    TestFalse(TEXT("Completed edit is not described as future work"),Edited.Contains(TEXT("would")));
    TestTrue(TEXT("Persistence limit explicit"),Edited.Contains(TEXT("No compile or save was requested")));
    const FString ReadBack=Read();
    TestTrue(TEXT("Reader confirms updated count"),ReadBack.Contains(TEXT("Count = 11")));
    TestTrue(TEXT("Reader confirms added row"),ReadBack.Contains(TEXT("Added")));
    TestTrue(TEXT("Unspecified field preserved through public edit"),Table->FindRow<FTurboMCPKitRow>(TEXT("Existing"),TEXT("public review"))->Label==FName(TEXT("Preserve")));
    TestFalse(TEXT("Public edit removed named row"),Table->GetRowMap().Contains(TEXT("New")));
    const bool DirtyBeforeRefusal=Package->IsDirty();
    const TArray<FString> BadRequest={TEXT("Existing = NoSuchField=1")};
    for (const FString& Refusal : {UTurboMCPPreviewToolset::PreviewTurboMCPDataTableRows(Asset,BadRequest),UTurboMCPEditToolset::EditTurboMCPDataTableRows(Asset,BadRequest)})
        TestTrue(TEXT("Invalid field has unambiguous error status"),Record(TEXT("invalid field"),Refusal).StartsWith(TEXT("error:")));
    TestEqual(TEXT("Public refusal preserves rows"),Read(),ReadBack);
    TestEqual(TEXT("Public refusal preserves dirty state"),Package->IsDirty(),DirtyBeforeRefusal);
    TestTrue(TEXT("Unknown reader column refused"),UTurboMCPReadToolset::ReadTurboMCPDataTable(Asset,TEXT("schema"),TEXT("Missing")).StartsWith(TEXT("error:")));
    const FString Folder=FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("TurboMCP/Workflow"));
    IFileManager::Get().MakeDirectory(*Folder,true);
    TestTrue(TEXT("Public tool transcript saved"),FFileHelper::SaveStringToFile(Transcript,*FPaths::Combine(Folder,TEXT("datatable-public-review.txt")),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM));
    return !HasAnyErrors();
}
#endif
