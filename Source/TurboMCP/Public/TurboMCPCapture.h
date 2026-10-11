// Copyright (c) 2026 Florian ESCHALIER (FloStyle)
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once

#include "Containers/UnrealString.h"
#include "CoreTypes.h"

class SGraphEditor;
class UEdGraph;
class UEdGraphNode;

/**
 * The four framing selectors added to `L-CAPTURE` by `WORKING_PLAN.md` row 2.1,
 * and the definition each one carries in its own output.
 *
 * WHY THIS IS A SEPARATE FILE FROM `TurboMCPCapture.cpp`
 * -----------------------------------------------------
 * `L-CAPTURE` already exists and is 792 lines: it opens a Blueprint editor, frames
 * the whole graph, one comment box or one node, and writes a PNG. This row adds
 * four SELECTORS to that tool, not a second tool, so there is exactly one
 * `LookTurboMCPCapture` and exactly one PNG contract (`04_FORMATS.md` section 6).
 *
 * A selector decides WHICH NODES the existing renderer frames, and that decision is
 * the whole of this file. Splitting it out keeps the frame arithmetic in
 * `TurboMCPCapture.cpp` untouched, which matters because a change there would move
 * the pictures the existing `test-capture` suite already measures.
 *
 * THE THREE CHOICES THIS FILE MAKES, EACH BECAUSE THE ALTERNATIVE IS A SILENT LIE
 * -----------------------------------------------------------------------------
 * `selection` means THE GRAPH CANVAS NODES SELECTED IN THE OPEN GRAPH EDITOR and
 * nothing else. It is read from the live `SGraphEditor`, because the engine stores
 * node selection in the Slate panel and NOT on `UEdGraphNode`: there is no
 * `IsSelected` on `EdGraph/EdGraphNode.h`. The actor selection
 * (`GEditor->GetSelectedActors`, `EditorEngine.h:1969`) and the asset browser
 * selection are DIFFERENT SELECTIONS, so the result says which one it read and a
 * caller who selected actors gets a refusal rather than a picture of a graph.
 *
 * `neighbours` follows THE GRAPH THE READER PRINTS, so a link through a plain
 * reroute knot is one hop and not two (`D-42`, `04_FORMATS.md` section 6.2). The
 * definition is PRINTED IN THE OUTPUT, because a selector whose definition is not
 * in its own result is a selector a caller cannot verify.
 *
 * `screen` is the whole editor window, and it REFUSES when the process does not
 * render instead of writing a black image. The engine line that decides it is
 * `RHIGlobals.h:894` (`GUsingNullRHI`), which is what `LaunchEngineLoop.cpp:3343`
 * tests before it installs the Slate NULL renderer at `:3345-3347`. THE PLUGIN CANNOT
 * READ THAT SYMBOL: the `RHI` module is not in `TurboMCP.Build.cs` and the link fails
 * with `unresolved external symbol GRHIGlobals`, so the decision is taken from the
 * `-nullrhi` command line flag (`App.h:405` parses it) and `FApp::IsRendering()`
 * (`App.h:406`). The output says which signal was used rather than implying the
 * engine's own flag was read.
 */
namespace TurboMCPCaptureSelector
{
	/** One comment box, as a selector sees it: its stable index and its rectangle. */
	struct FBox
	{
		/** The index `L-LAYOUT-REPORT` prints and `mode box` accepts. */
		int32 Index = 0;

		/** The box node itself. */
		UEdGraphNode* Node = nullptr;

		/** Its rectangle in graph coordinates. */
		double MinX = 0.0;
		double MinY = 0.0;
		double MaxX = 0.0;
		double MaxY = 0.0;
	};

	/** What a selector resolved: the framing mode, the nodes, and the words for both. */
	struct FSelection
	{
		/** The mode name, exactly as `04_FORMATS.md` section 6.1 spells it. */
		FString Mode;

		/** The nodes to frame. Empty is a refusal, never a wider frame (`D-41`). */
		TArray<UEdGraphNode*> Targets;

		/**
		 * What was actually framed, in words.
		 *
		 * This REPLACES the label the older modes build, because for these modes the
		 * label has to name the selection or the hop count, and only the selector
		 * knows either.
		 */
		FString FrameLabel;

		/**
		 * The selector's own definition, ready to print as a result block.
		 *
		 * It is PRINTED IN THE OUTPUT rather than kept here as a comment, which is
		 * the requirement this row was written with: the definition travels with the
		 * answer, so a caller can check the claim instead of trusting it.
		 */
		FString Definition;

		/** The box index a box framing used, or `INDEX_NONE`. */
		int32 TargetBox = INDEX_NONE;
	};

	/**
	 * The comment boxes of one graph, in the order `L-LAYOUT-REPORT` prints them.
	 *
	 * Top to bottom, then left to right, which is the order the existing `mode box`
	 * already uses. One implementation, so an index cannot mean two things.
	 */
	void CollectCommentBoxes(UEdGraph* InGraph, TArray<FBox>& OutBoxes);

	/**
	 * Resolves one of the four new selectors.
	 *
	 * @param InMode    The mode name, already lower cased and trimmed.
	 * @param InTarget  The node ID for `neighbours`, or the box index for
	 *                  `box-neighbours`.
	 * @param InHops    How many hops `neighbours` follows, or how many linked boxes
	 *                  `box-neighbours` adds.
	 * @param InAsset   The resolved asset path, for the refusal text.
	 * @param InGraph   The graph the caller named.
	 * @param InEditor  The live graph editor, or null when no tab is open. Only
	 *                  `selection` needs it.
	 * @param OutWhy    Why the selector refused. Set only when the result is FALSE.
	 *
	 * @return TRUE when the frame is resolved, FALSE when the caller is refused. A
	 *         refusal always carries a reason (`P1`): an empty reason is a defect,
	 *         not a terse answer.
	 */
	bool Resolve(
		const FString& InMode,
		const FString& InTarget,
		int32 InHops,
		const FString& InAsset,
		UEdGraph* InGraph,
		SGraphEditor* InEditor,
		FSelection& OutSelection,
		FString& OutWhy);

	/**
	 * Renders the whole editor window and writes it as a PNG, for `mode screen`.
	 *
	 * @param InOutDescription  The result text, appended to. Carries the window
	 *                          size, the image size and what was framed, so a
	 *                          caller never has to guess whether the picture is
	 *                          the window or a frame cut out of it.
	 * @param OutImagePath      The PNG written.
	 * @param OutError          Why nothing was written. Set only on FALSE.
	 *
	 * @return TRUE when a PNG was written. FALSE carries the reason BY NAME,
	 *         including the `-nullrhi` refusal, and never a black image.
	 */
	bool RenderScreen(FString& InOutDescription, FString& OutImagePath, FString& OutError);

	/**
	 * Reports whether `mode screen` can work HERE, and says why either way.
	 *
	 * It creates nothing and writes nothing, so it is safe to call from a suite that
	 * must not touch the machine's screen. It exists because the answer to "does
	 * `-nullrhi` permit a screen capture" has to be MEASURED on the running editor
	 * rather than inferred from a brief, and because `P1` forbids reporting an absent
	 * measurement as a bare `false`.
	 *
	 * @param InOutDescription  The report, appended to.
	 */
	void DescribeScreenAvailability(FString& InOutDescription);

	/**
	 * AN INTERNAL PROBE, NOT PART OF THE `L-CAPTURE` CONTRACT, AND IT IS THE ONE THING
	 * IN THIS FILE THAT CHANGES THE EDITOR.
	 *
	 * `selection` cannot be tested without a selection, and `P3` says a Look must not
	 * create one: "not the editor selection that the user can see"
	 * (`02_PRINCIPLES.md:29-33`). This probe exists so the positive branch can be
	 * measured AT ALL, and it is written so that the invariant `P3` protects still holds
	 * when it returns:
	 *
	 *   1. it reads the selection that is ALREADY there and reports it, so a caller can
	 *      see what it changed from;
	 *   2. it selects the named nodes through the engine's own
	 *      `SGraphEditor::SetNodeSelection` (`GraphEditor.h:418-424`);
	 *   3. it reads back what the engine now holds, so the caller is not told what was
	 *      asked for but what happened;
	 *   4. IT CLEARS THE SELECTION AND ASSERTS THAT IT IS EMPTY, so no user visible
	 *      selection is left behind, and the report says so;
	 *   5. it is reachable ONLY as an undocumented mode on the tool that already
	 *      exists, and it is NOT an `AICallable` tool of its own.
	 *
	 * A mode that changes the editor is exactly as much of a contract deviation as
	 * `debug-window`, which returns the whole window while the exported contract says it
	 * frames the graph. It is named here rather than hidden.
	 *
	 * @param InPanel   The live graph panel. Null is a refusal, not an empty answer.
	 * @param InGraph   The graph, for resolving keys.
	 * @param InKeys    Comma separated node keys, the same keys `R-NODE-FIND` prints.
	 * @param InAsset   The asset path, for the report.
	 * @param InOutDescription  The report, appended to.
	 * @param OutWhy    Why the probe refused. Set only when the result is FALSE.
	 */
	bool ProbeSelection(
		SGraphEditor* InPanel,
		UEdGraph* InGraph,
		const FString& InKeys,
		const FString& InAsset,
		FString& InOutDescription,
		FString& OutWhy);

	/**
	 * The measured rendering state of this process, in one block.
	 *
	 * `GUsingNullRHI` (`RHIGlobals.h:894`) and `FSlateApplication::IsInitialized()`
	 * are the two facts that decide whether any PNG path in this plugin can run, and
	 * they are printed as facts rather than as a true or a false alone.
	 *
	 * @param InOutDescription  The report, appended to.
	 */
	void DescribeRenderingState(FString& InOutDescription);
}
