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

class UBlueprint;
class UEdGraph;

/**
 * `P-GRAPH-CHECK`: checks graph text and writes NOTHING.
 *
 * THE GATE EVERY WRITE ENTRY POINT DEPENDS ON
 * -------------------------------------------
 * `03_TOOLS.md` requires this before any write, so it is the one tool that has to be RIGHT rather than
 * merely useful: `P/E-GRAPH-APPEND`, `REPLACE` and `PATCH` all take the same text, and a checker that
 * passes text they will refuse is worse than no checker, because the caller believes it.
 *
 * IT IS STRUCTURALLY INCAPABLE OF WRITING. It lives in `UTurboMCPPreviewToolset`, which by this project's
 * design holds no tool that can change an asset - the protection is the CLASS, not care. This class does
 * not call `Modify`, `MarkPackageDirty`, or any node or pin mutator, and `T-WRITE-1` requires that
 * "nothing was written" be provable, so the absence is a property a test can check rather than a promise.
 *
 * THE DIALECT IS THE READERS' DIALECT, AND THERE IS NO SECOND ONE
 * ---------------------------------------------------------------
 * The text accepted here is exactly what `FTurboMCPGraphText` prints - S-expressions, a node per form,
 * `(<TypeId> ; <node id> ; class <Class>` then one indented line per named pin. Parsing a SECOND dialect
 * would be the failure `03_TOOLS.md` names first under "Rules for all write entry points": "Accepts
 * exactly the graph text format that the readers print. No second dialect."
 *
 * WHAT IT REPORTS, each with LINE AND COLUMN, because a caller fixing text needs to know where:
 *
 *  - `node-type`     a type ID that resolves to nothing, with the CLOSEST REAL IDS;
 *  - `pin-name`      a pin that the resolved node does not have, with THE REAL NAMES;
 *  - `type-mismatch` a literal whose type cannot be the pin's type, naming BOTH SIDES;
 *  - `refused`       a construct this tool will not let a writer attempt, and WHY.
 *
 * WHY THE CLOSEST MATCHES COME FROM `P-NODE-TYPE` AND NOT FROM A NEW MATCHER
 * -------------------------------------------------------------------------
 * `03_TOOLS.md` names the `P-NODE-TYPE` matcher for this job. Writing a second matcher here would make
 * `P-GRAPH-CHECK` and `P-NODE-TYPE` disagree about what "closest" means, which is the same
 * two-tools-one-format bug this project has recorded for the graph text and the node ID.
 */
class FTurboMCPGraphCheck
{
public:
	/** What one finding is about. Reported as a word so a caller can parse it rather than read prose. */
	enum class EFindingKind : uint8
	{
		/** A node type ID that resolves to nothing. */
		NodeType,
		/** A pin name the resolved node does not have. */
		PinName,
		/** A literal that cannot be the pin's type. */
		TypeMismatch,
		/** A construct this tool will not let a writer attempt. */
		Refused,
		/**
		 * A literal this checker could not judge, with the reason.
		 *
		 * NOT A FAULT IN THE TEXT. It exists because the alternative is silence, and silence is
		 * indistinguishable from agreement: a caller reading "0 findings" cannot tell whether every
		 * literal was checked or none was. `P1` requires an empty answer to come with its reason, and
		 * this is that reason made countable, so the suite can assert that the checker JUDGED what it
		 * was supposed to judge rather than merely that it complained about nothing.
		 */
		Unjudged,
	};

	/** One thing wrong with the text, at a place in it. */
	struct FFinding
	{
		/** What it is about, so a caller can filter without matching prose. */
		EFindingKind Kind = EFindingKind::NodeType;

		/** 1-based line in the submitted text. */
		int32 Line = 0;

		/** 1-based column in that line. */
		int32 Column = 0;

		/** The word for `Kind`, for the report. One of `node-type`, `pin-name`, `type-mismatch`, `refused`. */
		FString KindWord;

		/** What was written, quoted exactly as the caller wrote it. */
		FString Written;

		/** One sentence saying what is wrong. */
		FString Message;

		/**
		 * The real answers, closest first.
		 *
		 * For a node type these are real type IDs from the `P-NODE-TYPE` matcher; for a pin name, the
		 * pins the node really has. An empty list means there is nothing to suggest, and the report says
		 * so rather than leaving a bare list.
		 */
		TArray<FString> Suggestions;
	};

	/** The parsed and checked submission. */
	struct FPlan
	{
		FString Asset;
		FString GraphName;

		/** Every node form found in the text, in the order it appears. */
		int32 NodeFormsFound = 0;

		/** Every pin line found, so a caller can see how much of the text was understood. */
		int32 PinLinesFound = 0;

		/** What is wrong, in text order, so a caller fixes the first thing first. */
		TArray<FFinding> Findings;

		/** True when the text would be accepted: no findings at all. */
		bool bAcceptable() const { return Findings.Num() == 0; }

		/** How many findings of one kind. Parsed by the suite rather than counted from prose. */
		int32 CountOf(EFindingKind InKind) const;
	};

	/**
	 * Parses, resolves and checks the text against a live graph. Writes nothing.
	 *
	 * @param InAsset      The asset the text is meant for, in any path form the resolver accepts.
	 * @param InGraphName  The graph the text is meant for.
	 * @param InText       The graph text, exactly as a reader prints it.
	 * @param OutPlan      Filled on success.
	 * @param OutError     A complete error line when the GRAPH cannot be read at all. An empty string
	 *                     does NOT mean the text is acceptable: that is what `OutPlan.Findings` says,
	 *                     and the two are deliberately separate, because "I could not look" and "I
	 *                     looked and it is wrong" are different answers that must not be conflated.
	 */
	static bool Plan(const FString& InAsset, const FString& InGraphName, const FString& InText,
		FPlan& OutPlan, FString& OutError);

	/** The word for a finding kind, as the report and the suite spell it. */
	static FString WordFor(EFindingKind InKind);

	/** The findings, one per line, each with its kind, line and column. Never empty. */
	static FString Render(const FPlan& InPlan);

	/**
	 * Locates a node form's type ID and the real node it names, for a caller that wants to resolve.
	 *
	 * Exposed because a write tool takes the same text and must reach the SAME decision this checker
	 * reached; re-deriving it would be the two-implementations bug this project keeps recording.
	 *
	 * @param InGraph    The graph to resolve against.
	 * @param InTypeId   The type ID as written.
	 * @param OutSuggestions  Real type IDs, closest first, filled when nothing resolves.
	 * @return The node the type ID names, or null.
	 */
	static const class UEdGraphNode* ResolveTypeId(const UEdGraph* InGraph, const FString& InTypeId,
		TArray<FString>& OutSuggestions);
};
