// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "TurboMCPAcceptanceAssetCommandlet.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Containers/UnrealString.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphNode_Comment.h"
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
#include "HAL/PlatformFileManager.h"
#include "Misc/Paths.h"
#include "Misc/StringOutputDevice.h"
#include "Modules/ModuleManager.h"
// `FTurboMCPComment::TitleBarHeight` is used to decide nesting, and it is REFERENCED RATHER THAN
// COPIED: the box rectangle starts that far below the node position because the title bar strip sits
// above it, and a second copy of the constant would agree with this one until one of them changed.
#include "TurboMCPComment.h"
#include "TurboMCPModule.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "UObject/UObjectGlobals.h"

#include "Logging.h"

#define LOCTEXT_NAMESPACE "TurboMCPAcceptanceAsset"

namespace
{
	const FString PackagePath = TEXT("/Game/Tests/TurboMCPAcceptance");

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

	/**
	 * Puts a node in a graph at a position, with a fresh GUID and ITS PINS ALLOCATED.
	 *
	 * `AllocateDefaultPins` IS NOT OPTIONAL HERE, AND LEAVING IT OUT CRASHES. Measured:
	 *
	 *     Error: Array index out of bounds: 1 into an array of size 0
	 *     UnrealEditor-TurboMCP.dll!BuildKnotProbe() [..Commandlet.cpp:144]
	 *
	 * `UK2Node_Knot::GetOutputPin` is `return Pins[1];` — an unguarded index. `NewObject` does not
	 * allocate a node's pins and `UEdGraph::AddNode` does not either: the editor allocates them when
	 * the node is RECONSTRUCTED for display, which never happens in a commandlet with no Slate. So a
	 * node with no pins is the normal state here, and every accessor that indexes `Pins` is a crash
	 * waiting to happen.
	 */
	void Place(UEdGraph* InGraph, UEdGraphNode* InNode, double InX, double InY)
	{
		InNode->CreateNewGuid();
		InNode->NodePosX = static_cast<int32>(InX);
		InNode->NodePosY = static_cast<int32>(InY);
		InGraph->AddNode(InNode, /*bUserAction=*/false, /*bSelectNewNode=*/false);
		InNode->AllocateDefaultPins();
	}

	/** The row spacing `L-LAYOUT-REPORT` uses, kept the same on purpose. */
	static const double PinRowSpacing = 24.0;

	/** The width a `K2Node` is drawn at, used as the fallback measure. */
	static const int32 FallbackNodeWidth = 220;
	static const int32 FallbackNodeMinHeight = 80;
	static const int32 FallbackTitleHeight = 40;
	static const int32 FallbackPadding = 20;

	/**
	 * A FALLBACK RECTANGLE FOR A NODE BUILT OUTSIDE THE EDITOR, because without one it has NO SIZE.
	 *
	 * THIS IS THE MEASUREMENT THAT CORRECTS AN EARLIER CLAIM, AND IT MATTERS FOR EVERY LAYOUT PASS.
	 * `NodeWidth` and `NodeHeight` are `int32` fields that SLATE fills in when a node is drawn. A
	 * commandlet draws nothing, so EVERY node in this asset reports `size (0,0)` and its rectangle
	 * degenerates to a POINT. Measured, dumped from this very commandlet:
	 *
	 *     node K2Node_CallFunction_3 at (1050,90) size (0,0) rect x 1050..1050 y 90..90
	 *
	 * OPEN-1 ASKS WHETHER NODE RECTANGLES ARE DEGENERATE IN A HEADLESS EDITOR, AND THE ANSWER IS THAT
	 * THERE ARE TWO STATES AND BOTH ARE REAL. A non-zero wire length reported by `L-LAYOUT-REPORT` for
	 * `TurboMCPAcceptance:KnotProbe` does NOT settle it, because such a report can be taken with the asset
	 * OPEN IN A RENDERED EDITOR, where Slate has already filled the sizes in:
	 *
	 *   - a node in an asset OPENED IN THE EDITOR has real sizes, because Slate measured it;
	 *   - a node freshly built by a COMMANDLET has size 0 until something draws it.
	 *
	 * So a layout pass must not assume either. `P/E-GRAPH-KNOTS`, `FIX-BACKWARD` and `TIDY` run in the
	 * editor and will normally see real sizes, but they must REFUSE rather than compute from points
	 * when they do not - because placing knots from point-rectangles is arithmetic on nothing.
	 *
	 * This commandlet sets the sizes EXPLICITLY so the fixture it writes carries geometry that a test
	 * can measure without opening a graph editor. It does NOT pretend to know what the editor would
	 * draw: the values are stated as a fallback and are the same shape a `K2Node` has, so the geometry
	 * is real, consistent and reproducible rather than zero.
	 */
	void SizeForMeasurement(UEdGraphNode* InNode)
	{
		if (InNode == nullptr || Cast<UEdGraphNode_Comment>(InNode) != nullptr)
		{
			// A comment box is GIVEN its size when it is made, so it is already correct and
			// overwriting it would destroy the nesting these probes exist to provide.
			return;
		}

		int32 Inputs = 0;
		int32 Outputs = 0;
		for (const UEdGraphPin* Pin : InNode->Pins)
		{
			if (Pin == nullptr)
			{
				continue;
			}
			if (Pin->Direction == EGPD_Input) { ++Inputs; } else { ++Outputs; }
		}

		const int32 Rows = FMath::Max(Inputs, Outputs);
		InNode->NodeWidth = FallbackNodeWidth;
		InNode->NodeHeight = FMath::Max(FallbackNodeMinHeight,
			FallbackTitleHeight + Rows * static_cast<int32>(PinRowSpacing)
				+ FallbackPadding);
	}


	/**
	 * A comment box, placed with a fresh GUID and added to the graph.
	 *
	 * THE GUID IS MADE HERE RATHER THAN LEFT TO `AddNode`. Measured in `TurboMCPComment.cpp`: two boxes
	 * created in one session BOTH carried the key `000000`, so a box could not be named again and a
	 * rename would have hit whichever came first. A key that does not identify is worse than no key,
	 * because it looks like one.
	 */
	UEdGraphNode_Comment* MakeCommentBox(UEdGraph* InGraph, const FString& InText,
		double InX, double InY, double InWidth, double InHeight)
	{
		UEdGraphNode_Comment* Comment = NewObject<UEdGraphNode_Comment>(InGraph);
		Comment->CreateNewGuid();
		Comment->NodePosX = static_cast<int32>(InX);
		Comment->NodePosY = static_cast<int32>(InY);
		Comment->NodeWidth = static_cast<int32>(InWidth);
		Comment->NodeHeight = static_cast<int32>(InHeight);
		Comment->NodeComment = InText;
		InGraph->AddNode(Comment, /*bUserAction=*/false, /*bSelectNewNode=*/false);
		return Comment;
	}

	/**
	 * A node's rectangle in graph space, as the layout tools compute it.
	 */
	struct FRect
	{
		double MinX = 0.0;
		double MinY = 0.0;
		double MaxX = 0.0;
		double MaxY = 0.0;

		bool Contains(double InX, double InY) const
		{
			return InX >= MinX && InX <= MaxX && InY >= MinY && InY <= MaxY;
		}
	};

	FRect RectOf(const UEdGraphNode* InNode)
	{
		FRect Rect;
		if (InNode == nullptr)
		{
			return Rect;
		}
		Rect.MinX = static_cast<double>(InNode->NodePosX);
		Rect.MinY = static_cast<double>(InNode->NodePosY);
		Rect.MaxX = Rect.MinX + static_cast<double>(InNode->NodeWidth);
		Rect.MaxY = Rect.MinY + static_cast<double>(InNode->NodeHeight);
		return Rect;
	}

	/**
	 * Where a pin sits in graph space, computed the way `L-LAYOUT-REPORT` computes it.
	 *
	 * THE ENGINE EXPOSES NO PIN POSITION TO A TOOL CALL, which is why the layout report derives one from
	 * the node's rectangle and the pin's index rather than asking. This reproduces that derivation
	 * EXACTLY - inputs down the left edge, outputs down the right, each counted separately, with the
	 * same row spacing - because a crossing measured with a DIFFERENT rule than the tool uses would
	 * disagree with the tool and the disagreement would be invisible.
	 */
	FVector2D PinPosition(const UEdGraphPin* InPin)
	{
		if (InPin == nullptr)
		{
			return FVector2D::ZeroVector;
		}
		const UEdGraphNode* Node = InPin->GetOwningNodeUnchecked();
		if (Node == nullptr)
		{
			return FVector2D::ZeroVector;
		}

		const bool bIsInput = InPin->Direction == EGPD_Input;
		int32 Index = 0;
		for (const UEdGraphPin* Pin : Node->Pins)
		{
			if (Pin == nullptr || Pin->Direction != InPin->Direction)
			{
				continue;
			}
			if (Pin == InPin)
			{
				break;
			}
			++Index;
		}

		const double EdgeX = bIsInput
			? static_cast<double>(Node->NodePosX)
			: static_cast<double>(Node->NodePosX + Node->NodeWidth);
		const double EdgeY = static_cast<double>(Node->NodePosY) + PinRowSpacing * (Index + 1);
		return FVector2D(EdgeX, EdgeY);
	}

	/**
	 * The standard orientation test, including the collinear case.
	 *
	 * DECLARED BEFORE ITS USE, because `SegmentCrossesRect` calls it and the two read best in this
	 * order: the rectangle test is the idea, and the segment test is the mechanism it needs.
	 */
	bool SegmentsIntersect(const FVector2D& InP1, const FVector2D& InP2,
		const FVector2D& InP3, const FVector2D& InP4);

	/** True when the segment A->B passes through the rectangle, endpoints included. */
	bool SegmentCrossesRect(const FVector2D& InA, const FVector2D& InB, const FRect& InRect)
	{
		// ENDPOINTS FIRST: a wire whose end sits on the rectangle has crossed it, and treating that as
		// "no crossing" would miss the exact case a node placed ON a route produces.
		if (InRect.Contains(InA.X, InA.Y) || InRect.Contains(InB.X, InB.Y))
		{
			return true;
		}

		// Then the four edges, by segment intersection. A wire can pass entirely through a rectangle
		// without either endpoint being inside it, which is the ordinary crossing case.
		const FVector2D Corners[4] = {
			FVector2D(InRect.MinX, InRect.MinY),
			FVector2D(InRect.MaxX, InRect.MinY),
			FVector2D(InRect.MaxX, InRect.MaxY),
			FVector2D(InRect.MinX, InRect.MaxY),
		};
		for (int32 Index = 0; Index < 4; ++Index)
		{
			const FVector2D& C = Corners[Index];
			const FVector2D& D = Corners[(Index + 1) % 4];
			if (SegmentsIntersect(InA, InB, C, D))
			{
				return true;
			}
		}
		return false;
	}

	/** The standard orientation test, including the collinear case. */
	bool SegmentsIntersect(const FVector2D& InP1, const FVector2D& InP2,
		const FVector2D& InP3, const FVector2D& InP4)
	{
		auto Cross = [](const FVector2D& O, const FVector2D& A, const FVector2D& B)
		{
			return (A.X - O.X) * (B.Y - O.Y) - (A.Y - O.Y) * (B.X - O.X);
		};
		auto OnSegment = [](const FVector2D& O, const FVector2D& A, const FVector2D& B)
		{
			return FMath::Min(O.X, B.X) <= A.X && A.X <= FMath::Max(O.X, B.X)
				&& FMath::Min(O.Y, B.Y) <= A.Y && A.Y <= FMath::Max(O.Y, B.Y);
		};

		const double D1 = Cross(InP3, InP4, InP1);
		const double D2 = Cross(InP3, InP4, InP2);
		const double D3 = Cross(InP1, InP2, InP3);
		const double D4 = Cross(InP1, InP2, InP4);

		if (((D1 > 0.0 && D2 < 0.0) || (D1 < 0.0 && D2 > 0.0))
			&& ((D3 > 0.0 && D4 < 0.0) || (D3 < 0.0 && D4 > 0.0)))
		{
			return true;
		}
		if (FMath::IsNearlyZero(D1) && OnSegment(InP3, InP1, InP4)) { return true; }
		if (FMath::IsNearlyZero(D2) && OnSegment(InP3, InP2, InP4)) { return true; }
		if (FMath::IsNearlyZero(D3) && OnSegment(InP1, InP3, InP2)) { return true; }
		if (FMath::IsNearlyZero(D4) && OnSegment(InP1, InP4, InP2)) { return true; }
		return false;
	}

	/**
	 * How many wires cross how many nodes.
	 *
	 * A WIRE'S OWN END NODES ARE EXCLUDED, and so are comment boxes: a wire always touches the
	 * rectangles of the two nodes it joins, and counting those would make every graph "crossing" and
	 * the measurement worthless.
	 */
	void MeasureCrossings(const UEdGraph* InGraph, int32& OutWires, int32& OutNodes)
	{
		OutWires = 0;
		OutNodes = 0;
		if (InGraph == nullptr)
		{
			return;
		}

		TSet<const UEdGraphNode*> AlreadyCounted;
		for (const UEdGraphNode* Node : InGraph->Nodes)
		{
			if (Node == nullptr || Cast<UEdGraphNode_Comment>(Node) != nullptr)
			{
				continue;
			}
			for (const UEdGraphPin* Pin : Node->Pins)
			{
				if (Pin == nullptr || Pin->Direction != EGPD_Output)
				{
					continue;
				}
				const FVector2D From = PinPosition(Pin);
				for (const UEdGraphPin* Linked : Pin->LinkedTo)
				{
					if (Linked == nullptr)
					{
						continue;
					}
					const UEdGraphNode* Target = Linked->GetOwningNodeUnchecked();
					if (Target == nullptr)
					{
						continue;
					}
					const FVector2D To = PinPosition(Linked);

					for (const UEdGraphNode* Other : InGraph->Nodes)
					{
						if (Other == nullptr || Other == Node || Other == Target
							|| Cast<UEdGraphNode_Comment>(Other) != nullptr)
						{
							continue;
						}
						if (SegmentCrossesRect(From, To, RectOf(Other)))
						{
							OutNodes++;
							AlreadyCounted.Add(Other);
						}
					}
					OutWires++;
				}
			}
		}
		// The node figure is reported as DISTINCT nodes crossed, which is the number a reader can check
		// against the graph rather than a sum that grows with every wire.
		OutNodes = AlreadyCounted.Num();
	}

	/**
	 * How many boxes there are, and how many are geometrically nested inside another.
	 *
	 * NESTING IS GEOMETRIC, the same rule the editor uses, and the box rectangle starts
	 * `TitleBarHeight` BELOW the node position because the title bar's own strip sits above it. That
	 * offset is why a box drawn around nodes can hold FEWER than its coordinates suggest, and it is
	 * taken from `TurboMCPComment.cpp` rather than guessed.
	 */
	void MeasureNesting(const UEdGraph* InGraph, int32& OutBoxes, int32& OutNested)
	{
		OutBoxes = 0;
		OutNested = 0;
		if (InGraph == nullptr)
		{
			return;
		}

		TArray<const UEdGraphNode_Comment*> Boxes;
		for (const UEdGraphNode* Node : InGraph->Nodes)
		{
			if (const UEdGraphNode_Comment* Box = Cast<UEdGraphNode_Comment>(Node))
			{
				Boxes.Add(Box);
			}
		}
		OutBoxes = Boxes.Num();

		for (const UEdGraphNode_Comment* Inner : Boxes)
		{
			const double InnerMinX = static_cast<double>(Inner->NodePosX);
			const double InnerMinY =
				static_cast<double>(Inner->NodePosY) + FTurboMCPComment::TitleBarHeight;
			const double InnerMaxX = InnerMinX + static_cast<double>(Inner->NodeWidth);
			const double InnerMaxY = static_cast<double>(Inner->NodePosY + Inner->NodeHeight);

			for (const UEdGraphNode_Comment* Outer : Boxes)
			{
				if (Outer == Inner)
				{
					continue;
				}
				const double OuterMinX = static_cast<double>(Outer->NodePosX);
				const double OuterMinY =
					static_cast<double>(Outer->NodePosY) + FTurboMCPComment::TitleBarHeight;
				const double OuterMaxX = OuterMinX + static_cast<double>(Outer->NodeWidth);
				const double OuterMaxY = static_cast<double>(Outer->NodePosY + Outer->NodeHeight);

				// STRICTLY INSIDE ON ALL FOUR SIDES. A box that merely overlapped would be a different
				// relationship, and calling it nesting would make the check pass on data that does not
				// exercise nesting at all.
				if (InnerMinX > OuterMinX && InnerMinY > OuterMinY
					&& InnerMaxX < OuterMaxX && InnerMaxY < OuterMaxY)
				{
					OutNested++;
					break;
				}
			}
		}
	}

	/**
	 * Wires whose two end nodes are in ADJACENT columns, which the knot spec says a pass must SKIP.
	 *
	 * "ADJACENT" is decided the way the spec decides it: the target starts to the right of the source
	 * and no other node's column lies between them. The measure exists because a skip rule with nothing
	 * to skip is untested, which is exactly the state the crossing case was in before this graph.
	 */
	void MeasureNeighbourWires(const UEdGraph* InGraph, int32& OutNeighbours)
	{
		OutNeighbours = 0;
		if (InGraph == nullptr)
		{
			return;
		}

		auto IsBetween = [InGraph](double InLeftX, double InRightX, const UEdGraphNode* InA,
			const UEdGraphNode* InB)
		{
			for (const UEdGraphNode* Other : InGraph->Nodes)
			{
				if (Other == nullptr || Other == InA || Other == InB
					|| Cast<UEdGraphNode_Comment>(Other) != nullptr)
				{
					continue;
				}
				const double X = static_cast<double>(Other->NodePosX);
				if (X > InLeftX && X < InRightX)
				{
					return true;
				}
			}
			return false;
		};

		for (const UEdGraphNode* Node : InGraph->Nodes)
		{
			if (Node == nullptr || Cast<UEdGraphNode_Comment>(Node) != nullptr)
			{
				continue;
			}
			for (const UEdGraphPin* Pin : Node->Pins)
			{
				if (Pin == nullptr || Pin->Direction != EGPD_Output)
				{
					continue;
				}
				for (const UEdGraphPin* Linked : Pin->LinkedTo)
				{
					const UEdGraphNode* Target =
						Linked != nullptr ? Linked->GetOwningNodeUnchecked() : nullptr;
					if (Target == nullptr || Target == Node)
					{
						continue;
					}
					const double SourceX = static_cast<double>(Node->NodePosX);
					const double TargetX = static_cast<double>(Target->NodePosX);
					if (TargetX <= SourceX)
					{
						continue;
					}
					if (!IsBetween(SourceX, TargetX, Node, Target))
					{
						OutNeighbours++;
					}
				}
			}
		}
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
	bool BuildKnotProbe(UBlueprint* InBlueprint, TArray<FString>& OutProblems)
	{
		UK2Node_FunctionEntry* Entry = nullptr;
		UK2Node_FunctionResult* Result = nullptr;
		UEdGraph* Graph = MakeFunction(InBlueprint, TEXT("KnotProbe"), Entry, Result);
		if (Graph == nullptr || Entry == nullptr)
		{
			OutProblems.Add(TEXT("KnotProbe: MakeFunction made no graph or no function entry, so there is ")
				TEXT("nothing to place the three knots on"));
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
		if (Entry->GetThenPin() == nullptr || First->GetExecPin() == nullptr)
		{
			OutProblems.Add(TEXT("KnotProbe: the entry cannot reach the first call, so the chain the three ")
				TEXT("knots sit on has no beginning"));
			return false;
		}
		Entry->GetThenPin()->MakeLinkTo(First->GetExecPin());

		// THREE KNOTS IN A ROW between the two calls.
		UEdGraphPin* From = First->GetThenPin();
		for (int32 Index = 0; Index < 3; ++Index)
		{
			UK2Node_Knot* Knot = NewObject<UK2Node_Knot>(Graph);
			Place(Graph, Knot, 700.0 + Index * 300.0, 200.0 + Index * 130.0);
			// *** A MISSING PIN IS REPORTED, NOT SKIPPED. `MakeLinkTo` RETURNS `void`, so a link that was
			// never made is invisible in the return value - and the guard below is what decides it. The
			// old code fell through with `From` unchanged, so the chain broke and the function still
			// returned `true`. `GetExecPin()` OR `GetInputPin()` RETURNING NULL ON A NODE THIS FUNCTION
			// JUST CREATED MEANS THE FACTORY MADE SOMETHING IT CANNOT WIRE, WHICH IS WORTH NAMING. ***
			if (From == nullptr || Knot->GetInputPin() == nullptr)
			{
				OutProblems.Add(FString::Printf(
					TEXT("KnotProbe: knot %d has no %s pin, so the three-knot chain is broken and this ")
					TEXT("probe does not test what P/E-GRAPH-KNOTS needs"),
					Index, From == nullptr ? TEXT("input") : TEXT("output")));
				return false;
			}
			From->MakeLinkTo(Knot->GetInputPin());
			From = Knot->GetOutputPin();
		}
		if (From == nullptr || Second->GetExecPin() == nullptr)
		{
			OutProblems.Add(TEXT("KnotProbe: the last knot cannot reach the second call, so the chain ")
				TEXT("ends nowhere"));
			return false;
		}
		From->MakeLinkTo(Second->GetExecPin());

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
	bool BuildDeadProbe(UBlueprint* InBlueprint, TArray<FString>& OutProblems)
	{
		UK2Node_FunctionEntry* Entry = nullptr;
		UK2Node_FunctionResult* Result = nullptr;
		UEdGraph* Graph = MakeFunction(InBlueprint, TEXT("DeadProbe"), Entry, Result);
		if (Graph == nullptr || Entry == nullptr)
		{
			OutProblems.Add(TEXT("DeadProbe: MakeFunction made no graph or no function entry, so there is no ")
				TEXT("reachable chain to contrast the orphans with"));
			return false;
		}

		UK2Node_CallFunction* Live = NewObject<UK2Node_CallFunction>(Graph);
		Live->SetFromFunction(UKismetSystemLibrary::StaticClass()->FindFunctionByName(
			TEXT("PrintString")));
		Place(Graph, Live, 400.0, 0.0);
		if (Entry->GetThenPin() == nullptr || Live->GetExecPin() == nullptr)
		{
			OutProblems.Add(TEXT("DeadProbe: the live chain has no beginning, so the graph holds no reachable ")
				TEXT("group for P/E-GRAPH-DEAD to leave alone and the probe tests nothing"));
			return false;
		}
		Entry->GetThenPin()->MakeLinkTo(Live->GetExecPin());

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

		if (OrphanA->GetThenPin() == nullptr || OrphanB->GetExecPin() == nullptr)
		{
			OutProblems.Add(TEXT("DeadProbe: the two orphans cannot be joined before they are broken apart, so ")
				TEXT("the make-then-break step that leaves each one its own group never ran"));
			return false;
		}
		OrphanA->GetThenPin()->MakeLinkTo(OrphanB->GetExecPin());

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
	bool BuildLocalVarProbe(UBlueprint* InBlueprint, TArray<FString>& OutProblems)
	{
		UK2Node_FunctionEntry* Entry = nullptr;
		UK2Node_FunctionResult* Result = nullptr;
		UEdGraph* Graph = MakeFunction(InBlueprint, TEXT("LocalVarProbe"), Entry, Result);
		if (Graph == nullptr || Entry == nullptr)
		{
			OutProblems.Add(TEXT("LocalVarProbe: MakeFunction made no graph or no function entry, so the ")
				TEXT("function inputs that make a value SAFE do not exist"));
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
		if (APin == nullptr || SumA == nullptr)
		{
			OutProblems.Add(TEXT("LocalVarProbe: the function input A does not reach the Add_IntInt, so the ")
				TEXT("SAFE value that is read twice does not exist"));
			return false;
		}
		APin->MakeLinkTo(SumA);
		if (BPin == nullptr || SumB == nullptr)
		{
			OutProblems.Add(TEXT("LocalVarProbe: the function input B does not reach the Add_IntInt, so the ")
				TEXT("SAFE value that is read twice does not exist"));
			return false;
		}
		BPin->MakeLinkTo(SumB);

		// TWO CONSUMERS of the same value. This is the structure no DSL expression can produce.
		for (int32 Index = 0; Index < 2; ++Index)
		{
			UK2Node_CallFunction* Consumer = NewObject<UK2Node_CallFunction>(Graph);
			Consumer->SetFromFunction(UKismetMathLibrary::StaticClass()->FindFunctionByName(
				TEXT("Multiply_IntInt")));
			Place(Graph, Consumer, 900.0, Index * 300.0);

			if (Sum->GetReturnValuePin() == nullptr || Consumer->FindPin(TEXT("A")) == nullptr)
			{
				OutProblems.Add(FString::Printf(
					TEXT("LocalVarProbe: consumer %d cannot read the SAFE value, so the value has fewer than ")
					TEXT("the two readers this probe exists to present"), Index));
				return false;
			}
			Sum->GetReturnValuePin()->MakeLinkTo(Consumer->FindPin(TEXT("A")));
		}

		// THE UNSAFE VALUE: a member variable plus an input. It is also read twice, so the tool has to
		// decide about it and must say NO.
		UK2Node_CallFunction* Biased = NewObject<UK2Node_CallFunction>(Graph);
		Biased->SetFromFunction(UKismetMathLibrary::StaticClass()->FindFunctionByName(
			TEXT("Add_IntInt")));
		Place(Graph, Biased, 400.0, 900.0);

		if (APin == nullptr || Biased->FindPin(TEXT("A")) == nullptr)
		{
			OutProblems.Add(TEXT("LocalVarProbe: the function input A does not reach the second Add_IntInt, so ")
				TEXT("the UNSAFE value that must be REFUSED does not exist"));
			return false;
		}
		APin->MakeLinkTo(Biased->FindPin(TEXT("A")));
		if (BPin == nullptr || Biased->FindPin(TEXT("B")) == nullptr)
		{
			OutProblems.Add(TEXT("LocalVarProbe: the function input B does not reach the second Add_IntInt, so ")
				TEXT("the UNSAFE value that must be REFUSED does not exist"));
			return false;
		}
		BPin->MakeLinkTo(Biased->FindPin(TEXT("B")));

		for (int32 Index = 0; Index < 2; ++Index)
		{
			UK2Node_CallFunction* Consumer = NewObject<UK2Node_CallFunction>(Graph);
			Consumer->SetFromFunction(UKismetMathLibrary::StaticClass()->FindFunctionByName(
				TEXT("Multiply_IntInt")));
			Place(Graph, Consumer, 900.0, 900.0 + Index * 300.0);
			if (Biased->GetReturnValuePin() == nullptr
				|| Consumer->FindPin(TEXT("A")) == nullptr)
			{
				OutProblems.Add(FString::Printf(
					TEXT("LocalVarProbe: consumer %d cannot read the UNSAFE value, so the value the tool must ")
					TEXT("REFUSE has fewer than the two readers that make it a candidate at all"), Index));
				return false;
			}
			Biased->GetReturnValuePin()->MakeLinkTo(Consumer->FindPin(TEXT("A")));
		}

		// The member variable is added AFTER the graph, so the node that reads it is made here too.
		FBlueprintEditorUtils::AddMemberVariable(InBlueprint, MemberName, IntType, TEXT("10"));
		UK2Node_VariableGet* BiasGet = NewObject<UK2Node_VariableGet>(Graph);
		BiasGet->VariableReference.SetSelfMember(MemberName);
		Place(Graph, BiasGet, 100.0, 1200.0);
		if (BiasGet->GetValuePin() == nullptr || Biased->FindPin(TEXT("A")) == nullptr)
		{
			OutProblems.Add(TEXT("LocalVarProbe: the member read cannot replace the input on one side of the ")
				TEXT("second Add_IntInt, so that value stays SAFE and the UNSAFE case is never exercised"));
			return false;
		}
		{
			// The member read REPLACES the input on one side, which is what makes this value unsafe:
			// the variable can change during the call.
			Biased->FindPin(TEXT("A"))->BreakAllPinLinks();
			BiasGet->GetValuePin()->MakeLinkTo(Biased->FindPin(TEXT("A")));
		}

		return true;
	}

	/**
	 * `LayoutProbe`: THE GRAPH TWO INDEPENDENT SPECIFICATIONS ASKED FOR, and neither capability could
	 * be exercised without it.
	 *
	 * `Spec/WORKING_GRAPH_KNOTS.md` found that `KnotProbe` cannot test the crossing case: its three
	 * knots are already on the wire and the straight route between its two calls passes over nothing,
	 * so a crossing predicate finds NOTHING and the primary positive case is unreachable.
	 *
	 * `Spec/WORKING_GRAPH_TIDY.md` found that the asset had ZERO comment boxes - measured through
	 * `R-COMMENT-LIST` as `0 box(es), 0 nested` - so tidy's box-corner rule, its wire-length rule and
	 * nesting itself all rested on code review.
	 *
	 * THIS ONE GRAPH SERVES BOTH. It carries a CROSSING, NESTING, a wire between NEIGHBOURING COLUMNS
	 * that a knot pass must SKIP, and a MESSY RUN for a tidy pass to improve.
	 *
	 * THE CROSSING, THE NESTING AND THE NEIGHBOUR WIRE ARE ALL MEASURED AND PRINTED rather than
	 * trusted. The coordinates were chosen by hand, and hand arithmetic is exactly the kind of claim
	 * this project has been wrong about before, so the graph is measured back after it is built and the
	 * function FAILS if any of the three structures is absent. A build whose own report says
	 * "0 crossing" is a FAILED build, not a saved asset.
	 */
	bool BuildLayoutProbe(UBlueprint* InBlueprint, TArray<FString>& OutProblems)
	{
		UK2Node_FunctionEntry* Entry = nullptr;
		UK2Node_FunctionResult* Result = nullptr;
		UEdGraph* Graph = MakeFunction(InBlueprint, TEXT("LayoutProbe"), Entry, Result);
		if (Graph == nullptr || Entry == nullptr)
		{
			OutProblems.Add(TEXT("LayoutProbe: MakeFunction made no graph or no function entry, so the ")
				TEXT("crossing chain has no beginning"));
			return false;
		}

		// A helper for the repeated "make a PrintString call" step, so the geometry below reads as
		// geometry rather than as boilerplate.
		auto MakeCall = [Graph](double InX, double InY) -> UK2Node_CallFunction*
		{
			UK2Node_CallFunction* Node = NewObject<UK2Node_CallFunction>(Graph);
			Node->SetFromFunction(
				UKismetSystemLibrary::StaticClass()->FindFunctionByName(TEXT("PrintString")));
			Place(Graph, Node, InX, InY);
			return Node;
		};

		// ---------------------------------------------------------------- the CROSSING chain
		//
		// A -> B -> C along an exec chain, with B pushed DOWN and C far to the right, so the wire from
		// B's output back up to C's input runs diagonally across the space the calls leave empty.
		UK2Node_CallFunction* A = MakeCall(300.0, 0.0);
		UK2Node_CallFunction* B = MakeCall(600.0, 300.0);
		UK2Node_CallFunction* C = MakeCall(1900.0, 0.0);

		// EACH LINK IS GUARDED AND NAMED, BECAUSE THE THREE MEASUREMENTS BELOW CANNOT TELL A WIRE THAT WAS
		// NEVER MADE FROM A WIRE THAT CROSSES NOTHING. `MakeLinkTo` returns `void`, so the guard is the only
		// place the missing pin can be reported, and the measurement further down would then blame the
		// COORDINATES for a graph that is missing its chain.
		if (Entry->GetThenPin() == nullptr || A->GetExecPin() == nullptr)
		{
			OutProblems.Add(TEXT("LayoutProbe: the entry cannot reach the crossing chain's first call, so the ")
				TEXT("wire the crossing measurement is about does not exist"));
			return false;
		}
		Entry->GetThenPin()->MakeLinkTo(A->GetExecPin());
		if (A->GetThenPin() == nullptr || B->GetExecPin() == nullptr)
		{
			OutProblems.Add(TEXT("LayoutProbe: the crossing chain's first link cannot be made, so the ")
				TEXT("B-to-C wire that crosses the obstacle has nothing to run from"));
			return false;
		}
		A->GetThenPin()->MakeLinkTo(B->GetExecPin());
		if (B->GetThenPin() == nullptr || C->GetExecPin() == nullptr)
		{
			OutProblems.Add(TEXT("LayoutProbe: the crossing chain's second link cannot be made, which is the ")
				TEXT("wire the crossing case exists for"));
			return false;
		}
		B->GetThenPin()->MakeLinkTo(C->GetExecPin());

		// THE OBSTACLE, placed ON the B -> C wire. It is deliberately NOT wired into the chain: a node
		// on the route is what a crossing test is about, and wiring it in would make it part of the
		// route instead of an obstacle to it.
		UK2Node_CallFunction* Obstacle = MakeCall(1050.0, 90.0);

		// ---------------------------------------------------------------- the NEIGHBOURING wire
		//
		// D sits one column right of the obstacle and is joined by a SHORT wire. The knot spec says a
		// pass must SKIP a wire between neighbouring columns, so this wire has to exist for that rule to
		// be testable at all. A capability that only ever skips bad input proves nothing.
		UK2Node_CallFunction* D = MakeCall(1300.0, 90.0);
		if (Obstacle->GetThenPin() == nullptr || D->GetExecPin() == nullptr)
		{
			OutProblems.Add(TEXT("LayoutProbe: no wire between neighbouring columns can be made, so the knot ")
				TEXT("pass SKIP rule has nothing to skip and the neighbour measurement reads zero"));
			return false;
		}
		Obstacle->GetThenPin()->MakeLinkTo(D->GetExecPin());

		// ---------------------------------------------------------------- the MESSY RUN
		//
		// Four calls placed OUT OF ORDER and OVERLAPPING, joined into a chain. A tidy pass should
		// reorder and separate them. A graph that is already tidy would let a broken pass look like a
		// working one, which is the failure `SaneProbe` catches for the rollback case.
		{
			const double MessyX[4] = { 1900.0, 2050.0, 1950.0, 2100.0 };
			const double MessyY[4] = { 800.0, 830.0, 860.0, 900.0 };
			UK2Node_CallFunction* Messy[4];
			for (int32 Index = 0; Index < 4; ++Index)
			{
				Messy[Index] = MakeCall(MessyX[Index], MessyY[Index]);
			}
			for (int32 Index = 0; Index < 3; ++Index)
			{
				if (Messy[Index]->GetThenPin() == nullptr
					|| Messy[Index + 1]->GetExecPin() == nullptr)
				{
					OutProblems.Add(FString::Printf(
						TEXT("LayoutProbe: link %d of the mess is missing, so the run a tidy pass is meant to ")
						TEXT("reorder is not a chain and a pass that does nothing would look correct"), Index));
					return false;
				}
				Messy[Index]->GetThenPin()->MakeLinkTo(Messy[Index + 1]->GetExecPin());
			}
		}

		// ---------------------------------------------------------------- NESTED BOXES
		//
		// MEMBERSHIP IS GEOMETRIC, decided by the editor through `FSlateRect::IsRectangleContained` in
		// graph space, with the box rectangle starting `TitleBarHeight` BELOW the node position because
		// the title bar strip sits above it. Both facts are recorded in `TurboMCPComment.cpp` from a
		// measurement, and they are why these coordinates leave room for the title strip rather than
		// assuming the box begins at `NodePosY`.
		//
		// INNER holds the first two boxed calls; OUTER holds those two AND two more, so OUTER's
		// membership is a strict superset and "nested" is a fact rather than a coincidence of sizes.
		{
			UK2Node_CallFunction* Boxed[4];
			const double BoxedX[4] = { 300.0, 620.0, 940.0, 300.0 };
			const double BoxedY[4] = { 1400.0, 1420.0, 1440.0, 1650.0 };
			for (int32 Index = 0; Index < 4; ++Index)
			{
				Boxed[Index] = MakeCall(BoxedX[Index], BoxedY[Index]);
			}
			for (int32 Index = 0; Index < 3; ++Index)
			{
				if (Boxed[Index]->GetThenPin() == nullptr
					|| Boxed[Index + 1]->GetExecPin() == nullptr)
				{
					OutProblems.Add(FString::Printf(
						TEXT("LayoutProbe: link %d of the boxed calls is missing, so the boxes enclose nodes ")
						TEXT("that are not a chain and the nesting measurement is about the wrong shape"), Index));
					return false;
				}
				Boxed[Index]->GetThenPin()->MakeLinkTo(Boxed[Index + 1]->GetExecPin());
			}
		}

		// INNER's rectangle is STRICTLY INSIDE OUTER's on all four sides. A box that merely overlapped
		// would be a different relationship, and calling it nesting would let the check pass on data
		// that does not exercise nesting at all.
		MakeCommentBox(Graph, TEXT("INNER box: two calls, nested inside OUTER"),
			260.0, 1330.0, 720.0, 260.0);
		MakeCommentBox(Graph, TEXT("OUTER box: four calls, contains INNER"),
			200.0, 1260.0, 1900.0, 600.0);

		// ---------------------------------------------------------------- GIVE THE NODES A SIZE
		//
		// WITHOUT THIS THE WHOLE GRAPH IS POINTS, and the crossing below cannot exist: every node
		// reports `size (0,0)` because a commandlet draws nothing. See `SizeForMeasurement` for the
		// measurement that corrects the earlier claim that this was a non-issue.
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			SizeForMeasurement(Node);
		}

		// ---------------------------------------------------------------- MEASURE, DO NOT TRUST
		int32 CrossingWires = 0;
		int32 CrossingNodes = 0;
		MeasureCrossings(Graph, CrossingWires, CrossingNodes);
		UE_LOG(LogTurboMCP, Display,
			TEXT("TurboMCP: LayoutProbe crossing: %d wire(s) cross %d distinct node(s)"),
			CrossingWires, CrossingNodes);
		if (CrossingWires == 0 || CrossingNodes == 0)
		{
			// THE GEOMETRY IS PRINTED WHEN THE CHECK FAILS. A hand-picked coordinate that does not
			// produce the intended structure is the thing most likely to be wrong and the hardest to
			// see, and without this dump the only information is "it did not work" - so the next
			// attempt is another guess. MEASURED: the first attempt reported
			// "10 wire(s) cross 0 distinct node(s)" against coordinates whose hand arithmetic had
			// looked obviously right.
			for (const UEdGraphNode* Node : Graph->Nodes)
			{
				if (Node == nullptr || Cast<UEdGraphNode_Comment>(Node) != nullptr)
				{
					continue;
				}
				const FRect Rect = RectOf(Node);
				UE_LOG(LogTurboMCP, Display,
					TEXT("  node %s at (%d,%d) size (%d,%d) rect x %.0f..%.0f y %.0f..%.0f"),
					*Node->GetName(), Node->NodePosX, Node->NodePosY,
					Node->NodeWidth, Node->NodeHeight,
					Rect.MinX, Rect.MaxX, Rect.MinY, Rect.MaxY);
				for (const UEdGraphPin* Pin : Node->Pins)
				{
					if (Pin == nullptr || Pin->Direction != EGPD_Output)
					{
						continue;
					}
					const FVector2D From = PinPosition(Pin);
					for (const UEdGraphPin* Linked : Pin->LinkedTo)
					{
						if (Linked == nullptr)
						{
							continue;
						}
						const UEdGraphNode* Target = Linked->GetOwningNodeUnchecked();
						const FVector2D To = PinPosition(Linked);
						UE_LOG(LogTurboMCP, Display,
							TEXT("    wire %s.%s (%.0f,%.0f) -> %s.%s (%.0f,%.0f)"),
							*Node->GetName(), *Pin->GetName(), From.X, From.Y,
							Target != nullptr ? *Target->GetName() : TEXT("?"),
							*Linked->GetName(), To.X, To.Y);
					}
				}
			}
			UE_LOG(LogTurboMCP, Error,
				TEXT("LayoutProbe was built but no wire crosses a node, so the graph cannot exercise ")
				TEXT("the crossing case it exists for. The COORDINATES need moving, not the check."));
			return false;
		}

		int32 BoxCount = 0;
		int32 NestedCount = 0;
		MeasureNesting(Graph, BoxCount, NestedCount);
		UE_LOG(LogTurboMCP, Display,
			TEXT("TurboMCP: LayoutProbe boxes: %d box(es), %d nested"), BoxCount, NestedCount);
		if (BoxCount < 2 || NestedCount < 1)
		{
			UE_LOG(LogTurboMCP, Error,
				TEXT("LayoutProbe has %d box(es) and %d nested, so nesting cannot be exercised"),
				BoxCount, NestedCount);
			return false;
		}

		int32 NeighbourWires = 0;
		MeasureNeighbourWires(Graph, NeighbourWires);
		UE_LOG(LogTurboMCP, Display,
			TEXT("TurboMCP: LayoutProbe neighbouring-column wires: %d"), NeighbourWires);
		if (NeighbourWires == 0)
		{
			UE_LOG(LogTurboMCP, Error,
				TEXT("LayoutProbe has no wire between neighbouring columns, so the knot pass SKIP ")
				TEXT("rule has nothing to skip"));
			return false;
		}

		// The result node is left unlinked on purpose, as in `KnotProbe`: this graph is about layout.
		return true;
	}

	/**
	 * `SaneProbe`: an ordinary chain that SHOULD NOT be disturbed.
	 *
	 * A layout pass that measures and rolls back needs a graph where the pass has nothing to improve,
	 * because that is where a rollback that does not work turns a fine graph into a broken one. This
	 * graph is already in order: an entry and one call, left to right on one row.
	 *
	 * *** IT HAS NO RESULT NODE, AND THAT IS THE ENGINE'S SHAPE FOR A VOID FUNCTION RATHER THAN AN OMISSION
	 * HERE. *** `BlueprintEditorUtils.cpp:2301` states the rule where it builds one - "only create a result
	 * node if there are out parameters" - so `MakeFunction` finds an entry and no result, and the graph is
	 * TWO nodes, the count the fixture prints as `SaneProbe: 2 node(s)`. THE CHAIN ENDS AT THE CALL.
	 * A guard demanding a result node would fail a graph that is exactly the shape this probe is about, and
	 * adding one to satisfy such a guard would change the fixture every suite measures.
	 */
	bool BuildSaneProbe(UBlueprint* InBlueprint, TArray<FString>& OutProblems)
	{
		UK2Node_FunctionEntry* Entry = nullptr;
		UK2Node_FunctionResult* Result = nullptr;
		UEdGraph* Graph = MakeFunction(InBlueprint, TEXT("SaneProbe"), Entry, Result);
		if (Graph == nullptr || Entry == nullptr)
		{
			OutProblems.Add(TEXT("SaneProbe: MakeFunction made no graph or no function entry, so there is no ")
				TEXT("ordered chain for a layout pass to leave alone"));
			return false;
		}

		UK2Node_CallFunction* Call = NewObject<UK2Node_CallFunction>(Graph);
		Call->SetFromFunction(UKismetSystemLibrary::StaticClass()->FindFunctionByName(
			TEXT("PrintString")));
		Place(Graph, Call, 450.0, 0.0);

		// *** A BROKEN CHAIN HERE IS THE WORST OF THE FIVE TO LEAVE SILENT. This graph is the one that must
		// come back UNCHANGED from a layout pass, so a hole in it makes the pass look correct on a graph that
		// was already broken - and "nothing to improve" is exactly the answer the probe is built to elicit. ***
		if (Entry->GetThenPin() == nullptr || Call->GetExecPin() == nullptr)
		{
			OutProblems.Add(TEXT("SaneProbe: the entry does not reach the call, so the already-ordered chain ")
				TEXT("this probe exists for does not exist"));
			return false;
		}
		Entry->GetThenPin()->MakeLinkTo(Call->GetExecPin());
		if (Call->GetThenPin() == nullptr)
		{
			OutProblems.Add(TEXT("SaneProbe: the PrintString call has no exec OUTPUT pin, so the ordered chain ")
				TEXT("this probe exists for ends at its second node"));
			return false;
		}
		// THE RESULT IS REACHED ONLY WHEN THE FUNCTION HAS ONE, AND A VOID FUNCTION HAS NONE. This is not a
		// silent skip: the reason is in the doc comment above and in `BlueprintEditorUtils.cpp:2301`, and the
		// alternative - adding a result node so that a guard can be satisfied - would change the fixture's own
		// node count to suit the check. A result node that IS present must still be reachable.
		if (Result != nullptr)
		{
			if (Result->GetExecPin() == nullptr)
			{
				OutProblems.Add(TEXT("SaneProbe: the function HAS a result node but its exec INPUT pin is ")
					TEXT("missing, so nothing can reach it and the chain is not ordered end to end"));
				return false;
			}
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

	// THE OLD OBJECT IS RENAMED OUT OF ITS PACKAGE, AND THE PACKAGE IS MADE AFTERWARDS.
	//
	// A package holds one object per name, and `MarkAsGarbage` does not free a name: the object stays in the
	// package's object list until the garbage is collected, which a commandlet never reaches. Renaming it to
	// the transient package is the step that frees the name, so that the `CreateBlueprint` below can take it.
	//
	// AND THE SAVE NEEDS ONE MORE THING, WHICH IS NOT THIS BLOCK'S BUSINESS BUT IS THIS COMMANDLET'S: see
	// `MarkAsFullyLoaded` at the save. Without it a RE-RUN is refused before a byte is written, so the
	// repeatability this block provides would not be reachable at all.
	{
		UPackage* Existing = FindPackage(nullptr, *PackagePath);
		if (Existing != nullptr)
		{
			Existing->SetDirtyFlag(false);
			if (UObject* Old = FindObject<UObject>(Existing, TEXT("TurboMCPAcceptance")))
			{
				// `Rename` to the transient package takes it out of the asset package's object list
				// without deleting it, which is the only safe way to free the name here.
				//
				// *** THE RESULT IS CHECKED, BECAUSE A FAILED RENAME LEAVES THE COLLISION IT EXISTS TO
				// PREVENT. `UObject::Rename` returns `bool` (`Object.h:693`). When it fails the old
				// object keeps its name inside the package, the `CreateBlueprint` below then makes a
				// second object of that name, and the save writes a package holding both. ***
				if (!Old->Rename(nullptr, GetTransientPackage(),
					REN_DontCreateRedirectors | REN_NonTransactional))
				{
					UE_LOG(LogTurboMCP, Error,
						TEXT("could not move the existing TurboMCPAcceptance out of %s, so the name ")
						TEXT("is not free and the package would hold two objects of that name"),
						*PackagePath);
					return 3;
				}
			}
		}
	}

	UPackage* Package = CreatePackage(*PackagePath);
	if (Package == nullptr)
	{
		UE_LOG(LogTurboMCP, Error, TEXT("could not create the package %s"), *PackagePath);
		return 1;
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

	// *** A MISSING PIN MAKES ANY OF THE FIVE PROBES RETURN `false` WITH A NAMED REASON RATHER THAN SUCCEED
	// WITH A CHAIN IT NEVER MADE. *** `MakeLinkTo` RETURNS `void`, so a link that was never made is invisible
	// in every return value; the pin guards are the only place it can be reported, and `GetExecPin()` or
	// `GetInputPin()` returning null on a node the factory just created means the factory produced something
	// it cannot wire. THE ASSET IS SAVED ONLY IF ALL FIVE BUILT, because the suites read this fixture and a
	// hole in it is a hole in what they measure. ***
	TArray<FString> ProbeProblems;
	const bool bKnots = BuildKnotProbe(Blueprint, ProbeProblems);
	const bool bDead = BuildDeadProbe(Blueprint, ProbeProblems);
	const bool bLocals = BuildLocalVarProbe(Blueprint, ProbeProblems);
	const bool bSane = BuildSaneProbe(Blueprint, ProbeProblems);
	// THE FIFTH PROBE, ADDED BECAUSE TWO INDEPENDENT SPECIFICATIONS FOUND THE SAME HOLE. `KnotProbe`
	// cannot test the crossing case and the asset had no comment box at all, so `P/E-GRAPH-KNOTS` and
	// `P/E-GRAPH-TIDY` both had their primary case resting on code review.
	const bool bLayout = BuildLayoutProbe(Blueprint, ProbeProblems);

	UE_LOG(LogTurboMCP, Display,
		TEXT("TurboMCP: KnotProbe=%s DeadProbe=%s LocalVarProbe=%s SaneProbe=%s LayoutProbe=%s"),
		bKnots ? TEXT("ok") : TEXT("FAILED"),
		bDead ? TEXT("ok") : TEXT("FAILED"),
		bLocals ? TEXT("ok") : TEXT("FAILED"),
		bSane ? TEXT("ok") : TEXT("FAILED"),
		bLayout ? TEXT("ok") : TEXT("FAILED"));

	// WHY A PROBE FAILED, PRINTED RATHER THAN ONLY COUNTED. Without this the log says `FAILED` and the
	// reason is discarded - the shape `P1` names.
	for (const FString& Problem : ProbeProblems)
	{
		UE_LOG(LogTurboMCP, Error, TEXT("TurboMCP: %s"), *Problem);
	}

	if (!bKnots || !bDead || !bLocals || !bSane || !bLayout)
	{
		UE_LOG(LogTurboMCP, Error, TEXT("one or more probe graphs could not be built"));
		return 2;
	}

	// THE SAVE IS THE POINT. An asset held only in memory dies with the editor, and this asset exists
	// to be committed and re-read by a test suite.
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	Package->MarkPackageDirty();

	// THE ABSOLUTE PATH IS BUILT BY HAND, BECAUSE THE ENGINE'S CONVERSION GOES RELATIVE HERE.
	//
	// MEASURED FAILURE: `FPackageName::LongPackageNameToFilename` answered
	// `../../../../(repo)/TestProject/Content/Tests/TurboMCPAcceptance.uasset`,
	// a path RELATIVE TO THE ENGINE DIRECTORY, and `UPackage::SavePackage` reported
	// "could not save". The package was in memory and every probe graph had built — the write was the
	// only thing that failed, which is the shape of failure this project keeps having to look for.
	//
	// The package path is a compile-time constant and the engine remaps `/Game` to the PROJECT content
	// directory, so the absolute path is that constant without its `/Game` prefix, under
	// `FPaths::ProjectContentDir()`.
	const FString ContentDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir());
	const FString FileName = ContentDir
		+ PackagePath.RightChop(FCString::Strlen(TEXT("/Game")))
		+ FPackageName::GetAssetPackageExtension();

	UE_LOG(LogTurboMCP, Display, TEXT("TurboMCP: saving to %s"), *FileName);

	// *** THE SAVE GOES TO A TEMPORARY FILE, AND THE ORIGINAL IS REPLACED ONLY ONCE IT SUCCEEDED. ***
	//
	// MEASURED: building into a package that still held an object of the same name made
	// `UPackage::SavePackage` answer "could not save" while every probe graph had built, and moving the old
	// object to the transient package did NOT settle it. So the file must not be there when the save runs.
	//
	// WRITING STRAIGHT TO `FileName` MEANT DELETING IT FIRST, WHICH LEFT A WINDOW: a failed save returned 3
	// with the original file already gone and nothing to restore it. The temporary file removes the window -
	// the original is untouched until a complete new file exists, so a failure anywhere before the move
	// leaves the asset exactly as it was.
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	const FString TempFileName = FileName + TEXT(".writing");
	if (PlatformFile.FileExists(*TempFileName) && !PlatformFile.DeleteFile(*TempFileName))
	{
		UE_LOG(LogTurboMCP, Error, TEXT("a stale %s could not be removed"), *TempFileName);
		return 3;
	}

	// *** THE PACKAGE MUST BE MARKED FULLY LOADED, OR A RE-RUN IS REFUSED BEFORE A BYTE IS WRITTEN. ***
	//
	// `SavePackage2.cpp:239` returns `ESavePackageResult::Error` when `!Package->IsFullyLoaded()`, and
	// `Package.cpp:359-363` answers false for a package that HAS A FILE ON DISK while its `bHasBeenFullyLoaded`
	// is false - which is exactly the state `CreatePackage` produces at :1090 once the asset exists from an
	// earlier run. So the FIRST run finds no file and succeeds, and EVERY RE-RUN IS REFUSED.
	//
	// `MarkAsFullyLoaded` is the engine's own API for this (`Package.h:685`), and it is accurate here: this
	// commandlet built the whole package in memory, so the process holds everything the file must contain.
	Package->MarkAsFullyLoaded();

	// *** THE REASON A SAVE FAILS IS CAPTURED RATHER THAN SUPPRESSED. ***
	//
	// `SAVE_NoError` means "Don't generate errors on save" (`ObjectMacros.h:101`), and every message the engine
	// would produce is gated on it (`SaveContext.h:833-836`, `IsGenerateSaveError`). With that flag set,
	// `UPackage::SavePackage` answers `false` and the log holds nothing but this file's own sentence - which is
	// how a save that could not state its cause came to be explained here by a collision theory.
	//
	// The flag is therefore DROPPED, and the engine's message is routed to a collector instead of `GError`, its
	// default (`SavePackage.h:90`), because `GError` raises `appError` and takes the editor down - which is not
	// how a commandlet reports a failed save.
	FStringOutputDevice SaveErrors;
	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	SaveArgs.Error = &SaveErrors;
	if (!UPackage::SavePackage(Package, Blueprint, *TempFileName, SaveArgs))
	{
		// The original is STILL THERE, so the failure costs nothing but the attempt.
		UE_LOG(LogTurboMCP, Error,
			TEXT("could not save %s, so the existing asset was left untouched"), *TempFileName);
		if (!SaveErrors.IsEmpty())
		{
			// THE ENGINE'S OWN SENTENCE, WHICH THE FLAG ABOVE USED TO THROW AWAY.
			UE_LOG(LogTurboMCP, Error, TEXT("the engine's reason: %s"), *SaveErrors);
		}
		return 3;
	}

	// THE ONLY DESTRUCTIVE STEP, AND IT RUNS ON A COMPLETE NEW FILE. A failure here leaves the temporary
	// file behind and the original still in place, which the message says.
	if (PlatformFile.FileExists(*FileName) && !PlatformFile.DeleteFile(*FileName))
	{
		UE_LOG(LogTurboMCP, Error,
			TEXT("wrote %s but could not replace the existing asset at %s, so BOTH files are now on disk"),
			*TempFileName, *FileName);
		return 3;
	}
	// *** `MoveFile` TAKES `(To, From)` AND THAT ORDER IS THE WHOLE POINT HERE. *** The declaration reads
	// `virtual bool MoveFile(const TCHAR* To, const TCHAR* From)` (verified in the engine's platform
	// headers). So the FIRST argument is the destination the new asset must end up at, and the SECOND is
	// the temporary file just written. WRITING THEM THE OTHER WAY ROUND WOULD MOVE THE ORIGINAL ONTO THE
	// `.writing` NAME, DESTROYING THE NEW FILE AND LEAVING THE OLD ASSET IN PLACE - the exact opposite of
	// the intent, and silent.
	if (!PlatformFile.MoveFile(*FileName, *TempFileName))
	{
		UE_LOG(LogTurboMCP, Error,
			TEXT("wrote %s but could not move it onto %s, so BOTH files are now on disk"),
			*TempFileName, *FileName);
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
