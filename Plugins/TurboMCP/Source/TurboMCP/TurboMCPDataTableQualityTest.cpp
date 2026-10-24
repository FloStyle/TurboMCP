// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Misc/Guid.h"
#include "TurboMCPDataTableRows.h"
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
    return !HasAnyErrors();
}
#endif
