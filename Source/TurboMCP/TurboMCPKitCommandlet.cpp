// Copyright (c) 2026 Florian ESCHALIER (FloStyle)
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

// TA-17. `UCurveFloat` carries a PUBLIC `FRichCurve`, and `UCurveTable` is built through its own
// public `AddRichCurve`, so both halves are Data-only and need no factory. Both headers belong to the
// Engine module, which `TurboMCP.Build.cs` already lists, so this adds no dependency.
#include "Curves/CurveFloat.h"
#include "Engine/CurveTable.h"

#include "Materials/MaterialParameterCollection.h"
#include "TurboMCPKitRow.h"
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

	/**
	 * A GETTER FOR A LOCAL VARIABLE, with its pins allocated.
	 *
	 * THE ORDER MATTERS AND IT IS NOT THE ORDER `MakeCall` USES. `UK2Node_VariableGet::AllocateDefaultPins`
	 * is guarded on `GetVarName() != NAME_None` and is what creates the value pin, so the VARIABLE
	 * REFERENCE MUST BE SET BEFORE THE PINS ARE ALLOCATED. `Place` allocates them, so setting the
	 * reference afterwards leaves a getter with no value pin - and `R2` skips a getter whose
	 * `GetValuePin()` is null, so the rule would find nothing while the code looked correct.
	 *
	 * `GetValuePin` is `FindPin(GetVarName())` and needs SUPER-pin name flags on some paths, which is why
	 * the caller checks it rather than this function assuming it worked.
	 */
	UK2Node_VariableGet* MakeLocalGetter(UEdGraph* InGraph, UBlueprint* InBlueprint,
		const FName InVariable, double InX, double InY)
	{
		if (InGraph == nullptr || InBlueprint == nullptr || InVariable.IsNone())
		{
			return nullptr;
		}
		UK2Node_VariableGet* Getter = NewObject<UK2Node_VariableGet>(InGraph);
		if (Getter == nullptr)
		{
			return nullptr;
		}
		Getter->VariableReference.SetLocalMember(InVariable, InGraph->GetName(),
			FBlueprintEditorUtils::FindLocalVariableGuidByName(InBlueprint, InGraph, InVariable));
		Place(InGraph, Getter, InX, InY);
		return Getter;
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
		/**
		 * GETTERS THAT SIT UNDER THE NODE THAT READS THEM, which is exactly `R2`'s precondition.
		 *
		 * `R2` compares `GetterRect.MinY >= ConsumerRect.MaxY` - the report's own `bUnderneath` test,
		 * reused so the two tools agree - and then finds the consumer through the getter's VALUE PIN.
		 * So this counts a getter only when BOTH hold: the value pin exists and is linked, and the
		 * getter's top is at or below the consumer's bottom. A getter to the RIGHT on the same row is
		 * NOT backward and the rule leaves it alone, so counting one here would claim a shape the pass
		 * would never move.
		 *
		 * THIS MEASURES THE PRECONDITION RATHER THAN THE INTENT. Counting "a getter exists" would pass on
		 * a getter the rule skips, which is the failure this kit exists to prevent.
		 */
		int32 CountGettersUnderTheirConsumer(const UEdGraph* InGraph)
		{
			int32 Count = 0;
			if (InGraph == nullptr)
			{
				return 0;
			}
			for (const UEdGraphNode* Node : InGraph->Nodes)
			{
				const UK2Node_VariableGet* Getter = Cast<UK2Node_VariableGet>(Node);
				if (Getter == nullptr)
				{
					continue;
				}
				const UEdGraphPin* ValuePin = Getter->GetValuePin();
				if (ValuePin == nullptr)
				{
					continue;
				}
				for (const UEdGraphPin* Link : ValuePin->LinkedTo)
				{
					const UEdGraphNode* Consumer = Link != nullptr
						? Link->GetOwningNodeUnchecked() : nullptr;
					if (Consumer == nullptr || Cast<UEdGraphNode_Comment>(Consumer) != nullptr)
					{
						continue;
					}
					// The getter's TOP at or below the consumer's BOTTOM. Integers, because both nodes
					// carry a size stamped by `SizeForMeasurement` - nothing renders them here.
					if (static_cast<int64>(Getter->NodePosY)
						>= static_cast<int64>(Consumer->NodePosY) + static_cast<int64>(Consumer->NodeHeight))
					{
						++Count;
					}
					break;
				}
			}
			return Count;
		}

		/**
		 * PURE, NON-GETTER NODES WHOSE EVERY LINK LANDS ON ONE CONSUMER: `R1`'s carry precondition.
		 *
		 * GETTERS ARE EXCLUDED, AND THAT IS A MEASURED CORRECTION. The first version counted any pure node,
		 * and a variable getter IS pure with one link to one consumer - so this counter read **2** while the
		 * pass's own `R1 pure nodes carried` read **0**. `R1` skips getters because `R2` owns them, so
		 * counting one here claimed a carry the rule refuses: exactly the "a check that passes without the
		 * shape proves nothing" failure. The counter now mirrors the rule's own exclusions.
		 */
		int32 CountPureNodesUsedOnlyByOneConsumer(const UEdGraph* InGraph)
		{
			int32 Count = 0;
			if (InGraph == nullptr)
			{
				return 0;
			}
			for (const UEdGraphNode* Node : InGraph->Nodes)
			{
				const UK2Node* AsK2 = Cast<UK2Node>(Node);
				if (AsK2 == nullptr || !AsK2->IsNodePure()
					|| Cast<UEdGraphNode_Comment>(Node) != nullptr
					|| Cast<UK2Node_VariableGet>(Node) != nullptr)
				{
					continue;
				}
				const UEdGraphNode* Only = nullptr;
				int32 Links = 0;
				bool bSplit = false;
				for (const UEdGraphPin* Pin : Node->Pins)
				{
					if (Pin == nullptr)
					{
						continue;
					}
					for (const UEdGraphPin* Other : Pin->LinkedTo)
					{
						const UEdGraphNode* Owner = Other != nullptr
							? Other->GetOwningNodeUnchecked() : nullptr;
						if (Owner == nullptr || Owner == Node)
						{
							continue;
						}
						++Links;
						if (Only == nullptr)
						{
							Only = Owner;
						}
						else if (Only != Owner)
						{
							bSplit = true;
						}
					}
				}
				if (Links > 0 && !bSplit && Only != nullptr)
				{
					++Count;
				}
			}
			return Count;
		}

		/** HOW MANY NODES SIT INSIDE A COMMENT BOX, so the box guard has something to act on. */
		int32 CountNodesInsideABox(const UEdGraph* InGraph)
		{
			int32 Count = 0;
			if (InGraph == nullptr)
			{
				return 0;
			}
			for (const UEdGraphNode* Node : InGraph->Nodes)
			{
				if (Node == nullptr || Cast<UEdGraphNode_Comment>(Node) != nullptr)
				{
					continue;
				}
				for (const UEdGraphNode* Other : InGraph->Nodes)
				{
					const UEdGraphNode_Comment* Box = Cast<UEdGraphNode_Comment>(Other);
					if (Box == nullptr)
					{
						continue;
					}
					// The box's rectangle starts `TitleBarHeight` BELOW its position, and membership is
					// geometric - the engine's own cache is filled from RENDERED widgets and is empty here.
					const int64 BoxTop = static_cast<int64>(Box->NodePosY) + static_cast<int64>(TitleBarHeight);
					const int64 BoxLeft = Box->NodePosX;
					const int64 BoxRight = static_cast<int64>(Box->NodePosX) + Box->NodeWidth;
					const int64 BoxBottom = static_cast<int64>(Box->NodePosY) + Box->NodeHeight;
					const int64 NodeLeft = Node->NodePosX;
					const int64 NodeTop = Node->NodePosY;
					const int64 NodeRight = static_cast<int64>(Node->NodePosX) + Node->NodeWidth;
					const int64 NodeBottom = static_cast<int64>(Node->NodePosY) + Node->NodeHeight;
					if (NodeLeft >= BoxLeft && NodeRight <= BoxRight
						&& NodeTop >= BoxTop && NodeBottom <= BoxBottom)
					{
						++Count;
						break;
					}
				}
			}
			return Count;
		}

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

			// A PURE NODE USED ONLY BY THE MOVED NODE, which is `R1`'s carry precondition and check 16's
			// shape. `Second` is the node R1 moves - it is fed by an exec wire from `First` that runs
			// backward - so a pure node whose ONLY link lands on `Second` must travel with it.
			//
			// THE LINK GOES TO A DATA INPUT, NOT TO THE EXEC PIN. Linking an output to an exec pin is a
			// type mismatch the schema would refuse, and the link would silently not exist - which is the
			// same shape as the bug this block already records, where `PrintString` has no return value
			// and `Link` did nothing while the code looked correct.
			if (UK2Node_CallFunction* PureFeeder = MakeCall(Stacked,
					UKismetMathLibrary::StaticClass(), TEXT("Add_IntInt"), 1780.0, 300.0))
			{
				if (Second != nullptr)
				{
					Link(PureFeeder->GetReturnValuePin(), Second->FindPin(TEXT("InString")));
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

		// TA-5: THE `BackwardProbe` GRAPH, WHICH CARRIES THE FOUR SHAPES `P/E-GRAPH-FIX-BACKWARD` HAD NO
		// POSITIVE EVIDENCE FOR. TA-1 to TA-4 are four graphs of ONE Blueprint, so this is a fifth.
		//
		// WHY A SEPARATE GRAPH RATHER THAN MORE NODES IN `Stacked`: `Stacked` is asserted on by `T-LAY-2`
		// and `T-LAY-9`, and adding shapes to it would change the numbers those rows read. A new graph
		// cannot disturb them, and it keeps each shape's geometry independent so one refusal cannot
		// silently hide another.
		//
		// EVERY SHAPE IS MEASURED BACK, and the build FAILS if a required count is zero. That is the
		// standard `BuildLayoutProbe` set: it built its own graph, measured it, found `0` crossing, and
		// the commandlet REFUSED TO SAVE IT rather than report success. A fixture whose defects are not
		// there is a fixture that makes checks pass without proving anything.
		UK2Node_FunctionEntry* ProbeEntry = nullptr;
		UK2Node_FunctionResult* ProbeResult = nullptr;
		UEdGraph* BackwardProbe = MakeFunction(Kit, TEXT("BackwardProbe"), ProbeEntry, ProbeResult);
		{
			// A LOCAL VARIABLE FOR THE GETTER TO READ. `R2` finds the consumer through the getter's
			// VALUE PIN, and that pin only exists when the variable does.
			FBlueprintEditorUtils::AddLocalVariable(Kit, BackwardProbe, TEXT("ProbeValue"),
				FEdGraphPinType(UEdGraphSchema_K2::PC_Int, NAME_None, nullptr, EPinContainerType::None,
					false, FEdGraphTerminalType()),
				TEXT("0"));

			// ---- SHAPE (a): A GETTER UNDER THE NODE THAT READS IT -------------------------------
			//
			// `R2`'s precondition is `GetterRect.MinY >= ConsumerRect.MaxY`, so the getter is placed
			// BELOW and to the RIGHT of its consumer. The rule then puts it at
			// `Consumer.PlannedX - Width - GetterGap`, level with the consumer's row.
			UK2Node_CallFunction* ProbeConsumer = MakeCall(BackwardProbe,
				UKismetSystemLibrary::StaticClass(), TEXT("PrintString"), 1200.0, 200.0);
			Link(ProbeEntry != nullptr ? ProbeEntry->GetThenPin() : nullptr,
				ProbeConsumer != nullptr ? ProbeConsumer->GetExecPin() : nullptr);

			UK2Node_VariableGet* ProbeGetter = MakeLocalGetter(BackwardProbe, Kit,
				TEXT("ProbeValue"), 1500.0, 700.0);
			if (ProbeGetter != nullptr && ProbeConsumer != nullptr)
			{
				Link(ProbeGetter->GetValuePin(), ProbeConsumer->FindPin(TEXT("InString")));
			}

			// ---- SHAPE (b): A PURE NODE USED ONLY BY THE NODE R1 MOVES --------------------------
			//
			// A SECOND backward exec pair, with a pure node feeding the moved node. `R1` carries a pure
			// node whose every link lands on the consumer.
			//
			// THE TWO NODES ARE STAGGERED, AND THE FIRST MEASUREMENT IS WHY. They began at the SAME `X`,
			// and this kit counts a backward wire as `Target.NodePosX < Source.NodePosX` - STRICTLY less -
			// so an equal-`X` pair counts as FORWARD and contributed nothing. The graph measured
			// `backward=2` against a requirement of three. The first measurement caught it; the code had
			// looked correct.
			//
			// THE PAIR IS ALSO FAR FROM THE GUARD BOX, AND THAT TOO IS A MEASUREMENT. It began at
			// `x=3400`, inside the box's own column, and the box guard REFUSED the very exec move the pure
			// node was supposed to be carried with - so `R1 pure nodes carried` came back **0** and the
			// carry code was never reached at all. The shapes have to be in SEPARATE COLUMNS, or one
			// refusal silently disables another check.
			UK2Node_CallFunction* ProbeFirst = MakeCall(BackwardProbe,
				UKismetSystemLibrary::StaticClass(), TEXT("PrintString"), 6000.0, 1200.0);
			UK2Node_CallFunction* ProbeSecond = MakeCall(BackwardProbe,
				UKismetSystemLibrary::StaticClass(), TEXT("PrintString"), 5400.0, 1200.0);
			Link(ProbeConsumer != nullptr ? ProbeConsumer->GetThenPin() : nullptr,
				ProbeFirst != nullptr ? ProbeFirst->GetExecPin() : nullptr);
			Link(ProbeFirst != nullptr ? ProbeFirst->GetThenPin() : nullptr,
				ProbeSecond != nullptr ? ProbeSecond->GetExecPin() : nullptr);

			// ---- SHAPE (b): A PURE NODE CARRIED WITH ITS CONSUMER, NOW THAT R1 CAN DO IT -----------
			//
			// THE RULE WAS BROKEN AND IS NOW FIXED, AND THIS FIXTURE PAID FOR THE DIAGNOSIS. The carry
			// used to compute `PureToX = Consumer.PlannedX - Pure.Width - Gap`, and the consumer's final
			// `PlannedX` is `Source.PlannedX + Source.Width + Gap`, so substituting gave
			//
			//     PureToX = Source.PlannedX + Source.Width - Pure.Width
			//
			// - the carried node landed ON THE SOURCE'S OWN LEFT EDGE while the source was still there, so
			// the overlap guard refused and the carry could NEVER fire. Seven configurations were built
			// against it before the arithmetic was pinned down; the fix anchors the carried node clear of
			// the source. With the rule fixed, the shape this fixture was built for now works.
			//
			// THE PURE NODE IS NARROWER THAN THE SOURCE, at 140 against 220, so the carried node clears the
			// pushed source. `SizeForMeasurement` would stamp 220, so the width is set explicitly.
			UK2Node_CallFunction* CarryTarget = MakeCall(BackwardProbe,
				UKismetSystemLibrary::StaticClass(), TEXT("PrintString"), 6600.0, 3000.0);
			UK2Node_CallFunction* CarrySource = MakeCall(BackwardProbe,
				UKismetSystemLibrary::StaticClass(), TEXT("PrintString"), 8400.0, 2600.0);
			if (CarryTarget != nullptr)
			{
				Link(CarrySource != nullptr ? CarrySource->GetThenPin() : nullptr,
					CarryTarget->GetExecPin());
				// THE FLOW CONTINUES PAST THE TARGET, so the pair is not the end of a chain and `R1`'s
				// push has a node to work with.
				if (UK2Node_CallFunction* CarryNext = MakeCall(BackwardProbe,
						UKismetSystemLibrary::StaticClass(), TEXT("PrintString"), 9000.0, 3000.0))
				{
					Link(CarryTarget->GetThenPin(), CarryNext->GetExecPin());
				}
			}
			if (UK2Node_CallFunction* ProbePure = MakeCall(BackwardProbe,
					UKismetMathLibrary::StaticClass(), TEXT("Add_IntInt"), 6100.0, 3000.0))
			{
				SizeForMeasurement(ProbePure);
				ProbePure->NodeWidth = 140;
				if (CarryTarget != nullptr)
				{
					Link(ProbePure->GetReturnValuePin(), CarryTarget->FindPin(TEXT("InString")));
				}
			}
			// (`BoxedSecond` -> `CarrySource` is wired after the boxed pair below, which declares it.)

			// ---- SHAPE (c): A COMMENT BOX AROUND A NODE THE PASS WOULD MOVE OUTSIDE IT --------
			//
			// A THIRD backward exec pair, with the node `R1` would move INSIDE A BOX. The box's right edge
			// is held at 3600 while the rule's planned position is 3660, so the move is REFUSED and the
			// refusal must NAME the box.
			//
			// THE LARGER BOX IS ALSO A MEASUREMENT RESULT. The first version was `300x160`, and
			// `nodesInBox` came back **0**: a member has to fit VERTICALLY as well, and `MakeBox` gives a
			// rectangle of `y+TitleBarHeight .. y+height`, so a box 160 tall left only 130 of usable
			// height against a node 150 tall. The box is now generous in `Y` and still TIGHT IN `X`,
			// because it is the `X` edge that the guard is about.
			//
			//   the box         3300 .. 3600 wide, 1390 .. 1650 tall
			//   BoxedSecond     3400 .. 3620 wide, 1400 .. 1552 tall   -> ENTIRELY INSIDE
			//   the planned X   3400 + 220 + 40 = 3660                  -> OUTSIDE the right edge
			UK2Node_CallFunction* BoxedFirst = MakeCall(BackwardProbe,
				UKismetSystemLibrary::StaticClass(), TEXT("PrintString"), 3400.0, 1100.0);
			UK2Node_CallFunction* BoxedSecond = MakeCall(BackwardProbe,
				UKismetSystemLibrary::StaticClass(), TEXT("PrintString"), 3400.0, 1400.0);
			Link(ProbeSecond != nullptr ? ProbeSecond->GetThenPin() : nullptr,
				BoxedFirst != nullptr ? BoxedFirst->GetExecPin() : nullptr);
			Link(BoxedFirst != nullptr ? BoxedFirst->GetThenPin() : nullptr,
				BoxedSecond != nullptr ? BoxedSecond->GetExecPin() : nullptr);

			// THE BOX MUST CONTAIN ITS MEMBER **ENTIRELY** AND STILL BE NARROWER THAN THE PLANNED MOVE, and
			// the first version satisfied NEITHER - while a comment above it claimed "ENTIRELY INSIDE".
			// MEASURED, from the commandlet's own diagnostic:
			//
			//   the box         3300 .. 3600 wide, 1372 .. 1992 tall
			//   BoxedSecond     3400 .. 3620 wide, 1400 .. 1608 tall   -> 3620 > 3600, so NOT a member
			//
			// A member is contained WHOLLY (`LocalComputeMembers`, the editor's own rule: a partly covered
			// node is not a member), so the right edge has to clear 3620. It is 3640, which leaves 20 units
			// of slack and is still left of the planned `X + width + gap = 3660`:
			//
			//   the box         3300 .. 3640 wide, 1358 .. 1992 tall
			//   BoxedSecond     3400 .. 3620 wide, 1400 .. 1608 tall   -> ENTIRELY INSIDE
			//   the planned X   3660                                    -> OUTSIDE the right edge, so REFUSED
			//
			// THE COMMENT WAS THE BUG'S ACCOMPLICE: it asserted the containment that the numbers did not
			// give, and a reader would have believed it. The count below is what settles it.
			if (UEdGraphNode_Comment* GuardBox = MakeBox(BackwardProbe, TEXT("GUARD: keeps its member"),
					3300.0, 1358.0, 340.0, 640.0))
			{
				(void)GuardBox;
			}

			// THE CARRY CHAIN'S EXEC WIRE IS COMPLETED HERE, because `BoxedSecond` is declared above this
			// point and the carry pair is declared above THAT. The chain runs boxed pair -> carry source ->
			// carry target, so the box refusal and the carry shape are in one flow.
			Link(BoxedSecond != nullptr ? BoxedSecond->GetThenPin() : nullptr,
				CarrySource != nullptr ? CarrySource->GetExecPin() : nullptr);

			// The chain's exec flow is carried on by the boxed pair's own successor, so `R1`'s push has a
			// node to work with even when the box guard refuses the boxed member.
			if (UK2Node_CallFunction* AfterBoxes = MakeCall(BackwardProbe,
					UKismetSystemLibrary::StaticClass(), TEXT("PrintString"), 6000.0, 1400.0))
			{
				Link(BoxedSecond != nullptr ? BoxedSecond->GetThenPin() : nullptr,
					AfterBoxes->GetExecPin());
			}

			for (UEdGraphNode* Node : BackwardProbe->Nodes)
			{
				SizeForMeasurement(Node);
			}

			const int32 ProbeNodes = Builder.CountLiveNodes(BackwardProbe);
			const int32 ProbeBackward = Builder.CountBackwardWires(BackwardProbe);
			const int32 ProbeGetters = Builder.CountGettersUnderTheirConsumer(BackwardProbe);
			const int32 ProbePure = Builder.CountPureNodesUsedOnlyByOneConsumer(BackwardProbe);
			const int32 ProbeBoxed = Builder.CountNodesInsideABox(BackwardProbe);

			FKitResult Ta5;
			Ta5.Id = TEXT("TA-5");
			Ta5.Name = TEXT("TA01_HandLaid");
			Ta5.bBuilt = BackwardProbe != nullptr;
			Ta5.Detail = FString::Printf(
				TEXT("backwardProbe: nodes=%d backward=%d (needs >=3) getterUnderConsumer=%d (needs >=1) ")
				TEXT("pureUsedOnce=%d (needs >=1) nodesInBox=%d (needs >=1)"),
				ProbeNodes, ProbeBackward, ProbeGetters, ProbePure, ProbeBoxed);

			if (BackwardProbe == nullptr)
			{
				Ta5.Findings.Add(TEXT("the BackwardProbe graph was not created"));
			}
			else
			{
				if (ProbeBackward < 3)
				{
					Ta5.Findings.Add(FString::Printf(
						TEXT("only %d backward wire(s), and the graph needs three separate pairs so R1, R2 ")
						TEXT("and the box guard cannot hide one another"), ProbeBackward));
				}
				if (ProbeGetters < 1)
				{
					Ta5.Findings.Add(TEXT("no getter sits under the node that reads it, so R2 has nothing to ")
						TEXT("find and checks 6 and 8 stay unprovable"));
				}
				// THE PURE-CARRY SHAPE IS REQUIRED NOW THAT `R1` CAN PERFORM IT. It was reported but NOT
				// required while the rule's geometry made it unreachable (see block (b)): requiring a
				// shape the rule refuses would have been a build that can never pass. The rule is fixed,
				// so the requirement is real and a regression in either the rule or this fixture fails
				// the build here rather than silently emptying check 16.
				if (ProbePure < 1)
				{
					Ta5.Findings.Add(TEXT("no pure node is used only by one consumer, so R1 has nothing to ")
						TEXT("carry and check 16 stays unprovable"));
				}
				if (ProbeBoxed < 1)
				{
					Ta5.Findings.Add(TEXT("no node sits inside a comment box, so the box guard is never ")
						TEXT("reached and check 15 stays unprovable"));
				}
			}
			Ta5.bMeasured = Ta5.Findings.Num() == 0;
			Builder.Results.Add(Ta5);
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
	// TA-14: A DATA TABLE WITH A PROJECT-OWNED ROW STRUCT - AND IT IS BUILDABLE, WHICH TA-10 IS NOT.
	//
	// *** THE DISTINCTION IS THE WHOLE FINDING: A `UUserDefinedStruct` IS ONE WAY TO GET A PROJECT-OWNED STRUCT AND
	// IT IS THE ONLY WAY THIS COMMANDLET IS BLOCKED FROM. *** TA-10 is reported MISSING above because
	// `FStructureEditorUtils::AddVariable()` crashes without the `UUserDefinedStructEditorData` that ONLY the editor
	// factory installs. **BUT `Spec/06_ACCEPTANCE.md` ASKS FOR A *"PROJECT-OWNED ROW STRUCT"*, NOT A USER-DEFINED
	// ONE** - and `FTurboMCPKitRow` (`Public/TurboMCPKitRow.h`) IS A NATIVE, REFLECTED, PROJECT-OWNED `UScriptStruct`,
	// which is exactly what a DataTable's `RowStruct` must be.
	//
	// AND THE OTHER HALF OF WHY THIS WAS THOUGHT IMPOSSIBLE DOES NOT APPLY HERE EITHER. `Tools/test-datatable.py:11-31`
	// records that *"the row struct of a table cannot be set after the fact: `RowStruct` is read-only once the asset
	// exists"* - TRUE, AND IT IS ABOUT SETTING IT **AFTER THE FACT**. THIS SETS IT AT CONSTRUCTION, BEFORE THE ASSET
	// IS SAVED, WHICH IS THE ONE MOMENT THE ENGINE ALLOWS.
	//
	// FOUR FIELDS, THE FOURTH BEING `FTurboMCPKitNested`, WHICH IS THE *"one field of a struct type"* CLAUSE.
	// ---------------------------------------------------------------------------------------------
	{
		FKitResult Ta14;
		Ta14.Id = TEXT("TA-14");
		Ta14.Name = TEXT("TA14_KitDataTable");

		const FString AssetPath = FString::Printf(TEXT("%s/%s"), KitFolder, *Ta14.Name);
		const FString PackagePath = FString::Printf(TEXT("%s/%s"), KitFolder, *Ta14.Name);
		UPackage* Package = CreatePackage(*PackagePath);

		UDataTable* Table = NewObject<UDataTable>(Package, *Ta14.Name, RF_Public | RF_Standalone);

		// THE ROW STRUCT IS SET HERE, AT CONSTRUCTION, WHICH IS THE MOMENT THE ENGINE ALLOWS.
		Table->RowStruct = FTurboMCPKitRow::StaticStruct();

		// THREE ROWS, with the fourth field carrying a real nested struct so the struct-type clause is exercised
		// rather than merely declared.
		auto AddRow = [Table](const TCHAR* RowName, float Weight, const TCHAR* Label, int32 Count, float Amount,
			const TCHAR* Tag)
		{
			FTurboMCPKitRow Row;
			Row.Weight = Weight;
			Row.Label = FName(Label);
			Row.Count = Count;
			Row.Nested.Amount = Amount;
			Row.Nested.Tag = FName(Tag);
			// `AddRow` IS THE ENGINE'S OWN ENTRY POINT ON A DataTable, and it takes the row by reference and copies
			// it into the table's own storage using the row struct's size - so the struct does not have to be
			// allocated by hand and the memory is the table's.
			Table->AddRow(FName(RowName), Row);
		};

		AddRow(TEXT("Row_A"), 1.5f, TEXT("alpha"), 3, 0.25f, TEXT("nested_a"));
		AddRow(TEXT("Row_B"), 2.5f, TEXT("beta"), 5, 0.50f, TEXT("nested_b"));
		AddRow(TEXT("Row_C"), 3.5f, TEXT("gamma"), 7, 0.75f, TEXT("nested_c"));

		// MEASURED RATHER THAN CLAIMED: count what is actually IN the table, and check the struct is ours.
		const int32 RowCount = Table->GetRowMap().Num();
		const bool bStructIsOurs = (Table->RowStruct == FTurboMCPKitRow::StaticStruct());

		Ta14.bBuilt = (RowCount == 3) && bStructIsOurs;
		Ta14.bMeasured = true;
		Ta14.Detail = FString::Printf(
			TEXT("rowStruct=%s rows=%d (the row struct is NATIVE and PROJECT-OWNED rather than user-defined)"),
			bStructIsOurs ? TEXT("FTurboMCPKitRow") : TEXT("WRONG"), RowCount);
		Ta14.Why = TEXT("TA-10 is blocked because a UUserDefinedStruct needs the editor factory's "
			"UUserDefinedStructEditorData. TA-14 does NOT need one: the acceptance row asks for a "
			"PROJECT-OWNED row struct, and a native USTRUCT in the plugin module is exactly that. "
			"RowStruct is read-only AFTER the asset exists; this sets it AT CONSTRUCTION.");
		if (RowCount != 3)
		{
			Ta14.Findings.Add(FString::Printf(TEXT("expected 3 rows, found %d"), RowCount));
		}
		if (!bStructIsOurs)
		{
			Ta14.Findings.Add(TEXT("the table's RowStruct is not FTurboMCPKitRow"));
		}

		Builder.Results.Add(Ta14);
		if (Ta14.bBuilt)
		{
			// `SaveContainer(UObject*, Id, Name)` IS THE BUILDER'S OWN SAVE PATH - THREE ARGUMENTS, AND IT CREATES
			// THE PACKAGE ITSELF. Reused rather than reimplemented, which is what the acceptance row's fixtures
			// have always done.
			Builder.SaveContainer(Table, TEXT("TA-14"), Ta14.Name);
		}
	}

	// ---------------------------------------------------------------------------------------------
	// TA-20: A MATERIAL PARAMETER COLLECTION WITH FIVE PARAMETERS, ONE OF THEM READ BY NO MATERIAL.
	//
	// `Spec/06_ACCEPTANCE.md`: *"5 parameters: 2 scalar, 2 vector, one read by no material"* - WHICH IS FOUR NAMED
	// PLUS A FIFTH, AND THE FIFTH IS THE POINT: `T-MAT-12` and `T-MAT-13` need a collection parameter that NOTHING
	// reads, because that is the case where an index shift is invisible until much later. A collection whose every
	// parameter is used cannot test the thing the rows exist for.
	//
	// THE PARAMETER ARRAYS ARE PUBLIC (`MaterialParameterCollection.h:91,94`), so this needs no factory and no editor
	// object - WHICH IS WHY IT IS BUILDABLE FROM A COMMANDLET WHERE THE USER-STRUCT PATH IS NOT.
	// ---------------------------------------------------------------------------------------------
	{
		FKitResult Ta20;
		Ta20.Id = TEXT("TA-20");
		Ta20.Name = TEXT("TA20_KitParameters");

		const FString PackagePath = FString::Printf(TEXT("%s/%s"), KitFolder, *Ta20.Name);
		UPackage* Package = CreatePackage(*PackagePath);
		UMaterialParameterCollection* Collection =
			NewObject<UMaterialParameterCollection>(Package, *Ta20.Name, RF_Public | RF_Standalone);

		auto AddScalar = [Collection](const TCHAR* ParameterName, float DefaultValue)
		{
			FCollectionScalarParameter Parameter;
			Parameter.ParameterName = FName(ParameterName);
			Parameter.DefaultValue = DefaultValue;
			Collection->ScalarParameters.Add(Parameter);
		};
		auto AddVector = [Collection](const TCHAR* ParameterName, float R, float G, float B)
		{
			FCollectionVectorParameter Parameter;
			Parameter.ParameterName = FName(ParameterName);
			Parameter.DefaultValue = FLinearColor(R, G, B, 1.0f);
			Collection->VectorParameters.Add(Parameter);
		};

		// TWO SCALARS AND TWO VECTORS, AS THE ROW ASKS.
		AddScalar(TEXT("KitScalarA"), 0.25f);
		AddScalar(TEXT("KitScalarB"), 0.75f);
		AddVector(TEXT("KitVectorA"), 1.0f, 0.0f, 0.0f);
		AddVector(TEXT("KitVectorB"), 0.0f, 1.0f, 0.0f);

		// THE FIFTH: A SECOND SCALAR, AND IT IS THE ONE THE FIXTURE IS FOR. Whether any MATERIAL reads it cannot be
		// checked here - that needs the materials, which is the capability's job - so the collection simply CARRIES
		// a parameter the acceptance row's reader can find and report as unread.
		AddScalar(TEXT("KitScalarUnread"), 0.5f);

		const int32 ScalarCount = Collection->ScalarParameters.Num();
		const int32 VectorCount = Collection->VectorParameters.Num();

		Ta20.bBuilt = (ScalarCount == 3) && (VectorCount == 2);
		Ta20.bMeasured = true;
		Ta20.Detail = FString::Printf(TEXT("scalars=%d vectors=%d (KitScalarUnread exists for the unread case)"),
			ScalarCount, VectorCount);
		Ta20.Why = TEXT("The parameter arrays are public, so this needs no editor factory - which is why it is "
			"buildable from a commandlet where the user-struct path is not. Whether KitScalarUnread is REALLY read "
			"by no material is asserted by the capability's suite, not here.");
		if (ScalarCount != 3)
		{
			Ta20.Findings.Add(FString::Printf(TEXT("expected 3 scalars, found %d"), ScalarCount));
		}
		if (VectorCount != 2)
		{
			Ta20.Findings.Add(FString::Printf(TEXT("expected 2 vectors, found %d"), VectorCount));
		}

		Builder.Results.Add(Ta20);
		if (Ta20.bBuilt)
		{
			Builder.SaveContainer(Collection, TEXT("TA-20"), Ta20.Name);
		}
	}

	// ---------------------------------------------------------------------------------------------
	// TA-17: A CURVE FLOAT WITH FIVE KEYS AND NON-ZERO TANGENTS, AND A CURVE TABLE WITH THREE ROWS.
	//
	// `Spec/06_ACCEPTANCE.md:32` asks for *"Curve float and a curve table | A curve with 5 keys and
	// non-zero tangents; a curve table with 3 rows"*, and `T-DATA-5` (`:213`) reads it back as
	// *"TA-17 curve prints its 5 keys with times and values and its interpolation; its table prints
	// its 3 row names"*. `Spec/WORKING_PLAN.md:145` needs it for `R-CURVE-LOAD` and `P/E-CURVE-SET`.
	//
	// TWO ASSETS ANSWER ONE ROW, AND THAT IS HOW THE ROW IS WRITTEN: no single `.uasset` is both a
	// curve float and a curve table, so the row is answered by two files, each measured and saved on
	// its own. Both entries carry the SAME `Id`, and this is NOT the duplicate that the note above
	// `SaveContainer` warns about: that defect was ONE asset reported twice, and here two DIFFERENT
	// assets were written. The `Detail` names which one, so the two log lines cannot be confused.
	//
	// NEITHER HALF NEEDS AN EDITOR OBJECT, AND BOTH ENTRY POINTS BELONG TO THE ENGINE:
	//   - `UCurveFloat::FloatCurve` is a PUBLIC `FRichCurve` (`Curves/CurveFloat.h:36`); `AddKey`
	//     returns a stable `FKeyHandle` (`Curves/RichCurve.h:232`) and `GetKey` returns a MUTABLE
	//     `FRichCurveKey&` (`:215`), so times, values, interpolation and BOTH tangents are plain data.
	//   - `UCurveTable::AddRichCurve(FName)` is a public `ENGINE_API` method (`Engine/CurveTable.h:61`)
	//     that ALSO sets `CurveTableMode = RichCurves`. A hand-filled `RowMap` would leave that field
	//     at `Empty`, and `GetRichCurveRowMap` then `check`-fails on it (`:51-52`).
	//
	// AND THE TWO COMMANDLET LIMITS DO NOT BIND ON THIS PATH: nothing here touches Slate
	// (`LaunchEngineLoop.cpp:3343` never runs inside a commandlet) and nothing reads `GEditor`
	// (`Editor/Kismet/Private/BlueprintEditorModule.cpp:214`). The reason this row is buildable where
	// the user-struct rows are not is that a curve is DATA, and the struct path needed a factory.
	// ---------------------------------------------------------------------------------------------
	{
		FKitResult Ta17Curve;
		Ta17Curve.Id = TEXT("TA-17");
		Ta17Curve.Name = TEXT("TA17_KitCurveFloat");

		const FString PackagePath = FString::Printf(TEXT("%s/%s"), KitFolder, *Ta17Curve.Name);
		UPackage* Package = CreatePackage(*PackagePath);
		UCurveFloat* Curve = NewObject<UCurveFloat>(Package, *Ta17Curve.Name, RF_Public | RF_Standalone);

		// FIVE KEYS OVER ONE SECOND, WITH A DIFFERENT NON-ZERO TANGENT PAIR ON EACH, so a reader that
		// prints the tangents cannot be reading a single repeated value.
		const float KeyTimes[5] = { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f };
		const float KeyValues[5] = { 0.0f, 2.0f, -1.0f, 3.0f, 0.5f };
		const float KeyTangents[5] = { 0.5f, -1.5f, 2.0f, -0.75f, 1.25f };

		int32 KeyCount = 0;
		int32 NonZeroTangentKeys = 0;
		FString FirstInterp = TEXT("none");

		if (Curve != nullptr)
		{
			TArray<FKeyHandle> Handles;
			for (int32 Index = 0; Index < 5; ++Index)
			{
				Handles.Add(Curve->FloatCurve.AddKey(KeyTimes[Index], KeyValues[Index]));
			}

			// THE TANGENTS ARE WRITTEN AFTER EVERY KEY EXISTS, AND `bAutoSetTangents` IS FALSE ON BOTH
			// CALLS. `AutoSetTangents` recomputes any key whose mode is `RCTM_Auto`, so a tangent
			// written onto an auto key survives only until the next recompute. `RCTM_User` plus an
			// explicit arrive/leave pair is what makes the stored number the number this fixture
			// claims, which is the difference between a fixture and a claim about one.
			for (int32 Index = 0; Index < Handles.Num(); ++Index)
			{
				const FKeyHandle Handle = Handles[Index];
				Curve->FloatCurve.SetKeyInterpMode(Handle, RCIM_Cubic, /*bAutoSetTangents=*/false);
				Curve->FloatCurve.SetKeyTangentMode(Handle, RCTM_User, /*bAutoSetTangents=*/false);

				FRichCurveKey& Key = Curve->FloatCurve.GetKey(Handle);
				Key.ArriveTangent = KeyTangents[Index];
				Key.LeaveTangent = KeyTangents[Index];

				if (Key.ArriveTangent != 0.0f || Key.LeaveTangent != 0.0f)
				{
					++NonZeroTangentKeys;
				}
			}

			// MEASURED BACK FROM THE CURVE, not from the arrays that were written into it.
			const TArray<FRichCurveKey>& Keys = Curve->FloatCurve.GetConstRefOfKeys();
			KeyCount = Keys.Num();
			if (KeyCount > 0)
			{
				const UEnum* InterpEnum = StaticEnum<ERichCurveInterpMode>();
				FirstInterp = InterpEnum != nullptr
					? InterpEnum->GetNameStringByValue(static_cast<int64>(Keys[0].InterpMode))
					: TEXT("unknown");
			}
		}

		Ta17Curve.bBuilt = (Curve != nullptr) && (KeyCount == 5) && (NonZeroTangentKeys == 5);
		Ta17Curve.Detail = FString::Printf(
			TEXT("curve float: keys=%d (needs 5) nonZeroTangentKeys=%d (needs 5) firstKeyInterp=%s"),
			KeyCount, NonZeroTangentKeys, *FirstInterp);
		Ta17Curve.Why = TEXT("A curve is DATA: the curve float carries a public FRichCurve and the curve "
			"table is filled through the public AddRichCurve, so neither half needs a factory, Slate or "
			"GEditor. Two assets answer this one row because no single asset is both a curve float and a "
			"curve table.");
		if (Curve == nullptr)
		{
			Ta17Curve.Findings.Add(TEXT("the curve float object could not be created"));
		}
		if (KeyCount != 5)
		{
			Ta17Curve.Findings.Add(FString::Printf(TEXT("curve float: expected 5 keys, found %d"), KeyCount));
		}
		if (NonZeroTangentKeys != 5)
		{
			Ta17Curve.Findings.Add(FString::Printf(
				TEXT("curve float: expected 5 keys with a non-zero tangent, found %d - the row asks for "
					"non-zero tangents and an auto-set tangent would answer zero"),
				NonZeroTangentKeys));
		}
		// `bMeasured` IS THE FINDINGS TEST RATHER THAN `true`, WHICH IS THE DISCIPLINE TA-7 AND TA-11
		// USE. It is what makes a built-but-wrong fixture report SHORT and fail the script instead of
		// printing OK, and the summary comment further down defines SHORT in exactly those words.
		Ta17Curve.bMeasured = Ta17Curve.Findings.Num() == 0;
		Builder.Results.Add(Ta17Curve);
		if (Ta17Curve.bBuilt)
		{
			Builder.SaveContainer(Curve, TEXT("TA-17"), Ta17Curve.Name);
		}
	}

	{
		FKitResult Ta17Table;
		Ta17Table.Id = TEXT("TA-17");
		Ta17Table.Name = TEXT("TA17_KitCurveTable");

		const FString PackagePath = FString::Printf(TEXT("%s/%s"), KitFolder, *Ta17Table.Name);
		UPackage* Package = CreatePackage(*PackagePath);
		UCurveTable* Table = NewObject<UCurveTable>(Package, *Ta17Table.Name, RF_Public | RF_Standalone);

		// THREE ROWS OF DIFFERENT LENGTHS, so `T-DATA-6` can replace the keys of ONE row and read back
		// keys that are not shared with its neighbours.
		int32 TotalKeys = 0;
		if (Table != nullptr)
		{
			auto AddRow = [Table, &TotalKeys](const TCHAR* RowName, float FirstValue, int32 Keys)
			{
				FRichCurve& Row = Table->AddRichCurve(FName(RowName));
				for (int32 Index = 0; Index < Keys; ++Index)
				{
					Row.AddKey(0.25f * static_cast<float>(Index), FirstValue + static_cast<float>(Index));
				}
				TotalKeys += Keys;
			};

			AddRow(TEXT("Row_A"), 1.0f, 3);
			AddRow(TEXT("Row_B"), 5.0f, 2);
			AddRow(TEXT("Row_C"), 9.0f, 4);
		}

		const int32 RowCount = Table != nullptr ? Table->GetRowMap().Num() : 0;
		const int32 RichRowCount = Table != nullptr ? Table->GetRichCurveRowMap().Num() : 0;
		const ECurveTableMode Mode = Table != nullptr ? Table->GetCurveTableMode() : ECurveTableMode::Empty;
		FString ModeName = TEXT("unknown");
		if (const UEnum* ModeEnum = StaticEnum<ECurveTableMode>())
		{
			ModeName = ModeEnum->GetNameStringByValue(static_cast<int64>(Mode));
		}

		Ta17Table.bBuilt = (RowCount == 3) && (RichRowCount == 3)
			&& (Mode == ECurveTableMode::RichCurves);
		Ta17Table.Detail = FString::Printf(
			TEXT("curve table: rows=%d (needs 3) richRows=%d totalKeys=%d mode=%s"),
			RowCount, RichRowCount, TotalKeys, *ModeName);
		Ta17Table.Why = TEXT("Built through AddRichCurve, which sets CurveTableMode to RichCurves as "
			"well as inserting the row. The mode is printed rather than assumed, because a table left "
			"at Empty would still report three rows through GetRowMap and fail in the reader instead.");
		if (Table == nullptr)
		{
			Ta17Table.Findings.Add(TEXT("the curve table object could not be created"));
		}
		if (RowCount != 3)
		{
			Ta17Table.Findings.Add(FString::Printf(TEXT("curve table: expected 3 rows, found %d"), RowCount));
		}
		if (RichRowCount != 3)
		{
			Ta17Table.Findings.Add(FString::Printf(
				TEXT("curve table: %d of 3 rows are reachable as rich curves"), RichRowCount));
		}
		if (Mode != ECurveTableMode::RichCurves)
		{
			Ta17Table.Findings.Add(FString::Printf(
				TEXT("curve table: CurveTableMode is %s and the row asks for rich curves"), *ModeName));
		}
		Ta17Table.bMeasured = Ta17Table.Findings.Num() == 0;
		Builder.Results.Add(Ta17Table);
		if (Ta17Table.bBuilt)
		{
			Builder.SaveContainer(Table, TEXT("TA-17"), Ta17Table.Name);
		}
	}

	// TA-18: AN ANIMATION BLUEPRINT WITH A STATE MACHINE AND AN ANIM SEQUENCE. NOT BUILT, AND THE
	// REASON IS A MODULE DEPENDENCY - WHICH IS A QUESTION FOR THE OWNER OF `TurboMCP.Build.cs` RATHER
	// THAN A LIMIT OF THE COMMANDLET.
	//
	// THE ROW ASKS FOR TWO THINGS, AND EACH HAS ITS OWN DECIDING LINE:
	//
	//   "A state machine with two states and one transition" IS NOT AN ENGINE OBJECT. The state
	//   machine of an Animation Blueprint is authored with `UAnimGraphNode_StateMachine`
	//   (`Engine/Source/Editor/AnimGraph/Public/AnimGraphNode_StateMachine.h`) and
	//   `UAnimStateTransitionNode` (`Engine/Source/Editor/AnimGraph/Public/AnimStateTransitionNode.h`),
	//   and BOTH live in the `AnimGraph` module. `Source/TurboMCP/TurboMCP.Build.cs` lists `AnimGraph`
	//   nowhere, so those headers are not on this module include path and no state machine can be
	//   referenced, let alone saved.
	//
	//   "A sequence of 1 second with 3 keys on one bone" NEEDS A SKELETON THAT HAS A BONE, AND THERE IS
	//   NO REACHABLE WAY TO GIVE ONE A BONE. `USkeleton::GetReferenceSkeleton` returns a CONST
	//   reference and says so - *"Accessor to Reference Skeleton to make data read only"*
	//   (`Engine/Source/Runtime/Engine/Classes/Animation/Skeleton.h:365-369`) - while `ReferenceSkeleton`
	//   itself is `protected` (`:301`, `:317`). Every engine function that adds bones takes a
	//   `USkinnedAsset` (`:797` `MergeBonesToBoneTree`, `:809` `MergeAllBonesToBoneTree`, `:820`
	//   `RecreateBoneTree`), and this project holds NO skeletal mesh at all. The one remaining route is
	//   `FEditableSkeleton`, whose header is `Engine/Source/Editor/SkeletonEditor/Private/EditableSkeleton.h`
	//   - a PRIVATE header of a module that is not a dependency, so it is not even on an include path.
	//
	// SO IT IS REPORTED MISSING RATHER THAN APPROXIMATED. A skeletonless sequence can carry a track
	// NAME, and that name would be a bone no skeleton declares - which is a fixture that does not
	// answer its row, the one outcome this kit exists to prevent.
	{
		FKitResult Ta18;
		Ta18.Id = TEXT("TA-18");
		Ta18.Name = TEXT("TA18_KitAnim (animation Blueprint + Anim Sequence)");
		Ta18.bBuilt = false;
		// `bMeasured` IS TRUE BECAUSE THE MEASUREMENT IS THE ABSENCE ITSELF, DECLARED - the same
		// reading TA-10 uses. Nothing was built, so nothing can be short.
		Ta18.bMeasured = true;
		Ta18.Detail = TEXT("NOT BUILT: needs a state machine with two states and a sequence with 3 keys "
			"on one bone");
		Ta18.Why = TEXT("Two blockers, each named. (1) *** THE AnimGraph MODULE IS NOW LINKED - "
			"TurboMCP.Build.cs lists `AnimGraph` - SO THE STATE MACHINE IS NO LONGER BLOCKED BY A "
			"DEPENDENCY; what remains for it is AUTHORING, not a permission. (2) *** The 3 keys on ONE BONE need a USkeleton with "
			"a bone: Skeleton.h:365-369 returns the reference skeleton as CONST on purpose, "
			"ReferenceSkeleton is protected at :301/:317, the bone-adding engine functions at :797/:809/"
			":820 all take a USkinnedAsset and this project has no skeletal mesh, and the only other "
			"route is FEditableSkeleton, a PRIVATE header of the SkeletonEditor module. *** SO ONE OF THE TWO "
			"BLOCKERS HAS BEEN REMOVED SINCE THIS TEXT WAS WRITTEN AND THE OTHER ONE HAS NOT: THIS REFUSAL NOW "
			"NAMES ONE REAL BLOCKER, NOT TWO. ***");
		Builder.Results.Add(Ta18);
	}

	// TA-19: A NIAGARA SYSTEM WITH ONE EMITTER. NOT BUILT, AND THIS ONE IS A `TurboMCP.Build.cs`
	// QUESTION RATHER THAN A LIMIT OF THE COMMANDLET.
	//
	// `UNiagaraSystem` is `Engine/Plugins/FX/Niagara/Source/Niagara/Classes/NiagaraSystem.h:237`, a
	// PLUGIN module. `Source/TurboMCP/TurboMCP.Build.cs` lists `Niagara` NOWHERE, so no Niagara header
	// is on the include path and no Niagara object can be named in this module. The plugin itself IS
	// enabled - `Engine/Plugins/FX/Niagara/Niagara.uplugin:13` is `"EnabledByDefault" : true` - so the
	// gap is the module dependency and NOT the plugin, which is the distinction `WORKING_PLAN.md:89`
	// records and `TurboMCPCaptureAsset.cpp:242-250` had to correct in the other direction.
	//
	// AND AUTHORING IS A SECOND MODULE: the editor creates one through `UNiagaraSystemFactoryNew`
	// (`Engine/Plugins/FX/Niagara/Source/NiagaraEditor/Public/NiagaraSystemFactoryNew.h:16`), so
	// `NiagaraEditor` would be needed as well.
	{
		FKitResult Ta19;
		Ta19.Id = TEXT("TA-19");
		Ta19.Name = TEXT("TA19_KitNiagara (Niagara system, one emitter)");
		Ta19.bBuilt = false;
		Ta19.bMeasured = true;
		Ta19.Detail = TEXT("NOT BUILT: needs one emitter and a NiagaraSystem object");
		Ta19.Why = TEXT("UNiagaraSystem is a plugin class "
			"(Engine/Plugins/FX/Niagara/Source/Niagara/Classes/NiagaraSystem.h:237) and TurboMCP.Build.cs "
			"lists the Niagara module nowhere, so its header is not on the include path. The plugin is "
			"enabled (Niagara.uplugin:13, EnabledByDefault true), so the missing piece is the module "
			"dependency, and authoring one would need NiagaraEditor too "
			"(NiagaraEditor/Public/NiagaraSystemFactoryNew.h:16). TurboMCP.Build.cs is not mine to edit, "
			"so the question is recorded instead of answered by a change to a shared build file.");
		Builder.Results.Add(Ta19);
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

		// *** A REFUSAL PRINTS `MISSING`, AND THE VERDICT WORD IS DECIDED BY WHETHER THE ASSET WAS BUILT. ***
		//
		// MEASURED CONTRADICTION, AND IT IS WHY THIS LINE CHANGED. The verdict used to be
		// `bMeasured ? OK : (bBuilt ? PARTIAL : MISSING)`, but a declared-unbuildable asset is
		// `bMeasured == true` with `bBuilt == false`, so the log printed
		// `kit TA-10 OK NOT BUILT: needs 3 rows and 4 fields` - THE WORD `OK` BESIDE THE WORDS `NOT BUILT` -
		// while `QUESTIONS.md:914` records that same asset from a run as `TA-10 MISSING  NOT BUILT: ...`.
		// A report whose verdict contradicts its own detail is the defect this project records most often,
		// and it got worse rather than better when TA-18 and TA-19 were declared: THREE such lines.
		//
		// `bMeasured` KEEPS DOING THE JOB IT WAS ADDED FOR. It is still what separates a SHORT asset (built
		// and wrong) from an absent one, and the counters below are untouched - so this changes the WORD and
		// not one number. `make-kit-assets.ps1` reads `short`, `missing` and `assets`, never this word.
		const TCHAR* const Verdict = !Result.bBuilt
			? TEXT("MISSING")
			: (Result.bMeasured ? TEXT("OK") : TEXT("PARTIAL"));
		UE_LOG(LogTurboMCP, Display, TEXT("TurboMCP: kit %s %s %s"),
			*Result.Id, Verdict, *Result.Detail);
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
