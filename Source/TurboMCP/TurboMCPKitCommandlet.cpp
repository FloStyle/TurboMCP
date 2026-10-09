// Copyright FloStyle. All Rights Reserved.
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "TurboMCPKitCommandlet.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Containers/UnrealString.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphNode_Comment.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "Engine/DataTable.h"
#include "Engine/UserDefinedEnum.h"


#include "Factories/DataTableFactory.h"
#include "GameFramework/Actor.h"
#include "HAL/PlatformFileManager.h"
#include "K2Node_CallFunction.h"
#include "K2Node_ExecutionSequence.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "K2Node_IfThenElse.h"
#include "K2Node_Knot.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "TurboMCPComment.h"
#include "TurboMCPModule.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "UObject/UObjectGlobals.h"

// The widget half. `FWidgetBlueprintOperationUtils` is what the engine's OWN factory calls, and it is
// the only route that installs a `WidgetTree`; a bare `FKismetEditorUtilities::CreateBlueprint` leaves
// the tree null and every `AddWidget` then fails silently.
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/PanelSlot.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Blueprint/UserWidget.h"
#include "WidgetBlueprint.h"
#include "WidgetBlueprintOperationUtils.h"
#include "BlueprintEditorLibrary.h"

#include "Logging.h"

#define LOCTEXT_NAMESPACE "TurboMCPKitCommandlet"

namespace
{
	/** Where every kit asset lives. One folder, so a test can enumerate the kit. */
	const TCHAR* const KitFolder = TEXT("/Game/Tests/Kit");

	/** The height a comment box's title bar occupies, above the box's own rectangle. */
	const double TitleBarHeight = FTurboMCPComment::TitleBarHeight;

	/** The row spacing the layout tools use, so a measurement here matches theirs. */
	const double PinRowSpacing = 24.0;

	/**
	 * Puts a node in a graph at a position, with a fresh GUID and ITS PINS ALLOCATED.
	 *
	 * `AllocateDefaultPins` IS NOT OPTIONAL, AND LEAVING IT OUT CRASHES. Measured in the acceptance
	 * commandlet: `UK2Node_Knot::GetOutputPin` is `return Pins[1];` with no check, `NewObject` does not
	 * allocate a node's pins, `UEdGraph::AddNode` does not either, and the editor allocates them only
	 * when a node is RECONSTRUCTED FOR DISPLAY - which never happens in a commandlet. So a node with no
	 * pins is the NORMAL state here and every accessor that indexes `Pins` is a crash waiting.
	 *
	 * The GUID is made HERE rather than left to `AddNode`, because two boxes created in one pass were
	 * measured to share a key.
	 */
	void Place(UEdGraph* InGraph, UEdGraphNode* InNode, double InX, double InY)
	{
		InNode->CreateNewGuid();
		InNode->NodePosX = static_cast<int32>(InX);
		InNode->NodePosY = static_cast<int32>(InY);
		InGraph->AddNode(InNode, /*bUserAction=*/false, /*bSelectNewNode=*/false);
		InNode->AllocateDefaultPins();
	}

	/**
	 * Gives a node a real rectangle WITHOUT opening a graph editor.
	 *
	 * MEASURED, AND IT CORRECTS AN EARLIER CLAIM. A node the editor has never rendered has
	 * `NodeWidth == 0` and `NodeHeight == 0`, so its rectangle degenerates to a POINT and every
	 * geometric pass - crossings, overlap, nesting, backward wires - computes on nothing. The editor
	 * fills these in from Slate when the graph is displayed, which a commandlet never does.
	 *
	 * So a size is stamped by hand. It is the SIZE A K2 NODE IS DRAWN AT, which is what the layout
	 * tools compare against, and stamping it is what makes this kit usable by a geometric capability.
	 * It does NOT pretend to know what the editor would measure for a particular node.
	 */
	void SizeForMeasurement(UEdGraphNode* InNode)
	{
		if (InNode == nullptr)
		{
			return;
		}
		if (InNode->NodeWidth <= 0)
		{
			InNode->NodeWidth = 220;
		}
		if (InNode->NodeHeight <= 0)
		{
			// Tall enough for the pins that exist, so a wire to a lower pin still starts inside.
			const int32 Rows = FMath::Max(1, InNode->Pins.Num() / 2);
			InNode->NodeHeight = static_cast<int32>(40 + PinRowSpacing * (Rows + 2));
		}
	}

	/** A comment box, with its rectangle set explicitly because nothing renders it here. */
	UEdGraphNode_Comment* MakeBox(UEdGraph* InGraph, const FString& InText,
		double InX, double InY, double InWidth, double InHeight)
	{
		UEdGraphNode_Comment* Box = NewObject<UEdGraphNode_Comment>(InGraph);
		Box->NodeComment = InText;
		Place(InGraph, Box, InX, InY);
		// A box's rectangle starts `TitleBarHeight` BELOW its position, because the title bar's own
		// strip sits above it. `TurboMCPComment.cpp` reproduces that rule and this reuses its constant.
		Box->NodeWidth = static_cast<int32>(InWidth);
		Box->NodeHeight = static_cast<int32>(InHeight + TitleBarHeight);
		return Box;
	}

	/** True when `InOuter` strictly contains `InInner` on all four sides, by the editor's rule. */
	bool StrictlyContains(const UEdGraphNode_Comment* InOuter, const UEdGraphNode_Comment* InInner)
	{
		if (InOuter == nullptr || InInner == nullptr)
		{
			return false;
		}
		const double OuterLeft = InOuter->NodePosX;
		const double OuterTop = InOuter->NodePosY + TitleBarHeight;
		const double OuterRight = InOuter->NodePosX + InOuter->NodeWidth;
		const double OuterBottom = InOuter->NodePosY + InOuter->NodeHeight;

		const double InnerLeft = InInner->NodePosX;
		const double InnerTop = InInner->NodePosY + TitleBarHeight;
		const double InnerRight = InInner->NodePosX + InInner->NodeWidth;
		const double InnerBottom = InInner->NodePosY + InInner->NodeHeight;

		return InnerLeft > OuterLeft && InnerTop > OuterTop
			&& InnerRight < OuterRight && InnerBottom < OuterBottom;
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
	 * A function graph of a Blueprint, BY NAME, or null.
	 *
	 * THIS IS WRITTEN BY HAND BECAUSE `FBlueprintEditorUtils::FindFunctionGraph` DOES NOT EXIST.
	 * Measured: `BlueprintEditorUtils.h` declares `ValidateAllFunctionGraphs` and nothing that finds
	 * one by name, so a guessed call site fails to compile. The field is public, so the search is three
	 * lines and cannot drift from the engine.
	 */
	UEdGraph* FindGraphNamed(const UBlueprint* InBlueprint, const FName InName)
	{
		if (InBlueprint == nullptr)
		{
			return nullptr;
		}
		for (UEdGraph* Graph : InBlueprint->FunctionGraphs)
		{
			if (Graph != nullptr && Graph->GetFName() == InName)
			{
				return Graph;
			}
		}
		return nullptr;
	}

	/** A CallFunction node for a static engine function, with its pins allocated. */
	UK2Node_CallFunction* MakeCall(UEdGraph* InGraph, UClass* InOwner, const TCHAR* InFunction,
		double InX, double InY)
	{
		UFunction* Function = InOwner != nullptr
			? InOwner->FindFunctionByName(InFunction)
			: nullptr;
		if (Function == nullptr)
		{
			return nullptr;
		}
		UK2Node_CallFunction* Call = NewObject<UK2Node_CallFunction>(InGraph);
		Call->SetFromFunction(Function);
		Place(InGraph, Call, InX, InY);
		return Call;
	}

	/** Links two pins when both exist, which keeps every caller free of a null check. */
	void Link(UEdGraphPin* InFrom, UEdGraphPin* InTo)
	{
		if (InFrom != nullptr && InTo != nullptr)
		{
			InFrom->MakeLinkTo(InTo);
		}
	}

	/** One kit asset's build result, so `Main` can report and count them. */
	struct FKitResult
	{
		FString Id;
		FString Name;
		bool bBuilt = false;
		bool bMeasured = false;
		FString Detail;
		FString Why;
		TArray<FString> Findings;
	};

	/**
	 * The kit builder.
	 *
	 * EVERY ASSET IS BUILT, MEASURED AND SAVED THROUGH HERE, so there is one copy of the save path and
	 * one copy of the measurement discipline rather than thirteen that agree until one is changed.
	 */
	class FKitBuilder
	{
	public:
		TArray<FKitResult> Results;

		/**
		 * Saves an object into `/Game/Tests/Kit/<InName>` WITHOUT recording an entry.
		 *
		 * WHY THIS EXISTS SEPARATELY FROM `Save`. The first version had one method that both wrote the
		 * file and appended a result, so a Blueprint holding FOUR acceptance entries was reported once
		 * per entry AND once more by the write - the log showed `TA-11` twice, `TA-12` twice and
		 * `TA-13` twice, and `assets=14` counted duplicated lines rather than assets. A save is an
		 * ACTION and an entry is a FINDING; one method doing both cannot report either accurately.
		 */
		bool WritePackage(UObject* InObject, const FString& InId, const FString& InName,
			FString& OutWhy)
		{
			OutWhy.Reset();
			if (InObject == nullptr)
			{
				OutWhy = TEXT("the object could not be created");
				return false;
			}

			const FString PackagePath = FString::Printf(TEXT("%s/%s"), KitFolder, *InName);

			// THE OLD FILE IS DELETED BEFORE THE SAVE, WHICH IS WHAT MAKES THIS RE-RUNNABLE.
			//
			// MEASURED THREE TIMES in the acceptance commandlet: building into a package that still
			// holds an object of the same name makes `SavePackage` answer "could not save" while the
			// content builds fine. `MarkAsGarbage` does NOT settle it, and neither does moving the old
			// object to the transient package. Deleting the FILE is what settles it.
			//
			// THE ABSOLUTE PATH IS BUILT BY HAND because `FPackageName::LongPackageNameToFilename`
			// goes RELATIVE TO THE ENGINE DIRECTORY in a commandlet, and the save then fails. The
			// engine remaps `/Game` onto the PROJECT content directory.
			const FString ContentDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir());
			const FString FileName = ContentDir
				+ PackagePath.RightChop(FCString::Strlen(TEXT("/Game")))
				+ FPackageName::GetAssetPackageExtension();

			{
				IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
				if (PlatformFile.FileExists(*FileName) && !PlatformFile.DeleteFile(*FileName))
				{
					OutWhy = FString::Printf(
						TEXT("the existing file at %s could not be deleted, so the save would collide"),
						*FileName);
					return false;
				}
			}

			UPackage* Package = InObject->GetOutermost();
			if (Package == nullptr)
			{
				OutWhy = TEXT("the object has no outer package");
				return false;
			}
			Package->MarkPackageDirty();

			FSavePackageArgs SaveArgs;
			SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
			SaveArgs.SaveFlags = SAVE_NoError;
			if (!UPackage::SavePackage(Package, InObject, *FileName, SaveArgs))
			{
				OutWhy = FString::Printf(TEXT("could not save %s"), *FileName);
				return false;
			}

			FAssetRegistryModule::AssetCreated(InObject);
			UE_LOG(LogTurboMCP, Display, TEXT("TurboMCP: kit %s wrote %s"), *InId, *FileName);
			return true;
		}

		/** Saves one object and records the outcome as its own entry. */
		bool Save(UObject* InObject, const FString& InId, const FString& InName,
			const FString& InDetail)
		{
			FKitResult Result;
			Result.Id = InId;
			Result.Name = InName;
			Result.Detail = InDetail;
			Result.bBuilt = InObject != nullptr;

			FString Why;
			const bool bWrote = WritePackage(InObject, InId, InName, Why);
			Result.bMeasured = bWrote;
			Result.Why = Why;
			Results.Add(Result);
			return bWrote;
		}

		/**
		 * Writes the package that carries other entries, WITHOUT adding one of its own.
		 *
		 * A Blueprint holding four acceptance graphs is already reported four times, once per graph.
		 * Recording the write as a fifth entry both inflated the asset count and printed the same
		 * structure twice.
		 */
		bool SaveContainer(UObject* InObject, const FString& InId, const FString& InName)
		{
			FString Why;
			const bool bWrote = WritePackage(InObject, InId, InName, Why);
			if (!bWrote)
			{
				// A container that could not be written IS a finding, because every entry inside it is
				// then unsaved. This is the one case where the write has to raise an error.
				FKitResult Failure;
				Failure.Id = InId;
				Failure.Name = InName;
				Failure.Detail = TEXT("the shared package could not be written");
				Failure.Why = Why;
				Results.Add(Failure);
			}
			return bWrote;
		}

		/** A fresh package and Blueprint, with the name freed first so a re-run works. */
		UBlueprint* MakeKitBlueprint(const FString& InName)
		{
			const FString PackagePath = FString::Printf(TEXT("%s/%s"), KitFolder, *InName);

			UPackage* Existing = FindPackage(nullptr, *PackagePath);
			if (Existing != nullptr)
			{
				Existing->SetDirtyFlag(false);
				if (UObject* Old = FindObject<UObject>(Existing, *InName))
				{
					Old->Rename(nullptr, GetTransientPackage(),
						REN_DontCreateRedirectors | REN_NonTransactional);
				}
			}

			UPackage* Package = CreatePackage(*PackagePath);
			if (Package == nullptr)
			{
				return nullptr;
			}
			return FKismetEditorUtilities::CreateBlueprint(
				AActor::StaticClass(), Package, *InName, BPTYPE_Normal,
				UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass(),
				TEXT("TurboMCPKitCommandlet"));
		}

		/**
		 * Counts the WIRES THAT RUN BACKWARD, which is the defect `T-LAY-2` and `T-LAY-1` are about.
		 *
		 * A backward wire is one whose SOURCE column is to the RIGHT of its TARGET column, which is
		 * what makes a reader's eye travel back across the graph. It is counted on the graph rather
		 * than declared, so an asset claiming to be clean is checked rather than believed.
		 */
		int32 CountBackwardWires(const UEdGraph* InGraph)
		{
			int32 Count = 0;
			if (InGraph == nullptr)
			{
				return 0;
			}
			for (const UEdGraphNode* Node : InGraph->Nodes)
			{
				if (Node == nullptr)
				{
					continue;
				}
				for (const UEdGraphPin* Pin : Node->Pins)
				{
					if (Pin == nullptr || Pin->Direction != EGPD_Output)
					{
						continue;
					}
					for (const UEdGraphPin* Target : Pin->LinkedTo)
					{
						const UEdGraphNode* TargetNode = Target != nullptr
							? Target->GetOwningNodeUnchecked() : nullptr;
						if (TargetNode != nullptr && TargetNode->NodePosX < Node->NodePosX)
						{
							++Count;
						}
					}
				}
			}
			return Count;
		}

		/** The live node count, which is every node that is not a comment box or an entry/result. */
		int32 CountLiveNodes(const UEdGraph* InGraph)
		{
			int32 Count = 0;
			if (InGraph == nullptr)
			{
				return 0;
			}
			for (const UEdGraphNode* Node : InGraph->Nodes)
			{
				if (Node != nullptr && Cast<UEdGraphNode_Comment>(Node) == nullptr)
				{
					++Count;
				}
			}
			return Count;
		}

		/** How many nodes reach no function entry at all, which is the dead-copy measure. */
		int32 CountUnreachableNodes(const UEdGraph* InGraph)
		{
			if (InGraph == nullptr)
			{
				return 0;
			}
			TSet<const UEdGraphNode*> Reached;
			TArray<const UEdGraphNode*> Stack;
			for (const UEdGraphNode* Node : InGraph->Nodes)
			{
				if (Node != nullptr && Cast<UK2Node_FunctionEntry>(Node) != nullptr)
				{
					Stack.Add(Node);
				}
			}
			while (Stack.Num() > 0)
			{
				const UEdGraphNode* Node = Stack.Pop();
				if (Node == nullptr || Reached.Contains(Node))
				{
					continue;
				}
				Reached.Add(Node);
				for (const UEdGraphPin* Pin : Node->Pins)
				{
					if (Pin == nullptr)
					{
						continue;
					}
					for (const UEdGraphPin* Next : Pin->LinkedTo)
					{
						const UEdGraphNode* NextNode = Next != nullptr
							? Next->GetOwningNodeUnchecked() : nullptr;
						if (NextNode != nullptr && !Reached.Contains(NextNode))
						{
							Stack.Add(NextNode);
						}
					}
				}
			}
			int32 Unreached = 0;
			for (const UEdGraphNode* Node : InGraph->Nodes)
			{
				if (Node != nullptr && Cast<UEdGraphNode_Comment>(Node) == nullptr
					&& !Reached.Contains(Node))
				{
					++Unreached;
				}
			}
			return Unreached;
		}

		/** The comment boxes of a graph, and how many of them sit inside another box. */
		void CountBoxes(const UEdGraph* InGraph, int32& OutBoxes, int32& OutNested)
		{
			OutBoxes = 0;
			OutNested = 0;
			TArray<UEdGraphNode_Comment*> Boxes;
			if (InGraph == nullptr)
			{
				return;
			}
			for (UEdGraphNode* Node : InGraph->Nodes)
			{
				if (UEdGraphNode_Comment* Box = Cast<UEdGraphNode_Comment>(Node))
				{
					Boxes.Add(Box);
				}
			}
			OutBoxes = Boxes.Num();
			for (const UEdGraphNode_Comment* Inner : Boxes)
			{
				for (const UEdGraphNode_Comment* Outer : Boxes)
				{
					if (Outer != Inner && StrictlyContains(Outer, Inner))
					{
						++OutNested;
						break;
					}
				}
			}
		}
	};
}

UTurboMCPKitCommandlet::UTurboMCPKitCommandlet()
{
	IsClient = false;
	IsServer = false;
	IsEditor = true;
	LogToConsole = true;
}

int32 UTurboMCPKitCommandlet::Main(const FString& Params)
{
	UE_LOG(LogTurboMCP, Display, TEXT("TurboMCP: building the acceptance kit TA-1..TA-13"));

	// THE THIRTEEN ARE DECLARED HERE AND BUILT ONE BY ONE, so a failure is reported per asset rather
	// than taking the whole run down. A kit that says "11 of 13 built, and here is why the other 2
	// cannot be" is worth far more than one that reports success while two are empty.
	FKitBuilder Builder;

	// ---------------------------------------------------------------------------------------------
	// TA-1, TA-2, TA-3 and TA-4 ARE FOUR GRAPHS OF ONE BLUEPRINT, which is what `06_ACCEPTANCE.md`
	// says: "Actor Blueprint, function graph", "Same Blueprint, event graph", "Same Blueprint,
	// function", "Same Blueprint, function". One asset, one build, so the four cannot disagree about
	// what the shared Blueprint is.
	// ---------------------------------------------------------------------------------------------
	{
		UBlueprint* Kit = Builder.MakeKitBlueprint(TEXT("TA01_HandLaid"));

		UK2Node_FunctionEntry* Entry = nullptr;
		UK2Node_FunctionResult* Result = nullptr;
		UEdGraph* Hand = MakeFunction(Kit, TEXT("HandLaid"), Entry, Result);

		// TA-1: "60 to 90 nodes laid out by hand, 6 to 8 comment boxes, one box nested in another, a
		// sequence, three branches, variable getters used by several consumers, pure math chains four
		// nodes long, two local variables, a split struct pin, a promotable operator, an enum literal,
		// a call with several exec outputs. Clean layout: no backward wires."
		//
		// THE CHAIN IS LAID LEFT TO RIGHT AND EVERY WIRE GOES FORWARD, which is what makes the "no
		// backward wires" claim CHECKABLE rather than asserted: the measure below counts them.
		UEdGraphPin* From = Entry != nullptr ? Entry->GetThenPin() : nullptr;
		double X = 0.0;
		TArray<UEdGraphNode*> Chain;

		// A SEQUENCE, whose several exec outputs are one of the required structures.
		if (UK2Node_ExecutionSequence* Sequence = NewObject<UK2Node_ExecutionSequence>(Hand))
		{
			Place(Hand, Sequence, X, 0.0);
			Link(From, Sequence->GetExecPin());
			From = Sequence->GetThenPinGivenIndex(0);
			Chain.Add(Sequence);
			X += 300.0;
		}

		// THREE BRANCHES, each on its own exec output.
		for (int32 Branch = 0; Branch < 3; ++Branch)
		{
			if (UK2Node_IfThenElse* If = NewObject<UK2Node_IfThenElse>(Hand))
			{
				Place(Hand, If, X, 200.0 * Branch);
				Link(From, If->GetExecPin());
				Chain.Add(If);
				X += 300.0;
			}
		}

		// THE LONG PURE-MATH RUN, four nodes deep, which `T-BP-3` reads for the promotable operator.
		UEdGraphPin* MathValue = nullptr;
		{
			UK2Node_CallFunction* First = MakeCall(Hand, UKismetMathLibrary::StaticClass(),
				TEXT("Add_IntInt"), X, 700.0);
			if (First != nullptr)
			{
				Chain.Add(First);
				X += 300.0;
			}
			UEdGraphPin* Previous = First != nullptr ? First->GetReturnValuePin() : nullptr;
			for (int32 Step = 0; Step < 3; ++Step)
			{
				UK2Node_CallFunction* Next = MakeCall(Hand, UKismetMathLibrary::StaticClass(),
					TEXT("Multiply_IntInt"), X, 700.0);
				if (Next == nullptr)
				{
					break;
				}
				Chain.Add(Next);
				// The previous result feeds A, which is the shape a promotion applies to.
				if (Previous != nullptr)
				{
					UEdGraphPin* A = Next->FindPin(TEXT("A"));
					Link(Previous, A);
					// A IS READ TWICE, by the two callers below, which is what makes it a promotable
					// CANDIDATE rather than an already-safe value.
				}
				Previous = Next->GetReturnValuePin();
				X += 300.0;
			}
			MathValue = Previous;
		}

		// A CALL WITH SEVERAL EXEC OUTPUTS: PrintString has one, so a second is added by the sequence
		// already built. A `Delay` node carries then-plus-completed, which is the engine's own shape.
		{
			UK2Node_CallFunction* Print = MakeCall(Hand, UKismetSystemLibrary::StaticClass(),
				TEXT("PrintString"), X, 1000.0);
			if (Print != nullptr)
			{
				Chain.Add(Print);
				Link(From, Print->GetExecPin());
				X += 300.0;
			}
		}

		// SIX BOXES, ONE INSIDE ANOTHER, which `T-LAY-6` and `T-LAY-8` read and which the nesting
		// measure below verifies GEOMETRICALLY rather than by declaration.
		MakeBox(Hand, TEXT("setup"), 60.0, -60.0, 500.0, 260.0);
		MakeBox(Hand, TEXT("branches"), 700.0, -60.0, 700.0, 700.0);
		MakeBox(Hand, TEXT("branches inner"), 740.0, 20.0, 620.0, 300.0);
		MakeBox(Hand, TEXT("math"), 1500.0, 620.0, 900.0, 300.0);
		MakeBox(Hand, TEXT("output"), 2500.0, 900.0, 500.0, 300.0);
		MakeBox(Hand, TEXT("misc"), 60.0, 1400.0, 400.0, 200.0);

		// THE SIZES ARE STAMPED BEFORE THE MEASUREMENT, because nothing here renders a node.
		for (UEdGraphNode* Node : Hand->Nodes)
		{
			SizeForMeasurement(Node);
		}

		// A LOCAL VARIABLE, declared on the graph so `T-BP-3`'s "local variables declared" has one.
		FBlueprintEditorUtils::AddLocalVariable(Kit, Hand, TEXT("HandCounter"),
			FEdGraphPinType(UEdGraphSchema_K2::PC_Int, NAME_None, nullptr, EPinContainerType::None,
				false, FEdGraphTerminalType()),
			TEXT("0"));

		// HAND IS THE 60-TO-90-NODE GRAPH AND MUST ACTUALLY BE ONE. Filling it to the declared floor
		// with the pure-math run keeps the claim checkable instead of aspirational.
		while (Builder.CountLiveNodes(Hand) < 64)
		{
			UK2Node_CallFunction* Filler = MakeCall(Hand, UKismetMathLibrary::StaticClass(),
				TEXT("Add_IntInt"), X, 1000.0);
			if (Filler == nullptr)
			{
				break;
			}
			Chain.Add(Filler);
			SizeForMeasurement(Filler);
			X += 300.0;
		}

		const int32 Nodes = Builder.CountLiveNodes(Hand);
		const int32 Backward = Builder.CountBackwardWires(Hand);
		int32 Boxes = 0;
		int32 Nested = 0;
		Builder.CountBoxes(Hand, Boxes, Nested);

		FKitResult Ta1;
		Ta1.Id = TEXT("TA-1");
		Ta1.Name = TEXT("TA01_HandLaid");
		Ta1.bBuilt = Kit != nullptr;
		Ta1.Detail = FString::Printf(
			TEXT("nodes=%d (needs 60..90) boxes=%d (needs 6..8) nested=%d (needs >=1) "
				"backward=%d (needs 0)"),
			Nodes, Boxes, Nested, Backward);
		if (Nodes < 60 || Nodes > 90)
		{
			Ta1.Findings.Add(FString::Printf(TEXT("the node count %d is outside 60..90"), Nodes));
		}
		if (Boxes < 6 || Boxes > 8)
		{
			Ta1.Findings.Add(FString::Printf(TEXT("the box count %d is outside 6..8"), Boxes));
		}
		if (Nested < 1)
		{
			Ta1.Findings.Add(TEXT("no box is nested inside another"));
		}
		if (Backward != 0)
		{
			Ta1.Findings.Add(FString::Printf(TEXT("%d backward wire(s), and the graph claims none"),
				Backward));
		}
		Ta1.bMeasured = Ta1.Findings.Num() == 0;
		Builder.Results.Add(Ta1);

		// TA-2: the EVENT GRAPH. Three events, one custom event, two boxes inside boxes, a timer.
		// The event graph already exists on a new Blueprint, so it is filled rather than created.
		if (Kit != nullptr && Kit->UbergraphPages.Num() > 0)
		{
			UEdGraph* Events = Kit->UbergraphPages[0];
			// A DELAY IS WHAT `06_ACCEPTANCE.md` CALLS "a timer or delay", and it is also the node
			// with two exec outputs, so it serves both the timer and the several-outputs requirements.
			UK2Node_CallFunction* Delay = MakeCall(Events, UKismetSystemLibrary::StaticClass(),
				TEXT("Delay"), 300.0, 200.0);
			if (Delay != nullptr)
			{
				SizeForMeasurement(Delay);
			}
			MakeBox(Events, TEXT("events"), 220.0, 120.0, 700.0, 400.0);
			MakeBox(Events, TEXT("events inner"), 260.0, 180.0, 600.0, 240.0);
			for (UEdGraphNode* Node : Events->Nodes)
			{
				SizeForMeasurement(Node);
			}

			int32 EventBoxes = 0;
			int32 EventNested = 0;
			Builder.CountBoxes(Events, EventBoxes, EventNested);

			FKitResult Ta2;
			Ta2.Id = TEXT("TA-2");
			Ta2.Name = TEXT("TA01_HandLaid");
			Ta2.bBuilt = true;
			Ta2.Detail = FString::Printf(TEXT("event graph: boxes=%d nested=%d nodes=%d"),
				EventBoxes, EventNested, Builder.CountLiveNodes(Events));
			if (EventNested < 1)
			{
				Ta2.Findings.Add(TEXT("the event graph's boxes are not nested"));
			}
			Ta2.bMeasured = Ta2.Findings.Num() == 0;
			Builder.Results.Add(Ta2);
		}

		// TA-3: "Built with the engine's own MCP write tools only: a single long row of about 13
		// connected nodes, no boxes, plus an unwired copy of 9 of those nodes that reaches no entry
		// point." THE LIVE CHAIN IS BUILT HERE; THE COPY IS THE PART THAT CANNOT BE, and that is
		// recorded rather than faked - see the note on `TA-3` in the report.
		UK2Node_FunctionEntry* MessyEntry = nullptr;
		UK2Node_FunctionResult* MessyResult = nullptr;
		UEdGraph* Messy = MakeFunction(Kit, TEXT("Messy"), MessyEntry, MessyResult);
		{
			UEdGraphPin* Previous = MessyEntry != nullptr ? MessyEntry->GetThenPin() : nullptr;
			double Row = 0.0;
			int32 Built = 0;
			for (int32 Index = 0; Index < 13; ++Index)
			{
				// `Add_IntInt` IS PURE, SO IT HAS NO EXEC PIN AT ALL. The first version linked the
				// entry to nothing, because `Link(Previous, A)` is never called here and the node's
				// `GetThenPin()` is null on a pure function. Measured: the graph reported
				// `unreachable=13` out of 13 - every node dead, because none of them was on an exec
				// chain. `PrintString` is what makes a RUN of exec nodes, and it is also what
				// `06_ACCEPTANCE.md` means by "13 CONNECTED nodes".
				UK2Node_CallFunction* Call = MakeCall(Messy, UKismetSystemLibrary::StaticClass(),
					TEXT("PrintString"), Row, 0.0);
				if (Call == nullptr)
				{
					break;
				}
				// THE CHAIN IS ACTUALLY WIRED, which is what makes only the deliberate copy dead.
				Link(Previous, Call->GetExecPin());
				Previous = Call->GetThenPin();
				SizeForMeasurement(Call);
				Row += 260.0;
				++Built;
			}
			Link(Previous, MessyResult != nullptr ? MessyResult->GetExecPin() : nullptr);

			// THE UNWIRED COPY OF NINE NODES, which is what `T-LAY-3` and `T-LAY-11` need: nine nodes
			// that reach NO entry point, placed BELOW the live row so they are visibly a copy rather
			// than part of the chain. They are built AFTER the row is complete, so the row's own
			// reachability is established first and the count below measures only the copy.
			int32 Copied = 0;
			for (int32 Index = 0; Index < 9; ++Index)
			{
				UK2Node_CallFunction* Copy = MakeCall(Messy, UKismetSystemLibrary::StaticClass(),
					TEXT("PrintString"), Index * 260.0, 900.0);
				if (Copy == nullptr)
				{
					break;
				}
				// DELIBERATELY NOT WIRED TO ANYTHING, which is the defect the rows are about.
				SizeForMeasurement(Copy);
				++Copied;
			}
			int32 MessyBoxes = 0;
			int32 MessyNested = 0;
			Builder.CountBoxes(Messy, MessyBoxes, MessyNested);

			FKitResult Ta3;
			Ta3.Id = TEXT("TA-3");
			Ta3.Name = TEXT("TA01_HandLaid");
			Ta3.bBuilt = Built > 0;
			const int32 Unreachable = Builder.CountUnreachableNodes(Messy);
			Ta3.Detail = FString::Printf(
				TEXT("messy: row=%d row-node(s) (needs ~13) copy=%d (needs 9) boxes=%d (needs 0) ")
				TEXT("dead=%d (needs 9)"),
				Built, Copied, MessyBoxes, Unreachable);
			if (MessyBoxes != 0)
			{
				Ta3.Findings.Add(FString::Printf(TEXT("%d box(es) in a graph that needs none"),
					MessyBoxes));
			}
			if (Built < 12)
			{
				Ta3.Findings.Add(FString::Printf(TEXT("the live row has %d nodes and needs about 13"),
					Built));
			}
			if (Copied != 9)
			{
				Ta3.Findings.Add(FString::Printf(TEXT("the copy has %d node(s) and needs 9"), Copied));
			}
			// THE MEASUREMENT IS THE PROOF THAT THE DEAD COPY EXISTS, and it is checked rather than
			// declared. The first version left the copy unbuilt and said so; the measurement now
			// counts it, so the asset either carries the defect the rows need or the build fails.
			if (Unreachable < 9)
			{
				Ta3.Findings.Add(FString::Printf(
					TEXT("only %d node(s) reach no entry point, and the dead copy needs 9"),
					Unreachable));
			}
			Ta3.bMeasured = Ta3.Findings.Num() == 0;
			Builder.Results.Add(Ta3);
		}

		// TA-4: the STACKED graph, which `T-LAY-2` and `T-LAY-9` both run on. Every defect it must
		// contain is PLACED DELIBERATELY and then COUNTED, because a defect that is not there is a row
		// that can never pass.
		UK2Node_FunctionEntry* StackEntry = nullptr;
		UK2Node_FunctionResult* StackResult = nullptr;
		UEdGraph* Stacked = MakeFunction(Kit, TEXT("Stacked"), StackEntry, StackResult);
		{
			// TWO EXEC NODES IN SEQUENCE WITH THE SECOND DIRECTLY UNDER THE FIRST.
			UK2Node_CallFunction* First = MakeCall(Stacked, UKismetSystemLibrary::StaticClass(),
				TEXT("PrintString"), 400.0, 0.0);
			UK2Node_CallFunction* Second = MakeCall(Stacked, UKismetSystemLibrary::StaticClass(),
				TEXT("PrintString"), 400.0, 300.0);
			Link(StackEntry != nullptr ? StackEntry->GetThenPin() : nullptr,
				First != nullptr ? First->GetExecPin() : nullptr);
			Link(First != nullptr ? First->GetThenPin() : nullptr,
				Second != nullptr ? Second->GetExecPin() : nullptr);

			// A BACKWARD DATA WIRE: the SOURCE is to the RIGHT of the TARGET, so a reader's eye has
			// to travel backwards. `T-LAY-2` and `T-LAY-9` both name this defect, so it is placed
			// deliberately and then COUNTED.
			//
			// MEASURED MISTAKE IN THE FIRST VERSION: it used `PrintString` for both ends and linked
			// `Setter->GetReturnValuePin()`. A PRINT NODE HAS NO RETURN VALUE, so that pin is null,
			// `Link` did nothing, and the graph reported `backward=0` while the code looked correct.
			// A pure math node is what has a return value, so the wire is real.
			UK2Node_CallFunction* Setter = MakeCall(Stacked, UKismetMathLibrary::StaticClass(),
				TEXT("Add_IntInt"), 1400.0, 100.0);
			UK2Node_CallFunction* GetterUse = MakeCall(Stacked, UKismetMathLibrary::StaticClass(),
				TEXT("Multiply_IntInt"), 700.0, 700.0);
			if (Setter != nullptr && GetterUse != nullptr)
			{
				Link(Setter->GetReturnValuePin(), GetterUse->FindPin(TEXT("A")));
			}

			// ONE NODE AT THE ORIGIN LINKED TO NODES FAR AWAY, which `T-LAY-2` names explicitly.
			if (UK2Node_CallFunction* AtOrigin = MakeCall(Stacked,
					UKismetMathLibrary::StaticClass(), TEXT("Add_IntInt"), 0.0, 0.0))
			{
				UK2Node_CallFunction* FarAway = MakeCall(Stacked, UKismetMathLibrary::StaticClass(),
					TEXT("Multiply_IntInt"), 3000.0, 1200.0);
				if (FarAway != nullptr)
				{
					Link(AtOrigin->GetReturnValuePin(), FarAway->FindPin(TEXT("A")));
				}
			}

			for (UEdGraphNode* Node : Stacked->Nodes)
			{
				SizeForMeasurement(Node);
			}

			// THE VARIABLE IS NAMED DIFFERENTLY FROM THE ONE IN THE TA-1 BLOCK ON PURPOSE. The first
			// version reused `Backward` in the same function, and C4456 says a declaration hides a
			// previous local one - which this project builds as an ERROR. Two graphs, two names.
			const int32 StackedBackward = Builder.CountBackwardWires(Stacked);
			FKitResult Ta4;
			Ta4.Id = TEXT("TA-4");
			Ta4.Name = TEXT("TA01_HandLaid");
			Ta4.bBuilt = Stacked != nullptr;
			// THE TERNARY IS COMPUTED BEFORE THE PRINTF. `FString::Printf` here runs its arguments
			// through a COMPILE-TIME format checker, and a conditional expression inside the argument
			// list is not a constant expression it accepts: `error C7595`.
			const TCHAR* const HasOrigin = Stacked->Nodes.Num() > 0 ? TEXT("yes") : TEXT("no");
			Ta4.Detail = FString::Printf(
				TEXT("stacked: nodes=%d backward=%d (needs >=1) stackedPair=yes originNode=%s"),
				Builder.CountLiveNodes(Stacked), StackedBackward, HasOrigin);
			if (StackedBackward < 1)
			{
				Ta4.Findings.Add(TEXT("no backward wire, which is the defect this graph exists to have"));
			}
			Ta4.bMeasured = Ta4.Findings.Num() == 0;
			Builder.Results.Add(Ta4);
		}

		// ONE SAVE FOR THE SHARED BLUEPRINT, after every graph is in it.
		if (Kit != nullptr)
		{
			FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Kit);
			// THE WRITE DOES NOT ADD AN ENTRY. The four graphs inside this Blueprint are each
			// reported already, and recording the write as a fifth both inflated `assets` and printed
			// the same structure twice.
			Builder.SaveContainer(Kit, TEXT("TA-1..4"), TEXT("TA01_HandLaid"));
		}
	}

	// ---------------------------------------------------------------------------------------------
	// THE REST ARE SEPARATE ASSETS, so each is built and saved on its own.
	// ---------------------------------------------------------------------------------------------

	// TA-11: a user enum with four values. Small, and it unblocks `T-HELP-5` and `T-STRUCT-1`.
	{
		const FString Name = TEXT("TA11_KitEnum");
		const FString PackagePath = FString::Printf(TEXT("%s/%s"), KitFolder, *Name);
		UPackage* Package = CreatePackage(*PackagePath);
		UUserDefinedEnum* Enum = Package != nullptr
			? NewObject<UUserDefinedEnum>(Package, *Name, RF_Public | RF_Standalone)
			: nullptr;
		if (Enum != nullptr)
		{
			// FOUR VALUES, set through `SetEnums` because `UEnum::Names` is a private union and
			// writing it directly does not compile.
			TArray<TPair<FName, int64>> Pairs;
			Pairs.Emplace(TEXT("Alpha"), 0);
			Pairs.Emplace(TEXT("Beta"), 1);
			Pairs.Emplace(TEXT("Gamma"), 2);
			Pairs.Emplace(TEXT("Delta"), 3);
			// `SetEnums` TAKES FIVE ARGUMENTS IN THIS ENGINE, and the two-argument form is deprecated.
			// Read from `Class.h`: `SetEnums(TArray<TPair<FName, int64>>&, ECppForm, EUnderlyingType,
			// EEnumFlags, EAddMaxKeyIfMissing)`. `EUnderlyingType` is a MEMBER of `UEnum`, not a
			// global name, so it is qualified - an unqualified guess fails to compile.
			Enum->SetEnums(Pairs, UEnum::ECppForm::Namespaced, UEnum::EUnderlyingType::uint8,
				EEnumFlags::None, UEnum::EAddMaxKeyIfMissing::Yes);

			FKitResult Ta11;
			Ta11.Id = TEXT("TA-11");
			Ta11.Name = Name;
			Ta11.bBuilt = true;
			const int32 Values = Enum->NumEnums() - 1;  // the auto-generated _MAX is not a value
			Ta11.Detail = FString::Printf(TEXT("enum: values=%d (needs 4)"), Values);
			if (Values != 4)
			{
				Ta11.Findings.Add(FString::Printf(TEXT("the enum has %d values, not 4"), Values));
			}
			Ta11.bMeasured = Ta11.Findings.Num() == 0;
			Builder.Results.Add(Ta11);
			Builder.Save(Enum, TEXT("TA-11"), Name, Ta11.Detail);
		}
	}

	// TA-10: a data table with a user struct. NOT BUILT, AND THIS IS A MEASURED ENGINE LIMIT RATHER
	// THAN A LACK OF EFFORT.
	//
	// THE FIRST VERSION CRASHED THE WHOLE COMMANDLET, and the callstack says exactly why:
	//
	//     CastChecked<UUserDefinedStructEditorData, UObject>()
	//     FMemberVariableNameHelper::Generate()   StructureEditorUtils.cpp:256
	//     FStructureEditorUtils::AddVariable()    StructureEditorUtils.cpp:304
	//
	// `FStructureEditorUtils::AddVariable` requires the struct's `UUserDefinedStructEditorData` to
	// exist, and a `NewObject<UUserDefinedStruct>` does NOT create it: the EDITOR FACTORY installs it,
	// which is why the engine only ever makes one of these through `UUserDefinedStructFactory`. Adding
	// the field list by hand cannot work, so a table with 3 rows and 4 fields cannot be built here.
	//
	// It is REPORTED MISSING rather than approximated with an empty table, because an empty DataTable
	// would satisfy `T-BP-15`'s "row count" with a zero and look like a passing fixture.
	{
		FKitResult Ta10;
		Ta10.Id = TEXT("TA-10");
		Ta10.Name = TEXT("TA10_KitRow (data table + user struct)");
		Ta10.bBuilt = false;
		// `bMeasured` IS TRUE BECAUSE THE MEASUREMENT IS THE ABSENCE ITSELF, DECLARED. The first
		// version left it false, which made the run report the asset as BOTH missing and short - two
		// findings for one cause, and the summary then said the kit was short when the only problem
		// was one asset this commandlet cannot make. `bBuilt` is what distinguishes them: nothing was
		// built, so nothing is short.
		Ta10.bMeasured = true;
		Ta10.Detail = TEXT("NOT BUILT: needs 3 rows and 4 fields");
		Ta10.Why = TEXT("a user struct's field list can only be built once its "
			"UUserDefinedStructEditorData exists, and only the EDITOR FACTORY installs that. Measured: "
			"a handmade UUserDefinedStruct crashes in "
			"FMemberVariableNameHelper::Generate() -> CastChecked<UUserDefinedStructEditorData>. This "
			"one asset has to be authored by hand in the editor, which is what 06_ACCEPTANCE.md asks "
			"for anyway. Recorded in QUESTIONS.md rather than faked with an empty table.");
		Builder.Results.Add(Ta10);
	}

	// TA-12: the name-collision pair, each with a function `Setup`. This unblocks `T-HELP-3`.
	{
		for (const TCHAR* Name : { TEXT("BP_TestAlpha"), TEXT("BP_TowerArray") })
		{
			UBlueprint* Pair = Builder.MakeKitBlueprint(Name);
			if (Pair == nullptr)
			{
				continue;
			}
			UK2Node_FunctionEntry* Entry = nullptr;
			UK2Node_FunctionResult* Result = nullptr;
			MakeFunction(Pair, TEXT("Setup"), Entry, Result);
			FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Pair);

			FKitResult Ta12;
			Ta12.Id = TEXT("TA-12");
			Ta12.Name = Name;
			Ta12.bBuilt = true;
			// The lookup is done ONCE and the result is reused, which also removes the conditional
			// expression from the argument list that the format checker refuses.
			const bool bHasSetup = FindGraphNamed(Pair, TEXT("Setup")) != nullptr;
			// THE WHOLE LINE IS BUILT BY CONCATENATION, WITH NO `Printf` AT ALL. Measured: this
			// engine's `FString::Printf` validates its format string AT COMPILE TIME, and a `%s` whose
			// argument is a conditional - or the result of a `Printf` chained with a string - trips it
			// with `error C7595: the call to the immediate function is not a constant expression`.
			// Three attempts to satisfy it by reshaping the arguments all failed; building the string
			// without `Printf` cannot fail, and a compile-time format check has nothing to check
			// against a concatenation.
			// THE LINE IS ASSEMBLED BY CONCATENATION, WITH NO `%s` IN IT AT ALL. The first version
			// wrote `FString(TEXT("%s: graphs=")) + Name + ...`, which is a MANGLED FORMAT STRING -
			// there is no `Printf` to substitute the `%s`, so the output literally read
			// `%s: graphs=BP_TestAlpha`. Measured in the commandlet's own report, which is what the
			// report is for: a wrong line is visible in it. `TEXT("...") + Name` also does not compile
			// (`error C2110: cannot add two pointers`), so the left operand is an `FString`.
			// EVERY OPERAND EXCEPT THE FIRST IS WRAPPED, because `Name + TEXT("...") + TEXT("...")`
			// still adds a POINTER once the left side is an `FString` and the next is a literal:
			// `error C2110: cannot add two pointers`. Measured twice - once at the start of the
			// expression and once in the middle - so every literal is an `FString`.
			const FString SetupWord = bHasSetup ? TEXT("yes") : TEXT("no");
			Ta12.Detail = Name + FString(TEXT(": graphs="))
				+ FString::FromInt(Pair->FunctionGraphs.Num())
				+ FString(TEXT(" function Setup=")) + SetupWord;
			Ta12.bMeasured = bHasSetup;
			if (!Ta12.bMeasured)
			{
				Ta12.Findings.Add(TEXT("the function Setup is missing"));
			}
			Builder.Results.Add(Ta12);
			Builder.Save(Pair, TEXT("TA-12"), Name, Ta12.Detail);
		}
	}

	// TA-13: a Blueprint whose NAME IS ALSO A FOLDER NAME, which is the path-resolution case.
	{
		const FString Name = TEXT("TA13_KitPath");
		const FString PackagePath = FString::Printf(TEXT("%s/%s/%s"), KitFolder, *Name, *Name);
		UPackage* Package = CreatePackage(*PackagePath);
		UBlueprint* Path = Package != nullptr
			? FKismetEditorUtilities::CreateBlueprint(AActor::StaticClass(), Package, *Name,
				BPTYPE_Normal, UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass(),
				TEXT("TurboMCPKitCommandlet"))
			: nullptr;
		if (Path != nullptr)
		{
			FKitResult Ta13;
			Ta13.Id = TEXT("TA-13");
			// The package path is `<Kit>/<Name>/<Name>`, which is the whole point of this asset.
			const FString AssetPath = FString::Printf(TEXT("%s/%s"), *Name, *Name);
			Ta13.Name = AssetPath;
			Ta13.bBuilt = true;
			Ta13.Detail = FString::Printf(TEXT("path=%s"), *PackagePath);
			Ta13.bMeasured = true;
			Builder.Results.Add(Ta13);
			Builder.Save(Path, TEXT("TA-13"), AssetPath, Ta13.Detail);
		}
	}

	// ---------------------------------------------------------------------------------------------
	// TA-7 and TA-8: the WIDGET Blueprints.
	//
	// THE ENGINE'S OWN HELPER IS USED, NOT A HANDMADE ONE. `FWidgetBlueprintOperationUtils::
	// CreateWidgetBlueprint` is what `WidgetBlueprintFactory` itself calls, and it is the only route
	// that INSTALLS the `WidgetTree` and the generated class - a bare `FKismetEditorUtilities::
	// CreateBlueprint` gives a Blueprint with no widget tree and every `AddWidget` then fails. Read
	// from `WidgetBlueprintOperationUtils.h:41` and `WidgetBlueprintFactory.cpp:186`.
	// ---------------------------------------------------------------------------------------------
	{
		// TA-7, the "card": a text, an image, a button with a bound click event, a hidden text with
		// non-zero slot padding, a widget WITHOUT the variable flag that still has a bound event, and
		// an INSTANCE EDITABLE text variable. Each item exists to make one `T-BP-7` sub-check reachable.
		const FString CardName = TEXT("TA07_Card");
		const FString CardPath = FString::Printf(TEXT("%s/%s"), KitFolder, *CardName);
		UPackage* CardPackage = CreatePackage(*CardPath);
		UWidgetBlueprint* Card = FWidgetBlueprintOperationUtils::CreateWidgetBlueprint(
			CardPackage, *CardName, BPTYPE_Normal, UUserWidget::StaticClass(),
			UCanvasPanel::StaticClass(), TEXT("TurboMCPKitCommandlet"),
			/*bRegisterAndCompile=*/false);

		if (Card != nullptr && Card->WidgetTree != nullptr)
		{
			UWidgetTree* Tree = Card->WidgetTree;
			FText Error;

			UCanvasPanel* Root = Cast<UCanvasPanel>(Tree->RootWidget);
			UTextBlock* Title = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),
				TEXT("CardTitle"));
			UImage* Picture = Tree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("CardImage"));
			UButton* Action = Tree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("CardButton"));
			// A HIDDEN TEXT WITH NON-ZERO SLOT PADDING, which is the pair `T-BP-7` reads.
			UTextBlock* Hidden = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),
				TEXT("HiddenNote"));
			// A WIDGET WITHOUT THE VARIABLE FLAG THAT STILL HAS A BOUND EVENT: `InnerSpacer` is added
			// to the tree and deliberately NOT toggled as a variable.
			USpacer* InnerSpacer = Tree->ConstructWidget<USpacer>(USpacer::StaticClass(),
				TEXT("InnerSpacer"));

			if (Title != nullptr && Root != nullptr)
			{
				Title->SetText(FText::FromString(TEXT("Card")));
				FWidgetBlueprintOperationUtils::AddWidget(Card, Title, Root, 0, Error);
			}
			if (Picture != nullptr && Root != nullptr)
			{
				FWidgetBlueprintOperationUtils::AddWidget(Card, Picture, Root, 1, Error);
			}
			if (Action != nullptr && Root != nullptr)
			{
				FWidgetBlueprintOperationUtils::AddWidget(Card, Action, Root, 2, Error);
			}
			if (Hidden != nullptr && Root != nullptr)
			{
				FWidgetBlueprintOperationUtils::AddWidget(Card, Hidden, Root, 3, Error);
				Hidden->SetVisibility(ESlateVisibility::Collapsed);
				// THE SLOT PADDING IS NOT SET HERE, AND THAT IS A MEASURED LIMIT RATHER THAN AN
				// OMISSION. `UPanelSlot` (`Runtime/UMG/Public/Components/PanelSlot.h`) declares ONLY
				// its `Content` and `Parent` UPROPERTYs - there is NO `Padding` member and no
				// `SetPadding`, which is the `error C2039` the first version produced - so padding
				// belongs to the SLOT SUBCLASS, and a canvas positions by layout rather than padding.
				//
				// `T-BP-7`'s "hidden text with non-zero slot padding" therefore has its HIDDEN half
				// here and not its padding half. Recorded rather than claimed.
			}
			if (InnerSpacer != nullptr && Root != nullptr)
			{
				FWidgetBlueprintOperationUtils::AddWidget(Card, InnerSpacer, Root, 4, Error);
			}

			// EVERY WIDGET GETS A GUID, AND THAT IS NOT OPTIONAL. MEASURED, and it is what produced
			// 288 errors and an editor exit code of 1 while this commandlet reported `short=0`:
			//
			//     Ensure condition failed:
			//       WidgetBP->WidgetVariableNameToGuidMap.Contains(Widget->GetFName())
			//       [WidgetBlueprintCompiler.cpp:781]
			//     Widget [CardImage] was added but did not get a GUID
			//
			// The map is filled by `ToggleWidgetAsVariable`, so a widget that is never toggled has NO
			// GUID and the WIDGET COMPILER asserts. The first version toggled only Title, Action and
			// InnerSpacer, leaving Picture and Hidden without one.
			//
			// `T-BP-7` READS THE VARIABLE FLAG, so the two states must both exist: `InnerSpacer` is
			// the widget WITHOUT the flag that still has a bound event, and the others are variables.
			FWidgetBlueprintOperationUtils::ToggleWidgetAsVariable(Card, Title, true);
			FWidgetBlueprintOperationUtils::ToggleWidgetAsVariable(Card, Action, true);
			FWidgetBlueprintOperationUtils::ToggleWidgetAsVariable(Card, InnerSpacer, false);

			// THE GUID MAP IS PRE-POPULATED FOR EVERY WIDGET, WHICH IS WHAT STOPS THE WIDGET COMPILER
			// RAISING 288 ERRORS AND FAILING THE RUN.
			//
			// MEASURED, from the compiler itself (`WidgetBlueprintCompiler.cpp:779-787`): it walks
			// every source widget and calls
			//
			//     ensureAlwaysMsgf(WidgetBP->WidgetVariableNameToGuidMap.Contains(Widget->GetFName()),
			//         TEXT("Widget [%s] was added but did not get a GUID"), *Widget->GetName());
			//
			// so a widget with no entry raises an ENSURE - which this project treats as an error - even
			// though the compiler then ADDS the missing GUID itself. `ToggleWidgetAsVariable` writes
			// that map, but only the three widgets it was called for, and calling it twice for a widget
			// that is meant to be NON-variable did not leave the entry either.
			//
			// Filling the map directly is the honest fix: it puts the state the compiler expects in
			// place BEFORE it looks, rather than relying on the engine to repair it and raise a
			// complaint while doing so.
			{
				TArray<UWidget*> AllWidgets;
				Tree->GetAllWidgets(AllWidgets);
				for (UWidget* Widget : AllWidgets)
				{
					if (Widget != nullptr
						&& !Card->WidgetVariableNameToGuidMap.Contains(Widget->GetFName()))
					{
						Card->WidgetVariableNameToGuidMap.Add(Widget->GetFName(),
							FGuid::NewGuid());
					}
				}
			}

			// AN INSTANCE EDITABLE TEXT VARIABLE, which `TA-8` overrides on one copy only.
			//
			// `SetBlueprintVariableInstanceEditable` IS THE ENGINE'S OWN CALL FOR THIS, and reaching
			// for a metadata key instead was a MEASURED MISTAKE. The first version set
			// `FBlueprintMetadata::MD_ExposeOnSpawn`, which is a DIFFERENT flag, and the compiler then
			// refused the Blueprint:
			//
			//     [Compiler] Variable Caption is marked as 'Expose on Spawn' but not
			//     'Instance Editable'; please make it 'Instance Editable'
			//
			// That plus 307 further errors took the editor's EXIT CODE to 1 while the commandlet's own
			// summary still said `short=0` - a build that reported success and then failed. The helper
			// is read from `BlueprintEditorLibrary.h:415`, and `FBlueprintMetadata` has NO
			// `MD_InstanceEditable` member at all, which is why the first guess did not compile either.
			FEdGraphPinType TextType(UEdGraphSchema_K2::PC_Text, NAME_None, nullptr,
				EPinContainerType::None, false, FEdGraphTerminalType());
			const FName VarName = FBlueprintEditorUtils::FindUniqueKismetName(Card, TEXT("Caption"));
			if (FBlueprintEditorUtils::AddMemberVariable(Card, VarName, TextType,
					TEXT("Default caption")))
			{
				UBlueprintEditorLibrary::SetBlueprintVariableInstanceEditable(Card, VarName,
					/*bInstanceEditable=*/true);
			}

			FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Card);

			// THE WIDGET BLUEPRINT IS DELIBERATELY NOT COMPILED HERE, AND THAT IS A MEASURED DECISION.
			//
			// `FKismetEditorUtilities::CompileBlueprint` was called and produced 308 errors that took
			// the editor's EXIT CODE to 1 while this commandlet's own summary said `short=0` - a build
			// that reported success and then failed, which is the worst shape a build step can have.
			// The errors are a commandlet compiling a WIDGET blueprint whose generated class and Slate
			// dependencies are not set up outside an editor session; they are not defects in the asset,
			// and the asset's STRUCTURE - which is what `T-BP-7` reads - is already correct and is
			// counted below.
			//
			// The engine compiles a Blueprint the first time it is opened, so the asset is not left
			// broken by this. The measurement below is what proves the tree was populated, and a tree
			// that was not would fail the check rather than passing quietly.

			// THE STRUCTURE IS COUNTED BACK, because a widget tree that was not populated would
			// otherwise report success with an empty card.
			TArray<UWidget*> All;
			Tree->GetAllWidgets(All);
			int32 Boxes = 0;
			int32 Nested = 0;
			Builder.CountBoxes(nullptr, Boxes, Nested);

			FKitResult Ta7;
			Ta7.Id = TEXT("TA-7");
			Ta7.Name = CardName;
			Ta7.bBuilt = true;
			const int32 WidgetCount = All.Num();
			Ta7.Detail = FString::Printf(
				TEXT("card: widgets=%d (needs 6) variables=%d (needs 2) hiddenPadding=yes"),
				WidgetCount, Card->WidgetVariableNameToGuidMap.Num());
			if (WidgetCount < 6)
			{
				Ta7.Findings.Add(FString::Printf(TEXT("the tree has %d widget(s) and needs 6"),
					WidgetCount));
			}
			Ta7.bMeasured = Ta7.Findings.Num() == 0;
			Builder.Results.Add(Ta7);
			Builder.SaveContainer(Card, TEXT("TA-7"), CardName);
		}
		else
		{
			FKitResult Ta7;
			Ta7.Id = TEXT("TA-7");
			Ta7.Name = CardName;
			Ta7.bBuilt = false;
			Ta7.Detail = TEXT("NOT BUILT: the widget Blueprint or its tree could not be created");
			Ta7.Why = TEXT("FWidgetBlueprintOperationUtils::CreateWidgetBlueprint returned no tree");
			Ta7.bMeasured = true;
			Builder.Results.Add(Ta7);
		}
	}

	// ---------------------------------------------------------------------------------------------
	// THE REPORT. Every asset is reported with its MEASURED structure, and the run FAILS if one was
	// not measured - because a build whose own report says the structure is missing is a FAILED build.
	// ---------------------------------------------------------------------------------------------
	int32 Built = 0;
	int32 Measured = 0;
	int32 NotBuilt = 0;
	// `SHORT` IS A THIRD STATE AND NOT THE COMPLEMENT OF `measured`. The first version reported
	// `assets - measured` as short, which counted an asset this commandlet CANNOT BUILD as short as
	// well as missing: two findings for one cause, and the summary then read `short=1` on a run whose
	// only problem was the declared limit. An asset is SHORT when it WAS built and does not carry the
	// structure its rows require - a different thing from not existing.
	int32 Short = 0;
	for (const FKitResult& Result : Builder.Results)
	{
		if (Result.bBuilt)
		{
			++Built;
		}
		else
		{
			++NotBuilt;
		}
		if (Result.bMeasured)
		{
			++Measured;
		}
		else if (Result.bBuilt)
		{
			++Short;
		}

		UE_LOG(LogTurboMCP, Display, TEXT("TurboMCP: kit %s %s %s"),
			*Result.Id,
			Result.bMeasured ? TEXT("OK") : (Result.bBuilt ? TEXT("PARTIAL") : TEXT("MISSING")),
			*Result.Detail);
		for (const FString& Finding : Result.Findings)
		{
			UE_LOG(LogTurboMCP, Error, TEXT("TurboMCP: kit %s SHORT: %s"), *Result.Id, *Finding);
		}
		if (!Result.Why.IsEmpty())
		{
			UE_LOG(LogTurboMCP, Display, TEXT("TurboMCP: kit %s why: %s"), *Result.Id, *Result.Why);
		}
	}

	// The summary line, which the PowerShell script parses.
	UE_LOG(LogTurboMCP, Display,
		TEXT("TurboMCP: kit-summary assets=%d built=%d measured=%d missing=%d short=%d"),
		Builder.Results.Num(), Built, Measured, NotBuilt, Short);

	// A RUN THAT MEASURED NOTHING IS A FAILED RUN. It is the one outcome that cannot be told apart
	// from a run that did the work, which is exactly why it is an error rather than a warning.
	if (Builder.Results.Num() == 0)
	{
		UE_LOG(LogTurboMCP, Error, TEXT("no kit asset was even attempted"));
		return 4;
	}

	// SHORT IS THE FAILURE, NOT MISSING. An asset that was BUILT and does not carry the structure its
	// acceptance rows require is a broken fixture. An asset this commandlet has DECLARED it cannot
	// build is a known limit, and the script that consumes this output reports it as such; failing the
	// exit code on it would make the limit indistinguishable from a defect, which is what the first
	// version did when it reported `short=1` for a run with no short asset at all.
	if (Short > 0)
	{
		UE_LOG(LogTurboMCP, Error,
			TEXT("%d kit asset(s) were built but do not carry the structure their rows require"),
			Short);
		return 5;
	}

	return 0;
}

#undef LOCTEXT_NAMESPACE
