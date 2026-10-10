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

/**
 * The ONE lexer and line reader for the graph text dialect, shared by the checker and every writer.
 *
 * WHY THIS EXISTS, AND WHY IT IS NOT A SECOND DIALECT
 * --------------------------------------------------
 * `03_TOOLS.md` puts one rule first among the write-entry-point rules: "Accepts exactly the graph text
 * format that the readers print. No second dialect." `FTurboMCPGraphText::Render` is the only thing that
 * PRODUCES that format, and until now the only thing that READ it was `FTurboMCPGraphCheck::Plan`, whose
 * lexing lived in an anonymous namespace inside its own .cpp.
 *
 * That was fine while there was exactly one reader. `P/E-GRAPH-APPEND` is the first WRITER, and a writer
 * that lexes the text for itself would be the second reader - and TWO READERS IS EXACTLY HOW A SECOND
 * DIALECT APPEARS. The project has recorded this shape twice already: two tools disagreeing about the graph
 * text, and two tools disagreeing about the node ID. This header is the answer to both: the rules for what a
 * line is, where a node form starts and what a pin line looks like live in ONE place, with ONE column
 * convention, and a reader that disagrees with a writer becomes impossible rather than merely discouraged.
 *
 * WHAT IS SHARED AND WHAT IS NOT
 * ------------------------------
 * SHARED: the line reader, the comment rule, the head-of-form rule, the pin-line rule, the literal
 * classifiers, and the one fact both halves need - that a line and a column are 1-BASED, because every
 * editor, compiler and log a caller has open uses that, and a 0-based report once cost a caller a whole
 * line of confusion.
 *
 * NOT SHARED, DELIBERATELY: what a form MEANS. Resolving a type ID against a live graph, judging whether a
 * literal can be a pin's type, and the closest-match suggestions all stay with the caller, because the
 * checker judges and the writer builds and those are different questions about the same text.
 */
namespace TurboMCPGraphTextLex
{
	/**
	 * One line of submitted text, with the number a reader starts at.
	 *
	 * A LINE AND A COLUMN ARE 1-BASED because that is what every editor, compiler and log the caller has
	 * open uses. An earlier format in this project reported 0-based indexes and the cost was a caller
	 * looking one line above the fault.
	 */
	struct FLine
	{
		FString Text;
		int32 Number = 0;
	};

	/**
	 * One node form: its head type ID and the pin lines written under it.
	 *
	 * THE STRUCTURE `FTurboMCPGraphCheck::FPlan` DID NOT RETAIN, and the reason a writer could not reuse
	 * the checker's parse. The checker counted forms and forgot them; this keeps them, so a writer can
	 * create a node for each one and set its pins without lexing the text a second time.
	 */
	struct FPinForm
	{
		/** The pin's name, exactly as written. */
		FString Name;

		/** The literal or link written after the name, exactly as written, or empty when none was given. */
		FString Value;

		/** 1-based line the pin was written on. */
		int32 Line = 0;

		/** 1-based column the pin NAME starts at. */
		int32 Column = 0;
	};

	struct FNodeForm
	{
		/** The type ID at the head, e.g. `Development|PrintString`. */
		FString TypeId;

		/**
		 * The node ID the form carries, or empty.
		 *
		 * The format prints a node's ID in the first comment line of its form, which is what lets a writer
		 * LINK to an existing node rather than creating a duplicate of it.
		 */
		FString NodeId;

		/** The class named after `class`, or empty. Printed at `EDetail::Full` only. */
		FString ClassName;

		/** 1-based line the form starts on. */
		int32 Line = 0;

		/** 1-based column the type ID starts at. */
		int32 Column = 0;

		/** Every pin written under this form, in the order written. */
		TArray<FPinForm> Pins;
	};

	/** Splits text on newlines, keeping every line including blanks and stripping carriage returns. */
	TArray<FLine> SplitLines(const FString& InText);

	/** True when the whole line is a comment, which the format allows anywhere. */
	bool IsCommentLine(const FLine& InLine);

	/**
	 * The type ID at the start of a node form, with the column it starts at.
	 *
	 * A node form is `(<TypeId>` at the head of a line. This deliberately does NOT accept a form that merely
	 * contains a parenthesis, because the format prints one node per line and the value nodes inside a pin
	 * are introduced by a SPACE followed by `(`. Recognising only a head-of-line form is what keeps
	 * `(Math|Float|float>float` after `Condition ` from being mistaken for a statement rather than a value -
	 * and it is also what makes the column meaningful. `(-> target)` is skipped: it is a link, not a node.
	 */
	bool ExtractHeadTypeId(const FString& InLine, FString& OutTypeId, int32& OutColumn, int32& OutAfter);

	/**
	 * One pin line: its name and the value written after it.
	 *
	 * Returns false when the line is not a pin. `OutColumn` is the 1-based column the NAME starts at, which
	 * is the column a caller needs: a fault in a pin is a fault in its name, not in its indent.
	 */
	bool ParsePinLine(const FString& InLine, FString& OutName, FString& OutValue, int32& OutColumn);

	/** True when a value is written as a quoted string, which the writer does for text and for structs. */
	bool IsQuoted(const FString& InValue);

	/** True when the value is a plain number the writer would print for a numeric pin. */
	bool LooksNumeric(const FString& InValue);

	/** True when the value is a `true` or `false` the writer would print for a boolean pin. */
	bool LooksBoolean(const FString& InValue);

	/**
	 * A wire written in a pin's value position, as `(-> Node:Pin)`.
	 *
	 * THE FORMAT PRINTS A WIRE THIS WAY AND THIS IS THE ONLY PLACE THAT KNOWS SO. It used to live as a local
	 * helper in the append tool, which is precisely the shape the spec's first write rule forbids: a second
	 * reader of the dialect is how a second dialect appears, and this project has recorded two-tools-one-format
	 * bugs twice already. The checker, the writer and any later PATCH all ask THIS function.
	 */
	struct FLinkForm
	{
		/** The node the wire starts at, as written: a node id, or a form this same text creates. */
		FString FromNode;

		/** The pin on that node, as written. */
		FString FromPin;
	};

	/** True when a pin value is written as a link rather than as a literal. */
	bool ValueIsALink(const FString& InValue);

	/**
	 * Reads `(-> Node:Pin)` into its two halves.
	 *
	 * Returns FALSE when the value is a link that cannot be split, which is a fault worth naming rather than
	 * silently treating as a literal: writing `(-> Foo)` into a pin as text is exactly the failure this
	 * recognition exists to prevent. When it returns false, `OutReason` says what was wrong with the value.
	 */
	bool ParseLinkValue(const FString& InValue, FLinkForm& OutLink, FString& OutReason);

	/**
	 * The node ID carried by a comment line, or empty.
	 *
	 * The format puts the node's ID in the first comment inside its form. Recognising the long form
	 * `Asset.Object:Graph:Key` is what lets a writer match a submitted form to the node it already is
	 * rather than creating a second one beside it.
	 */
	FString NodeIdFromComment(const FString& InComment);
}
