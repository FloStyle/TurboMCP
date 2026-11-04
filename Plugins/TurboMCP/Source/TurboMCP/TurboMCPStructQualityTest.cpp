// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Tools/TurboMCPToolsets.h"
#include "Misc/AutomationTest.h"
#include "StructUtils/UserDefinedStruct.h"
#include "Kismet2/StructureEditorUtils.h"
#include "UserDefinedStructure/UserDefinedStructEditorData.h"
#include "ScopedTransaction.h"
#include "UObject/Package.h"
#include "Misc/PackageName.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTurboMCPStructQualityTest,
    "TurboMCP.Struct.Quality", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTurboMCPStructQualityTest::RunTest(const FString& Parameters)
{
    const FString Name=TEXT("S_ItemReview_")+FGuid::NewGuid().ToString(EGuidFormats::Digits);
    const FString Path=TEXT("/Game/Tests/")+Name;
    const FString Fields=TEXT("Count: int, default=3, Label: name, Offset: Vector");
    const FString Preview=UTurboMCPPreviewToolset::PreviewTurboMCPStructNew(Path,Fields); AddInfo(Preview);
    TestTrue(TEXT("New preview"),Preview.StartsWith(TEXT("preview:")));
    TestNull(TEXT("Preview creates no package"),FindPackage(nullptr,*Path));
    {
        FScopedTransaction Transaction(NSLOCTEXT("TurboMCPTest","StructGuard","Struct creation guard"));
        const FString Guard=UTurboMCPEditToolset::EditTurboMCPStructNew(Path+TEXT("_Guard"),Fields); AddInfo(Guard);
        TestTrue(TEXT("Creation inside transaction refused"),Guard.StartsWith(TEXT("unchanged:")));
        TestNull(TEXT("Creation guard no package"),FindPackage(nullptr,*(Path+TEXT("_Guard"))));
        Transaction.Cancel();
    }
    const FString Created=UTurboMCPEditToolset::EditTurboMCPStructNew(Path,Fields); AddInfo(Created);
    TestTrue(TEXT("Created"),Created.StartsWith(TEXT("changed:")));
    UUserDefinedStruct* Struct=FindObject<UUserDefinedStruct>(nullptr,*(Path+TEXT(".")+Name));
    if (!TestNotNull(TEXT("Struct exists"),Struct)) return false;
    auto Names=[&](){TArray<FString> Result; for(const auto& D:FStructureEditorUtils::GetVarDesc(Struct)) Result.Add(D.FriendlyName); return Result;};
    TestEqual(TEXT("Exactly requested fields"),Names().Num(),3);
    TestTrue(TEXT("Initial field names"),Names().Contains(TEXT("Count")) && Names().Contains(TEXT("Label")) && Names().Contains(TEXT("Offset")));
    auto Edit=[&](const FString& Changes){const FString Result=UTurboMCPEditToolset::EditTurboMCPStructFields(Path,Changes); AddInfo(Result); return Result;};
    Struct->GetPackage()->SetDirtyFlag(false);
    {
        FScopedTransaction Transaction(NSLOCTEXT("TurboMCPTest","FieldGuard","Struct field guard"));
        TestTrue(TEXT("Field edit inside transaction refused"),Edit(TEXT("Blocked: int")).StartsWith(TEXT("unchanged:")));
        TestFalse(TEXT("No blocked field"),Names().Contains(TEXT("Blocked")));
        TestFalse(TEXT("Field guard clean"),Struct->GetPackage()->IsDirty());
        Transaction.Cancel();
    }
    const FString EditPreview=UTurboMCPPreviewToolset::PreviewTurboMCPStructFields(Path,TEXT("Price: float, default=2")); AddInfo(EditPreview);
    TestTrue(TEXT("Field preview"),EditPreview.StartsWith(TEXT("preview:")));
    TestFalse(TEXT("Preview does not add"),Names().Contains(TEXT("Price")));
    TestTrue(TEXT("Add field"),Edit(TEXT("Price: float, default=2")).StartsWith(TEXT("changed:")));
    TestTrue(TEXT("Added field observed"),Names().Contains(TEXT("Price")));
    TestTrue(TEXT("Move field"),Edit(TEXT("move:Price, above=Count")).StartsWith(TEXT("changed:")));
    TestTrue(TEXT("Moved order observed"),Names().IndexOfByKey(TEXT("Price"))<Names().IndexOfByKey(TEXT("Count")));
    TestTrue(TEXT("Remove unused field"),Edit(TEXT("remove:Price")).StartsWith(TEXT("changed:")));
    TestFalse(TEXT("Removed field absent"),Names().Contains(TEXT("Price")));
    TestTrue(TEXT("Unrelated fields preserved"),Names().Contains(TEXT("Count")) && Names().Contains(TEXT("Label")) && Names().Contains(TEXT("Offset")));
    TestTrue(TEXT("Valid default and metadata change"),Edit(TEXT("Count: int, default=5, tooltip=Item count, category=Item")).StartsWith(TEXT("changed:")));
    for (const auto& D:FStructureEditorUtils::GetVarDesc(Struct)) if(D.FriendlyName==TEXT("Count"))
    {
        TestEqual(TEXT("Default observed"),D.DefaultValue,FString(TEXT("5")));
        TestEqual(TEXT("Tooltip observed"),D.ToolTip,FString(TEXT("Item count")));
        TestEqual(TEXT("Category observed"),D.Category,FName(TEXT("Item")));
    }
    const FString RejectedDefault=Edit(TEXT("Count: int, default=not_an_integer"));
    TestTrue(TEXT("Rejected default reports partial"),RejectedDefault.StartsWith(TEXT("partial:")));
    const FString InvalidNew=UTurboMCPEditToolset::EditTurboMCPStructNew(Path+TEXT("_InvalidDefault"),TEXT("Count: int, default=not_an_integer")); AddInfo(InvalidNew);
    TestTrue(TEXT("Rejected new default reports partial"),InvalidNew.StartsWith(TEXT("partial:")));
    TestTrue(TEXT("Existing path refused"),UTurboMCPEditToolset::EditTurboMCPStructNew(Path,Fields).StartsWith(TEXT("unchanged:")));
    TestFalse(TEXT("No implicit save"),FPackageName::DoesPackageExist(Path));
    return true;
}
#endif

