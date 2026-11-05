// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
#include "Tools/TurboMCPToolsets.h"
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "GameFramework/Actor.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet/KismetSystemLibrary.h"
#include "K2Node_CallFunction.h"
#include "EdGraphSchema_K2.h"
#include "TurboMCPGraphText.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Package.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTurboMCPGraphJsonTest,"TurboMCP.GraphAppend.GraphJson",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTurboMCPGraphJsonTest::RunTest(const FString& Parameters)
{
    UPackage* Package=CreatePackage(*(TEXT("/Game/Tests/GraphJson_")+FGuid::NewGuid().ToString(EGuidFormats::Digits)));
    ON_SCOPE_EXIT {Package->SetDirtyFlag(false);};
    UBlueprint* BP=FKismetEditorUtilities::CreateBlueprint(AActor::StaticClass(),Package,TEXT("BP_Json"),BPTYPE_Normal,UBlueprint::StaticClass(),UBlueprintGeneratedClass::StaticClass());
    if (!TestNotNull(TEXT("Blueprint"),BP)) return false;
    UEdGraph* Graph=FBlueprintEditorUtils::CreateNewGraph(BP,TEXT("JsonGraph"),UEdGraph::StaticClass(),UEdGraphSchema_K2::StaticClass());
    BP->UbergraphPages.Add(Graph);
    auto* Node=NewObject<UK2Node_CallFunction>(Graph);
    Node->SetFromFunction(UKismetSystemLibrary::StaticClass()->FindFunctionByName(TEXT("PrintString")));
    Graph->AddNode(Node,false,false);Node->CreateNewGuid();Node->AllocateDefaultPins();
    Node->FindPin(TEXT("InString"))->DefaultValue=TEXT("A quoted \"value\" and newline\nsecond line");
    Package->SetDirtyFlag(false);
    auto Read=[&](int32 Offset,int32 Limit,const FString& Revision){return UTurboMCPReadToolset::ReadTurboMCPGraphLoad(BP->GetPathName(),Graph->GetName(),TEXT("json"),TEXT("full"),Offset,Limit,Revision,TEXT(""));};
    TSharedPtr<FJsonObject> Full;
    if (!TestTrue(TEXT("Complete JSON parses"),FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Read(0,24000,TEXT(""))),Full))) return false;
    TArray<FString> Notes;
    const FString Expected=FTurboMCPGraphText().Render(Graph,FTurboMCPGraphText::EDetail::Full,BP->GetPathName(),Notes);
    TestEqual(TEXT("Body matches existing writer"),Full->GetStringField(TEXT("body")),Expected);
    FString Combined,Revision;int32 Offset=0;
    for (int32 Page=0;Page<100;++Page)
    {
        TSharedPtr<FJsonObject> Item;
        if (!TestTrue(TEXT("Each page parses"),FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Read(Offset,256,Revision)),Item))) return false;
        Combined+=Item->GetStringField(TEXT("body"));Revision=Item->GetStringField(TEXT("revision"));
        const int32 Next=Item->GetIntegerField(TEXT("nextOffset"));
        if (Next==-1) break;
        if (!TestTrue(TEXT("Page advances"),Next>Offset)) return false;
        Offset=Next;
    }
    TestEqual(TEXT("Pages reconstruct exact body"),Combined,Expected);
    TestFalse(TEXT("Reads preserve clean package"),Package->IsDirty());
    Node->FindPin(TEXT("InString"))->DefaultValue=TEXT("Changed");
    TestTrue(TEXT("Stale revision refuses"),Read(256,256,Revision).StartsWith(TEXT("error:")));
    TestTrue(TEXT("Invalid format refuses"),UTurboMCPReadToolset::ReadTurboMCPGraphLoad(BP->GetPathName(),Graph->GetName(),TEXT("yaml")).StartsWith(TEXT("error:")));
    return !HasAnyErrors();
}
#endif
