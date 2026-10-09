// Copyright FloStyle. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/Map.h"
#include "Containers/Set.h"
#include "Containers/UnrealString.h"
#include "CoreTypes.h"

class UEdGraph;
class UEdGraphNode;
class UEdGraphPin;

/**
 * The graph text format from `04_FORMATS.md` section 2.
 *
 * One writer, used by both `R-GRAPH-LOAD` and `R-BP-LOAD`, so the two cannot
 * disagree about the format. That is the whole reason this is a class rather
 * than a function on a tool: the spec says most bugs in earlier work came from
 * two tools disagreeing about one of the shared formats.
 *
 * The output is S-expressions, because the engine's own graph writer uses them
 * and agents already know the shape. Comments start with `;`, and the first
 * comment on a node carries its node ID so a reader can act on it.
 */
class FTurboMCPGraphText
{
public:
	/** How much of the graph to print. Each level is a superset of the one below. */
	enum class EDetail : uint8
	{
		/** Entry points by name only, with counts. No bodies. */
		Manifest,
		/** Entry points with their signature, still no bodies. */
		Signatures,
		/** The default: every entry point with its body in graph text. */
		Bodies,
		/** Bodies plus the editing details: type IDs everywhere, knots, pin indexes. */
		Full,
	};

	/** Parses a detail name, returning false when it is not one of the four. */
	static bool ParseDetail(const FString& InName, EDetail& OutDetail);

	/** The detail names, for an error that has to list what is accepted. */
	static FString DetailNames();

	/**
	 * Renders one graph.
	 *
	 * @param InGraph    The graph to read. Never modified: this is a Read tool.
	 * @param InDetail   How much to print.
	 * @param InAssetPath  The asset's object path, so node IDs are the long form
	 *                   that `04_FORMATS.md` 3.2 requires at the top of output.
	 * @param OutNotes   Appended to with anything that could not be printed. The
	 *                   spec requires each of those to be an inline `; error:` line
	 *                   rather than a silent gap, and this list is how the caller
	 *                   knows the text is incomplete.
	 */
	FString Render(const UEdGraph* InGraph, EDetail InDetail,
		const FString& InAssetPath, TArray<FString>& OutNotes) const;

private:
	/**
	 * One entry point: an event, a custom event, or a function's entry.
	 *
	 * A graph is a list of these. `04_FORMATS.md` 2.1.2 says there is no top
	 * level form for "a whole graph", so nothing wraps them.
	 */
	struct FEntryPoint
	{
		UEdGraphNode* Node = nullptr;
		FString Kind;   // `event` or `fn`
		FString Name;
		bool bIsCustomEvent = false;
	};

	/** Every entry point of a graph, in a stable order. */
	void CollectEntryPoints(const UEdGraph* InGraph,
		TArray<FEntryPoint>& OutEntries) const;

	/**
	 * Walks one exec chain and prints it, following **every** exec output.
	 *
	 * Following only the first output loses the other arms of a branch: an
	 * `if/else` printed one arm and left the other as an orphan in the
	 * unreachable list. Each further exec output is printed as a named
	 * continuation sub-block, which is the shape the engine's own graph text uses
	 * for a node with several exec outputs.
	 *
	 * @param InDepth  How far to indent, so a nested continuation reads as nested.
	 */
	FString RenderChain(const UEdGraphNode* InFirst, EDetail InDetail,
		int32 InDepth, const FString& InAssetPath,
		TSet<const UEdGraphNode*>& InOutPrinted,
		TSet<const UEdGraphNode*>& InOutSharedValues,
		TArray<FString>& OutNotes) const;

	/**
	 * Prints one entry point with its body.
	 *
	 * Exec nodes are walked in execution order, which is what makes a chain read
	 * top to bottom (`2.1.3`). Value nodes feeding a used pin are printed inline,
	 * and a value node feeding more than one consumer is defined once and
	 * referred to by name (`2.1.6`).
	 */
	FString RenderEntryBody(const UEdGraph* InGraph, const FEntryPoint& InEntry,
		EDetail InDetail, const FString& InAssetPath,
		TSet<const UEdGraphNode*>& InOutPrinted,
		TArray<FString>& OutNotes) const;

	/** One node as an S-expression, with its pin values. */
	FString RenderNode(const UEdGraphNode* InNode, EDetail InDetail,
		const FString& InAssetPath,
		TSet<const UEdGraphNode*>& InOutPrinted,
		TSet<const UEdGraphNode*>& InOutSharedValues,
		TArray<FString>& OutNotes) const;

	/**
	 * A pin's value: the link target, the literal default, or nothing.
	 *
	 * `2.1.8` says a pin left at its default is not printed, and the `self` pin
	 * and the world context pin are never printed when they are implicit.
	 */
	FString RenderPinValue(const UEdGraphPin* InPin, EDetail InDetail,
		const FString& InAssetPath,
		TSet<const UEdGraphNode*>& InOutPrinted,
		TSet<const UEdGraphNode*>& InOutSharedValues,
		TArray<FString>& OutNotes) const;

	/** True when this pin is one the format hides when it is at its default. */
	static bool IsImplicitPin(const UEdGraphPin* InPin);

	/**
	 * The exec pin that continues this node, if any.
	 *
	 * A node with several exec outputs (a branch, a sequence) has more than one;
	 * only the first is followed in order and the rest are printed as named
	 * continuations, which is how `2.1.3` says a join reads.
	 */
	static UEdGraphPin* FindExecOut(const UEdGraphNode* InNode);

	/** The node a pin links to, or null. */
	static UEdGraphNode* LinkedNode(const UEdGraphPin* InPin);

	/** A value node is one that only computes a value: no exec pins at all. */
	static bool IsValueNode(const UEdGraphNode* InNode);
};
