// Copyright FloStyle. All Rights Reserved.

#include "TurboMCPAcceptanceAssetCommandlet.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Containers/UnrealString.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "GameFramework/Actor.h"
#include "K2Node_CallFunction.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "K2Node_Knot.h"
#include "K2Node_VariableGet.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/PackageName.h"
#include "Modules/ModuleManager.h"
#include "TurboMCPModule.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "UObject/UObjectGlobals.h"

#include "Logging.h"

#define LOCTEXT_NAMESPACE "TurboMCPAcceptanceAsset"

namespace
{
	const TCHAR* const PackagePath = TEXT("/Game/Tests/TurboMCPAcceptance");

	/**
	 * NOTHING HERE FORCES THE MODULE TO LOAD, AND THAT IS A MEASURED CONCLUSION RATHER THAN AN
	 * OMISSION.
	 *
	 * The engine's commandlet lookup infers a module name from the commandlet name
	 * (`TurboMCPAcceptanceAsset`), calls `LoadModule` for it, finds nothing, and reports
	 * "looked like a commandlet, but we could not find the class" — it does not know to load THIS
	 * plugin, and the `.uplugin`'s `PostEngineInit` phase is later than the lookup.
	 *
	 * A STATIC INITIALISER THAT CALLS `LoadModule` WAS TRIED AND CRASHED. Measured:
	 *
	 *     Fatal error! Unhandled Exception: EXCEPTION_ACCESS_VIOLATION
	 *     FModuleManager::LoadModuleWithFailureReason() ModuleManager.cpp:1222
	 *
	 * A static initialiser inside a module that is being loaded runs DURING that load, so asking the
	 * manager to load the same module again re-enters it before it is on the list. The manager does
	 * not detect the in-progress load here.
	 *
	 * So the commandlet is built and reachable by `-run=` once the module is loaded by other means —
	 * the same `-ExecCmds` route the engine's own hash-mapping commandlet uses — and the acceptance
	 * asset is produced by a Python commandlet script instead, which is the route phase 2 already
	 * proved works for `make-test-blueprint.py`.
	 */

	/** Puts a node in a graph at a position, with a fresh GUID. */
	void Place(UEdGraph* InGraph, UEdGraphNode* InNode, double InX, double InY)
	{
		InNode->CreateNewGuid();
		InNode->NodePosX = static_cast<int32>(InX);
		InNode->NodePosY = static_cast<int32>(InY);
		InGraph->AddNode(InNode, /*bUserAction=*/false, /*bSelectNewNode=*/false);
	}

	/** A function graph with an entry and a result node, ready to be wired. */
	UEdGraph* MakeFunction(UBlueprint* InBlueprint, const FName InName,
		UK2Node_FunctionEntry*& OutEntry, UK2Node_FunctionResult*& OutResult)
	{
		UEdGraph* Graph = FBlueprintEditorUtils::CreateNewGraph(
			InBlueprint, InName, UEdGraph::StaticClass(), UEdGraphSchema_K2::StaticClass());
		if (Graph == nullptr)
		{
			return nullptr;
		}
		FBlueprintEditorUtils::AddFunctionGraph<UFunction>(InBlueprint, Graph,
			/*bIsUserCreated=*/true, /*SignatureFromObject=*/nullptr);

		OutEntry = nullptr;
		OutResult = nullptr;
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (UK2Node_FunctionEntry* Entry = Cast<UK2Node_FunctionEntry>(Node))
			{
				OutEntry = Entry;
			}
			else if (UK2Node_FunctionResult* Result = Cast<UK2Node_FunctionResult>(Node))
			{
				OutResult = Result;
			}
		}
		return Graph;
	}

	/**
	 * `KnotProbe`: two PrintString nodes far apart, joined by THREE knots.
	 *
	 * A knot is what `P/E-GRAPH-KNOTS` exists to place and what `write_graph_dsl` refuses to make, so
	 * this is the only way the capability can be pointed at one. The knots are placed level with the
	 * wire rather than on top of each other, which is the shape the tool is meant to produce.
	 */
	bool BuildKnotProbe(UBlueprint* InBlueprint)
	{
		UK2Node_FunctionEntry* Entry = nullptr;
		UK2Node_FunctionResult* Result = nullptr;
		UEdGraph* Graph = MakeFunction(InBlueprint, TEXT("KnotProbe"), Entry, Result);
		if (Graph == nullptr || Entry == nullptr)
		{
			return false;
		}

		UK2Node_CallFunction* First = NewObject<UK2Node_CallFunction>(Graph);
		First->SetFromFunction(UKismetSystemLibrary::StaticClass()->FindFunctionByName(
			TEXT("PrintString")));
		Place(Graph, First, 400.0, 0.0);

		UK2Node_CallFunction* Second = NewObject<UK2Node_CallFunction>(Graph);
		Second->SetFromFunction(UKismetSystemLibrary::StaticClass()->FindFunctionByName(
			TEXT("PrintString")));
		// Far to the right AND far down, so a straight wire from the first would cross the graph
		// diagonally. That is the case a knot improves and the case the tool must find.
		Place(Graph, Second, 1600.0, 600.0);

		// The entry's exec output drives the first call.
		if (Entry->GetThenPin() != nullptr && First->GetExecPin() != nullptr)
		{
			Entry->GetThenPin()->MakeLinkTo(First->GetExecPin());
		}

		// THREE KNOTS IN A ROW between the two calls.
		UEdGraphPin* From = First->GetThenPin();
		for (int32 Index = 0; Index < 3; ++Index)
		{
			UK2Node_Knot* Knot = NewObject<UK2Node_Knot>(Graph);
			Place(Graph, Knot, 700.0 + Index * 300.0, 200.0 + Index * 130.0);
			if (From != nullptr && Knot->GetInputPin() != nullptr)
			{
				From->MakeLinkTo(Knot->GetInputPin());
			}
			From = Knot->GetOutputPin();
		}
		if (From != nullptr && Second->GetExecPin() != nullptr)
		{
			From->MakeLinkTo(Second->GetExecPin());
		}

		// The result node is left unlinked on purpose: this graph is about the wire, not the return.
		return true;
	}

	/**
	 * `DeadProbe`: a group of nodes that NOTHING reaches, with every link broken.
	 *
	 * `P/E-GRAPH-DEAD` removes disconnected groups whose writes are never read. The engine's own
	 * writer cannot make one — it needs a top-level `event`/`fn` and wires every node into that
	 * form's exec chain — so the links here are made and then broken, which is what a user does by
	 * accident and what the tool has to find.
	 */
	bool BuildDeadProbe(UBlueprint* InBlueprint)
	{
		UK2Node_FunctionEntry* Entry = nullptr;
		UK2Node_FunctionResult* Result = nullptr;
		UEdGraph* Graph = MakeFunction(InBlueprint, TEXT("DeadProbe"), Entry, Result);
		if (Graph == nullptr)
		{
			return false;
		}

		UK2Node_CallFunction* Live = NewObject<UK2Node_CallFunction>(Graph);
		Live->SetFromFunction(UKismetSystemLibrary::StaticClass()->FindFunctionByName(
			TEXT("PrintString")));
		Place(Graph, Live, 400.0, 0.0);
		if (Entry != nullptr && Entry->GetThenPin() != nullptr && Live->GetExecPin() != nullptr)
		{
			Entry->GetThenPin()->MakeLinkTo(Live->GetExecPin());
		}

		// TWO ORPHANS, wired to each other and to nothing else. After the links below are broken not
		// even that remains, so each is its own disconnected group.
		UK2Node_CallFunction* OrphanA = NewObject<UK2Node_CallFunction>(Graph);
		OrphanA->SetFromFunction(UKismetSystemLibrary::StaticClass()->FindFunctionByName(
			TEXT("PrintString")));
		Place(Graph, OrphanA, 400.0, 900.0);

		UK2Node_CallFunction* OrphanB = NewObject<UK2Node_CallFunction>(Graph);
		OrphanB->SetFromFunction(UKismetSystemLibrary::StaticClass()->FindFunctionByName(
			TEXT("PrintString")));
		Place(Graph, OrphanB, 800.0, 900.0);

		if (OrphanA->GetThenPin() != nullptr && OrphanB->GetExecPin() != nullptr)
		{
			OrphanA->GetThenPin()->MakeLinkTo(OrphanB->GetExecPin());
		}

		// THE LINKS ARE BROKEN, which is the whole point: this is the state a user leaves behind when
		// they delete the node that used to call a chain.
		OrphanA->BreakAllNodeLinks();
		OrphanB->BreakAllNodeLinks();

		return true;
	}

	/**
	 * `LocalVarProbe`: ONE pure node read TWICE, with both a safe and an unsafe classification.
	 *
	 * `P/E-LOCAL-VARS` promotes a pure value read more than once, but only when everything feeding it
	 * is a function input or a literal. A DSL expression is a TREE, so it cannot express a value with
	 * two consumers at all — which is why this asset exists.
	 *
	 * The pure node is a `Add_IntInt`, whose two inputs are both function inputs and therefore SAFE.
	 * A second pure node adds a MEMBER VARIABLE to a parameter, which is UNSAFE, so both answers are
	 * exercised by one asset.
	 */
	bool BuildLocalVarProbe(UBlueprint* InBlueprint)
	{
		UK2Node_FunctionEntry* Entry = nullptr;
		UK2Node_FunctionResult* Result = nullptr;
		UEdGraph* Graph = MakeFunction(InBlueprint, TEXT("LocalVarProbe"), Entry, Result);
		if (Graph == nullptr || Entry == nullptr)
		{
			return false;
		}

		// The function's own inputs, which is what makes a value safe.
		FEdGraphPinType IntType;
		IntType.PinCategory = UEdGraphSchema_K2::PC_Int;
		Entry->CreateUserDefinedPin(TEXT("A"), IntType, EGPD_Output);
		Entry->CreateUserDefinedPin(TEXT("B"), IntType, EGPD_Output);

		// A member variable, which is what makes a value UNSAFE.
		const FName MemberName = FBlueprintEditorUtils::FindUniqueKismetName(
			InBlueprint, TEXT("Bias"));

		// THE SAFE VALUE: A + B, both function inputs, read by TWO consumers below.
		UK2Node_CallFunction* Sum = NewObject<UK2Node_CallFunction>(Graph);
		Sum->SetFromFunction(UKismetMathLibrary::StaticClass()->FindFunctionByName(TEXT("Add_IntInt")));
		Place(Graph, Sum, 400.0, 0.0);

		UEdGraphPin* APin = Entry->FindPin(TEXT("A"));
		UEdGraphPin* BPin = Entry->FindPin(TEXT("B"));
		UEdGraphPin* SumA = Sum->FindPin(TEXT("A"));
		UEdGraphPin* SumB = Sum->FindPin(TEXT("B"));
		if (APin != nullptr && SumA != nullptr) { APin->MakeLinkTo(SumA); }
		if (BPin != nullptr && SumB != nullptr) { BPin->MakeLinkTo(SumB); }

		// TWO CONSUMERS of the same value. This is the structure no DSL expression can produce.
		for (int32 Index = 0; Index < 2; ++Index)
		{
			UK2Node_CallFunction* Consumer = NewObject<UK2Node_CallFunction>(Graph);
			Consumer->SetFromFunction(UKismetMathLibrary::StaticClass()->FindFunctionByName(
				TEXT("Multiply_IntInt")));
			Place(Graph, Consumer, 900.0, Index * 300.0);

			if (Sum->GetReturnValuePin() != nullptr && Consumer->FindPin(TEXT("A")) != nullptr)
			{
				Sum->GetReturnValuePin()->MakeLinkTo(Consumer->FindPin(TEXT("A")));
			}
		}

		// THE UNSAFE VALUE: a member variable plus an input. It is also read twice, so the tool has to
		// decide about it and must say NO.
		UK2Node_CallFunction* Biased = NewObject<UK2Node_CallFunction>(Graph);
		Biased->SetFromFunction(UKismetMathLibrary::StaticClass()->FindFunctionByName(
			TEXT("Add_IntInt")));
		Place(Graph, Biased, 400.0, 900.0);

		if (APin != nullptr && Biased->FindPin(TEXT("A")) != nullptr)
		{
			APin->MakeLinkTo(Biased->FindPin(TEXT("A")));
		}
		if (BPin != nullptr && Biased->FindPin(TEXT("B")) != nullptr)
		{
			BPin->MakeLinkTo(Biased->FindPin(TEXT("B")));
		}

		for (int32 Index = 0; Index < 2; ++Index)
		{
			UK2Node_CallFunction* Consumer = NewObject<UK2Node_CallFunction>(Graph);
			Consumer->SetFromFunction(UKismetMathLibrary::StaticClass()->FindFunctionByName(
				TEXT("Multiply_IntInt")));
			Place(Graph, Consumer, 900.0, 900.0 + Index * 300.0);
			if (Biased->GetReturnValuePin() != nullptr
				&& Consumer->FindPin(TEXT("A")) != nullptr)
			{
				Biased->GetReturnValuePin()->MakeLinkTo(Consumer->FindPin(TEXT("A")));
			}
		}

		// The member variable is added AFTER the graph, so the node that reads it is made here too.
		FBlueprintEditorUtils::AddMemberVariable(InBlueprint, MemberName, IntType, TEXT("10"));
		UK2Node_VariableGet* BiasGet = NewObject<UK2Node_VariableGet>(Graph);
		BiasGet->VariableReference.SetSelfMember(MemberName);
		Place(Graph, BiasGet, 100.0, 1200.0);
		if (BiasGet->GetValuePin() != nullptr && Biased->FindPin(TEXT("A")) != nullptr)
		{
			// The member read REPLACES the input on one side, which is what makes this value unsafe:
			// the variable can change during the call.
			Biased->FindPin(TEXT("A"))->BreakAllPinLinks();
			BiasGet->GetValuePin()->MakeLinkTo(Biased->FindPin(TEXT("A")));
		}

		return true;
	}

	/**
	 * `SaneProbe`: an ordinary chain that SHOULD NOT be disturbed.
	 *
	 * A layout pass that measures and rolls back needs a graph where the pass has nothing to improve,
	 * because that is where a rollback that does not work turns a fine graph into a broken one. This
	 * graph is already in order: one entry, one call, one result, left to right on one row.
	 */
	bool BuildSaneProbe(UBlueprint* InBlueprint)
	{
		UK2Node_FunctionEntry* Entry = nullptr;
		UK2Node_FunctionResult* Result = nullptr;
		UEdGraph* Graph = MakeFunction(InBlueprint, TEXT("SaneProbe"), Entry, Result);
		if (Graph == nullptr || Entry == nullptr)
		{
			return false;
		}

		UK2Node_CallFunction* Call = NewObject<UK2Node_CallFunction>(Graph);
		Call->SetFromFunction(UKismetSystemLibrary::StaticClass()->FindFunctionByName(
			TEXT("PrintString")));
		Place(Graph, Call, 450.0, 0.0);

		if (Entry->GetThenPin() != nullptr && Call->GetExecPin() != nullptr)
		{
			Entry->GetThenPin()->MakeLinkTo(Call->GetExecPin());
		}
		if (Call->GetThenPin() != nullptr && Result != nullptr
			&& Result->GetExecPin() != nullptr)
		{
			Call->GetThenPin()->MakeLinkTo(Result->GetExecPin());
		}
		return true;
	}
}

UTurboMCPAcceptanceAssetCommandlet::UTurboMCPAcceptanceAssetCommandlet()
{
	IsClient = false;
	IsServer = false;
	IsEditor = true;
	LogToConsole = true;
}

int32 UTurboMCPAcceptanceAssetCommandlet::Main(const FString& Params)
{
	UE_LOG(LogTurboMCP, Display, TEXT("TurboMCP: building the acceptance asset"));

	// A FRESH PACKAGE, so re-running the commandlet reproduces the asset rather than piling up
	// copies. That is what makes the structure legible and the asset regenerable.
	UPackage* Package = FindPackage(nullptr, PackagePath);	if (Package != nullptr)
	{
		Package->SetDirtyFlag(false);
	}

	Package = CreatePackage(PackagePath);
	if (Package == nullptr)
	{
		UE_LOG(LogTurboMCP, Error, TEXT("could not create the package %s"), PackagePath);
		return 1;
	}

	// The old asset is discarded so the new one is the only content, which keeps the asset's node
	// counts stable between runs.
	if (UBlueprint* Existing = FindObject<UBlueprint>(Package, TEXT("TurboMCPAcceptance")))
	{
		Existing->MarkAsGarbage();
	}

	UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(
		AActor::StaticClass(), Package, TEXT("TurboMCPAcceptance"),
		BPTYPE_Normal, UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass(),
		TEXT("TurboMCPAcceptanceAssetCommandlet"));

	if (Blueprint == nullptr)
	{
		UE_LOG(LogTurboMCP, Error, TEXT("could not create the Blueprint"));
		return 1;
	}

	const bool bKnots = BuildKnotProbe(Blueprint);
	const bool bDead = BuildDeadProbe(Blueprint);
	const bool bLocals = BuildLocalVarProbe(Blueprint);
	const bool bSane = BuildSaneProbe(Blueprint);

	UE_LOG(LogTurboMCP, Display,
		TEXT("TurboMCP: KnotProbe=%s DeadProbe=%s LocalVarProbe=%s SaneProbe=%s"),
		bKnots ? TEXT("ok") : TEXT("FAILED"),
		bDead ? TEXT("ok") : TEXT("FAILED"),
		bLocals ? TEXT("ok") : TEXT("FAILED"),
		bSane ? TEXT("ok") : TEXT("FAILED"));

	if (!bKnots || !bDead || !bLocals || !bSane)
	{
		UE_LOG(LogTurboMCP, Error, TEXT("one or more probe graphs could not be built"));
		return 2;
	}

	// THE SAVE IS THE POINT. An asset held only in memory dies with the editor, and this asset exists
	// to be committed and re-read by a test suite.
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	Package->MarkPackageDirty();

	const FString FileName = FPackageName::LongPackageNameToFilename(
		PackagePath, FPackageName::GetAssetPackageExtension());
	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	SaveArgs.SaveFlags = SAVE_NoError;
	if (!UPackage::SavePackage(Package, Blueprint, *FileName, SaveArgs))
	{
		UE_LOG(LogTurboMCP, Error, TEXT("could not save %s"), *FileName);
		return 3;
	}

	FAssetRegistryModule::AssetCreated(Blueprint);

	UE_LOG(LogTurboMCP, Display, TEXT("TurboMCP: wrote %s"), *FileName);

	// The counts are the evidence a caller has, so they are printed rather than left to the tool.
	for (UEdGraph* Graph : Blueprint->FunctionGraphs)
	{
		if (Graph != nullptr)
		{
			UE_LOG(LogTurboMCP, Display, TEXT("  %s: %d node(s)"),
				*Graph->GetName(), Graph->Nodes.Num());
		}
	}

	return 0;
}

#undef LOCTEXT_NAMESPACE
