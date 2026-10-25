// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once

#include "Containers/UnrealString.h"
#include "CoreTypes.h"

class FString;
class UObject;

/**
 * `L-CAPTURE-ASSET`: a picture of ANY asset editor, not only a Blueprint graph.
 *
 * WHY THIS IS A SEPARATE FILE FROM `TurboMCPCapture.cpp` AND
 * `TurboMCPCaptureSelector.cpp`
 * -----------------------------------------------------------------------
 * `WORKING_PLAN.md` row 2.2 extends `L-CAPTURE` with a second axis. Row 2.1 added
 * SELECTORS, which decide WHICH NODES of one Blueprint graph are framed, and it put
 * them in `TurboMCPCaptureSelector.cpp` for exactly one reason: `TurboMCPCapture.cpp`
 * already owned the frame arithmetic and the PNG contract, and a change there would
 * move the pictures the tracked `Tools/test-capture.py` already measures.
 *
 * This row asks a question those selectors cannot answer: WHICH EDITORS EXIST AS A
 * SLATE WIDGET AT ALL. That is a different axis (an editor tab, not a graph frame),
 * it needs different engine headers, and it must not disturb the node-frame path. So
 * it is a third file, and `TurboMCPCapture.cpp` gains one mode that delegates here.
 *
 * THE SHAPE: SOME KINDS BUILT, AND EVERY OTHER KIND REFUSED BY NAME
 * -----------------------------------------------------------------
 * `03_TOOLS.md` section 18 (`L-CAPTURE-ASSET`) requires each editor kind to be
 * "either supported or REFUSED BY NAME, listed in the tool's own answer, so 'which
 * editors can this capture' is never a guess", and `T-LOOK-8` requires the same
 * sentence in the acceptance row.
 *
 * A tool that CLAIMS ten kinds and renders four is worse than one that renders four
 * and names the six it cannot, so the catalogue below carries one ENTRY PER KIND with
 * the module that owns its editor, and the decision is taken from what is actually
 * loaded in this process rather than from a guess about what should be.
 *
 * *** WHAT DECIDES EACH KIND IS A MODULE, AND THAT IS WHY IT IS MEASURABLE. ***
 * An asset editor in this engine is published by an editor module through an
 * `AssetTypeActions` registration, and `UAssetEditorSubsystem::OpenEditorForAsset`
 * finds nothing to open when that module is not loaded. So the test
 * `FModuleManager::Get().IsModuleLoaded(OwnerModule)` is a REAL measurement of whether
 * the kind is reachable here, taken in the running editor, and it is the same test
 * that decides whether this tool attempts a capture or refuses. The output prints
 * which module was checked and whether it answered.
 *
 * THE ONE KIND THAT IS NOT AN ASSET EDITOR AT ALL
 * ----------------------------------------------
 * `L-WIDGET-PREVIEW` is a different tool with a different contract (`03_TOOLS.md`
 * section 18): it instantiates a widget off screen with sample values and renders it.
 * It is named in the catalogue because a caller looking for a picture of a widget
 * should be sent to it rather than told only that this tool cannot help.
 */
namespace TurboMCPCaptureAsset
{
	/**
	 * The three answers a kind can get, and why there are three rather than a boolean.
	 *
	 * A tool that collapsed these into true and false could not say WHICH TOOL to use
	 * instead, which `03_TOOLS.md` section 18 requires of a refused kind.
	 *
	 * THIS IS DECLARED OUTSIDE `FKind` ON PURPOSE: a nested type used as the type of a
	 * member of its own enclosing class is an incomplete type at that point, and MSVC
	 * rejects it. THAT IS A COMPILER RULE ABOUT INCOMPLETE TYPES, NOT A PREFERENCE.
	 */
	enum class EStatus : uint8
	{
		/**
		 * The editor module is LOADED in this process, so the editor tab can be rendered.
		 * This is measured, by loading the module, not declared.
		 */
		BuiltHere,

		/**
		 * This tool cannot, but another tool in this plugin can, and `AlternativeTool`
		 * names it.
		 */
		CapturedByAnotherTool,

		/**
		 * The editor module could not be loaded in this process, so there is no editor to
		 * render.
		 */
		Refused
	};

	/** One editor kind this tool knows about, and what was decided about it. */
	struct FKind
	{
		/**
		 * The name the CONTRACT uses, spelled as `03_TOOLS.md` section 18 spells it.
		 *
		 * It is matched case insensitively against the `kind` argument, so a caller can
		 * ask "can you capture a DataTable" without knowing which spelling this row
		 * chose.
		 */
		FString Name;

		/** The asset class this kind is, for the caller's benefit. */
		FString AssetClass;

		/**
		 * The editor module whose `AssetTypeActions` publishes this kind's editor.
		 *
		 * Empty for a kind that is not an asset editor at all, or for a kind whose editor
		 * is published by a module this plugin already links and has already loaded.
		 */
		FString OwnerModule;

		/**
		 * Whether this kind's editor really can be captured HERE, and why.
		 *
		 * Taken from `Reachability`, which is measured, and never from a guess.
		 */
		EStatus Status = EStatus::Refused;

		/** MEASURED: the module that publishes this kind's editor, and how it answered. */
		FString Reachability;

		/**
		 * The tool that CAN capture this kind when this one cannot, or empty.
		 *
		 * `03_TOOLS.md` section 18 requires a refused kind to say "which tool can capture
		 * it instead, if any", and an empty string here is the "if any" case stated as
		 * itself rather than left out.
		 */
		FString AlternativeTool;

		/** Why this kind is or is not supported, naming the file and line that decides it. */
		FString Reason;
	};

	/**
	 * The kinds this tool knows about, whether or not they can be captured HERE.
	 *
	 * Returns every kind the row was written for, including the ones whose editor module
	 * is not loaded in this process, because "which editors can this capture" is
	 * answered by the SET and the reason, not by the survivors alone.
	 */
	void CollectKinds(TArray<FKind>& OutKinds);

	/**
	 * Writes the whole support table: one line per kind, BUILT or REFUSED, and why.
	 *
	 * This is the answer to "which editors can this capture", and it is printed whether
	 * or not an asset was named, so a caller can ask the question without capturing
	 * anything. IT ASKS THE ENGINE TO LOAD EACH KIND'S EDITOR MODULE, so the answer is a
	 * measurement of reachability rather than of what happens to be open, and it is the
	 * same on the first call of a process as on the twentieth.
	 *
	 * It opens no ASSET EDITOR and writes no file. Loading a module is not opening an
	 * editor: nothing is rendered, nothing is shown and nothing is saved.
	 */
	void DescribeKinds(FString& InOutDescription);

	/**
	 * The catalogue entry whose `Name` matches, or false when nothing matches.
	 *
	 * @param InKind  The caller's `kind` argument, compared case insensitively.
	 * @param OutKind The matched entry.
	 */
	bool FindKind(const FString& InKind, FKind& OutKind);

	/**
	 * Opens the asset editor and captures its toolkit host using Slate TakeScreenshot.
	 * Captures the current presentation; it does not establish shader readiness.
	 * The resulting PNG is capped at 4096 pixels per edge, preserving aspect ratio.
	 *
	 * bFitGraph supports the standard Material Editor only. It changes the view to
	 * include all measured nodes without changing node positions or selection.
	 * Unsupported, ambiguous or unmeasurable graphs refuse rather than claim a fit.
	 * Opening editor UI can invoke editor callbacks; no explicit save is performed.
	 *
	 * @param InAsset Asset path or unique name.
	 * @param InOutDescription Capture details, appended on success.
	 * @param OutImagePath Written PNG path; empty on refusal.
	 * @param OutError Reason for refusal; empty on success.
	 * @param bFitGraph Fit the material graph before capture. False keeps its view.
	 * @return True when a PNG was written.
	 */
	bool CaptureAssetEditor(
		const FString& InAsset,
		FString& InOutDescription,
		FString& OutImagePath,
		FString& OutError,
		bool bFitGraph = false);
}
