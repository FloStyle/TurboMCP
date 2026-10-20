// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Engine/DataTable.h"
#include "Kismet/DataTableFunctionLibrary.h"
#include "UObject/StrongObjectPtr.h"

// Diagnostic only: the pinned native importer may crash while replacing a held
// object. Never run against user content. Each table is transient and unsaved.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTurboMCPDataTableImportLifetimeTest,
    "TurboMCP.DataTable.ImportLifetimeDiagnostic",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTurboMCPDataTableImportLifetimeTest::RunTest(const FString& Parameters)
{
    UDataTable* Table = NewObject<UDataTable>(GetTransientPackage(), NAME_None, RF_Transient);
    Table->RowStruct = FTableRowBase::StaticStruct();
    const FString Initial = TEXT("[{\"Name\":\"Before\"}]");
    const FString Changed = TEXT("[{\"Name\":\"Before\"},{\"Name\":\"After\"}]");
    AddInfo(TEXT("FIELD-01: entering unheld native import control"));
    if (!TestTrue(TEXT("Unheld native import succeeds"),
        UDataTableFunctionLibrary::FillDataTableFromJSONString(Table, Initial, FTableRowBase::StaticStruct())))
        return false;
    TStrongObjectPtr<UDataTable> Held(Table);
    AddInfo(TEXT("FIELD-01: entering held native import; an assertion is a reproduced failure, not a pass"));
    TestTrue(TEXT("Held native import succeeds"),
        UDataTableFunctionLibrary::FillDataTableFromJSONString(Table, Changed, FTableRowBase::StaticStruct()));
    TestTrue(TEXT("Existing row retained"), Held->GetRowMap().Contains(FName(TEXT("Before"))));
    TestTrue(TEXT("Added row exists"), Held->GetRowMap().Contains(FName(TEXT("After"))));
    TestEqual(TEXT("Exactly two rows"), Held->GetRowMap().Num(), 2);
    return true;
}
#endif
