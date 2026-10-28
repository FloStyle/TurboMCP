// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Misc/AutomationTest.h"

#include "TurboMCPAssetResolver.h"
#include "TurboMCPGraphAppend.h"
#include "TurboMCPGraphTextLex.h"
#include "Tools/TurboMCPToolsets.h"
#include "K2Node_VariableGet.h"
#include "BlueprintActionDatabase.h"
#include "TurboMCPNodeId.h"

#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraph/EdGraphSchema.h"
#include "EdGraphNode_Comment.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "GameFramework/Actor.h"
#include "K2Node_CallFunction.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/Guid.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"

/**
 * THE APPEND ALIAS COLLISION, TESTED THROUGH THE REAL `Plan` AND NOT THROUGH A TEST SEAM.
 *
 * WHAT THE DEFECT WAS: `FTurboMCPGraphAppend::Plan` indexed every node twice by its qualified ID and once by
 * `NodeGuid.ToString(Digits).Left(6)`. Two GUIDs can share six digits, and the map kept the LAST node it
 * met, so a link written with the short alias resolved to whichever node happened to be later in
 * `Graph->Nodes`. The same text linked a different node after the same block was inserted in a different
 * order, and nothing refused.
 *
 * WHAT THIS TEST DOES: it builds disposable Blueprints and graphs IN MEMORY, never saves them and never calls
 * `Apply`, forces the GUIDs so the collision is deterministic, and calls the production
 * `FTurboMCPGraphAppend::Plan`. The fixture is resolved through the same asset resolver a tool call uses, by
 * its full object path, so the in-memory Blueprint must be addressable exactly like a loaded asset.
 *
 * WHY AN IN-MEMORY FIXTURE: a saved collision asset would be a committed binary and a fixture generator;
 * `Plan` needs only a resolvable Blueprint, and `FTurboMCPAssetResolver` completes from any loaded object
 * before it consults the registry. The package name carries a fresh GUID so repeated runs in one editor
 * cannot collide, the package and Blueprint are marked transient, and a scoped cleanup retires only the
 * fixture's own objects on every return. The package is never added to the asset registry or saved.
 *
 * WHAT IT DOES NOT PROVE: the MCP tool text (`unchanged:`), `Apply`, compilation, saving, or a reopened
 * editor. Those are editor acceptance steps for the integrator, not claims of this file, and the test
 * records the dirty flag rather than asserting it is untouched because `Plan` builds and removes probe
 * nodes through the engine's own spawner.
 */

namespace
{
	/**
	 * THE FIXED GUIDS. The first pair shares six characters and differs at the eighth, so `UniqueKeyDigits`
	 * returns 8 for both; the pair is chosen rather than left to chance because a random pair almost never
	 * collides.
	 */
	constexpr uint32 GuidForwardA = 0xABCDEF01;
	constexpr uint32 GuidForwardB = 0xABCDEF02;
	constexpr uint32 GuidReverseA = 0xABCDEF11;
	constexpr uint32 GuidReverseB = 0xABCDEF12;
	constexpr uint32 GuidUniqueNode = 0xAB12CD34;
	constexpr uint32 GuidCommentForward = 0x33330001;
	constexpr uint32 GuidCommentReverse = 0x33330002;
	constexpr uint32 GuidCommentUnique = 0x33330003;
	constexpr uint32 GuidEntryForward = 0x11110001;
	constexpr uint32 GuidResultForward = 0x22220001;
	constexpr uint32 GuidEntryReverse = 0x11110002;
	constexpr uint32 GuidResultReverse = 0x22220002;
	constexpr uint32 GuidEntryUnique = 0x11110003;
	constexpr uint32 GuidResultUnique = 0x22220003;

	struct FFixture
	{
		UPackage* Package = nullptr;
		UBlueprint* Blueprint = nullptr;
		UEdGraph* Forward = nullptr;
		UEdGraph* Reverse = nullptr;
		UEdGraph* Unique = nullptr;
		UEdGraphNode_Comment* ForwardComment = nullptr;
		UK2Node_CallFunction* ForwardA = nullptr;
		UK2Node_CallFunction* ForwardB = nullptr;
		UK2Node_CallFunction* ReverseA = nullptr;
		UK2Node_CallFunction* ReverseB = nullptr;
		UK2Node_CallFunction* UniqueNode = nullptr;
		FString AssetPath;
	};

	/**
	 * Retires only this fixture's objects, on every return path.
	 *
	 * The package keeps its /Game name so the resolver can address a loaded object by it, and it is marked
	 * transient so the editor never treats it as a save candidate. `MarkAsGarbage` is the engine's package
	 * cleanup pattern; no global collection is requested, and `IsRooted` guards the engine's own check.
	 */
	struct FScopedFixtureCleanup
	{
		explicit FScopedFixtureCleanup(FFixture& InFixture) : Fixture(InFixture) {}

		~FScopedFixtureCleanup()
		{
			auto Retire = [](UObject* Object)
			{
				if (Object != nullptr && !Object->IsRooted())
				{
					Object->MarkAsGarbage();
				}
			};

			auto RetireGraph = [&Retire](UEdGraph* Graph)
			{
				if (Graph == nullptr)
				{
					return;
				}
				for (UEdGraphNode* Node : Graph->Nodes)
				{
					Retire(Node);
				}
				Retire(Graph);
			};

			if (Fixture.Blueprint != nullptr)
			{
				Retire(Fixture.Blueprint->GeneratedClass.Get());
				Retire(Fixture.Blueprint->SkeletonGeneratedClass.Get());
				Retire(Fixture.Blueprint);
			}
			RetireGraph(Fixture.Forward);
			RetireGraph(Fixture.Reverse);
			RetireGraph(Fixture.Unique);
			if (Fixture.Package != nullptr)
			{
				Fixture.Package->SetDirtyFlag(false);
				Retire(Fixture.Package);
			}
		}

		FFixture& Fixture;
	};

	struct FPlanResult
	{
		bool bPlanned = false;
		bool bDirtyAfter = false;
		FString Error;
		FString GraphBefore;
		FString GraphAfter;
		FTurboMCPGraphAppend::FPlan Plan;
	};

	/** Replaces the GUID the node made for itself, so the collision is a property of the fixture. */
	void ForceGuid(UEdGraphNode* InNode, uint32 InA, uint32 InB = 0, uint32 InC = 0, uint32 InD = 0)
	{
		if (InNode == nullptr)
		{
			return;
		}
		InNode->CreateNewGuid();
		InNode->NodeGuid = FGuid(InA, InB, InC, InD);
	}

	/** A function graph with an entry and a result, the shape the acceptance commandlet builds. */
	UEdGraph* AddFunctionGraph(UBlueprint* InBlueprint, const TCHAR* InName, uint32 InEntryGuid,
		uint32 InResultGuid)
	{
		UEdGraph* Graph = FBlueprintEditorUtils::CreateNewGraph(
			InBlueprint, FName(InName), UEdGraph::StaticClass(), UEdGraphSchema_K2::StaticClass());
		if (Graph == nullptr)
		{
			return nullptr;
		}
		FBlueprintEditorUtils::AddFunctionGraph<UFunction>(InBlueprint, Graph,
			/*bIsUserCreated=*/true, /*SignatureFromObject=*/nullptr);
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (Cast<UK2Node_FunctionEntry>(Node) != nullptr)
			{
				ForceGuid(Node, InEntryGuid);
			}
			else if (Cast<UK2Node_FunctionResult>(Node) != nullptr)
			{
				ForceGuid(Node, InResultGuid);
			}
		}
		return Graph;
	}

	UK2Node_CallFunction* PlacePrintString(UEdGraph* InGraph, uint32 InGuid, int32 InX, int32 InY)
	{
		UK2Node_CallFunction* Node = NewObject<UK2Node_CallFunction>(InGraph);
		if (Node == nullptr)
		{
			return nullptr;
		}
		UFunction* Function = UKismetSystemLibrary::StaticClass()->FindFunctionByName(TEXT("PrintString"));
		if (Function == nullptr)
		{
			return nullptr;
		}
		Node->SetFromFunction(Function);
		Node->NodePosX = InX;
		Node->NodePosY = InY;
		InGraph->AddNode(Node, /*bUserAction=*/false, /*bSelectNewNode=*/false);
		// THE GUID IS FORCED AFTER `AddNode`, so nothing in node insertion can replace it, and before the
		// pins are allocated, so every later read sees the final identity.
		ForceGuid(Node, InGuid);
		Node->AllocateDefaultPins();
		return Node;
	}

	/** A comment box, so the snapshot covers comment bounds rather than only ordinary nodes. */
	UEdGraphNode_Comment* PlaceComment(UEdGraph* InGraph, uint32 InGuid, int32 InX, int32 InY,
		int32 InWidth, int32 InHeight)
	{
		UEdGraphNode_Comment* Node = NewObject<UEdGraphNode_Comment>(InGraph);
		if (Node == nullptr)
		{
			return nullptr;
		}
		ForceGuid(Node, InGuid);
		Node->NodePosX = InX;
		Node->NodePosY = InY;
		Node->NodeWidth = InWidth;
		Node->NodeHeight = InHeight;
		InGraph->AddNode(Node, /*bUserAction=*/false, /*bSelectNewNode=*/false);
		// FORCED AGAIN AFTER INSERTION, for the same reason as the call node above.
		ForceGuid(Node, InGuid);
		Node->AllocateDefaultPins();
		return Node;
	}

	FString ClassNameOf(const UEdGraphNode* InNode)
	{
		return InNode != nullptr && InNode->GetClass() != nullptr ? InNode->GetClass()->GetName() : FString();
	}

	/**
	 * Every fact a refused plan must not change, in graph order.
	 *
	 * The node GUID and position, the class, the size (the bounds of a comment), every pin's name, direction,
	 * type, defaults and text, and every link as a PAIR OF ENDPOINT IDENTITIES rather than a count.
	 */
	FString SerializeGraph(const UEdGraph* InGraph)
	{
		FString Out;
		if (InGraph == nullptr)
		{
			return Out;
		}
		for (const UEdGraphNode* Node : InGraph->Nodes)
		{
			if (Node == nullptr)
			{
				continue;
			}
			Out += FString::Printf(TEXT("node %s class %s pos %d,%d size %d,%d\n"),
				*Node->NodeGuid.ToString(EGuidFormats::DigitsLower), *ClassNameOf(Node),
				Node->NodePosX, Node->NodePosY, Node->NodeWidth, Node->NodeHeight);

			for (int32 PinIndex = 0; PinIndex < Node->Pins.Num(); ++PinIndex)
			{
				const UEdGraphPin* Pin = Node->Pins[PinIndex];
				if (Pin == nullptr)
				{
					continue;
				}
				const FString PinId = Pin->PinId.ToString(EGuidFormats::DigitsLower);
				TArray<FString> Links;
				for (const UEdGraphPin* Linked : Pin->LinkedTo)
				{
					if (Linked != nullptr && Linked->GetOwningNode() != nullptr)
					{
						Links.Add(FString::Printf(TEXT("%s|%s"),
							*Linked->GetOwningNode()->NodeGuid.ToString(EGuidFormats::DigitsLower),
							*Linked->PinId.ToString(EGuidFormats::DigitsLower)));
					}
				}
				Links.Sort();

				const FString SubObjectPath = Pin->PinType.PinSubCategoryObject != nullptr
					? Pin->PinType.PinSubCategoryObject->GetPathName() : FString();
				const FString DefaultObjectPath = Pin->DefaultObject != nullptr
					? Pin->DefaultObject->GetPathName() : FString();
				// The two directions are named by the comparison every other reader uses, rather than by a
				// cast whose spelling depends on the engine type behind `Direction`.
				const int32 DirectionValue = Pin->Direction == EGPD_Input ? 0 : 1;
				Out += FString::Printf(
					TEXT("  pin %s id %s dir %d index %d type %s|%s|%s container %d ref %d default '%s' object '%s' text '%s' links [%s]\n"),
					*Pin->PinName.ToString(), *PinId, DirectionValue, PinIndex,
					*Pin->PinType.PinCategory.ToString(), *Pin->PinType.PinSubCategory.ToString(),
					*SubObjectPath, static_cast<int32>(Pin->PinType.ContainerType),
					Pin->PinType.bIsReference ? 1 : 0, *Pin->DefaultValue, *DefaultObjectPath,
					*Pin->DefaultTextValue.ToString(), *FString::Join(Links, TEXT(",")));
			}
		}
		return Out;
	}

	FString DescribeFirstDifference(const FString& InBefore, const FString& InAfter)
	{
		TArray<FString> BeforeLines;
		TArray<FString> AfterLines;
		InBefore.ParseIntoArrayLines(BeforeLines);
		InAfter.ParseIntoArrayLines(AfterLines);
		const int32 Count = FMath::Max(BeforeLines.Num(), AfterLines.Num());
		for (int32 Index = 0; Index < Count; ++Index)
		{
			const FString Before = BeforeLines.IsValidIndex(Index) ? BeforeLines[Index] : TEXT("<missing>");
			const FString After = AfterLines.IsValidIndex(Index) ? AfterLines[Index] : TEXT("<missing>");
			if (Before != After)
			{
				return FString::Printf(TEXT("line %d: before [%s] after [%s]"), Index + 1, *Before, *After);
			}
		}
		return TEXT("no textual difference");
	}

	bool BuildFixture(FFixture& OutFixture, FString& OutError)
	{
		const FString Suffix = FGuid::NewGuid().ToString(EGuidFormats::DigitsLower);
		const FString Leaf = FString::Printf(TEXT("TurboMCPAppendAlias_%s"), *Suffix);
		const FString PackageName = FString::Printf(TEXT("/Game/Tests/%s"), *Leaf);

		UPackage* Package = CreatePackage(*PackageName);
		if (Package == nullptr)
		{
			OutError = TEXT("could not create the in-memory package");
			return false;
		}
		// EXPLICITLY TRANSIENT, AND STILL ADDRESSABLE. The package keeps its /Game name because the resolver
		// resolves a loaded object by that path; the transient flag keeps it out of save and asset views.
		Package->SetFlags(RF_Transient);
		OutFixture.Package = Package;

		OutFixture.Blueprint = FKismetEditorUtilities::CreateBlueprint(
			AActor::StaticClass(), Package, FName(*Leaf), BPTYPE_Normal,
			UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass(),
			TEXT("TurboMCPGraphAppendAliasTest"));
		if (OutFixture.Blueprint == nullptr)
		{
			OutError = TEXT("could not create the in-memory Blueprint");
			return false;
		}
		// `CreateBlueprint` marks the asset standalone; making it transient as well matches the engine's own
		// temporary-Blueprint pattern.
		OutFixture.Blueprint->SetFlags(RF_Transient);
		OutFixture.AssetPath = OutFixture.Blueprint->GetPathName();

		OutFixture.Forward = AddFunctionGraph(OutFixture.Blueprint, TEXT("CollideForward"),
			GuidEntryForward, GuidResultForward);
		OutFixture.Reverse = AddFunctionGraph(OutFixture.Blueprint, TEXT("CollideReverse"),
			GuidEntryReverse, GuidResultReverse);
		OutFixture.Unique = AddFunctionGraph(OutFixture.Blueprint, TEXT("UniqueAlias"),
			GuidEntryUnique, GuidResultUnique);
		if (OutFixture.Forward == nullptr || OutFixture.Reverse == nullptr || OutFixture.Unique == nullptr)
		{
			OutError = TEXT("could not create the fixture function graphs");
			return false;
		}

		// FORWARD: A then B. REVERSE: B then A. The collision is identical; only the array order differs.
		OutFixture.ForwardComment = PlaceComment(OutFixture.Forward, GuidCommentForward, 40, 40, 640, 360);
		OutFixture.ForwardA = PlacePrintString(OutFixture.Forward, GuidForwardA, 120, 120);
		OutFixture.ForwardB = PlacePrintString(OutFixture.Forward, GuidForwardB, 120, 260);

		PlaceComment(OutFixture.Reverse, GuidCommentReverse, 40, 40, 640, 360);
		OutFixture.ReverseB = PlacePrintString(OutFixture.Reverse, GuidReverseB, 120, 260);
		OutFixture.ReverseA = PlacePrintString(OutFixture.Reverse, GuidReverseA, 120, 120);

		PlaceComment(OutFixture.Unique, GuidCommentUnique, 40, 40, 640, 360);
		OutFixture.UniqueNode = PlacePrintString(OutFixture.Unique, GuidUniqueNode, 120, 120);

		if (OutFixture.ForwardA == nullptr || OutFixture.ForwardB == nullptr
			|| OutFixture.ReverseA == nullptr || OutFixture.ReverseB == nullptr
			|| OutFixture.UniqueNode == nullptr)
		{
			OutError = TEXT("could not place the fixture nodes");
			return false;
		}

		// A KNOWN LINK AND NON-DEFAULT PIN DATA, so the refusal snapshot has something it could lose rather
		// than only empty pins. `TryCreateConnection` is the schema's own path and reports success.
		const UEdGraphSchema* Schema = OutFixture.Forward->GetSchema();
		UEdGraphPin* ThenPin = OutFixture.ForwardA->FindPin(TEXT("then"), EGPD_Output);
		UEdGraphPin* ExecPin = OutFixture.ForwardB->FindPin(TEXT("execute"), EGPD_Input);
		UEdGraphPin* InStringPin = OutFixture.ForwardB->FindPin(TEXT("InString"), EGPD_Input);
		UEdGraphPin* DurationPin = OutFixture.ForwardB->FindPin(TEXT("Duration"), EGPD_Input);
		if (Schema == nullptr || ThenPin == nullptr || ExecPin == nullptr
			|| InStringPin == nullptr || DurationPin == nullptr)
		{
			OutError = TEXT("the seeded fixture pins are missing");
			return false;
		}
		if (!Schema->TryCreateConnection(ThenPin, ExecPin))
		{
			OutError = TEXT("the seeded fixture link could not be made");
			return false;
		}
		InStringPin->DefaultValue = TEXT("seeded default");
		InStringPin->DefaultTextValue = FText::FromString(TEXT("seeded text"));
		DurationPin->DefaultValue = TEXT("2.5");

		return true;
	}

	/** Runs the production plan and captures the graph before and after it. Nothing is applied or saved. */
	FPlanResult PlanText(UBlueprint* InBlueprint, UEdGraph* InGraph, const FString& InText)
	{
		FPlanResult Result;
		if (InBlueprint == nullptr || InGraph == nullptr)
		{
			Result.Error = TEXT("no Blueprint or graph");
			return Result;
		}

		// START CLEAN, so a dirty flag the plan raises is visible rather than hidden by fixture construction.
		UPackage* Package = InGraph->GetOutermost();
		if (Package != nullptr)
		{
			Package->SetDirtyFlag(false);
		}

		Result.GraphBefore = SerializeGraph(InGraph);
		Result.bPlanned = FTurboMCPGraphAppend::Plan(
			InBlueprint->GetPathName(), InGraph->GetName(), InText, Result.Plan, Result.Error);
		Result.GraphAfter = SerializeGraph(InGraph);
		Result.bDirtyAfter = Package != nullptr && Package->IsDirty();
		return Result;
	}

	FString LinkText(const FString& InNodeToken)
	{
		return FString::Printf(TEXT("(Development|PrintString\n    execute (-> %s:then))"), *InNodeToken);
	}

	FString LabelledLinkText(const FString& InLabel)
	{
		return FString::Printf(
			TEXT("(Development|PrintString\n    ; label %s\n    InString \"first\")\n")
			TEXT("(Development|PrintString\n    execute (-> %s:then))"),
			*InLabel, *InLabel);
	}

	const TCHAR* TypeIdLinkText =
		TEXT("(Development|PrintString\n    InString \"first\")\n")
		TEXT("(Development|PrintString\n    execute (-> Development|PrintString:then))");
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTurboMCPGraphAppendAliasTest,
	"TurboMCP.GraphAppend.AliasCollision",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTurboMCPGraphAppendAliasTest::RunTest(const FString& Parameters)
{

    // The graph-text escape layer must preserve Unreal's inner struct syntax exactly.
    const TArray<FString> LiteralCases = {
        TEXT("(TagName=\"UI.Menu.Pause\")"),
        TEXT("quoted \"value; still content\" and C:\\Temp\\file"),
        TEXT("line one\nline two\tend\r"),
        TEXT("")
    };
    for (const FString& Expected : LiteralCases)
    {
        const FString Encoded = TurboMCPGraphTextLex::QuoteLiteral(Expected);
        FString Name, Value, Decoded, Error;
        int32 Column = 0;
        TestTrue(TEXT("parse encoded literal with trailing comment"),
            TurboMCPGraphTextLex::ParsePinLine(TEXT("Value ") + Encoded + TEXT(" ; in 0)"), Name, Value, Column));
        TestTrue(TEXT("decode quoted literal"), TurboMCPGraphTextLex::DecodeLiteral(Value, Decoded, Error));
        TestEqual(TEXT("literal preserves exact content"), Decoded, Expected);
    }
    FString InvalidDecoded, InvalidError;
    TestFalse(TEXT("unknown escape refuses"), TurboMCPGraphTextLex::DecodeLiteral(TEXT("\"bad\\q\""), InvalidDecoded, InvalidError));
    TestFalse(TEXT("unescaped inner quote refuses"), TurboMCPGraphTextLex::DecodeLiteral(TEXT("\"bad\"quote\""), InvalidDecoded, InvalidError));

	FFixture Fixture;
	// CLEANUP IS SCOPED, NOT A SUCCESS-ONLY TAIL: every return below, including a partial fixture, runs it.
	FScopedFixtureCleanup Cleanup(Fixture);
	FString FixtureError;
	if (!BuildFixture(Fixture, FixtureError))
	{
		AddError(FString::Printf(TEXT("the append alias fixture could not be built: %s"), *FixtureError));
		return false;
	}

	// THE IN-MEMORY BLUEPRINT MUST BE ADDRESSABLE THE WAY A TOOL CALL ADDRESSES AN ASSET, or every check
	// below would be measuring a different resolver than the production one.
	FTurboMCPAssetResolver::FResolved Resolved;
	const FString ResolveError = FTurboMCPAssetResolver::Resolve(Fixture.AssetPath, Resolved);
	if (!ResolveError.IsEmpty())
	{
		AddError(FString::Printf(
			TEXT("the in-memory fixture does not resolve by its full object path: %s"), *ResolveError));
		return false;
	}


    // Exercise the public entry points and inspect actual stored defaults independently.
    for (const FName FunctionName : { FName(TEXT("MakeLiteralString")), FName(TEXT("MakeLiteralGameplayTag")) })
    {
        const bool bTag = FunctionName == TEXT("MakeLiteralGameplayTag");
        UClass* Library = bTag ? LoadObject<UClass>(nullptr, TEXT("/Script/GameplayTags.BlueprintGameplayTagLibrary"))
            : UKismetSystemLibrary::StaticClass();
        UFunction* Function = Library ? Library->FindFunctionByName(FunctionName) : nullptr;
        if (!TestNotNull(TEXT("literal fixture function exists"), Function)) return false;
        UK2Node_CallFunction* Probe = NewObject<UK2Node_CallFunction>(Fixture.Unique);
        Probe->SetFromFunction(Function);
        Probe->CreateNewGuid();
        Probe->AllocateDefaultPins();
        const FString TypeId = TurboMCPNodeId::GetNodeTypeId(Probe);
        const FString Expected = bTag ? TEXT("(TagName=\"UI.Menu.Pause\")") : TEXT("quoted \"value; still content\" and C:\\Temp\\file");
        const FString Request = FString::Printf(TEXT("(%s\n    Value %s\n)"),
            *TypeId, *TurboMCPGraphTextLex::QuoteLiteral(Expected));
        const FString Before = SerializeGraph(Fixture.Unique);
        const FString Preview = UTurboMCPPreviewToolset::PreviewTurboMCPGraphAppend(Fixture.AssetPath, Fixture.Unique->GetName(), Request);
        TestTrue(FString::Printf(TEXT("literal preview accepted: %s"), *Preview), Preview.StartsWith(TEXT("preview:")));
        TestEqual(TEXT("literal preview preserves graph"), SerializeGraph(Fixture.Unique), Before);
        const FString Edited = UTurboMCPEditToolset::EditTurboMCPGraphAppend(Fixture.AssetPath, Fixture.Unique->GetName(), Request);
        TestTrue(FString::Printf(TEXT("literal edit accepted: %s"), *Edited), Edited.StartsWith(TEXT("changed:")));
        int32 MatchingDefaults = 0;
        for (UEdGraphNode* Node : Fixture.Unique->Nodes)
            if (UK2Node_CallFunction* Call = Cast<UK2Node_CallFunction>(Node))
                if (Call->GetTargetFunction() == Function)
                    if (UEdGraphPin* ValuePin = Call->FindPin(TEXT("Value"), EGPD_Input))
                        if (ValuePin->DefaultValue == Expected) ++MatchingDefaults;
        TestEqual(TEXT("one appended node stores the exact decoded value"), MatchingDefaults, 1);

        UK2Node_CallFunction* WrittenCall = nullptr;
        UK2Node_FunctionResult* ResultNode = nullptr;
        UK2Node_FunctionEntry* EntryNode = nullptr;
        for (UEdGraphNode* Node : Fixture.Unique->Nodes)
        {
            if (UK2Node_CallFunction* Call = Cast<UK2Node_CallFunction>(Node))
                if (Call->GetTargetFunction() == Function) WrittenCall = Call;
            if (auto* Result = Cast<UK2Node_FunctionResult>(Node)) ResultNode = Result;
            if (auto* Entry = Cast<UK2Node_FunctionEntry>(Node)) EntryNode = Entry;
        }
        if (!TestNotNull(TEXT("written call available for readback"), WrittenCall)
            || !TestNotNull(TEXT("fixture return node"), ResultNode)
            || !TestNotNull(TEXT("fixture entry node"), EntryNode)) return false;
        UEdGraphPin* ReturnedValue = WrittenCall->FindPin(TEXT("ReturnValue"), EGPD_Output);
        if (!TestNotNull(TEXT("literal return value"), ReturnedValue)) return false;
        UEdGraphPin* Output = ResultNode->CreateUserDefinedPin(bTag ? TEXT("TagResult") : TEXT("TextResult"), ReturnedValue->PinType, EGPD_Input, false);
        const UEdGraphSchema* Schema = Fixture.Unique->GetSchema();
        TestTrue(TEXT("connect literal to observable return"), Schema->TryCreateConnection(ReturnedValue, Output));
        TestTrue(TEXT("connect function return execution"), Schema->TryCreateConnection(
            EntryNode->FindPin(TEXT("then"), EGPD_Output), ResultNode->FindPin(TEXT("execute"), EGPD_Input)));
        const FString Readback = UTurboMCPReadToolset::ReadTurboMCPGraphLoad(Fixture.AssetPath, Fixture.Unique->GetName(), TEXT("read"), TEXT("full"));
        TestTrue(FString::Printf(TEXT("reader emits escaped stored literal: %s"), *Readback), Readback.Contains(TurboMCPGraphTextLex::QuoteLiteral(Expected)));
        const FString PatchedValue = bTag ? TEXT("(TagName=\"UI.Menu.Resume\")") : TEXT("patched \"quoted; text\" C:\\Temp\\next");
        const FString PatchRequest = FString::Printf(TEXT("(%s ; %s\n    Value %s\n)"),
            *TypeId, *TurboMCPNodeId::MakeForNode(WrittenCall, true), *TurboMCPGraphTextLex::QuoteLiteral(PatchedValue));
        const FString PatchResult = UTurboMCPEditToolset::EditTurboMCPGraphPatch(Fixture.AssetPath, Fixture.Unique->GetName(), PatchRequest);
        TestTrue(FString::Printf(TEXT("patch accepts escaped literal: %s"), *PatchResult), PatchResult.StartsWith(TEXT("changed:")));
        TestEqual(TEXT("patch stores exact decoded value"), WrittenCall->FindPin(TEXT("Value"), EGPD_Input)->DefaultValue, PatchedValue);

        const FString BeforeRefusal = SerializeGraph(Fixture.Unique);
        const FString Invalid = FString::Printf(TEXT("(%s\n    Value \"bad\\q\"\n)"), *TypeId);
        const FString Refused = UTurboMCPEditToolset::EditTurboMCPGraphAppend(Fixture.AssetPath, Fixture.Unique->GetName(), Invalid);
        TestTrue(TEXT("unsupported escape reports error"), Refused.StartsWith(TEXT("error:")));
        TestEqual(TEXT("invalid escape preserves graph"), SerializeGraph(Fixture.Unique), BeforeRefusal);
    }

	// A variable with a space must round-trip through its exact discovered type ID.
	FEdGraphPinType SpacedType;
	SpacedType.PinCategory = UEdGraphSchema_K2::PC_Int;
	TestTrue(TEXT("create spaced variable"), FBlueprintEditorUtils::AddMemberVariable(Fixture.Blueprint, TEXT("Purchase Count"), SpacedType));
	FKismetEditorUtilities::CompileBlueprint(Fixture.Blueprint);
	// Unreal excludes transient assets and packages from its action catalogue.
	// Temporarily classify this unsaved fixture as an asset only for registration.
	Fixture.Blueprint->ClearFlags(RF_Transient);
	Fixture.Package->ClearFlags(RF_Transient);
	FBlueprintActionDatabase::Get().RefreshAssetActions(Fixture.Blueprint);
	Fixture.Package->SetFlags(RF_Transient);
	Fixture.Blueprint->SetFlags(RF_Transient);
	UK2Node_VariableGet* Getter = NewObject<UK2Node_VariableGet>(Fixture.Unique);
	Getter->VariableReference.SetSelfMember(TEXT("Purchase Count"));
	Getter->CreateNewGuid();
	Fixture.Unique->AddNode(Getter, false, false);
	Getter->AllocateDefaultPins();
	const FString SpacedId = TurboMCPNodeId::GetVariableNodeTypeId(Getter);
	TestTrue(TEXT("discovered type ID preserves space"), SpacedId.Contains(TEXT("Purchase Count")));
	const FString QuotedText = FString::Printf(TEXT("(\"%s\"\n)"), *SpacedId);
	FTurboMCPGraphAppend::FPlan SpacedPlan;
	FString SpacedError;
	TestTrue(TEXT("append accepts quoted discovered ID"), FTurboMCPGraphAppend::Plan(Fixture.AssetPath, Fixture.Unique->GetName(), QuotedText, SpacedPlan, SpacedError));
	TestEqual(TEXT("quoted getter produces one planned node"), SpacedPlan.Nodes.Num(), 1);
	TestTrue(FString::Printf(TEXT("quoted getter has no refusals: %s"), *FString::Join(SpacedPlan.Refusals, TEXT("; "))), SpacedPlan.Refusals.IsEmpty());
	FTurboMCPGraphAppend::FPlan BrokenPlan;
	TestFalse(TEXT("unterminated quoted ID refuses"), FTurboMCPGraphAppend::Plan(Fixture.AssetPath, Fixture.Unique->GetName(), TEXT("(\"unterminated\n)"), BrokenPlan, SpacedError));
	FString LexId; int32 LexColumn = 0, LexAfter = 0;
	TestTrue(TEXT("quoted ID permits closing parenthesis"), TurboMCPGraphTextLex::ExtractHeadTypeId(TEXT("(\"Example ID\")"), LexId, LexColumn, LexAfter));
	TestEqual(TEXT("quoted ID decoded"), LexId, FString(TEXT("Example ID")));

	const FString ForwardAKey = TurboMCPNodeId::MakeForNode(Fixture.ForwardA);
	const FString ForwardBKey = TurboMCPNodeId::MakeForNode(Fixture.ForwardB);
	const FString ReverseAKey = TurboMCPNodeId::MakeForNode(Fixture.ReverseA);
	const FString ReverseBKey = TurboMCPNodeId::MakeForNode(Fixture.ReverseB);

	// THE PREFIX LENGTH IS ASSERTED FROM THE FORCED GUIDS, NOT FROM A FIXED STRIDE. `UniqueKeyDigits`
	// grows one digit at a time while another node shares the prefix, so the answer depends on where the
	// two GUIDs first differ.
	TestEqual(TEXT("the forward pair shares exactly six characters"),
		TurboMCPNodeId::MakeKey(Fixture.ForwardA->NodeGuid), TEXT("abcdef"));
	TestEqual(TEXT("the forward A key grows to eight characters"),
		TurboMCPNodeId::UniqueKeyDigits(Fixture.Forward, Fixture.ForwardA->NodeGuid), 8);
	TestEqual(TEXT("the forward B key grows to eight characters"),
		TurboMCPNodeId::UniqueKeyDigits(Fixture.Forward, Fixture.ForwardB->NodeGuid), 8);
	TestTrue(TEXT("the forward A qualified key is the eight-character one"),
		ForwardAKey.EndsWith(TEXT("abcdef01")));
	TestTrue(TEXT("the forward B qualified key is the eight-character one"),
		ForwardBKey.EndsWith(TEXT("abcdef02")));
	TestTrue(TEXT("the two qualified keys differ even though the aliases collide"),
		ForwardAKey != ForwardBKey);
	TestEqual(TEXT("the reverse pair shares exactly six characters"),
		TurboMCPNodeId::MakeKey(Fixture.ReverseA->NodeGuid), TEXT("abcdef"));
	TestEqual(TEXT("the reverse A key grows to eight characters"),
		TurboMCPNodeId::UniqueKeyDigits(Fixture.Reverse, Fixture.ReverseA->NodeGuid), 8);
	TestTrue(TEXT("the reverse A qualified key is the eight-character one"),
		ReverseAKey.EndsWith(TEXT("abcdef11")));
	TestTrue(TEXT("the reverse B qualified key is the eight-character one"),
		ReverseBKey.EndsWith(TEXT("abcdef12")));
	TestEqual(TEXT("the unique node keeps the standard six-character alias"),
		TurboMCPNodeId::MakeKey(Fixture.UniqueNode->NodeGuid), TEXT("ab12cd"));
	TestEqual(TEXT("the unique node needs no extra digits"),
		TurboMCPNodeId::UniqueKeyDigits(Fixture.Unique, Fixture.UniqueNode->NodeGuid), 6);

	// THE COMMENT BOX IS PART OF THE SNAPSHOT, so its bounds must be real rather than zero.
	bool bFoundComment = false;
	for (const UEdGraphNode* Node : Fixture.Forward->Nodes)
	{
		if (const UEdGraphNode_Comment* Comment = Cast<UEdGraphNode_Comment>(Node))
		{
			bFoundComment = true;
			TestEqual(TEXT("the fixture comment carries its width"), Comment->NodeWidth, 640);
			TestEqual(TEXT("the fixture comment carries its height"), Comment->NodeHeight, 360);
		}
	}
	TestTrue(TEXT("the fixture contains a comment box, so comment bounds are snapshotted"), bFoundComment);
	TestTrue(TEXT("the fixture stores its comment for the snapshot-negative check"),
		Fixture.ForwardComment != nullptr);
	if (Fixture.ForwardComment == nullptr)
	{
		AddError(TEXT("the fixture comment is missing, so the position-negative check cannot run"));
		return false;
	}

	// ---- 0. THE FIXTURE HAS A REAL LINK AND NON-DEFAULT DATA TO LOSE -----------------------------------
	UEdGraphPin* SeedThen = Fixture.ForwardA->FindPin(TEXT("then"), EGPD_Output);
	UEdGraphPin* SeedExec = Fixture.ForwardB->FindPin(TEXT("execute"), EGPD_Input);
	UEdGraphPin* SeedString = Fixture.ForwardB->FindPin(TEXT("InString"), EGPD_Input);
	UEdGraphPin* SeedDuration = Fixture.ForwardB->FindPin(TEXT("Duration"), EGPD_Input);
	TestTrue(TEXT("the seeded endpoint pins exist"), SeedThen != nullptr && SeedExec != nullptr);
	TestTrue(TEXT("the seeded string and duration pins exist"), SeedString != nullptr && SeedDuration != nullptr);
	if (SeedThen == nullptr || SeedExec == nullptr || SeedString == nullptr || SeedDuration == nullptr)
	{
		AddError(TEXT("the fixture failed its own pin preconditions, so the snapshot checks would be vacuous"));
		return false;
	}
	TestTrue(TEXT("the seeded link is present in both directions"),
		SeedThen->LinkedTo.Contains(SeedExec) && SeedExec->LinkedTo.Contains(SeedThen));
	TestTrue(TEXT("the seeded endpoint pins have stable identities"),
		SeedThen->PinId.IsValid() && SeedExec->PinId.IsValid());
	TestTrue(TEXT("the seeded string default is non-default"),
		SeedString->DefaultValue == TEXT("seeded default"));
	TestTrue(TEXT("the seeded text default is non-empty"),
		SeedString->DefaultTextValue.ToString() == TEXT("seeded text"));
	TestTrue(TEXT("the seeded duration default is non-default"),
		SeedDuration->DefaultValue == TEXT("2.5"));

	// ---- 0b. THE SNAPSHOT ITSELF MUST BITE: UNRELATED CHANGES ARE DETECTABLE ---------------------------
	const FString SeededSnapshot = SerializeGraph(Fixture.Forward);
	TestTrue(TEXT("the snapshot names the seeded source pin identity"),
		SeededSnapshot.Contains(SeedThen->PinId.ToString(EGuidFormats::DigitsLower)));

	const int32 OriginalCommentX = Fixture.ForwardComment->NodePosX;
	Fixture.ForwardComment->NodePosX = OriginalCommentX + 40;
	TestTrue(TEXT("an unrelated position change is detectable"),
		SerializeGraph(Fixture.Forward) != SeededSnapshot);
	Fixture.ForwardComment->NodePosX = OriginalCommentX;
	TestTrue(TEXT("restoring the position restores the snapshot"),
		SerializeGraph(Fixture.Forward) == SeededSnapshot);

	const UEdGraphSchema* SeedSchema = Fixture.Forward->GetSchema();
	TestTrue(TEXT("the fixture graph exposes its schema"), SeedSchema != nullptr);
	if (SeedSchema != nullptr)
	{
		SeedSchema->BreakSinglePinLink(SeedThen, SeedExec);
		TestTrue(TEXT("an unrelated endpoint change is detectable"),
			SerializeGraph(Fixture.Forward) != SeededSnapshot);
		TestTrue(TEXT("the seeded link really was broken"),
			!SeedThen->LinkedTo.Contains(SeedExec) && !SeedExec->LinkedTo.Contains(SeedThen));
		SeedSchema->TryCreateConnection(SeedThen, SeedExec);
		TestTrue(TEXT("the seeded link was restored"),
			SeedThen->LinkedTo.Contains(SeedExec) && SeedExec->LinkedTo.Contains(SeedThen));
		TestTrue(TEXT("restoring the endpoint restores the snapshot"),
			SerializeGraph(Fixture.Forward) == SeededSnapshot);
	}

	// ---- 1. THE AMBIGUOUS ALIAS REFUSES, IN BOTH INSERTION ORDERS -------------------------------------
	const FPlanResult ForwardRefusal = PlanText(Fixture.Blueprint, Fixture.Forward, LinkText(TEXT("abcdef")));
	TestTrue(TEXT("a plan is built for the ambiguous alias"), ForwardRefusal.bPlanned);
	TestEqual(TEXT("the ambiguous alias produces exactly one refusal"),
		ForwardRefusal.Plan.Refusals.Num(), 1);
	TestTrue(TEXT("the ambiguous alias is not an acceptable plan"),
		!ForwardRefusal.Plan.bAcceptable());
	TestEqual(TEXT("the ambiguous alias plans no link"), ForwardRefusal.Plan.Links.Num(), 0);
	TestTrue(TEXT("a refused ambiguous plan retains no graph change"),
		ForwardRefusal.GraphBefore == ForwardRefusal.GraphAfter);
	if (ForwardRefusal.GraphBefore != ForwardRefusal.GraphAfter)
	{
		AddError(FString::Printf(TEXT("forward order: %s"),
			*DescribeFirstDifference(ForwardRefusal.GraphBefore, ForwardRefusal.GraphAfter)));
	}
	if (ForwardRefusal.Plan.Refusals.Num() == 1)
	{
		const FString& Reason = ForwardRefusal.Plan.Refusals[0];
		TestTrue(TEXT("the refusal points at the link line and its pin column"),
			Reason.Contains(TEXT("line 2, column 5")));
		TestTrue(TEXT("the refusal says the alias fits two nodes"), Reason.Contains(TEXT("fits 2 nodes")));
		TestTrue(TEXT("the refusal says the alias is ambiguous"), Reason.Contains(TEXT("ambiguous")));
		TestTrue(TEXT("the refusal names the first qualified candidate"), Reason.Contains(ForwardAKey));
		TestTrue(TEXT("the refusal names the second qualified candidate"), Reason.Contains(ForwardBKey));
		TestTrue(TEXT("the refusal is the ambiguity message, not the unknown-node message"),
			!Reason.Contains(TEXT("neither a node")));
	}

	const FPlanResult ReverseRefusal = PlanText(Fixture.Blueprint, Fixture.Reverse, LinkText(TEXT("abcdef")));
	TestTrue(TEXT("the reverse insertion order is refused too"), ReverseRefusal.bPlanned);
	TestEqual(TEXT("the reverse insertion order produces exactly one refusal"),
		ReverseRefusal.Plan.Refusals.Num(), 1);
	TestTrue(TEXT("the reverse refusal retains no graph change"),
		ReverseRefusal.GraphBefore == ReverseRefusal.GraphAfter);
	if (ReverseRefusal.GraphBefore != ReverseRefusal.GraphAfter)
	{
		AddError(FString::Printf(TEXT("reverse order: %s"),
			*DescribeFirstDifference(ReverseRefusal.GraphBefore, ReverseRefusal.GraphAfter)));
	}
	if (ReverseRefusal.Plan.Refusals.Num() == 1)
	{
		TestTrue(TEXT("the reverse refusal names the reverse first candidate"),
			ReverseRefusal.Plan.Refusals[0].Contains(ReverseAKey));
		TestTrue(TEXT("the reverse refusal names the reverse second candidate"),
			ReverseRefusal.Plan.Refusals[0].Contains(ReverseBKey));
	}

	// ---- 2. THE CONTROL: AN UNKNOWN NODE, WHICH ALREADY REFUSED BEFORE THIS REPAIR ---------------------
	const FPlanResult UnknownRefusal = PlanText(Fixture.Blueprint, Fixture.Forward, LinkText(TEXT("deadbeef")));
	TestTrue(TEXT("an unknown node token is refused as before"), UnknownRefusal.bPlanned);
	TestEqual(TEXT("an unknown node token produces exactly one refusal"),
		UnknownRefusal.Plan.Refusals.Num(), 1);
	TestTrue(TEXT("an unknown node token retains no graph change"),
		UnknownRefusal.GraphBefore == UnknownRefusal.GraphAfter);

	// THE DIRTY FLAG IS RECORDED, NOT ASSUMED CLEAN. `Plan` builds and removes a probe node, so a transient
	// engine-side mark is possible; what must hold is that an ambiguous alias behaves like the already
	// accepted refusal class rather than introducing a new transition.
	TestTrue(TEXT("the ambiguous alias changes the dirty flag exactly as an unknown node does"),
		ForwardRefusal.bDirtyAfter == UnknownRefusal.bDirtyAfter);
	AddInfo(FString::Printf(TEXT("dirty flag after a refused plan: ambiguous=%s unknown-node=%s"),
		ForwardRefusal.bDirtyAfter ? TEXT("true") : TEXT("false"),
		UnknownRefusal.bDirtyAfter ? TEXT("true") : TEXT("false")));

	// ---- 3. QUALIFIED KEYS FOR THE COLLIDING PAIR STILL RESOLVE ----------------------------------------
	const FPlanResult ShortKeyPlan = PlanText(Fixture.Blueprint, Fixture.Forward,
		LinkText(TurboMCPNodeId::MakeForNode(Fixture.ForwardA, /*bLongForm=*/false)));
	TestTrue(TEXT("a qualified short key is planned"), ShortKeyPlan.bPlanned);
	TestEqual(TEXT("a qualified short key produces no refusal"), ShortKeyPlan.Plan.Refusals.Num(), 0);
	TestTrue(TEXT("a qualified short key produces an acceptable plan"), ShortKeyPlan.Plan.bAcceptable());
	TestEqual(TEXT("a qualified short key plans one link"), ShortKeyPlan.Plan.Links.Num(), 1);
	TestTrue(TEXT("an accepted plan retains no graph change either"),
		ShortKeyPlan.GraphBefore == ShortKeyPlan.GraphAfter);
	if (ShortKeyPlan.Plan.Links.Num() == 1)
	{
		TestTrue(TEXT("the short key resolves to the canonical long key"),
			ShortKeyPlan.Plan.Links[0].FromNodeKey
				== TurboMCPNodeId::MakeForNode(Fixture.ForwardA, /*bLongForm=*/true));
	}

	const FPlanResult LongKeyPlan = PlanText(Fixture.Blueprint, Fixture.Forward,
		LinkText(TurboMCPNodeId::MakeForNode(Fixture.ForwardB, /*bLongForm=*/true)));
	TestTrue(TEXT("a qualified long key is planned"), LongKeyPlan.bPlanned);
	TestEqual(TEXT("a qualified long key produces no refusal"), LongKeyPlan.Plan.Refusals.Num(), 0);
	TestTrue(TEXT("a qualified long key produces an acceptable plan"), LongKeyPlan.Plan.bAcceptable());
	TestEqual(TEXT("a qualified long key plans one link"), LongKeyPlan.Plan.Links.Num(), 1);
	if (LongKeyPlan.Plan.Links.Num() == 1)
	{
		TestTrue(TEXT("the long key resolves to the canonical long key"),
			LongKeyPlan.Plan.Links[0].FromNodeKey
				== TurboMCPNodeId::MakeForNode(Fixture.ForwardB, /*bLongForm=*/true));
	}

	// ---- 4. THE UNIQUE SIX-CHARACTER ALIAS WORKS IN EITHER CASE ----------------------------------------
	const FString UniqueAlias = TurboMCPNodeId::MakeKey(Fixture.UniqueNode->NodeGuid);
	const FPlanResult LowerAliasPlan = PlanText(Fixture.Blueprint, Fixture.Unique, LinkText(UniqueAlias));
	TestTrue(TEXT("a lower-case unique alias is planned"), LowerAliasPlan.bPlanned);
	TestEqual(TEXT("a lower-case unique alias produces no refusal"), LowerAliasPlan.Plan.Refusals.Num(), 0);
	TestTrue(TEXT("a lower-case unique alias is acceptable"), LowerAliasPlan.Plan.bAcceptable());
	TestEqual(TEXT("a lower-case unique alias plans one link"), LowerAliasPlan.Plan.Links.Num(), 1);
	if (LowerAliasPlan.Plan.Links.Num() == 1)
	{
		TestTrue(TEXT("the lower-case alias resolves to its canonical key"),
			LowerAliasPlan.Plan.Links[0].FromNodeKey
				== TurboMCPNodeId::MakeForNode(Fixture.UniqueNode, /*bLongForm=*/true));
	}

	const FPlanResult UpperAliasPlan = PlanText(Fixture.Blueprint, Fixture.Unique,
		LinkText(UniqueAlias.ToUpper()));
	TestTrue(TEXT("an upper-case unique alias is planned"), UpperAliasPlan.bPlanned);
	TestEqual(TEXT("an upper-case unique alias produces no refusal"), UpperAliasPlan.Plan.Refusals.Num(), 0);
	TestTrue(TEXT("an upper-case unique alias is acceptable"), UpperAliasPlan.Plan.bAcceptable());
	TestEqual(TEXT("an upper-case unique alias plans one link"), UpperAliasPlan.Plan.Links.Num(), 1);

	// ---- 5. TOKENS OUTSIDE THE SIX-HEX NAMESPACE STILL FALL BACK TO LABELS AND TYPE IDS -----------------
	const FPlanResult LabelPlan = PlanText(Fixture.Blueprint, Fixture.Forward,
		LabelledLinkText(TEXT("First")));
	TestTrue(TEXT("a non-hex label is planned"), LabelPlan.bPlanned);
	TestEqual(TEXT("a non-hex label produces no refusal"), LabelPlan.Plan.Refusals.Num(), 0);
	TestTrue(TEXT("a non-hex label is acceptable"), LabelPlan.Plan.bAcceptable());
	TestEqual(TEXT("a non-hex label plans one link"), LabelPlan.Plan.Links.Num(), 1);
	if (LabelPlan.Plan.Links.Num() == 1)
	{
		TestEqual(TEXT("the non-hex label resolves to the first new form"),
			LabelPlan.Plan.Links[0].FromFormIndex, 0);
	}

	// SEVEN HEX CHARACTERS IS A LABEL, NOT AN ALIAS, even though it is also the shared seven-character
	// prefix of the colliding pair. A `NormaliseKey` call without the length test would swallow it.
	const FPlanResult LongHexLabelPlan = PlanText(Fixture.Blueprint, Fixture.Forward,
		LabelledLinkText(TEXT("abcdef0")));
	TestTrue(TEXT("a seven-character hex label is planned"), LongHexLabelPlan.bPlanned);
	TestEqual(TEXT("a seven-character hex label produces no refusal"),
		LongHexLabelPlan.Plan.Refusals.Num(), 0);
	TestEqual(TEXT("a seven-character hex label plans one link"), LongHexLabelPlan.Plan.Links.Num(), 1);
	if (LongHexLabelPlan.Plan.Links.Num() == 1)
	{
		TestEqual(TEXT("a seven-character hex label resolves to the new form, not to an alias"),
			LongHexLabelPlan.Plan.Links[0].FromFormIndex, 0);
	}

	const FPlanResult TypeIdPlan = PlanText(Fixture.Blueprint, Fixture.Forward, FString(TypeIdLinkText));
	TestTrue(TEXT("a type-id reference is planned"), TypeIdPlan.bPlanned);
	TestEqual(TEXT("a type-id reference produces no refusal"), TypeIdPlan.Plan.Refusals.Num(), 0);
	TestEqual(TEXT("a type-id reference plans one link"), TypeIdPlan.Plan.Links.Num(), 1);
	if (TypeIdPlan.Plan.Links.Num() == 1)
	{
		TestEqual(TEXT("the type-id reference resolves to the first new form"),
			TypeIdPlan.Plan.Links[0].FromFormIndex, 0);
	}

	// ---- 6. PRECEDENCE: AN AMBIGUOUS EXISTING ALIAS DOES NOT BECOME A SAME-SPELLING LABEL --------------
	const FPlanResult AliasAsLabelPlan = PlanText(Fixture.Blueprint, Fixture.Forward,
		LabelledLinkText(TEXT("abcdef")));
	TestTrue(TEXT("the ambiguous-alias-as-label text is planned"), AliasAsLabelPlan.bPlanned);
	TestEqual(TEXT("an ambiguous alias equal to a new label is still refused"),
		AliasAsLabelPlan.Plan.Refusals.Num(), 1);
	TestEqual(TEXT("the ambiguous alias does not silently bind to the label"),
		AliasAsLabelPlan.Plan.Links.Num(), 0);
	TestTrue(TEXT("the label-spelling refusal retains no graph change"),
		AliasAsLabelPlan.GraphBefore == AliasAsLabelPlan.GraphAfter);

	// ---- 7. AN UNREFERENCED COLLISION BLOCKS NOTHING ---------------------------------------------------
	const FPlanResult UnreferencedPlan = PlanText(Fixture.Blueprint, Fixture.Reverse,
		LinkText(ReverseBKey));
	TestTrue(TEXT("a qualified key in a graph with an unreferenced collision is planned"),
		UnreferencedPlan.bPlanned);
	TestEqual(TEXT("an unreferenced collision produces no refusal"),
		UnreferencedPlan.Plan.Refusals.Num(), 0);
	TestTrue(TEXT("an unreferenced collision produces an acceptable plan"),
		UnreferencedPlan.Plan.bAcceptable());
	TestEqual(TEXT("an unreferenced collision still plans one link"), UnreferencedPlan.Plan.Links.Num(), 1);

	const FPlanResult NoLinkPlan = PlanText(Fixture.Blueprint, Fixture.Forward,
		TEXT("(Development|PrintString\n    InString \"no link\")"));
	TestTrue(TEXT("a text with no link is planned in a graph with a collision"), NoLinkPlan.bPlanned);
	TestEqual(TEXT("a text with no link produces no refusal"), NoLinkPlan.Plan.Refusals.Num(), 0);
	TestTrue(TEXT("a text with no link is acceptable"), NoLinkPlan.Plan.bAcceptable());

	// THE FIXTURE IS IN MEMORY ONLY. The scoped cleanup above clears its dirty flag and retires its own
	// objects after this return, including every early return; nothing is saved or registered as an asset.
	AddInfo(FString::Printf(TEXT("the fixture %s is transient, was never saved and never called Apply"),
		*Fixture.AssetPath));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
