// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0

#include "Tools/TurboMCPToolsets.h"

#include "AssetRegistry/ARFilter.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Blueprint/WidgetTree.h"
#include "Components/InputKeySelector.h"
#include "Components/NamedSlotInterface.h"
#include "Components/PanelWidget.h"
#include "Components/RichTextBlockDecorator.h"
#include "Components/Widget.h"
#include "Containers/StaticArray.h"
#include "Dom/JsonObject.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "Editor.h"
#include "Engine/DataTable.h"
#include "InputAction.h"
#include "Interfaces/IPluginManager.h"
#include "K2Node_CallFunction.h"
#include "K2Node_Event.h"
#include "K2Node_FunctionEntry.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Misc/PackageName.h"
#include "Misc/ScopeExit.h"
#include "Misc/SecureHash.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Styling/SlateBrush.h"
#include "TurboMCPResult.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"
#include "WidgetBlueprint.h"

namespace
{
using FJsonFields = TArray<TSharedPtr<FJsonValue>>;
using FJsonRecord = TSharedRef<FJsonObject>;

constexpr int32 MaxPageSize = 16;
constexpr int32 MaxEvidencePerAsset = 12;
constexpr int32 MaxInspectionSteps = 30000;
constexpr int32 MaxDependencyPackages = 2048;
constexpr int32 MaxPropertyDepth = 8;

enum class ERule : uint8 { Class, Asset, AssetFolder, Override, NodeClass, Call, Count };

constexpr const TCHAR* RuleNames[] = {
    TEXT("class"), TEXT("asset"), TEXT("assetFolder"), TEXT("override"), TEXT("nodeClass"), TEXT("call")
};
static_assert(UE_ARRAY_COUNT(RuleNames) == static_cast<int32>(ERule::Count));

using FRuleSet = TStaticArray<FString, static_cast<int32>(ERule::Count)>;

const FString& Rule(const FRuleSet& Rules, ERule Kind)
{
    return Rules[static_cast<int32>(Kind)];
}

FString WriteJson(const FJsonRecord& Object)
{
    FString Output;
    FJsonSerializer::Serialize(Object, TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Output));
    return Output;
}

FString Refuse(const TCHAR* Code, const FString& Message)
{
    const FJsonRecord Body = MakeShared<FJsonObject>();
    Body->SetStringField(TEXT("error"), Message);
    FTurboMCPResult Result;
    Result.OperationId = FTurboMCPResult::NewOperationId();
    Result.Outcome = ETurboMCPResultOutcome::Refused;
    Result.Code = Code;
    Result.bComplete = false;
    return Result.AppendTo(WriteJson(Body));
}

bool IsContentFolder(const FString& Folder)
{
    FString Root;
    FString Remainder;
    if (!Folder.StartsWith(TEXT("/")) || Folder.EndsWith(TEXT("/")) || Folder.Contains(TEXT("//")))
    {
        return false;
    }
    if (!Folder.Mid(1).Split(TEXT("/"), &Root, &Remainder))
    {
        Root = Folder.Mid(1);
    }
    return !Root.IsEmpty() && Root != TEXT("Script") && Root != TEXT("Temp") &&
        Root != TEXT("Memory") && Root != TEXT("Transient") &&
        FPackageName::MountPointExists(TEXT("/") + Root + TEXT("/")) &&
        FPackageName::IsValidLongPackageName(Folder + TEXT("/_InventoryValidation"), true);
}

bool IsObjectPath(const FString& Text)
{
    return Text.Len() <= 512 && !Text.Contains(TEXT(":")) && FPackageName::IsValidObjectPath(Text) &&
        !FTopLevelAssetPath(Text).GetAssetName().IsNone();
}

FString ValidateRules(const FString& Json, IAssetRegistry& Registry,
    const TSet<FTopLevelAssetPath>& WidgetAssetClasses, FRuleSet& Rules)
{
    TSharedPtr<FJsonObject> Object;
    if (Json.Len() > 4096 || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Object)
        || !Object || Object->Values.IsEmpty())
    {
        return TEXT("Rules must be a nonempty JSON object of supported filter names and string values.");
    }

    for (const auto& Pair : Object->Values)
    {
        int32 Index = 0;
        while (Index < UE_ARRAY_COUNT(RuleNames) && Pair.Key != RuleNames[Index])
        {
            ++Index;
        }
        if (Index == UE_ARRAY_COUNT(RuleNames))
        {
            return FString::Printf(TEXT("Unsupported rule '%s'. Use class, asset, assetFolder, override, nodeClass or call."), *Pair.Key);
        }

        FString Value;
        if (!Pair.Value->TryGetString(Value) || Value.IsEmpty() || Value.Len() > 512 || Value != Value.TrimStartAndEnd())
        {
            return FString::Printf(TEXT("Rule '%s' needs a nonempty exact string of at most 512 characters."), RuleNames[Index]);
        }

        const ERule Kind = static_cast<ERule>(Index);
        switch (Kind)
        {
        case ERule::Class:
        case ERule::NodeClass:
        {
            if (!IsObjectPath(Value))
            {
                return TEXT("class and nodeClass require an exact top-level class path such as /Script/UMG.Image.");
            }
            const UClass* Type = FindObject<UClass>(nullptr, *Value);
            if (Kind == ERule::NodeClass)
            {
                if (!Type || !Type->IsChildOf(UEdGraphNode::StaticClass()))
                {
                    return TEXT("nodeClass requires an available EdGraphNode class. Enable the owning plugin if needed.");
                }
            }
            else if (!Type || (!Type->IsChildOf(UWidget::StaticClass()) && !Type->IsChildOf(URichTextBlockDecorator::StaticClass())))
            {
                // A generated widget class can be identified from its asset data without loading it.
                const FAssetData Blueprint = Value.EndsWith(TEXT("_C"))
                    ? Registry.GetAssetByObjectPath(FSoftObjectPath(Value.LeftChop(2))) : FAssetData();
                if (!Blueprint.IsValid() || !WidgetAssetClasses.Contains(Blueprint.AssetClassPath))
                {
                    return TEXT("class requires an available Widget or RichTextBlockDecorator class, or a registered Widget Blueprint generated class ending in _C.");
                }
            }
            break;
        }
        case ERule::Asset:
            if (!IsObjectPath(Value))
            {
                return TEXT("asset requires an exact top-level object path such as /Game/UI/T_Icon.T_Icon.");
            }
            break;
        case ERule::AssetFolder:
            if (!IsContentFolder(Value))
            {
                return TEXT("assetFolder requires a mounted content folder such as /Game/UI/Icons, without a trailing slash.");
            }
            break;
        case ERule::Override:
        case ERule::Call:
            if (Value.Len() >= NAME_SIZE || Value.Contains(TEXT("/")) || Value.Contains(TEXT(".")) ||
                Value.Contains(TEXT(":")) || Value.Contains(TEXT("*")) || Value.Contains(TEXT("?")) ||
                Value.Contains(TEXT(" ")) || FName(*Value).IsNone())
            {
                return TEXT("override and call require an exact, unqualified Blueprint function name.");
            }
            break;
        default:
            checkNoEntry();
        }
        Rules[Index] = MoveTemp(Value);
    }
    return FString();
}

bool ClassMatches(const UClass* Class, const FString& Path)
{
    if (!Path.IsEmpty())
    {
        for (const UClass* Type = Class; Type; Type = Type->GetSuperClass())
        {
            if (Type->GetPathName().Equals(Path, ESearchCase::IgnoreCase))
            {
                return true;
            }
        }
    }
    return false;
}

// Asset matching is based on the value at the observed site, never an inferred registry dependency.
FString AssetPath(const UObject* Object)
{
    if (const UClass* Class = Cast<UClass>(Object))
    {
        Object = Class->ClassGeneratedBy;
    }
    return Object && Object->IsAsset() ? Object->GetPathName() : FString();
}

struct FInspection
{
    explicit FInspection(const FRuleSet& InRules) : Rules(InRules) {}

    const FRuleSet& Rules;
    FJsonFields Evidence;
    int32 Steps = 0;
    int32 OmittedEvidence = 0;
    uint32 Matched = 0;
    bool bLimited = false;

    bool Visit()
    {
        if (Steps >= MaxInspectionSteps)
        {
            bLimited = true;
            return false;
        }
        ++Steps;
        return true;
    }

    void Add(ERule Kind, const FString& Source, const FString& Site, const FString& Value,
        const TCHAR* Origin = nullptr, const FString& Row = FString())
    {
        const int32 Index = static_cast<int32>(Kind);
        const uint32 Bit = 1u << Index;
        const bool bFirst = (Matched & Bit) == 0;
        Matched |= Bit;
        // Leave one slot for each remaining possible rule, so its first witness is always returned.
        if (!bFirst && Evidence.Num() >= MaxEvidencePerAsset - static_cast<int32>(ERule::Count))
        {
            ++OmittedEvidence;
            return;
        }
        const FJsonRecord Entry = MakeShared<FJsonObject>();
        Entry->SetStringField(TEXT("rule"), RuleNames[Index]);
        Entry->SetStringField(TEXT("source"), Source);
        Entry->SetStringField(TEXT("path"), Site);
        Entry->SetStringField(TEXT("value"), Value);
        if (Origin)
        {
            Entry->SetStringField(TEXT("origin"), Origin);
        }
        if (!Row.IsEmpty())
        {
            Entry->SetStringField(TEXT("row"), Row);
        }
        Evidence.Add(MakeShared<FJsonValueObject>(Entry));
    }

    void Reference(const FString& Object, const FString& Source, const FString& Site,
        const TCHAR* Origin = nullptr, const FString& Row = FString())
    {
        if (Object.IsEmpty())
        {
            return;
        }
        if (Object.Equals(Rule(Rules, ERule::Asset), ESearchCase::IgnoreCase))
        {
            Add(ERule::Asset, Source, Site, Object, Origin, Row);
        }
        const FString& Folder = Rule(Rules, ERule::AssetFolder);
        if (!Folder.IsEmpty() && Object.StartsWith(Folder + TEXT("/"), ESearchCase::IgnoreCase))
        {
            Add(ERule::AssetFolder, Source, Site, Object, Origin, Row);
        }
    }
};

const TCHAR* Origin(bool bHasDefault, bool bEqualsDefault)
{
    return !bHasDefault ? TEXT("unknown") :
        bEqualsDefault ? TEXT("class_default") : TEXT("instance_value");
}

void InspectProperty(FInspection& Out, const FProperty* Property, const void* Value, const void* Default,
    const FString& Widget, const FString& Path, bool bClassMatch, bool bDefaultKnown, int32 Depth)
{
    if (!Out.Visit())
    {
        return;
    }
    if (const FStructProperty* Struct = CastField<FStructProperty>(Property))
    {
        if (Struct->Struct == FSlateBrush::StaticStruct())
        {
            const FString Resource = AssetPath(static_cast<const FSlateBrush*>(Value)->GetResourceObject());
            const FString DefaultResource = Default ? AssetPath(static_cast<const FSlateBrush*>(Default)->GetResourceObject()) : FString();
            const TCHAR* Source = Origin(bDefaultKnown, Default && Resource == DefaultResource);
            const FString Site = Path + TEXT(".ResourceObject");
            if (bClassMatch && !Resource.IsEmpty())
            {
                Out.Add(ERule::Class, Widget, Site, Resource, Source);
            }
            Out.Reference(Resource, Widget, Site, Source);
            return;
        }
        if (Struct->Struct == FDataTableRowHandle::StaticStruct())
        {
            const FDataTableRowHandle& Handle = *static_cast<const FDataTableRowHandle*>(Value);
            const FString Table = AssetPath(Handle.DataTable.Get());
            const TCHAR* Source = Origin(bDefaultKnown, Default && Handle == *static_cast<const FDataTableRowHandle*>(Default));
            const FString Row = Handle.RowName.IsNone() ? FString() : Handle.RowName.ToString();
            if (bClassMatch && (!Table.IsEmpty() || !Row.IsEmpty()))
            {
                Out.Add(ERule::Class, Widget, Path, Table, Source, Row);
            }
            Out.Reference(Table, Widget, Path, Source, Row);
            return;
        }
        if (Struct->Struct == FInputChord::StaticStruct())
        {
            const FInputChord& Chord = *static_cast<const FInputChord*>(Value);
            if (bClassMatch && Chord.Key.IsValid())
            {
                Out.Add(ERule::Class, Widget, Path, Chord.GetInputText().ToString(),
                    Origin(bDefaultKnown, Default && Chord == *static_cast<const FInputChord*>(Default)));
            }
            return;
        }
        if (Depth < MaxPropertyDepth)
        {
            for (TFieldIterator<FProperty> Field(Struct->Struct); Field && !Out.bLimited; ++Field)
            {
                if (Field->HasAnyPropertyFlags(CPF_Transient | CPF_Deprecated) || Field->ArrayDim != 1)
                {
                    continue;
                }
                InspectProperty(Out, *Field, Field->ContainerPtrToValuePtr<void>(Value),
                    Default ? Field->ContainerPtrToValuePtr<void>(Default) : nullptr,
                    Widget, Path + TEXT(".") + Field->GetName(), bClassMatch, bDefaultKnown, Depth + 1);
            }
        }
        return;
    }

    if (const FArrayProperty* Array = CastField<FArrayProperty>(Property))
    {
        if (Depth < MaxPropertyDepth)
        {
            FScriptArrayHelper Values(Array, Value);
            FScriptArrayHelper Defaults(Array, Default);
            for (int32 Index = 0; Index < Values.Num() && !Out.bLimited; ++Index)
            {
                InspectProperty(Out, Array->Inner, Values.GetRawPtr(Index),
                    Default && Defaults.IsValidIndex(Index) ? Defaults.GetRawPtr(Index) : nullptr,
                    Widget, FString::Printf(TEXT("%s[%d]"), *Path, Index), bClassMatch, bDefaultKnown, Depth + 1);
            }
        }
        return;
    }

    FString Reference;
    bool bImportant = false;
    if (const FSoftObjectProperty* Soft = CastField<FSoftObjectProperty>(Property))
    {
        const FSoftObjectPath Target = Soft->GetPropertyValue(Value).ToSoftObjectPath();
        if (Target.GetSubPathString().IsEmpty())
        {
            Reference = Target.GetAssetPath().ToString();
        }
        bImportant = Soft->PropertyClass && Soft->PropertyClass->IsChildOf(UInputAction::StaticClass());
    }
    else if (const FObjectPropertyBase* Hard = CastField<FObjectPropertyBase>(Property))
    {
        UObject* Object = Hard->GetObjectPropertyValue(Value);
        const UClass* Type = Cast<UClass>(Object);
        if (Type && Type->IsChildOf(URichTextBlockDecorator::StaticClass()))
        {
            const FString TypePath = Type->GetPathName();
            const TCHAR* Source = Origin(bDefaultKnown, Default && Property->Identical(Value, Default));
            if (bClassMatch || ClassMatches(Type, Rule(Out.Rules, ERule::Class)))
            {
                Out.Add(ERule::Class, Widget, Path, TypePath, Source);
            }
        }
        Reference = AssetPath(Object);
        bImportant = Object && Object->IsA<UInputAction>();
    }

    if (!Reference.IsEmpty())
    {
        const TCHAR* Source = Origin(bDefaultKnown, Default && Property->Identical(Value, Default));
        if (bClassMatch && bImportant)
        {
            Out.Add(ERule::Class, Widget, Path, Reference, Source);
        }
        Out.Reference(Reference, Widget, Path, Source);
    }
}

void InspectTree(const UWidgetBlueprint& Blueprint, const FString& CandidateClass, FInspection& Out)
{
    const FString& ClassRule = Rule(Out.Rules, ERule::Class);
    if (CandidateClass.Equals(ClassRule, ESearchCase::IgnoreCase) || ClassMatches(Blueprint.ParentClass, ClassRule))
    {
        Out.Add(ERule::Class, TEXT("blueprint"), TEXT("generatedClass"), CandidateClass);
    }
    if (!Blueprint.WidgetTree)
    {
        return;
    }

    const bool bReferences = !Rule(Out.Rules, ERule::Asset).IsEmpty() || !Rule(Out.Rules, ERule::AssetFolder).IsEmpty();
    // Walk owned widgets iteratively so a malformed slot cycle cannot expand the scan.
    TSet<const UWidget*> Seen;
    TArray<UWidget*> Pending;
    auto Queue = [&](UWidget* Widget)
    {
        if (!Widget || Widget->GetOuter() != Blueprint.WidgetTree || Seen.Contains(Widget))
        {
            return;
        }
        if (Seen.Num() >= MaxInspectionSteps)
        {
            Out.bLimited = true;
            return;
        }
        Seen.Add(Widget);
        Pending.Add(Widget);
    };

    for (const auto& Slot : Blueprint.WidgetTree->NamedSlotBindings)
    {
        Queue(Slot.Value);
        if (Out.bLimited)
        {
            return;
        }
    }
    Queue(Blueprint.WidgetTree->RootWidget);
    while (!Pending.IsEmpty() && Out.Visit())
    {
        const UWidget* Widget = Pending.Pop(EAllowShrinking::No);
        const bool bClassMatch = ClassMatches(Widget->GetClass(), ClassRule);
        const FString Name = Widget->GetName();
        if (bClassMatch)
        {
            Out.Add(ERule::Class, Name, TEXT("class"), Widget->GetClass()->GetPathName());
        }
        const bool bDecoratorSearch = !ClassRule.IsEmpty() &&
            FindFProperty<FProperty>(Widget->GetClass(), TEXT("DecoratorClasses")) != nullptr;
        if (bClassMatch || bDecoratorSearch || bReferences)
        {
            const UObject* Default = Widget->GetClass()->GetDefaultObject(false);
            for (TFieldIterator<FProperty> Field(Widget->GetClass()); Field && !Out.bLimited; ++Field)
            {
                if (Field->HasAnyPropertyFlags(CPF_Transient | CPF_Deprecated) || Field->ArrayDim != 1 ||
                    (!Field->HasAnyPropertyFlags(CPF_Edit) &&
                        !(Widget->IsA<UInputKeySelector>() && Field->GetFName() == TEXT("SelectedKey"))))
                {
                    continue;
                }
                InspectProperty(Out, *Field, Field->ContainerPtrToValuePtr<void>(Widget),
                    Default ? Field->ContainerPtrToValuePtr<void>(Default) : nullptr,
                    Name, Field->GetName(), bClassMatch, Default != nullptr, 0);
            }
        }
        if (const UPanelWidget* Panel = Cast<UPanelWidget>(Widget))
        {
            for (int32 Index = Panel->GetChildrenCount() - 1; Index >= 0 && !Out.bLimited; --Index)
            {
                Queue(Panel->GetChildAt(Index));
            }
        }
        if (const INamedSlotInterface* Host = Cast<INamedSlotInterface>(Widget); Host && !Out.bLimited)
        {
            TArray<FName> Slots;
            Host->GetSlotNames(Slots);
            for (const FName Slot : Slots)
            {
                Queue(Host->GetContentForSlot(Slot));
                if (Out.bLimited)
                {
                    return;
                }
            }
        }
        if (Out.bLimited)
        {
            return;
        }
    }
}

void InspectGraphs(const UWidgetBlueprint& Blueprint, FInspection& Out)
{
    const FName Override(*Rule(Out.Rules, ERule::Override));
    const FName Call(*Rule(Out.Rules, ERule::Call));
    const FString& NodeClass = Rule(Out.Rules, ERule::NodeClass);
    const bool bReferences = !Rule(Out.Rules, ERule::Asset).IsEmpty() || !Rule(Out.Rules, ERule::AssetFolder).IsEmpty();
    if (Override.IsNone() && Call.IsNone() && NodeClass.IsEmpty() && !bReferences)
    {
        return;
    }

    const bool bInheritedFunction = !Override.IsNone() &&
        ((Blueprint.ParentClass && Blueprint.ParentClass->FindFunctionByName(Override)) ||
            FBlueprintEditorUtils::FindFunctionInImplementedInterfaces(&Blueprint, Override, nullptr, true));
    TArray<UEdGraph*> Graphs;
    Blueprint.GetAllGraphs(Graphs);
    for (const UEdGraph* Graph : Graphs)
    {
        if (!Out.Visit())
        {
            return;
        }
        if (!Graph || !Graph->IsIn(&Blueprint))
        {
            continue;
        }
        const FString GraphName = Graph->GetPathName(&Blueprint);
        for (const UEdGraphNode* Node : Graph->Nodes)
        {
            if (!Out.Visit())
            {
                return;
            }
            if (!Node)
            {
                continue;
            }
            const FString NodeName = Node->GetName();
            if (ClassMatches(Node->GetClass(), NodeClass))
            {
                Out.Add(ERule::NodeClass, GraphName, NodeName, Node->GetClass()->GetPathName());
            }
            if (const UK2Node_CallFunction* CallNode = Cast<UK2Node_CallFunction>(Node);
                !Call.IsNone() && CallNode && CallNode->GetFunctionName() == Call)
            {
                Out.Add(ERule::Call, GraphName, NodeName, CallNode->GetFunctionName().ToString());
            }
            if (!Override.IsNone())
            {
                const UK2Node_Event* Event = Cast<UK2Node_Event>(Node);
                const bool bOverrideEvent = Event && Event->bOverrideFunction && Event->GetFunctionName() == Override;
                const bool bOverrideGraph = bInheritedFunction && Node->IsA<UK2Node_FunctionEntry>() &&
                    Graph->GetFName() == Override;
                if (bOverrideEvent || bOverrideGraph)
                {
                    Out.Add(ERule::Override, GraphName, NodeName, Override.ToString());
                }
            }

            if (bReferences)
            {
                for (const UEdGraphPin* Pin : Node->Pins)
                {
                    if (!Out.Visit())
                    {
                        return;
                    }
                    if (!Pin || Pin->Direction != EGPD_Input || Pin->bOrphanedPin || !Pin->LinkedTo.IsEmpty())
                    {
                        continue;
                    }
                    FString Target = AssetPath(Pin->DefaultObject);
                    if (Target.IsEmpty() && (Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_SoftObject ||
                        Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_SoftClass) && IsObjectPath(Pin->DefaultValue))
                    {
                        Target = Pin->DefaultValue;
                    }
                    if (!Target.IsEmpty())
                    {
                        Out.Reference(Target, GraphName, NodeName + TEXT(".") + Pin->PinName.ToString(), TEXT("pin_literal"));
                    }
                }
            }
        }
    }
}

FString Digest(const FString& Text)
{
    const FTCHARToUTF8 Bytes(*Text);
    return FMD5::HashBytes(reinterpret_cast<const uint8*>(Bytes.Get()), Bytes.Length());
}

// Cursors bind the scan position and cumulative counts to this query and sorted candidate list.
// Content is still read live, so cumulative results are observations over the scan, not a snapshot.
struct FProgress
{
    int32 Next = 0;
    int32 Matched = 0;
    int32 Skipped = 0;
    int32 Errors = 0;

    FString Encode(const FString& Revision) const
    {
        const FString Counts = FString::Printf(TEXT("%d.%d.%d.%d"), Next, Matched, Skipped, Errors);
        return Counts + TEXT(".") + Digest(Revision + TEXT(".") + Counts);
    }

    bool Decode(const FString& Cursor, const FString& Revision, int32 Total)
    {
        if (Cursor.IsEmpty())
        {
            return true;
        }
        TArray<FString> Parts;
        Cursor.ParseIntoArray(Parts, TEXT("."), false);
        return Cursor.Len() <= 96 && Parts.Num() == 5
            && LexTryParseString(Next, *Parts[0]) && LexTryParseString(Matched, *Parts[1])
            && LexTryParseString(Skipped, *Parts[2]) && LexTryParseString(Errors, *Parts[3])
            && Next > 0 && Next < Total && Matched >= 0 && Matched <= Next && Skipped >= 0 && Skipped <= Next
            && Errors >= 0 && Errors <= Next && static_cast<int64>(Matched) + Skipped + Errors <= Next
            && Encode(Revision) == Cursor;
    }
};

// Unreal can load hard dependencies when resolving a Blueprint. Decline known map/redirector
// dependencies and unverified on-disk dependency data. Native PostLoad code remains outside this check.
FString LoadBlocker(IAssetRegistry& Registry, FName Candidate)
{
    using namespace UE::AssetRegistry;
    TArray<FName> Packages{Candidate};
    TSet<FName> Seen;
    Seen.Add(Candidate);
    for (int32 Index = 0; Index < Packages.Num(); ++Index)
    {
        const FName Package = Packages[Index];
        TArray<FAssetData> Assets;
        Registry.GetAssetsByPackageName(Package, Assets, true, false);
        if (Assets.ContainsByPredicate([](const FAssetData& Asset)
            { return Asset.HasAnyPackageFlags(PKG_ContainsMap) || Asset.IsRedirector(); }))
        {
            return TEXT("map_or_redirector_dependency");
        }
        TArray<FName> Dependencies;
        if (!Registry.GetDependencies(Package, Dependencies, EDependencyCategory::Package, EDependencyQuery::Hard))
        {
            return TEXT("dependency_data_unavailable");
        }
        for (FName Dependency : Dependencies)
        {
            if (!Dependency.ToString().StartsWith(TEXT("/Script/")) && !Seen.Contains(Dependency))
            {
                if (Packages.Num() >= MaxDependencyPackages)
                {
                    return TEXT("dependency_budget");
                }
                Seen.Add(Dependency);
                Packages.Add(Dependency);
            }
        }
    }
    return FString();
}

FARFilter BuildFilter(const FString& Folder, bool bRecursive)
{
    FARFilter Filter;
    Filter.ClassPaths.Add(UWidgetBlueprint::StaticClass()->GetClassPathName());
    Filter.bRecursiveClasses = true;
    if (!Folder.IsEmpty())
    {
        Filter.PackagePaths.Add(FName(*Folder));
        Filter.bRecursivePaths = bRecursive;
        return Filter;
    }
    Filter.PackagePaths.Add(TEXT("/Game"));
    for (const TSharedRef<IPlugin>& Plugin : IPluginManager::Get().GetEnabledPlugins())
    {
        if (Plugin->GetLoadedFrom() == EPluginLoadedFrom::Project && Plugin->CanContainContent())
        {
            FString Mount = Plugin->GetMountedAssetPath();
            Mount.RemoveFromEnd(TEXT("/"));
            if (IsContentFolder(Mount))
            {
                Filter.PackagePaths.AddUnique(FName(*Mount));
            }
        }
    }
    Filter.bRecursivePaths = true;
    return Filter;
}
}

FString UTurboMCPReadToolset::ReadTurboMCPWidgetInventory(const FString& Rules, const FString& Folder,
    bool Recursive, const FString& MatchMode, int32 PageSize, const FString& Cursor, bool LoadUnloaded)
{
    if (!IsInGameThread() || !GEditor || GEditor->IsPlaySessionInProgress() || GEditor->IsTransactionActive())
    {
        return Refuse(TEXT("editor_busy"), TEXT("Widget inventory requires an idle editor outside play and transactions."));
    }
    const bool bProject = Folder.IsEmpty() || Folder == TEXT("project");
    if ((!bProject && (Folder.Len() > 512 || !IsContentFolder(Folder))) ||
        (MatchMode != TEXT("all") && MatchMode != TEXT("any")) ||
        PageSize < 1 || PageSize > MaxPageSize)
    {
        return Refuse(TEXT("invalid_inventory_options"),
            TEXT("Use a mounted content folder or project for project scope, MatchMode all or any, and PageSize 1 through 16."));
    }

    IAssetRegistry& Registry = FAssetRegistryModule::GetRegistry();
    if (Registry.IsGathering())
    {
        return Refuse(TEXT("registry_busy"), TEXT("The asset registry is gathering data. Retry after asset discovery completes."));
    }
    const FARFilter Filter = BuildFilter(bProject ? FString() : Folder, Recursive);
    TSet<FTopLevelAssetPath> WidgetAssetClasses;
    Registry.GetDerivedClassNames(Filter.ClassPaths, {}, WidgetAssetClasses);
    WidgetAssetClasses.Append(Filter.ClassPaths);
    FRuleSet ParsedRules;
    if (const FString Error = ValidateRules(Rules, Registry, WidgetAssetClasses, ParsedRules); !Error.IsEmpty())
    {
        return Refuse(TEXT("invalid_inventory_rule"), Error);
    }
    uint32 RequiredRules = 0;
    FString RevisionData = FString::Printf(TEXT("%s\n%d\n%s\n%d\n"), *Folder, Recursive, *MatchMode, LoadUnloaded);
    for (int32 Index = 0; Index < ParsedRules.Num(); ++Index)
    {
        RevisionData += Digest(ParsedRules[Index]);
        if (!ParsedRules[Index].IsEmpty())
        {
            RequiredRules |= 1u << Index;
        }
    }

    TArray<FAssetData> Candidates;
    if (!Registry.GetAssets(Filter, Candidates))
    {
        return Refuse(TEXT("inventory_registry_failed"), TEXT("The asset registry could not enumerate Widget Blueprint candidates."));
    }
    Candidates.Sort([](const FAssetData& A, const FAssetData& B)
    {
        return A.GetSoftObjectPath().LexicalLess(B.GetSoftObjectPath());
    });
    for (const FAssetData& Candidate : Candidates)
    {
        RevisionData += TEXT("\n") + Candidate.GetSoftObjectPath().ToString();
    }
    const FString Revision = Digest(RevisionData);
    FProgress Progress;
    if (!Progress.Decode(Cursor == TEXT("first") ? FString() : Cursor, Revision, Candidates.Num()))
    {
        return Refuse(TEXT("stale_inventory_cursor"), TEXT("Cursor is invalid or the query or candidate set changed. Restart with Cursor=first."));
    }

    const FProgress Start = Progress;
    const bool bAll = MatchMode == TEXT("all");
    const int32 End = Start.Next + FMath::Min(PageSize, Candidates.Num() - Start.Next);
    FJsonFields Matches;
    FJsonFields Issues;
    int32 LoadAttempts = 0;
    int32 OmittedEvidence = 0;
    TSet<FString> DirtiedPackages;
    const FDelegateHandle DirtyListener = UPackage::PackageDirtyStateChangedEvent.AddLambda(
        [&](UPackage* Package)
        {
            if (IsInGameThread() && Package && Package->IsDirty())
            {
                DirtiedPackages.Add(Package->GetName());
            }
        });
    ON_SCOPE_EXIT { UPackage::PackageDirtyStateChangedEvent.Remove(DirtyListener); };

    auto AddIssue = [&](const FString& Path, const FString& Code)
    {
        const FJsonRecord Issue = MakeShared<FJsonObject>();
        Issue->SetStringField(TEXT("asset"), Path);
        Issue->SetStringField(TEXT("code"), Code);
        Issues.Add(MakeShared<FJsonValueObject>(Issue));
    };

    for (; Progress.Next < End; ++Progress.Next)
    {
        const FAssetData& Candidate = Candidates[Progress.Next];
        const FString Path = Candidate.GetSoftObjectPath().ToString();
        UObject* Object = Candidate.FastGetAsset(false);
        if (!Object)
        {
            const FString Blocker = !LoadUnloaded ? TEXT("unloaded") : LoadBlocker(Registry, Candidate.PackageName);
            if (!Blocker.IsEmpty())
            {
                ++Progress.Skipped;
                AddIssue(Path, Blocker);
                continue;
            }
            ++LoadAttempts;
            Object = StaticLoadObject(UWidgetBlueprint::StaticClass(), nullptr, *Path, nullptr,
                LOAD_DisableCompileOnLoad | LOAD_NoRedirects | LOAD_NoWarn);
        }

        TStrongObjectPtr<UWidgetBlueprint> Blueprint(Cast<UWidgetBlueprint>(Object));
        if (!Blueprint)
        {
            ++Progress.Errors;
            AddIssue(Path, TEXT("widget_load_failed"));
            continue;
        }
        FInspection Inspection(ParsedRules);
        if (!Rule(ParsedRules, ERule::Class).IsEmpty() ||
            !Rule(ParsedRules, ERule::Asset).IsEmpty() || !Rule(ParsedRules, ERule::AssetFolder).IsEmpty())
        {
            InspectTree(*Blueprint, Path + TEXT("_C"), Inspection);
        }
        if (!Inspection.bLimited)
        {
            InspectGraphs(*Blueprint, Inspection);
        }
        if (Inspection.bLimited)
        {
            ++Progress.Skipped;
            AddIssue(Path, TEXT("inspection_budget"));
            continue;
        }

        if (bAll ? Inspection.Matched != RequiredRules : Inspection.Matched == 0)
        {
            continue;
        }
        ++Progress.Matched;
        FJsonFields MatchedRules;
        for (int32 Index = 0; Index < ParsedRules.Num(); ++Index)
        {
            if ((Inspection.Matched & (1u << Index)) != 0)
            {
                MatchedRules.Add(MakeShared<FJsonValueString>(RuleNames[Index]));
            }
        }
        const FJsonRecord Match = MakeShared<FJsonObject>();
        Match->SetStringField(TEXT("asset"), Path);
        Match->SetArrayField(TEXT("matchedRules"), MoveTemp(MatchedRules));
        Match->SetArrayField(TEXT("evidence"), MoveTemp(Inspection.Evidence));
        if (Inspection.OmittedEvidence > 0)
        {
            Match->SetNumberField(TEXT("omittedEvidence"), Inspection.OmittedEvidence);
            OmittedEvidence += Inspection.OmittedEvidence;
        }
        Matches.Add(MakeShared<FJsonValueObject>(Match));
    }

    const bool bMore = Progress.Next < Candidates.Num();
    const bool bUnknown = bMore || Progress.Skipped > 0 || Progress.Errors > 0;
    const FJsonRecord Body = MakeShared<FJsonObject>();
    Body->SetNumberField(TEXT("totalCandidates"), Candidates.Num());
    Body->SetNumberField(TEXT("pageScanned"), Progress.Next - Start.Next - (Progress.Skipped - Start.Skipped) -
        (Progress.Errors - Start.Errors));
    Body->SetNumberField(TEXT("scanned"), Progress.Next - Progress.Skipped - Progress.Errors);
    Body->SetNumberField(TEXT("skipped"), Progress.Skipped);
    Body->SetNumberField(TEXT("errors"), Progress.Errors);
    Body->SetNumberField(TEXT("matchesScanned"), Progress.Matched);
    if (bUnknown)
    {
        Body->SetField(TEXT("totalMatches"), MakeShared<FJsonValueNull>());
    }
    else
    {
        Body->SetNumberField(TEXT("totalMatches"), Progress.Matched);
    }
    Body->SetStringField(TEXT("nextCursor"), bMore ? Progress.Encode(Revision) : FString());
    Body->SetArrayField(TEXT("matches"), MoveTemp(Matches));
    Body->SetArrayField(TEXT("pageIssues"), MoveTemp(Issues));
    Body->SetNumberField(TEXT("loadAttempts"), LoadAttempts);
    Body->SetStringField(TEXT("scope"), TEXT("Live designer tree and owned graphs. Embedded widget internals, native overrides, dynamic references, fixed arrays, maps and sets are outside this scan."));
    Body->SetStringField(TEXT("loadEffects"), LoadAttempts > 0
        ? TEXT("Package loading can run PostLoad and dependency compilation. Observed dirty transitions are listed; other load effects are unknown.")
        : TEXT("No explicit candidate loads on this page."));
    if (OmittedEvidence > 0)
    {
        Body->SetStringField(TEXT("evidenceHint"),
            TEXT("Evidence samples are capped per asset. Refine Rules or inspect a matching asset using WidgetTree or BlueprintLoad."));
    }

    FTurboMCPResult Result;
    Result.OperationId = FTurboMCPResult::NewOperationId();
    Result.bComplete = !bUnknown && OmittedEvidence == 0;
    if (Progress.Errors > Start.Errors || Progress.Skipped > Start.Skipped)
    {
        Result.Outcome = ETurboMCPResultOutcome::Partial;
        Result.Code = TEXT("inventory_incomplete");
    }
    Result.bTruncated = OmittedEvidence > 0;
    Result.Omitted = OmittedEvidence;
    if (LoadAttempts > 0)
    {
        Result.SetChangedUnknown();
        Result.Effects.Add(TEXT("widget_blueprint_load_attempted"));
    }
    if (!DirtiedPackages.IsEmpty())
    {
        FJsonFields Dirtied;
        Result.Assets = DirtiedPackages.Array();
        Result.Assets.Sort();
        for (const FString& Package : Result.Assets)
        {
            Dirtied.Add(MakeShared<FJsonValueString>(Package));
        }
        Body->SetArrayField(TEXT("observedDirtiedPackages"), MoveTemp(Dirtied));
        Result.SetChanged(true);
        Result.Effects.Add(TEXT("package_dirtied_during_inventory"));
    }
    return Result.AppendTo(WriteJson(Body));
}
