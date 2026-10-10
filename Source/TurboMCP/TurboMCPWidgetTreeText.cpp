// Copyright (c) 2026 Florian ESCHALIER (FloStyle)
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "TurboMCPWidgetTreeText.h"

#include "TurboMCPGraphTextLex.h"

namespace
{
	/** The top level form name this dialect owns. */
	const TCHAR* TreeFormName = TEXT("widget-tree");

	/**
	 * The clause and keyword names, as constants rather than inline literals, because a reader and a writer
	 * that disagree about a spelling are a second dialect with one typo between them.
	 */
	const TCHAR* ClauseWidget = TEXT("widget");
	const TCHAR* ClauseSlot = TEXT("slot");
	const TCHAR* ClauseProperty = TEXT("property");
	const TCHAR* KeywordClass = TEXT(":class");
	const TCHAR* KeywordPath = TEXT(":path");
	const TCHAR* KeywordVariable = TEXT(":variable");

	/** One value token, with a flag for whether it was written as a quoted string. */
	struct FToken
	{
		FString Text;
		bool bQuoted = false;
	};

	/**
	 * A scanner over the submitted text.
	 *
	 * WHY A SCANNER AND NOT A TOKEN ARRAY: this dialect's forms nest to arbitrary depth and are short, and the
	 * faults worth naming are all POSITIONAL - "unclosed parenthesis at line 12", "expected a class after
	 * `:class`". A scanner can say where it stopped; a pre-tokenised array has to carry a position on every
	 * token to say the same thing. The shared lexer already answers the questions that must be shared (what a
	 * comment line is, how lines split, the literal classifiers); this answers what a tree form MEANS, which
	 * `D-48` keeps here.
	 */
	struct FScanner
	{
		FString Text;
		int32 Pos = 0;
		int32 Line = 1;

		explicit FScanner(const FString& InText)
			: Text(InText)
		{
		}

		bool AtEnd() const { return Pos >= Text.Len(); }
		TCHAR Peek() const { return AtEnd() ? TEXT('\0') : Text[Pos]; }

		TCHAR Take()
		{
			if (AtEnd())
			{
				return TEXT('\0');
			}
			const TCHAR C = Text[Pos++];
			if (C == TEXT('\n'))
			{
				++Line;
			}
			return C;
		}

		void SkipSpace()
		{
			while (!AtEnd() && (Peek() == TEXT(' ') || Peek() == TEXT('\t') || Peek() == TEXT('\n') || Peek() == TEXT('\r')))
			{
				Take();
			}
		}

		void SkipLine()
		{
			while (!AtEnd() && Peek() != TEXT('\n'))
			{
				Take();
			}
		}

		/** A bare token: up to whitespace, a closing parenthesis or a comment. */
		FString TakeToken()
		{
			FString Out;
			while (!AtEnd())
			{
				const TCHAR C = Peek();
				if (C == TEXT(' ') || C == TEXT('\t') || C == TEXT('\n') || C == TEXT('\r')
					|| C == TEXT(')') || C == TEXT(';') || C == TEXT('('))
				{
					break;
				}
				Out.AppendChar(Take());
			}
			return Out;
		}
	};

	using TurboMCPWidgetTreeText::FAssignment;
	using TurboMCPWidgetTreeText::FNode;
	using TurboMCPWidgetTreeText::FTree;

	/** `Root` when the parent path is empty, otherwise `Parent/Child`. */
	void AppendPathSegment(const FString& InParentPath, const FString& InName, FString& OutPath)
	{
		OutPath = InParentPath.IsEmpty() ? InName : (InParentPath + TEXT("/") + InName);
	}

	/** Writes a value, quoting it when the dialect could not read it back bare. */
	FString WriteValue(const FString& InValue)
	{
		if (InValue.IsEmpty())
		{
			return TEXT("\"\"");
		}
		if (TurboMCPWidgetTreeText::IsPlainName(InValue))
		{
			return InValue;
		}

		// ESCAPE THE CHARACTERS THAT WOULD END THE STRING EARLY OR START A COMMENT INSIDE IT. `;` matters as
		// much as the quote: an unescaped semicolon inside a quoted value would still be read back as the start
		// of a comment by a reader that applied the comment rule before the quoting rule.
		FString Escaped = InValue;
		Escaped.ReplaceInline(TEXT("\\"), TEXT("\\\\"));
		Escaped.ReplaceInline(TEXT("\""), TEXT("\\\""));
		Escaped.ReplaceInline(TEXT(";"), TEXT("\\;"));
		return FString::Printf(TEXT("\"%s\""), *Escaped);
	}

	/** Reads a value token, resolving the escapes `WriteValue` produces. */
	bool ReadValue(FScanner& In, FToken& OutToken, FString& OutReason)
	{
		In.SkipSpace();
		if (In.AtEnd())
		{
			OutReason = TEXT("expected a value and the text ended");
			return false;
		}

		if (In.Peek() == TEXT('"'))
		{
			In.Take();
			FString Value;
			bool bClosed = false;
			while (!In.AtEnd())
			{
				const TCHAR C = In.Take();
				if (C == TEXT('\\'))
				{
					// AN ESCAPED CHARACTER IS TAKEN LITERALLY, which is what makes `\;` not begin a comment.
					if (!In.AtEnd())
					{
						Value.AppendChar(In.Take());
					}
					continue;
				}
				if (C == TEXT('"'))
				{
					bClosed = true;
					break;
				}
				Value.AppendChar(C);
			}
			if (!bClosed)
			{
				OutReason = FString::Printf(TEXT("line %d: a quoted value is never closed"), In.Line);
				return false;
			}
			OutToken.Text = Value;
			OutToken.bQuoted = true;
			return true;
		}

		const FString Value = In.TakeToken();
		if (Value.IsEmpty())
		{
			OutReason = FString::Printf(TEXT("line %d: expected a value"), In.Line);
			return false;
		}
		OutToken.Text = Value;
		OutToken.bQuoted = false;
		return true;
	}

	/** `name=value`, the form both `slot` and `property` clauses use. */
	bool ReadAssignment(FScanner& In, FAssignment& Out, FString& OutReason)
	{
		In.SkipSpace();
		const int32 AtLine = In.Line;

		const FString Name = In.TakeToken();
		if (Name.IsEmpty())
		{
			OutReason = FString::Printf(TEXT("line %d: expected a name before `=` in this clause"), AtLine);
			return false;
		}

		In.SkipSpace();
		if (In.AtEnd() || In.Peek() != TEXT('='))
		{
			OutReason = FString::Printf(
				TEXT("line %d: `%s` is written without `=`, so the dialect cannot tell a name from a value"),
				AtLine, *Name);
			return false;
		}
		In.Take();

		FToken Token;
		if (!ReadValue(In, Token, OutReason))
		{
			OutReason = FString::Printf(TEXT("line %d: `%s` has no value after `=` (%s)"), AtLine, *Name, *OutReason);
			return false;
		}

		Out.Name = Name;
		Out.Value = Token.Text;
		Out.Line = AtLine;
		return true;
	}

	bool ParseNode(FScanner& In, FNode& Out, FString& OutReason);

	/** Parses the body of `(slot ...)` or `(property ...)`, up to and including its close. */
	bool ParseAssignmentClause(FScanner& In, const TCHAR* InClauseName, TArray<FAssignment>& Out, FString& OutReason)
	{
		In.SkipSpace();
		while (!In.AtEnd() && In.Peek() != TEXT(')'))
		{
			// A COMMENT INSIDE A FORM IS ALLOWED - `04_FORMATS.md` section 8.1 inherits the graph text's
			// "comments anywhere" rule - and it ends the line's meaning without ending the clause.
			if (In.Peek() == TEXT(';'))
			{
				In.SkipLine();
				continue;
			}

			FAssignment Assignment;
			if (!ReadAssignment(In, Assignment, OutReason))
			{
				return false;
			}
			Out.Add(MoveTemp(Assignment));
			In.SkipSpace();
		}

		if (In.AtEnd())
		{
			OutReason = FString::Printf(TEXT("line %d: the `%s` clause is never closed"), In.Line, InClauseName);
			return false;
		}
		In.Take();
		return true;
	}

	bool ParseNode(FScanner& In, FNode& Out, FString& OutReason)
	{
		In.SkipSpace();
		Out.Line = In.Line;

		if (In.AtEnd() || In.Peek() != TEXT('('))
		{
			OutReason = FString::Printf(TEXT("line %d: expected `(widget ...`"), In.Line);
			return false;
		}
		In.Take();

		In.SkipSpace();
		if (In.TakeToken() != ClauseWidget)
		{
			OutReason = FString::Printf(TEXT("line %d: a child clause has to be `%s`"), Out.Line, ClauseWidget);
			return false;
		}

		In.SkipSpace();
		Out.Name = In.TakeToken();
		if (Out.Name.IsEmpty())
		{
			// THE NAME IS THE ONLY THING THAT IDENTIFIES A NODE, so a form without one is refused rather than
			// given a generated name: a generated name is a widget nothing can address afterwards.
			OutReason = FString::Printf(TEXT("line %d: this widget has no name, so nothing can address it"), Out.Line);
			return false;
		}

		bool bSawClass = false;
		while (true)
		{
			In.SkipSpace();
			if (In.AtEnd())
			{
				OutReason = FString::Printf(TEXT("line %d: `%s` is never closed"), Out.Line, *Out.Name);
				return false;
			}
			if (In.Peek() == TEXT(';'))
			{
				In.SkipLine();
				continue;
			}
			if (In.Peek() == TEXT(')'))
			{
				In.Take();
				break;
			}

			if (In.Peek() == TEXT('('))
			{
				In.Take();
				In.SkipSpace();
				const FString Clause = In.TakeToken();

				if (Clause == ClauseSlot)
				{
					if (!ParseAssignmentClause(In, ClauseSlot, Out.Slot, OutReason))
					{
						return false;
					}
				}
				else if (Clause == ClauseProperty)
				{
					if (!ParseAssignmentClause(In, ClauseProperty, Out.Properties, OutReason))
					{
						return false;
					}
				}
				else if (Clause == ClauseWidget)
				{
					// A CHILD. ITS POSITION IN THIS ARRAY IS ITS SLOT ORDER, which is the draw order, so it is
					// appended and never sorted - rule 8.2.3, and the reason this dialect exists at all.
					FNode Child;
					if (!ParseNode(In, Child, OutReason))
					{
						return false;
					}
					Out.Children.Add(MoveTemp(Child));
				}
				else
				{
					OutReason = FString::Printf(
						TEXT("line %d: `%s` is not a clause this dialect knows; they are `%s`, `%s` and a nested `%s`"),
						In.Line, *Clause, ClauseSlot, ClauseProperty, ClauseWidget);
					return false;
				}
				continue;
			}

			const FString Keyword = In.TakeToken();

			if (Keyword == KeywordClass)
			{
				FToken Token;
				if (!ReadValue(In, Token, OutReason))
				{
					OutReason = FString::Printf(TEXT("line %d: `%s` has no class after it"), Out.Line, *Out.Name);
					return false;
				}
				Out.ClassName = Token.Text;
				bSawClass = true;
			}
			else if (Keyword == KeywordPath)
			{
				FToken Token;
				if (!ReadValue(In, Token, OutReason))
				{
					OutReason = FString::Printf(TEXT("line %d: `%s` has no path after `%s`"), Out.Line, *Out.Name, KeywordPath);
					return false;
				}
				Out.StatedPath = Token.Text;
			}
			else if (Keyword == KeywordVariable)
			{
				Out.bIsVariable = true;
			}
			else
			{
				OutReason = FString::Printf(
					TEXT("line %d: `%s` is not a keyword this dialect knows; they are `%s`, `%s` and `%s`"),
					In.Line, *Keyword, KeywordClass, KeywordPath, KeywordVariable);
				return false;
			}
		}

		if (!bSawClass)
		{
			// RULE 8.2.5: THE CLASS IS WRITTEN ONCE, in the form `R-TYPE-FIND` returns, *"so the writer never
			// has to guess a class from a name"*. A form without one is refused rather than defaulted, because
			// a default would create a widget of a class nobody asked for.
			OutReason = FString::Printf(
				TEXT("line %d: `%s` has no `%s`, and the class is what the writer creates the widget from"),
				Out.Line, *Out.Name, KeywordClass);
			return false;
		}

		return true;
	}

	/**
	 * FILLS IN every node's derived `Path`, and checks any stated one against it.
	 *
	 * RULE 8.2.4 PUTS THE PATH IN THE TEXT, AND THIS IS WHY IT IS VERIFIED RATHER THAN TRUSTED: a submitted
	 * patch addresses widgets BY PATH, so a text whose paths disagree with its own nesting is a text whose
	 * author is addressing widgets that do not exist. Accepting it and silently correcting the path is the
	 * "the tool did what you did not ask" failure this project has recorded in several shapes; refusing it
	 * names the line and both paths instead.
	 *
	 * IT TAKES A MUTABLE NODE because filling in the derived path is the other half of the same walk: the value
	 * that is CHECKED and the value that later addresses are compared against have to be THE SAME VALUE, and
	 * computing it twice in two places is how the two would come to differ.
	 */
	bool FillAndCheckPaths(FNode& InOutNode, const FString& InParentPath, FString& OutReason)
	{
		AppendPathSegment(InParentPath, InOutNode.Name, InOutNode.Path);

		if (!InOutNode.StatedPath.IsEmpty() && InOutNode.StatedPath != InOutNode.Path)
		{
			OutReason = FString::Printf(
				TEXT("line %d: the widget `%s` states `:path %s`, but its nesting puts it at `%s`"),
				InOutNode.Line, *InOutNode.Name, *InOutNode.StatedPath, *InOutNode.Path);
			return false;
		}

		for (FNode& Child : InOutNode.Children)
		{
			if (!FillAndCheckPaths(Child, InOutNode.Path, OutReason))
			{
				return false;
			}
		}
		return true;
	}

	/** Emits one node and then its children IN THE ORDER HELD, which is the order they draw in. */
	void RenderNode(const FNode& InNode, const FString& InPad, FString& Out)
	{
		// EVERY LINE INSIDE A FORM IS INDENTED ONE LEVEL FROM THE FORM, and the indentation is a function of the
		// pad passed in rather than of a depth number - so a nested form cannot drift by a space at depth 6.
		const FString Inner = InPad + TEXT("  ");

		Out += FString::Printf(TEXT("(%s %s :class %s :path %s"),
			ClauseWidget, *InNode.Name, *InNode.ClassName, *InNode.Path);
		if (InNode.bIsVariable)
		{
			Out += FString::Printf(TEXT(" %s"), KeywordVariable);
		}

		// RULE 8.2.6 AND `D-50`: SLOT LAYOUT AND THE WIDGET'S OWN PROPERTIES ARE DIFFERENT CLAUSES, and which
		// one an assignment belongs to is part of the syntax rather than a convention. Emitting them together
		// would be the confusion the rule exists to prevent.
		if (InNode.Slot.Num() > 0)
		{
			Out += TEXT("\n") + Inner + FString::Printf(TEXT("(%s"), ClauseSlot);
			for (const FAssignment& Assignment : InNode.Slot)
			{
				Out += FString::Printf(TEXT(" %s=%s"), *Assignment.Name, *WriteValue(Assignment.Value));
			}
			Out += TEXT(")");
		}

		if (InNode.Properties.Num() > 0)
		{
			Out += TEXT("\n") + Inner + FString::Printf(TEXT("(%s"), ClauseProperty);
			for (const FAssignment& Assignment : InNode.Properties)
			{
				Out += FString::Printf(TEXT(" %s=%s"), *Assignment.Name, *WriteValue(Assignment.Value));
			}
			Out += TEXT(")");
		}

		// CHILDREN KEEP THEIR ORDER. This loop is the only thing that decides draw order on the way out, and it
		// walks the array as read: no sort, no grouping, no map.
		for (const FNode& Child : InNode.Children)
		{
			Out += TEXT("\n") + Inner;
			RenderNode(Child, Inner, Out);
		}

		Out += TEXT(")");
	}
}

bool TurboMCPWidgetTreeText::IsPlainName(const FString& InName)
{
	if (InName.IsEmpty())
	{
		return false;
	}

	// A NAME THAT NEEDS QUOTING IS STILL WRITABLE, so this is not a validity check - it tells the writer
	// whether the name can be emitted bare. A space, a parenthesis, a quote, a semicolon or a leading colon
	// would end the token early or start a comment, so those are the ones that have to be quoted.
	if (InName.StartsWith(TEXT(":")))
	{
		return false;
	}

	for (const TCHAR C : InName)
	{
		if (C == TEXT(' ') || C == TEXT('\t') || C == TEXT('\n') || C == TEXT('\r')
			|| C == TEXT('(') || C == TEXT(')') || C == TEXT('"') || C == TEXT(';'))
		{
			return false;
		}
	}
	return true;
}

bool TurboMCPWidgetTreeText::Parse(const FString& InText, FTree& OutTree, FString& OutError)
{
	OutTree = FTree();
	OutError.Reset();

	// THE LINE READER AND THE COMMENT RULE ARE THE SHARED LEXER'S, which is the part `D-48` means by "sharing
	// the shape rules" and the reason this file does not tokenise for itself.
	const TArray<TurboMCPGraphTextLex::FLine> Lines = TurboMCPGraphTextLex::SplitLines(InText);

	FScanner In(InText);
	FString Stripped;

	// A WHOLE-LINE COMMENT IS DROPPED HERE, IN ONE PLACE, using the shared lexer's own predicate rather than a
	// second definition of what a comment is. A trailing comment is handled by the scanner's value rule, which
	// ends a value at `;` - so this deliberately does NOT strip mid-line comments, because doing so would be the
	// second place that decides.
	for (const TurboMCPGraphTextLex::FLine& Line : Lines)
	{
		if (TurboMCPGraphTextLex::IsCommentLine(Line))
		{
			Stripped += TEXT("\n");
			continue;
		}
		// The line is re-emitted WITHOUT its trailing carriage return, because `SplitLines` already stripped
		// it and re-adding one would show up as a stray character in a quoted value.
		Stripped += Line.Text;
		Stripped += TEXT("\n");
	}

	In = FScanner(Stripped);

	while (true)
	{
		In.SkipSpace();
		if (In.AtEnd())
		{
			break;
		}
		if (In.Peek() == TEXT(';'))
		{
			In.SkipLine();
			continue;
		}
		if (In.Peek() != TEXT('('))
		{
			OutError = FString::Printf(
				TEXT("line %d: a top level form has to start with `(`, and this one starts with `%c`"),
				In.Line, In.Peek());
			return false;
		}

		const int32 FormStart = In.Pos;
		const int32 FormLine = In.Line;
		In.Take();
		In.SkipSpace();
		const FString FormName = In.TakeToken();

		if (FormName == TreeFormName)
		{
			if (OutTree.bHasTree)
			{
				OutError = FString::Printf(
					TEXT("line %d: the text has more than one `%s` form, and a Widget Blueprint has ONE root widget"),
					FormLine, TreeFormName);
				return false;
			}

			In.SkipSpace();
			if (In.AtEnd() || In.Peek() != TEXT('('))
			{
				OutError = FString::Printf(
					TEXT("line %d: `%s` is empty; a tree form contains the root widget"), In.Line, TreeFormName);
				return false;
			}

			FNode Root;
			if (!ParseNode(In, Root, OutError))
			{
				return false;
			}

			In.SkipSpace();
			if (In.AtEnd() || In.Peek() != TEXT(')'))
			{
				OutError = FString::Printf(
					TEXT("line %d: `%s` has more than one root widget; a Widget Blueprint has exactly one, so give the extra widgets a parent"),
					In.Line, TreeFormName);
				return false;
			}
			In.Take();

			// RULE 8.2.4, AT EVERY DEPTH RATHER THAN ONLY AT THE ROOT: the walk fills in each node's derived
			// `Path` and refuses any node whose stated `:path` disagrees with it. Checking only the root would
			// let a wrong path survive at depth 3, which is exactly where a path is hardest to get right by eye.
			if (!FillAndCheckPaths(Root, TEXT(""), OutError))
			{
				return false;
			}

			OutTree.bHasTree = true;
			OutTree.Root = MoveTemp(Root);
			continue;
		}

		// A TOP LEVEL FORM THIS DIALECT DOES NOT OWN, KEPT VERBATIM RATHER THAN DROPPED.
		//
		// RULE 8.2.2 says named animations are their own top level forms, and they belong to
		// `P/E-WIDGET-ANIM`. A `replace` that deleted every animation because this file did not recognise the
		// form would be the worst bug the dialect could have - the caller asked to rewrite a TREE and lost their
		// animations - so the text is carried through byte for byte and re-emitted on the way out.
		In.Pos = FormStart;
		In.Line = FormLine;

		int32 Depth = 0;
		bool bClosed = false;
		while (!In.AtEnd())
		{
			const TCHAR C = In.Take();
			if (C == TEXT('('))
			{
				++Depth;
			}
			else if (C == TEXT(')'))
			{
				--Depth;
				if (Depth == 0)
				{
					bClosed = true;
					break;
				}
			}
		}
		if (!bClosed)
		{
			OutError = FString::Printf(TEXT("line %d: the `%s` form is never closed"), FormLine, *FormName);
			return false;
		}

		OutTree.OtherForms.Add(In.Text.Mid(FormStart, In.Pos - FormStart));
	}

	if (!OutTree.bHasTree && OutTree.OtherForms.Num() == 0)
	{
		OutError = TEXT("the text has no `widget-tree` form, so there is no tree to read");
		return false;
	}

	return true;
}

FString TurboMCPWidgetTreeText::Render(const FTree& InTree, int32 InIndent)
{
	FString Out;
	const FString Pad = FString::ChrN(InIndent, TEXT(' '));

	if (InTree.bHasTree)
	{
		Out += Pad + FString::Printf(TEXT("(%s\n"), TreeFormName);
		RenderNode(InTree.Root, Pad, Out);
		Out += TEXT(")");
	}

	// THE FORMS THIS DIALECT DOES NOT OWN ARE RE-EMITTED VERBATIM. Dropping them would be a `replace` that
	// deleted every animation the caller had, which is the worst bug this dialect could have.
	for (const FString& Other : InTree.OtherForms)
	{
		if (!Out.IsEmpty())
		{
			Out += TEXT("\n");
		}
		Out += Pad + Other;
	}

	return Out;
}
