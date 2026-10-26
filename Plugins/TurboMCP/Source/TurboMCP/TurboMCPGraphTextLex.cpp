// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "TurboMCPGraphTextLex.h"
#include "TurboMCPNodeId.h"

#include "Misc/Char.h"

namespace TurboMCPGraphTextLex
{
	TArray<FLine> SplitLines(const FString& InText)
	{
		TArray<FLine> Out;
		FString Current;
		int32 Number = 1;
		for (int32 Index = 0; Index < InText.Len(); ++Index)
		{
			const TCHAR Ch = InText[Index];
			if (Ch == TEXT('\n'))
			{
				Out.Add(FLine{ Current, Number });
				Current.Empty();
				++Number;
				continue;
			}
			// A carriage return is part of the line ending, never of the content, so a file saved on
			// Windows and one saved on Linux produce the SAME findings rather than differing by an
			// invisible character at the end of every line.
			if (Ch == TEXT('\r'))
			{
				continue;
			}
			Current.AppendChar(Ch);
		}
		Out.Add(FLine{ Current, Number });
		return Out;
	}

	bool IsCommentLine(const FLine& InLine)
	{
		return InLine.Text.TrimStartAndEnd().StartsWith(TEXT(";"));
	}

	bool ExtractHeadTypeId(const FString& InLine, FString& OutTypeId, int32& OutColumn, int32& OutAfter)
	{
		OutTypeId.Empty();
		OutColumn = 0;
		OutAfter = 0;

		int32 Index = 0;
		while (Index < InLine.Len() && FChar::IsWhitespace(InLine[Index]))
		{
			++Index;
		}
		if (Index >= InLine.Len() || InLine[Index] != TEXT('('))
		{
			return false;
		}

		const int32 Open = Index;
		++Index;
		// Type IDs are opaque identifiers. Quotes let names contain spaces without
		// confusing the identifier with the pin fields that follow it.
		if (Index < InLine.Len() && InLine[Index] == TEXT('"'))
		{
			++Index;
			FString QuotedHead;
			bool bEscaped = false;
			for (; Index < InLine.Len(); ++Index)
			{
				const TCHAR Ch = InLine[Index];
				if (bEscaped) { QuotedHead.AppendChar(Ch); bEscaped = false; continue; }
				if (Ch == TEXT('\\')) { bEscaped = true; continue; }
				if (Ch == TEXT('"'))
				{
					++Index;
					if (QuotedHead.IsEmpty() || (Index < InLine.Len()
						&& !FChar::IsWhitespace(InLine[Index]) && InLine[Index] != TEXT(')') && InLine[Index] != TEXT(';')))
						return false;
					OutTypeId = QuotedHead;
					OutColumn = Open + 1;
					OutAfter = Index;
					return true;
				}
				QuotedHead.AppendChar(Ch);
			}
			return false;
		}
		const int32 Start = Index;
		int32 ParenthesisDepth = 1;
		while (Index < InLine.Len() && !FChar::IsWhitespace(InLine[Index]))
		{
			if (InLine[Index] == TEXT('('))
			{
				++ParenthesisDepth;
			}
			else if (InLine[Index] == TEXT(')'))
			{
				--ParenthesisDepth;
				if (ParenthesisDepth == 0)
				{
					break;
				}
			}
			++Index;
		}

		const FString Head = InLine.Mid(Start, Index - Start);
		if (Head.IsEmpty())
		{
			return false;
		}

		// A link form and the two continuation forms the writer emits are not nodes.
		if (Head == TEXT("->") || Head.StartsWith(TEXT(":")))
		{
			return false;
		}

		OutTypeId = Head;
		OutColumn = Open + 1;
		OutAfter = Index;
		return true;
	}

	bool ParsePinLine(const FString& InLine, FString& OutName, FString& OutValue, int32& OutColumn)
	{
		OutName.Empty();
		OutValue.Empty();
		OutColumn = 0;

		// Drop the trailing editing comment (`; in 3`, `; out 0 then -> 1 link(s)`), which is never part
		// of the value. The writer only ever emits ONE `;` per pin line and it is always last.
		FString Body = InLine;
		const int32 Comment = Body.Find(TEXT(";"), ESearchCase::CaseSensitive, ESearchDir::FromStart);
		if (Comment != INDEX_NONE)
		{
			// A `;` inside a quoted value is content, so only a `;` OUTSIDE quotes ends the body.
			bool bInQuotes = false;
			int32 Cut = INDEX_NONE;
			for (int32 Index = 0; Index < Body.Len(); ++Index)
			{
				if (Body[Index] == TEXT('"'))
				{
					bInQuotes = !bInQuotes;
					continue;
				}
				if (!bInQuotes && Body[Index] == TEXT(';'))
				{
					Cut = Index;
					break;
				}
			}
			if (Cut != INDEX_NONE)
			{
				Body = Body.Left(Cut);
			}
		}

		int32 Index = 0;
		while (Index < Body.Len() && FChar::IsWhitespace(Body[Index]))
		{
			++Index;
		}
		if (Index >= Body.Len())
		{
			return false;
		}

        // A delimiter-only line closes forms; it is never a pin assignment.
        // Balance validation belongs to GraphCheck. Ignore whitespace between
        // closing delimiters too, including a trailing comment already stripped.
        bool bOnlyClosers = true;
        for (int32 Cursor = Index; Cursor < Body.Len(); ++Cursor)
            if (Body[Cursor] != TEXT(')') && !FChar::IsWhitespace(Body[Cursor]))
            { bOnlyClosers = false; break; }
        if (bOnlyClosers) return false;

		OutColumn = Index + 1;

		// A quoted pin name, which the writer uses when the real name holds a space.
		if (Body[Index] == TEXT('"'))
		{
			const int32 Close = Body.Find(TEXT("\""), ESearchCase::CaseSensitive, ESearchDir::FromStart,
				Index + 1);
			if (Close == INDEX_NONE)
			{
				return false;
			}
			OutName = Body.Mid(Index + 1, Close - Index - 1);
			Index = Close + 1;
		}
		else
		{
			const int32 Start = Index;
			while (Index < Body.Len() && !FChar::IsWhitespace(Body[Index]))
			{
				++Index;
			}
			OutName = Body.Mid(Start, Index - Start);
		}

		if (OutName.IsEmpty())
		{
			return false;
		}

		while (Index < Body.Len() && FChar::IsWhitespace(Body[Index]))
		{
			++Index;
		}
		OutValue = Body.Mid(Index).TrimStartAndEnd();

		// THE FORM'S CLOSING PARENTHESIS IS GLUED TO THE LAST PIN'S VALUE, AND MEASURED, THIS COST A
		// WRONG ANSWER RATHER THAN A CRASH.
		//
		// The writer emits `... InString "hi" ; in 3)` when a pin is last, so the value arrives as
		// `"hi")` - and passing that straight on made a genuine mismatch (`InString 42.0`) read as an
		// UNJUDGED bare name, `42.0)`, because the value no longer looked like a number. The checker was
		// not wrong about the text being odd; it was judging a value the caller never wrote.
		//
		// Every `)` that closes the enclosing form or a nested one is TRAILING, because the format puts
		// one node per line and a nested value node opens its own `(` earlier in the same line. A `)`
		// inside a QUOTED value is content and is protected by the quote scan above, which is why this
		// only touches the tail.
		while (OutValue.EndsWith(TEXT(")")))
		{
			OutValue.LeftChopInline(1);
			OutValue.TrimEndInline();
		}

		return true;
	}

	bool IsQuoted(const FString& InValue)
	{
		return InValue.Len() >= 2 && InValue[0] == TEXT('"') && InValue[InValue.Len() - 1] == TEXT('"');
	}

	// ---- LINKS, `(-> Node:Pin)` -------------------------------------------------------------------------
	//
	// THE ONE PLACE THAT KNOWS THE WIRE SYNTAX. The append tool used to test `StartsWith("(")` locally and
	// refuse, which meant the dialect's link rule lived in a tool rather than in the format's own reader.

	bool ValueIsALink(const FString& InValue)
	{
		return InValue.StartsWith(TEXT("("));
	}

	bool ParseLinkValue(const FString& InValue, FLinkForm& OutLink, FString& OutReason)
	{
		OutReason.Reset();

		// A link is parenthesised, and the arrow is what makes it a WIRE rather than some other grouped form
		// the reader may print.
		//
		// THE CLOSING PARENTHESIS IS OPTIONAL HERE, AND THAT IS MEASURED RATHER THAN LENIENT. The format writes
		// a form as `(Head` ... `)` and puts the form's closing parenthesis on the LAST pin's line, so a link
		// written last arrives with that parenthesis already stripped by the pin-line reader:
		//
		//     (Development|PrintString
		//         InString (-> BP_NodeTest:EventGraph:65754a:ReturnValue)
		//     )
		//
		// The REQUIREMENT is the arrow and the two halves, not the trailing bracket - and demanding the bracket
		// rejected the dialect's own output, which is the most damaging kind of parser fault because the reader
		// and the writer would then disagree about text the reader itself printed.
		FString Body = InValue.TrimStartAndEnd();
		if (!Body.StartsWith(TEXT("(")))
		{
			OutReason = FString::Printf(
				TEXT("'%s' does not start with '(' , so it is not a link"), *InValue);
			return false;
		}
		Body = Body.Mid(1).TrimStartAndEnd();
		if (Body.EndsWith(TEXT(")")))
		{
			Body = Body.LeftChop(1).TrimStartAndEnd();
		}

		if (!Body.StartsWith(TEXT("->")))
		{
			OutReason = FString::Printf(
				TEXT("'%s' is a parenthesised value but not a wire: a link is written '(-> Node:Pin)'"),
				*InValue);
			return false;
		}
		Body = Body.Mid(2).TrimStartAndEnd();

		// THE LAST COLON SPLITS THE TWO HALVES, because a node id itself contains colons - the long form is
		// `Asset.Object:Graph:Key`. Splitting on the FIRST colon would read the asset path as the node and the
		// rest as the pin, which is the mistake this comment exists to prevent.
		int32 Split = INDEX_NONE;
		if (!Body.FindLastChar(TEXT(':'), Split) || Split <= 0 || Split >= Body.Len() - 1)
		{
			OutReason = FString::Printf(
				TEXT("'%s' names no pin: a link is written '(-> Node:Pin)' and the pin is missing"),
				*InValue);
			return false;
		}

		OutLink.FromNode = Body.Left(Split).TrimStartAndEnd();
		OutLink.FromPin = Body.Mid(Split + 1).TrimStartAndEnd();

		if (OutLink.FromNode.IsEmpty() || OutLink.FromPin.IsEmpty())
		{
			OutReason = FString::Printf(
				TEXT("'%s' has an empty node or pin, so there is nothing to link from"), *InValue);
			return false;
		}

		return true;
	}

	bool LooksNumeric(const FString& InValue)
	{
		if (InValue.IsEmpty())
		{
			return false;
		}
		return InValue.IsNumeric();
	}

	bool LooksBoolean(const FString& InValue)
	{
		return InValue == TEXT("true") || InValue == TEXT("false");
	}

	FString NodeIdFromComment(const FString& InComment)
	{
		FString Body = InComment.TrimStartAndEnd();
		if (Body.StartsWith(TEXT(";")))
		{
			Body = Body.RightChop(1).TrimStartAndEnd();
		}

		// Whitespace inside a quoted graph name belongs to the identifier.
		bool bQuoted = false;
		int32 End = Body.Len();
		for (int32 Index = 0; Index < Body.Len(); ++Index)
		{
			if (Body[Index] == TEXT('"')) { bQuoted = !bQuoted; }
			if (!bQuoted && FChar::IsWhitespace(Body[Index])) { End = Index; break; }
		}
		if (bQuoted) { return FString(); }
		const FString Candidate = Body.Left(End);
		const TurboMCPNodeId::FParsed Parsed = TurboMCPNodeId::Parse(Candidate);
		if (Parsed.IsComplete() && !TurboMCPNodeId::NormaliseKey(Parsed.NodeKey).IsEmpty())
		{
			return Candidate;
		}
		// Bare keys are scoped to the explicitly selected graph. Do not interpret
		// prose or a local-label declaration as a node identity.
		return !TurboMCPNodeId::NormaliseKey(Candidate).IsEmpty() ? Candidate : FString();
	}

	FString LocalLabelFromComment(const FString& InComment)
	{
		FString Body = InComment.TrimStartAndEnd();
        // Closed pinless forms may carry their label after the closing delimiter.
        while(Body.StartsWith(TEXT(")"))) Body=Body.RightChop(1).TrimStartAndEnd();
		if (Body.StartsWith(TEXT(";")))
		{
			Body = Body.RightChop(1).TrimStartAndEnd();
		}
		if (!Body.StartsWith(TEXT("label "), ESearchCase::IgnoreCase))
		{
			return FString();
		}

		const FString Label = Body.RightChop(6).TrimStartAndEnd();
		// Labels are tokens because a link's node half is split from its pin at the final colon.
		return !Label.IsEmpty() && !Label.Contains(TEXT(":")) && !Label.Contains(TEXT(" "))
			? Label : FString();
	}
}
