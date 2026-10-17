// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once

#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "CoreTypes.h"

/**
 * THE WIDGET TREE TEXT DIALECT: one parser, one writer, and the tree they agree about.
 *
 * WHY THIS IS A SECOND DIALECT AND NOT A REUSE OF THE GRAPH TEXT - `D-48`, and `04_FORMATS.md` section 8.1
 * -------------------------------------------------------------------------------------------------------
 * A widget tree is NOT A GRAPH. IT IS AN ORDERED HIERARCHY WHERE THE ORDER OF CHILDREN IS THE ORDER THEY DRAW
 * IN, AND EVERY CHILD CARRIES LAYOUT ON ITS SLOT, WHICH LIVES ON THE SLOT TYPE AND NOT ON THE WIDGET. A graph
 * text cannot express either fact.
 *
 *     "A dialect that cannot express order would silently reorder children on a round trip."
 *
 * SO WHAT IS SHARED WITH THE GRAPH TEXT IS THE LEXER AND THE SHAPE RULES, AND NOTHING ELSE - `D-15` and `D-48`
 * in one sentence: *"a second format, sharing the shape rules of the graph text and nothing else."* The
 * tokenizer is `TurboMCPGraphTextLex` (line splitting, the comment rule, the head-of-form rule, the pin rule,
 * the literal classifiers), BECAUSE A SECOND LEXER IS HOW A SECOND DIALECT APPEARS - the rule the graph side
 * settled and this file inherits rather than re-argues. What is NOT shared is what a form MEANS, because a tree
 * form and a node form mean different things.
 *
 * THE SYNTAX, WHICH `04_FORMATS.md` section 8.1 DELEGATES TO THIS FILE ("You design the exact syntax")
 * -------------------------------------------------------------------------------------------------------
 * S-expressions, `;` comments, one top level form per thing that can be edited on its own:
 *
 *     (widget-tree
 *       (widget Root :class Overlay :path Root
 *         (slot padding=0,0,0,0)
 *         (widget Card :class Border :path Root/Card
 *           (slot padding=4,4,4,4)
 *           (widget PriceText :class TextBlock :path Root/Card/PriceText :variable
 *             (property Text="42")))))
 *
 * CHILDREN APPEAR IN PANEL SLOT ORDER, TOP TO BOTTOM, AND THAT ORDER IS THE MEANING - rule 8.2.3. The parser
 * KEEPS the order it read and the writer EMITS the order it holds, so a round trip cannot reorder a tree.
 *
 * SLOT LAYOUT IS ITS OWN CLAUSE - rule 8.2.6 and `D-50`. `(slot ...)` and `(property ...)` are DIFFERENT
 * FORMS ON PURPOSE: the two live on different objects, and which clause an assignment belongs to is PART OF
 * THE SYNTAX rather than a convention a reader has to know. `P/E-WIDGET-SET` owns the full set of both; this
 * dialect carries whatever it is given and does not decide what is legal on which class.
 *
 * THE NAME IS A CLAUSE OF ITS OWN, AND THE PATH IS DERIVED FROM IT - see `FNode::Path`. A subtree whose NAMES
 * and ORDER are identical before and after an operation has the same PATHS, which is what makes a rename
 * detectable as a refusal rather than a silent change (`D-49`).
 */
namespace TurboMCPWidgetTreeText
{
	/** One property or slot assignment, as written: a name and its value. */
	struct FAssignment
	{
		FString Name;
		FString Value;

		/** 1-based line it was written on, so a fault can be reported where the author wrote it. */
		int32 Line = 0;
	};

	/**
	 * One widget, with its children IN SLOT ORDER.
	 *
	 * `Path` IS DERIVED RATHER THAN STORED BY THE PARSER, and that is deliberate: the path is a FUNCTION of the
	 * names above a node and their order, so deriving it means a parser cannot produce a tree whose paths
	 * disagree with its own structure. It is written into the text (rule 8.2.4) and CHECKED on the way back in
	 * rather than trusted, because a text that carries a wrong path is a text whose author believes something
	 * false about the tree.
	 */
	struct FNode
	{
		/** The widget's name. THE ONLY THING A NODE IS IDENTIFIED BY, and the reason a rename is a refusal. */
		FString Name;

		/** The widget's class, in the form `R-TYPE-FIND` returns - rule 8.2.5. */
		FString ClassName;

		/** True when the form carried `:variable`. */
		bool bIsVariable = false;

		/**
		 * The path clause as written, or empty when the form carried none.
		 *
		 * KEPT SO THE PARSER CAN COMPARE IT WITH THE DERIVED ONE. A mismatch is reported rather than ignored:
		 * a text whose path clause disagrees with its own nesting would otherwise be accepted and then
		 * silently corrected, which is the "the tool did something you did not ask for" failure.
		 */
		FString StatedPath;

		/**
		 * The path this node's position in the tree implies, FILLED IN BY THE PARSER for every node.
		 *
		 * IT IS DERIVED RATHER THAN AUTHORED, which is what makes it trustworthy: a path an author wrote can
		 * disagree with the tree, and a path the parser computed cannot. `CheckPaths` verifies that any
		 * `StatedPath` matches the derived one, so the two can never silently differ.
		 */
		FString Path;

		/** `(slot ...)` assignments, in the order written. */
		TArray<FAssignment> Slot;

		/** `(property ...)` assignments, in the order written. */
		TArray<FAssignment> Properties;

		/** Children IN SLOT ORDER. Order is meaning; reordering these changes what draws on top. */
		TArray<FNode> Children;

		/** 1-based line the form starts on. */
		int32 Line = 0;
	};

	/** A whole tree text: the root form, plus any other top level forms the text carried. */
	struct FTree
	{
		/** The root widget, when the text had a `widget-tree` form. */
		bool bHasTree = false;
		FNode Root;

		/**
		 * Top level forms this dialect does NOT understand, kept verbatim.
		 *
		 * RULE 8.2.2 SAYS NAMED ANIMATIONS ARE THEIR OWN TOP LEVEL FORMS. They are `P/E-WIDGET-ANIM`'s to read
		 * and write, not this file's, SO THIS FILE MUST NOT DROP THEM - a `replace` that silently deleted every
		 * animation because it did not recognise the form would be the worst bug this dialect could have. They
		 * are carried through byte for byte and re-emitted on the way out.
		 */
		TArray<FString> OtherForms;
	};

	/**
	 * Parses a tree text. Returns false and fills `OutError` with the REASON AND ITS LINE on a fault.
	 *
	 * REFUSES RATHER THAN GUESSES, which is the whole point of a parser over a scanner: an unbalanced
	 * parenthesis, a clause that is neither `slot` nor `property`, a widget form with no `:class`, or a stated
	 * path that disagrees with the nesting are all named faults rather than things to interpret generously.
	 */
	bool Parse(const FString& InText, FTree& OutTree, FString& OutError);

	/**
	 * The tree, rendered in the dialect `Parse` accepts.
	 *
	 * `InIndent` IS THE COLUMN THE TEXT STARTS AT, because `R-BP-LOAD` prints this inside its own report and a
	 * dialect that could only start at column zero would have to be reindented by its caller - which is how two
	 * renderings of one format appear.
	 */
	FString Render(const FTree& InTree, int32 InIndent = 0);

	/** True when `InName` is a name this dialect can write and read back without quoting. */
	bool IsPlainName(const FString& InName);
}

/*
 * *** WHAT IS NOT HERE YET, STATED PLAINLY RATHER THAN LEFT TO BE DISCOVERED. ***
 *
 * THIS FILE IS THE TEXT LAYER AND ONLY THE TEXT LAYER: the dialect, its parser and its writer, with the three
 * rules that a text layer can enforce on its own - ORDER IS MEANING (8.2.3), THE PATH IS VERIFIED RATHER THAN
 * TRUSTED (8.2.4), and A CLASS IS REQUIRED (8.2.5) - plus the clause split that `D-50` demands and the verbatim
 * carriage of forms this dialect does not own, which is what stops a `replace` from silently deleting the
 * caller's animations.
 *
 * NOT IMPLEMENTED, AND THEREFORE DELIBERATELY NOT DECLARED:
 *   - the walk of a live `UBaseWidgetBlueprint` into an `FTree` (rule 8.2.1's "the reader and the writer share
 *     one code path", which needs `FTurboMCPWidgetTree::Collect` and a real UMG prerequisite check);
 *   - `append`, `replace` and `patch` as `P/E-WIDGET-TREE` tools, which need that walk plus the transaction,
 *     the compile and the read-back;
 *   - the refusal of a rename (`D-49`) and of an over-full panel at apply time, both of which are comparisons
 *     against a LIVE tree and cannot be made against text alone.
 *
 * A DECLARATION WITHOUT A DEFINITION WOULD COMPILE AND THEN FAIL TO LINK, so the honest shape is a header that
 * describes what exists. The next step is the walk, and it is the only thing standing between this dialect and
 * the tool: `FTurboMCPWidgetTree::Collect` already enumerates a tree depth-first with names, classes, depths,
 * visibility and bound events, and `UPanelWidget::GetChildrenCount` / `GetChildAt` (`PanelWidget.h:28,36`) give
 * the SLOT ORDER that `Collect` flattens away - which is precisely the fact rule 8.2.3 says a graph text cannot
 * express and this dialect exists to carry.
 */
