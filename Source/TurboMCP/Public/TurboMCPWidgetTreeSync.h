// Copyright (c) 2026 Florian ESCHALIER (FloStyle)
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once

#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "CoreTypes.h"
#include "TurboMCPWidgetTreeText.h"

class UBaseWidgetBlueprint;
class UPanelWidget;
class UWidget;

/**
 * `P/E-WIDGET-TREE`: write a widget tree from the text dialect.
 *
 * WHAT WAS MISSING, AND IT IS THE WHOLE REASON THIS FILE EXISTS
 * -------------------------------------------------------------
 * `TurboMCPWidgetTreeText` already parses and renders the dialect, and `FTurboMCPWidgetTree::Collect`
 * already enumerates a tree. NEITHER ANSWERS THE QUESTION THIS CAPABILITY ASKS, which is *what is the tree
 * IN SLOT ORDER* - `Collect` walks depth first and FLATTENS THE ORDER AWAY, and slot order is exactly what
 * dialect rule 8.2.3 says the graph text cannot express and this dialect exists to carry.
 *
 * So the first thing here is `Walk`: a live `UWidgetTree` turned into the dialect's `FTree`, PRESERVING
 * SLOT ORDER. It reads children through `UPanelWidget::GetChildrenCount` and `GetChildAt`
 * (`Components/PanelWidget.h:28,36`) rather than through the widget tree's flat array, because the flat
 * array is the flattening this file was written to undo.
 *
 * `Walk` IS A PURE READ, AND THE ROUND TRIP IT ENABLES NEEDS NO WRITE AT ALL: walk a tree, render it,
 * parse it back, walk again and compare. If those two texts differ, the dialect cannot represent the tree
 * it came from - WHICH IS THE ONLY FAILURE A READ-ONLY TEST CAN DETECT AND THE ONE THAT MATTERS MOST,
 * because a dialect that loses information silently reorders children on the next write. `test-widget-tree`
 * uses it for that reason.
 *
 * THE THREE VERBS MEAN WHAT THEIR GRAPH TWINS MEAN
 * ------------------------------------------------
 * `append` adds and CANNOT REMOVE. `replace` rewrites the tree under the root and LISTS EVERY WIDGET IT
 * REMOVES (`D-45`: a list, never a count). `patch` matches by widget path and changes only what the text
 * describes.
 *
 * AND A RENAME IS A REFUSAL RATHER THAN A RENAME - `D-49`, and the reason is that a tree writer has no view
 * of the graph side: *"a name change breaks every reference to it, and a tree writer has no view of the
 * graph side where those references live."* `P/E-WIDGET-SET` changes a name and updates its references, in
 * one place, on purpose. THE DETECTION IS POSSIBLE HERE PRECISELY BECAUSE PATHS ARE DERIVED FROM NAMES:
 * a node whose PATH matched but whose NAME differs from the live widget at that path is a rename.
 *
 * SLOT COUNT IS ENFORCED WHERE THE ENGINE ENFORCES IT. Adding a third child to a `SizeBox` is refused BY
 * NAME at plan time rather than accepted and discovered at compile time - `UPanelWidget::CanHaveMultipleChildren`
 * is the engine's own answer and is asked rather than restated.
 */
class FTurboMCPWidgetTreeSync
{
public:
	/** What the text asks for. */
	enum class EVerb : uint8
	{
		Append,
		Replace,
		Patch,
	};

	/** One widget a `replace` would remove, named so the list says WHAT rather than how much. */
	struct FRemoval
	{
		FString Path;
		FString ClassName;
	};

	/**
	 * The decision, made ONCE so the Preview and its Edit twin cannot disagree.
	 *
	 * The rule this project states for every `P/E` pair is that there is exactly ONE place that can answer
	 * "no" and both tools reach it; a Preview that accepted what its Edit twin refused would be worse than
	 * no Preview at all.
	 */
	struct FPlan
	{
		/** Resolved from the asset string, so `Apply` and `RenderPreview` agree about the target. */
		FString AssetPath;

		EVerb Verb = EVerb::Append;

		/** The parsed text, kept so `Apply` writes the tree the Preview described. */
		TurboMCPWidgetTreeText::FTree Text;

		/** Every widget the text describes, by derived path, so the preview can say what it will touch. */
		TArray<FString> Added;

		/** Every widget `replace` would remove, BY PATH AND CLASS rather than counted - `D-45`. */
		TArray<FRemoval> Removed;

		/** A path the text names that is not in the live tree, with the closest real paths for the refusal. */
		FString UnknownPath;

		/** The closest real paths, filled only when `UnknownPath` is set. */
		TArray<FString> ClosestPaths;

		/** A widget whose path matched but whose NAME changed: the rename this tool refuses to perform. */
		FString RenamedFrom;

		/** The name it would have become. */
		FString RenamedTo;

		/** A panel that cannot take another child, named with the engine's own reason. */
		FString OverfullPanel;

		/** How many children it has and how many it can hold, filled only when `OverfullPanel` is set. */
		int32 OverfullHave = 0;
		int32 OverfullLimit = 0;

		/** One refusal for the whole operation when the plan was rejected outright. */
		FString Refusal;
	};

	/**
	 * Parses the text, resolves it against the LIVE tree, and decides.
	 *
	 * Returns false with `OutError` when the OPERATION cannot be planned at all - an unreadable asset, a text
	 * that does not parse - which is a different thing from a plan that decides to refuse, because a refusal
	 * names a real input the caller can fix.
	 */
	static bool Plan(const FString& Asset,
		const FString& TreeText,
		EVerb InVerb,
		FPlan& OutPlan,
		FString& OutError);

	/** The tree as the dialect writes it - the SAME code path the writer accepts, which is `D-15`'s rule. */
	static bool Walk(UBaseWidgetBlueprint* InWidgetBlueprint,
		TurboMCPWidgetTreeText::FTree& OutTree,
		FString& OutError);

	/** The live tree as dialect text. A pure read, and the round trip's first half. */
	static FString Read(const FString& Asset);
};
