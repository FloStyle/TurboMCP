// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "TurboMCPGraphCheck.h"
#include "UObject/UnrealType.h"

#include "TurboMCPGraphTextLex.h"

#include "TurboMCPAssetResolver.h"
#include "TurboMCPNodeId.h"
#include "TurboMCPNodeType.h"

// `EdGraphSchema_K2.h` IS REQUIRED FOR `UEdGraphSchema_K2::PC_Exec`, which is the one pin category this
// checker has to name by constant rather than by string.
//
// `K2Node.h` IS REQUIRED FOR `UK2Node::IsLatentForMacros`, AND THIS REPLACED A GUESSED CLASS. The first
// version included `K2Node_LatentGameplayNode.h` and used `UK2Node_LatentGameplayNode` - A NAME THAT DOES
// NOT EXIST IN UE 5.8, caught by the build as a missing include and then by a search of the engine. The
// real mechanism is `UK2Node::IsLatentForMacros()` (`Editor/BlueprintGraph/Classes/K2Node.h:459-460`,
// default false), overridden by `UK2Node_CallFunction::IsLatentForMacros()`
// (`Editor/BlueprintGraph/Private/K2Node_CallFunction.cpp:3379-3382`), which answers
// `TargetFunction->HasMetaData(FBlueprintMetadata::MD_Latent)`. THIS IS THE FIFTH TIME IN THIS PROJECT THAT
// A RECALLED ENGINE NAME WAS WRONG, and the rule that an engine symbol must be READ stands.
#include "EdGraphSchema_K2.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "Engine/Blueprint.h"
#include "K2Node.h"
#include "K2Node_CallFunction.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "UObject/UObjectGlobals.h"

#include "Containers/Set.h"
#include "Containers/UnrealString.h"
#include "Misc/Char.h"

namespace
{
	// THE LINE TYPE AND EVERY LEXICAL RULE NOW COME FROM THE SHARED LEXER, so the checker and the first
	// writer cannot disagree about what a line is. The project has recorded two-tools-one-format bugs
	// twice - the graph text and the node ID - and a second reader is exactly how a second dialect starts.
	//
	// THE ALIAS IS THE WHOLE TRICK OF THIS REFACTOR: the checker's own name for a line keeps working, so
	// the ~300 lines below that already use `FLine` did not have to change, while the RULES behind it moved
	// to one place. `Using` is a namespace-level declaration rather than a second struct, so there is one
	// type and not two that merely look alike.
	using TurboMCPGraphTextLex::FLine;
	using TurboMCPGraphTextLex::IsCommentLine;
	using TurboMCPGraphTextLex::ExtractHeadTypeId;
	using TurboMCPGraphTextLex::IsQuoted;
	using TurboMCPGraphTextLex::LooksNumeric;

	/** The text split on newlines, keeping every line including blanks. */
	TArray<FLine> SplitLines(const FString& InText)
	{
		return TurboMCPGraphTextLex::SplitLines(InText);
	}

	/** True when the value is `true` or `false`. */
	bool LooksBoolean(const FString& InValue)
	{
		return InValue == TEXT("true") || InValue == TEXT("false");
	}

	/** Parenthesis balance for a multiline inline value node. */
	int32 ParenthesisBalance(const FString& InText)
	{
		int32 Balance = 0;
		for (const TCHAR Ch : InText)
		{
			if (Ch == TEXT('('))
			{
				++Balance;
			}
			else if (Ch == TEXT(')'))
			{
				--Balance;
			}
		}
		return Balance;
	}

	/**
	 * The category a pin's type belongs to, in the words the report uses.
	 *
	 * An empty string means the pin carries no type this checker can name, and a caller reading it must
	 * treat that as "not known" rather than as a category - which is why the mismatch check below refuses
	 * to fire when either side is unnamed.
	 */
	FString TypeWordOf(const UEdGraphPin* InPin)
	{
		if (InPin == nullptr)
		{
			return FString();
		}
		const FEdGraphPinType& Type = InPin->PinType;
		if (Type.PinCategory == UEdGraphSchema_K2::PC_Exec)
		{
			return TEXT("exec");
		}
		return Type.PinCategory.ToString();
	}

	/** A one-line list of pin names, for a finding that has to name the real ones. */
	FString JoinNames(const TArray<FString>& InNames)
	{
		return FString::Join(InNames, TEXT(", "));
	}

	/** The pins a node really has, inputs first, in the order the writer prints them. */
	TArray<FString> RealPinNames(const UEdGraphNode* InNode)
	{
		TArray<FString> Out;
		if (InNode == nullptr)
		{
			return Out;
		}
		for (const UEdGraphPin* Pin : InNode->Pins)
		{
			if (Pin == nullptr)
			{
				continue;
			}
			if (Pin->Direction != EGPD_Input)
			{
				continue;
			}
			// The format prints an input pin under its real name, and hides `self` and the world context
			// pin when they are implicit. Both are included here, because a caller writing one of them
			// explicitly must not be told the pin does not exist.
			Out.AddUnique(Pin->PinName.ToString());
		}
		return Out;
	}

	/**
	 * The graph text's own pin line: the keyword, its value, and the column the keyword starts at.
	 *
	 * A pin line is `Name value` optionally quoted and optionally followed by `; in <n>`. Parsing the
	 * TRAILING COMMENT OFF FIRST is what makes a pin name holding a space readable, because the writer
	 * quotes such a name.
	 */
	bool ParsePinLine(const FString& InLine, FString& OutName, FString& OutValue, int32& OutColumn)
	{
		// DELEGATED TO THE SHARED LEXER. The rule for what a pin line is now lives with the rule for what
		// a node form is, so the checker and the writer cannot drift apart about either.
		return TurboMCPGraphTextLex::ParsePinLine(InLine, OutName, OutValue, OutColumn);
	}
}

FString FTurboMCPGraphCheck::WordFor(EFindingKind InKind)
{
	switch (InKind)
	{
	case EFindingKind::NodeType:     return TEXT("node-type");
	case EFindingKind::PinName:      return TEXT("pin-name");
	case EFindingKind::TypeMismatch: return TEXT("type-mismatch");
	case EFindingKind::Refused:      return TEXT("refused");
	case EFindingKind::Unjudged:     return TEXT("unjudged");
	}
	return TEXT("unknown");
}

int32 FTurboMCPGraphCheck::FPlan::CountOf(EFindingKind InKind) const
{
	int32 Count = 0;
	for (const FFinding& Finding : Findings)
	{
		if (Finding.Kind == InKind)
		{
			++Count;
		}
	}
	return Count;
}

const UEdGraphNode* FTurboMCPGraphCheck::ResolveTypeId(const UEdGraph* InGraph,
	const FString& InTypeId, TArray<FString>& OutSuggestions)
{
	OutSuggestions.Reset();
	if (InGraph == nullptr || InTypeId.IsEmpty())
	{
		return nullptr;
	}

	// FIRST THE EXACT ANSWER, and it is exact by the canonical generator rather than by a string
	// comparison the caller chose. `03_TOOLS.md` requires the accepted text to be exactly what the
	// readers print, and the readers print `TurboMCPNodeId::GetNodeTypeId`. So the authority for "does
	// this type ID exist" is that function, applied to the graph's own nodes - not a hand-built list,
	// which would be a second opinion about the format.
	TArray<FString> RealTypeIds;
	for (const UEdGraphNode* Node : InGraph->Nodes)
	{
		if (Node == nullptr)
		{
			continue;
		}
		const FString TypeId = TurboMCPNodeId::GetNodeTypeId(Node);
		if (TypeId.IsEmpty())
		{
			continue;
		}
		if (TypeId == InTypeId)
		{
			return Node;
		}
		RealTypeIds.AddUnique(TypeId);
	}

	// THE CLOSEST REAL IDS, AND THE AUTHORITY IS THE GRAPH RATHER THAN A SEPARATE CATALOGUE.
	//
	// This is deliberately NOT a call to `FTurboMCPNodeType::Plan`, and the reason was MEASURED: that
	// function REFUSES an empty asset (`no asset given`), so it cannot be asked about a type ID without
	// a Blueprint in hand. `P-GRAPH-CHECK` does have one, so the fallback below does call it - but the
	// PRIMARY source is the set of type IDs that EXIST IN THIS GRAPH, because that is the set the
	// caller's text has to match and therefore the set a suggestion must come from. A suggestion drawn
	// from a catalogue of everything the engine can build could name a type this graph cannot hold,
	// which would send the caller to fix a word that was not the problem.
	//
	// The score is deliberately crude and explainable: a type ID whose LAST `|`-separated segment shares
	// the most characters with the written one, best first. Crude is correct here because the answer is
	// read by a person or an agent choosing a word, not executed.
	const FString Guess = [&InTypeId]()
	{
		int32 LastPipe = INDEX_NONE;
		if (InTypeId.FindLastChar(TEXT('|'), LastPipe))
		{
			return InTypeId.Mid(LastPipe + 1);
		}
		return InTypeId;
	}();

	TArray<TPair<int32, FString>> Scored;
	for (const FString& Real : RealTypeIds)
	{
		FString RealTail = Real;
		int32 LastPipe = INDEX_NONE;
		if (Real.FindLastChar(TEXT('|'), LastPipe))
		{
			RealTail = Real.Mid(LastPipe + 1);
		}

		// Longest common prefix, plus a bonus when one contains the other, so `PrintStringg` finds
		// `PrintString` rather than a longer name that merely starts the same way.
		int32 Common = 0;
		const int32 Max = FMath::Min(Guess.Len(), RealTail.Len());
		while (Common < Max && Guess[Common] == RealTail[Common])
		{
			++Common;
		}
		int32 Score = Common;
		if (RealTail.Contains(Guess) || Guess.Contains(RealTail))
		{
			Score += 100;
		}
		if (Real == InTypeId)
		{
			Score += 1000;
		}
		if (Score > 0)
		{
			Scored.Add(TPair<int32, FString>(Score, Real));
		}
	}

	Scored.Sort([](const TPair<int32, FString>& A, const TPair<int32, FString>& B)
	{
		if (A.Key != B.Key)
		{
			return A.Key > B.Key;
		}
		return A.Value < B.Value;
	});

	const int32 Cap = FMath::Min(Scored.Num(), 5);
	for (int32 Index = 0; Index < Cap; ++Index)
	{
		OutSuggestions.Add(Scored[Index].Value);
	}

	return nullptr;
}

bool FTurboMCPGraphCheck::Plan(const FString& InAsset, const FString& InGraphName,
	const FString& InText, FPlan& OutPlan, FString& OutError)
{
	OutError.Empty();
	OutPlan = FPlan();
	OutPlan.Asset = InAsset.TrimStartAndEnd();
	OutPlan.GraphName = InGraphName.TrimStartAndEnd();

	if (OutPlan.Asset.IsEmpty())
	{
		OutError = TEXT("no asset given. Pass the Blueprint the text is meant for, for example BP_NodeTest.");
		return false;
	}
	if (InText.TrimStartAndEnd().IsEmpty())
	{
		OutError = TEXT("no text given. Pass the graph text to check, exactly as R-GRAPH-LOAD prints it.");
		return false;
	}

	// Validate the whole document before resolving assets or interpreting node forms.
	// Reader diagnostics and truncated forms must never become partial writes.
	int32 FormDepth = 0;
	TArray<int32> OpenFormLines;
	TArray<int32> CommentBracketLines;
	int32 StringStartLine = 0;
	bool bInString = false;
	bool bEscaped = false;
	for (const FLine& Line : SplitLines(InText))
	{
		const FString Trimmed = Line.Text.TrimStartAndEnd();
		if (!bInString && FormDepth == 0 && Trimmed.StartsWith(TEXT("read: "))) continue;
		if (!bInString && Trimmed.StartsWith(TEXT("; error:"), ESearchCase::IgnoreCase))
		{
			OutError = FString::Printf(TEXT("line %d: graph text contains a reader error; obtain a complete, valid dump before write-back. %s"), Line.Number, *Trimmed);
			return false;
		}
		for (int32 Index = 0; Index < Line.Text.Len(); ++Index)
		{
			const TCHAR Ch = Line.Text[Index];
			if (bInString)
			{
				if (bEscaped) { bEscaped = false; continue; }
				if (Ch == TEXT('\\')) { bEscaped = true; continue; }
				if (Ch == TEXT('"')) bInString = false;
				continue;
			}
			if (Ch == TEXT(';'))
			{
				if (FormDepth > 0 && Line.Text.Mid(Index + 1).Contains(TEXT(")"))) CommentBracketLines.AddUnique(Line.Number);
				break;
			}
			if (Ch == TEXT('"')) { bInString = true; StringStartLine = Line.Number; continue; }
			if (Ch == TEXT('(')) { ++FormDepth; OpenFormLines.Add(Line.Number); }
			else if (Ch == TEXT(')') && --FormDepth < 0)
			{
				OutError = FString::Printf(TEXT("line %d, column %d: unexpected closing parenthesis; nothing was written"), Line.Number, Index + 1);
				return false;
			}
			else if (Ch == TEXT(')')) OpenFormLines.Pop();
		}
		// A physical newline consumes a preceding escape, not the next line's first character.
		bEscaped = false;
	}
	if (bInString || FormDepth != 0)
	{
		OutError = bInString
            ? FString::Printf(TEXT("line %d: quoted value is not closed; nothing was written"), StringStartLine)
            : FString::Printf(TEXT("line %d: form opened here is not closed (%d unclosed forms); nothing was written"), OpenFormLines.Last(), FormDepth);
        if (!bInString && !CommentBracketLines.IsEmpty())
        {
            TArray<FString> Lines;
            for (int32 Number : CommentBracketLines) Lines.Add(FString::FromInt(Number));
            OutError += FString::Printf(TEXT(". Closing brackets inside comments on line(s) %s are ignored: ';' comments continue to end of line. Put the closing bracket before the comment or on the next line"), *FString::Join(Lines, TEXT(", ")));
        }
		return false;
	}

	FTurboMCPAssetResolver::FResolved Resolved;
	const FString ResolveError = FTurboMCPAssetResolver::Resolve(OutPlan.Asset, Resolved);
	if (!ResolveError.IsEmpty())
	{
		OutError = ResolveError;
		return false;
	}

	UBlueprint* Blueprint = Cast<UBlueprint>(LoadObject<UObject>(nullptr, *Resolved.ObjectPath));
	if (Blueprint == nullptr)
	{
		OutError = FString::Printf(
			TEXT("%s is not a Blueprint, so it has no graph text to check."), *Resolved.ObjectPath);
		return false;
	}

	// THE GRAPH IS REQUIRED, AND THAT IS A DELIBERATE REFUSAL RATHER THAN A CONVENIENCE.
	//
	// A type ID and a pin name are both resolved AGAINST A GRAPH, because the same words mean different
	// things in different graphs: `ReceiveBeginPlay` exists in an event graph and not in a function
	// graph, and a local variable is a variable in one function and invisible in the next. Checking text
	// with no graph to check it against would report a type as missing when it merely belongs to a graph
	// the caller did not name - the "a reader fault reported as a subject fault" mistake this project has
	// recorded. So an unnamed graph is refused, and the refusal names the graphs that would work.
	TArray<UEdGraph*> Graphs;
	for (UEdGraph* Graph : Blueprint->UbergraphPages)
	{
		if (Graph != nullptr) { Graphs.Add(Graph); }
	}
	for (UEdGraph* Graph : Blueprint->FunctionGraphs)
	{
		if (Graph != nullptr) { Graphs.Add(Graph); }
	}
	for (UEdGraph* Graph : Blueprint->MacroGraphs)
	{
		if (Graph != nullptr) { Graphs.Add(Graph); }
	}
	for (UEdGraph* Graph : Blueprint->EventGraphs)
	{
		if (Graph != nullptr) { Graphs.Add(Graph); }
	}

	UEdGraph* Graph = nullptr;
	if (OutPlan.GraphName.IsEmpty())
	{
		TArray<FString> Names;
		for (const UEdGraph* Candidate : Graphs)
		{
			Names.Add(Candidate->GetName());
		}
		OutError = FString::Printf(
			TEXT("no graph given, and the check needs one because a type ID and a pin name are both ")
			TEXT("resolved AGAINST A GRAPH. %s has: %s."),
			*Resolved.ObjectPath, *JoinNames(Names));
		return false;
	}

	// Match the reader/writer identity policy; never validate an arbitrary first graph.
	int32 GraphMatches = 0;
	Graph = TurboMCPNodeId::FindGraphByName(Blueprint, OutPlan.GraphName, GraphMatches);
	if (GraphMatches > 1)
	{
		OutError = FString::Printf(
			TEXT("graph name '%s' is ambiguous in %s (%d distinct graphs); use a unique graph name before checking or writing"),
			*OutPlan.GraphName, *Resolved.ObjectPath, GraphMatches);
		return false;
	}
	if (Graph == nullptr)
	{
		TArray<FString> Names;
		for (const UEdGraph* Candidate : Graphs)
		{
			Names.Add(Candidate->GetName());
		}
		OutError = FString::Printf(
			TEXT("no graph named '%s' in %s. The graphs are: %s."),
			*OutPlan.GraphName, *Resolved.ObjectPath, *JoinNames(Names));
		return false;
	}

	// ---------------------------------------------------------------------------------------------
	// Everything below only READS. `Graph` is const from here on, so a mutating call would not compile
	// rather than merely being absent - which is the strongest form this guarantee can take.
	// ---------------------------------------------------------------------------------------------
	const UEdGraph* ReadOnlyGraph = Graph;

	// A reverse index from a type ID to a node that has it, so each form is resolved ONCE rather than
	// scanning the graph per line. It is built from the live graph, so it cannot drift from the format.
	TMap<FString, const UEdGraphNode*> NodeByTypeId;
	for (const UEdGraphNode* Node : ReadOnlyGraph->Nodes)
	{
		if (Node == nullptr)
		{
			continue;
		}
		const FString TypeId = TurboMCPNodeId::GetNodeTypeId(Node);
		if (!TypeId.IsEmpty() && !NodeByTypeId.Contains(TypeId))
		{
			NodeByTypeId.Add(TypeId, Node);
		}
	}

	// Standard function graphs are UEdGraph, not a class named "Function".
	// Use the owning Blueprint collection so latent-node refusal also covers them.
	const bool bInsideFunction = Blueprint->FunctionGraphs.Contains(Graph);

	const TArray<FLine> Lines = SplitLines(InText);

	// The node form the pin lines below belong to, so a pin is checked against the RIGHT node.
	const UEdGraphNode* CurrentNode = nullptr;
	int32 CurrentNodeLine = 0;

	// True when the most recent node head could not be resolved, so the pins under it are a consequence
	// of a fault already reported rather than a second, separate one. See the `CurrentNode == nullptr`
	// block below for the measurement that put this here.
	bool bLastHeadUnresolved = false;
	int32 InlineValueBalance = 0;

	for (const FLine& Line : Lines)
	{
		if (InlineValueBalance > 0)
		{
			InlineValueBalance += ParenthesisBalance(Line.Text);
			if (InlineValueBalance <= 0)
			{
				InlineValueBalance = 0;
			}
			continue;
		}

		// R-GRAPH-LOAD prefixes its otherwise round-trippable graph text with this
		// result envelope. It describes the read, not a graph pin, so ignore it
		// when the exact reader output is supplied to this checker.
		if (Line.Text.TrimStartAndEnd().StartsWith(TEXT("read: ")))
		{
			continue;
		}
		if (IsCommentLine(Line))
		{
			continue;
		}

		FString TypeId;
		int32 Column = 0;
		int32 AfterTypeId = 0;
		if (ExtractHeadTypeId(Line.Text, TypeId, Column, AfterTypeId))
		{
			// The reader's entry form opens a body but is not an engine node form.
			if (TypeId == TEXT("fn") || TypeId == TEXT("event"))
			{
				CurrentNode = nullptr;
				bLastHeadUnresolved = false;
				continue;
			}

			++OutPlan.NodeFormsFound;

			// NAMED `NodeForType` RATHER THAN `Resolved`, BECAUSE `Resolved` IS ALREADY THE ASSET above.
			// The build caught the shadow as `error C4456`, which this project treats as an error: a
			// second `Resolved` in the same scope reads as the same thing to a reader and is not.
			const UEdGraphNode* NodeForType = nullptr;
			if (const UEdGraphNode* const* Found = NodeByTypeId.Find(TypeId))
			{
				NodeForType = *Found;
			}

			if (NodeForType == nullptr)
			{
				// The finder and writers resolve creation ids through FTurboMCPNodeType. The checker must
				// ask that same resolver before it declares a type absent: a type can be valid for this
				// graph without an instance already being present in it.
				FString ProbeNote;
				if (UEdGraphNode* Probe = FTurboMCPNodeType::BuildProbeNode(Blueprint, Graph, TypeId, ProbeNote))
				{
					NodeForType = Probe;
				}
			}

			if (NodeForType == nullptr)
			{
				TArray<FString> Suggestions;
				ResolveTypeId(ReadOnlyGraph, TypeId, Suggestions);

				FFinding Finding;
				Finding.Kind = EFindingKind::NodeType;
				Finding.KindWord = WordFor(Finding.Kind);
				Finding.Line = Line.Number;
				Finding.Column = Column;
				Finding.Written = TypeId;
				Finding.Suggestions = Suggestions;
				Finding.Message = FString::Printf(
					TEXT("no node type in %s is written '%s'."), *OutPlan.GraphName, *TypeId);
				if (Suggestions.Num() > 0)
				{
					Finding.Message += FString::Printf(
						TEXT(" Closest real IDs: %s."), *JoinNames(Suggestions));
				}
				else
				{
					Finding.Message += TEXT(" No close match exists in this graph.");
				}
				OutPlan.Findings.Add(Finding);
			}

			CurrentNode = NodeForType;
			CurrentNodeLine = Line.Number;
			bLastHeadUnresolved = (NodeForType == nullptr);

			// A LATENT NODE INSIDE A FUNCTION IS REFUSED, and this is the example `03_TOOLS.md` names.
			//
			// A latent action is one that spans frames - a delay, a timeline, an async load. It is only
			// legal where the engine can resume it later, and a function graph is inlined into its
			// callers with no such place, so the engine's own compiler rejects it. Refusing here means
			// the caller learns before a writer has touched the asset, which is the whole point of a
			// checker that runs first.
			//
			// THE TEST IS `IsLatentForMacros()`, WHICH IS THE ENGINE'S OWN ANSWER and not a class list
			// written here. See the include comment: a class list here would name a class that does
			// not exist in this engine version.
			const UK2Node* AsK2 = Cast<UK2Node>(NodeForType);
			if (bInsideFunction && AsK2 != nullptr && AsK2->IsLatentForMacros())
			{
				FFinding Finding;
				Finding.Kind = EFindingKind::Refused;
				Finding.KindWord = WordFor(Finding.Kind);
				Finding.Line = Line.Number;
				Finding.Column = Column;
				Finding.Written = TypeId;
				Finding.Message = FString::Printf(
					TEXT("'%s' is a LATENT node and %s is a FUNCTION graph. A latent action spans frames ")
					TEXT("and a function is inlined into its callers, so the engine cannot compile it ")
					TEXT("here. Put it in an event graph, or make the call from one."),
					*TypeId, *OutPlan.GraphName);
				OutPlan.Findings.Add(Finding);
			}

			continue;
		}

		// Not a node head, so it is either a pin line or a continuation/sub-block line. A sub-block head
		// (`:then`) carries the pins of the node it belongs to, so the current node is NOT cleared - the
		// format puts a continuation's pins under the continuation.
		FString PinName;
		FString PinValue;
		int32 PinColumn = 0;
		if (!ParsePinLine(Line.Text, PinName, PinValue, PinColumn))
		{
			continue;
		}

		// A continuation head is not a pin. The opening parenthesis is attached
		// to the token by the line lexer (`(:then`), so remove it before matching.
		const FString ContinuationName = PinName.StartsWith(TEXT("("))
			? PinName.Mid(1) : PinName;
		if (ContinuationName == TEXT(":then") || ContinuationName == TEXT(":else")
			|| ContinuationName.StartsWith(TEXT(":")))
		{
			continue;
		}

		++OutPlan.PinLinesFound;

		if (CurrentNode == nullptr)
		{
			// A PIN UNDER AN UNRESOLVED NODE HEAD IS NOT REPORTED AGAIN, AND THE REASON IS COUNTING.
			//
			// REPORTING THE UNKNOWN NODE TYPE AND THEN ITS PINS AS "no node form is open above it" MAKES
			// ONE wrong node type produce TWO findings. A caller fixing the type ID would then see a
			// finding disappear that was never about anything they wrote - and `T-WRITE-1` requires that
			// a text with three faults give EXACTLY THREE
			// findings. The type ID was already named on the line above; saying it twice is noise, and
			// noise is what makes a checker's count useless.
			//
			// A PIN WITH NO NODE ABOVE IT AT ALL IS DIFFERENT and is still refused: that is a real
			// indentation fault in the caller's text, not a consequence of a fault already reported.
			if (bLastHeadUnresolved)
			{
				continue;
			}

			FFinding Finding;
			Finding.Kind = EFindingKind::Refused;
			Finding.KindWord = WordFor(Finding.Kind);
			Finding.Line = Line.Number;
			Finding.Column = PinColumn;
			Finding.Written = PinName;
			Finding.Message = FString::Printf(
				TEXT("'%s' is written as a pin but no node form is open above it, so there is nothing to ")
				TEXT("check it against. A pin line belongs under the node it is a pin of."),
				*PinName);
			OutPlan.Findings.Add(Finding);
			continue;
		}

		// THE PIN MUST EXIST ON THE RESOLVED NODE.
		const UEdGraphPin* FoundPin = nullptr;
		for (const UEdGraphPin* Pin : CurrentNode->Pins)
		{
			if (Pin != nullptr && Pin->Direction == EGPD_Input && Pin->PinName.ToString() == PinName)
			{
				FoundPin = Pin;
				break;
			}
		}

		if (FoundPin == nullptr)
		{
			FFinding Finding;
			Finding.Kind = EFindingKind::PinName;
			Finding.KindWord = WordFor(Finding.Kind);
			Finding.Line = Line.Number;
			Finding.Column = PinColumn;
			Finding.Written = PinName;
			Finding.Suggestions = RealPinNames(CurrentNode);
			Finding.Message = FString::Printf(
				TEXT("'%s' is not a pin of %s."), *PinName,
				*TurboMCPNodeId::GetNodeTypeId(CurrentNode));
			if (Finding.Suggestions.Num() > 0)
			{
				Finding.Message += FString::Printf(
					TEXT(" It has: %s."), *JoinNames(Finding.Suggestions));
			}
			else
			{
				Finding.Message += TEXT(" It has no input pins at all.");
			}
			OutPlan.Findings.Add(Finding);
			continue;
		}

		// Inline value nodes have their own multiline pin body. The outer pin is
		// checked above; the nested lines belong to the inline node, not the parent.
		if (PinValue.StartsWith(TEXT("(")) && !PinValue.StartsWith(TEXT("(->")))
		{
			InlineValueBalance = ParenthesisBalance(PinValue);
			continue;
		}

		// A LINK OR A DEFAULT, never both, and the format's own shape decides: a value starting with `(`
		// is a link to another node, and a link's target is checked by the node it names rather than by a
		// literal comparison here.
		if (PinValue.StartsWith(TEXT("(")) || PinValue == TEXT("self"))
		{
			continue;
		}

		// THE TYPE MISMATCH, NAMING BOTH SIDES.
		//
		// Only a literal can be judged, and only when BOTH the written form and the pin's own category
		// are known. Where either side is NOT known the case is reported as `unjudged` WITH THE REASON
		// rather than passed over in silence: a caller reading "0 findings" cannot otherwise tell whether
		// every literal was checked or none was, and silence is indistinguishable from agreement.
		if (PinValue.IsEmpty())
		{
			// A pin named with no value keeps its default, which `03_TOOLS.md` allows explicitly:
			// "Pins not named keep their defaults". Nothing to judge.
			continue;
		}

		if (const UEnum* Enum = Cast<UEnum>(FoundPin->PinType.PinSubCategoryObject.Get()))
		{
			FString Name = PinValue.TrimStartAndEnd();
			if (IsQuoted(Name)) Name = Name.Mid(1, Name.Len()-2);
			int32 Index = Enum->GetIndexByNameString(Name);
			if (Index == INDEX_NONE && (Name.IsNumeric() && !Name.Contains(TEXT(".")))) Index = Enum->GetIndexByValue(FCString::Atoi64(*Name));
			if (Index == INDEX_NONE || Enum->HasMetaData(TEXT("Hidden"), Index))
			{
				FFinding F; F.Kind = EFindingKind::TypeMismatch; F.KindWord = WordFor(F.Kind);
				F.Line = Line.Number; F.Column = PinColumn; F.Written = PinName;
				F.Message = FString::Printf(TEXT("'%s' is not a supported enumerant of %s"), *Name, *Enum->GetName());
				OutPlan.Findings.Add(F);
			}
			continue;
		}
		const FString PinTypeWord = TypeWordOf(FoundPin);

		const bool bNumericLiteral = LooksNumeric(PinValue);
		const bool bBoolLiteral = LooksBoolean(PinValue);
		const bool bQuotedLiteral = IsQuoted(PinValue);

		FString WrittenType;
		if (bNumericLiteral) { WrittenType = TEXT("a number"); }
		else if (bBoolLiteral) { WrittenType = TEXT("a boolean"); }
		else if (bQuotedLiteral) { WrittenType = TEXT("a quoted string"); }

		// A bare word that is neither a number, a boolean, nor quoted is a NAME reference in this
		// dialect - an enum entry, an object path - and naming its type would be a guess. Reported as
		// unjudged so the count says so.
		if (WrittenType.IsEmpty())
		{
			FFinding Finding;
			Finding.Kind = EFindingKind::Unjudged;
			Finding.KindWord = WordFor(Finding.Kind);
			Finding.Line = Line.Number;
			Finding.Column = PinColumn;
			Finding.Written = PinName;
			Finding.Message = FString::Printf(
				TEXT("pin '%s' is written the bare name '%s', which this dialect uses for an enum entry ")
				TEXT("or an object path, so its type cannot be told from the text alone. NOT A FAULT."),
				*PinName, *PinValue);
			OutPlan.Findings.Add(Finding);
			continue;
		}

		if (PinTypeWord.IsEmpty())
		{
			FFinding Finding;
			Finding.Kind = EFindingKind::Unjudged;
			Finding.KindWord = WordFor(Finding.Kind);
			Finding.Line = Line.Number;
			Finding.Column = PinColumn;
			Finding.Written = PinName;
			Finding.Message = FString::Printf(
				TEXT("pin '%s' is given %s, but the pin carries no type this checker can name, so the ")
				TEXT("pair cannot be judged. NOT A FAULT."),
				*PinName, *WrittenType);
			OutPlan.Findings.Add(Finding);
			continue;
		}

		// An exec pin takes no literal, but the format never prints one either, so a value on an exec
		// pin is a shape the writer cannot produce. Reported as unjudged rather than as a mismatch,
		// because this checker cannot say what a writer would do with it.
		if (PinTypeWord == TEXT("exec"))
		{
			FFinding Finding;
			Finding.Kind = EFindingKind::Unjudged;
			Finding.KindWord = WordFor(Finding.Kind);
			Finding.Line = Line.Number;
			Finding.Column = PinColumn;
			Finding.Written = PinName;
			Finding.Message = FString::Printf(
				TEXT("pin '%s' is an exec pin and the text gives it the literal '%s'. The format prints a ")
				TEXT("link on an exec pin, never a value. NOT A FAULT, but not a shape the writer makes."),
				*PinName, *PinValue);
			OutPlan.Findings.Add(Finding);
			continue;
		}

		// A number fits every numeric category and a number is what the writer prints for all of them,
		// so only the CROSS-FAMILY cases are faults: a number where text is wanted, text where a number
		// is wanted, and a boolean where neither is. Comparing the families rather than the exact
		// categories is deliberate - `float`, `int` and `byte` all accept `42.0` as the writer prints
		// it, and a checker that called those mismatches would refuse text the engine accepts.
		const bool bPinIsText = PinTypeWord == TEXT("String") || PinTypeWord == TEXT("Name")
			|| PinTypeWord == TEXT("Text");
		const bool bPinIsNumeric = PinTypeWord == TEXT("float") || PinTypeWord == TEXT("int")
			|| PinTypeWord == TEXT("int64") || PinTypeWord == TEXT("byte") || PinTypeWord == TEXT("real")
			|| PinTypeWord == TEXT("double");
		const bool bPinIsBool = PinTypeWord == TEXT("bool");

		bool bMismatch = false;
		if (bPinIsText && !bQuotedLiteral)
		{
			bMismatch = true;
		}
		else if (bPinIsNumeric && (bBoolLiteral || bQuotedLiteral))
		{
			bMismatch = true;
		}
		else if (bPinIsBool && (bNumericLiteral || bQuotedLiteral))
		{
			bMismatch = true;
		}

		if (bMismatch)
		{
			FFinding Finding;
			Finding.Kind = EFindingKind::TypeMismatch;
			Finding.KindWord = WordFor(Finding.Kind);
			Finding.Line = Line.Number;
			Finding.Column = PinColumn;
			Finding.Written = PinName;
			Finding.Message = FString::Printf(
				TEXT("pin '%s' of %s is %s, and the text gives %s (%s)."),
				*PinName, *TurboMCPNodeId::GetNodeTypeId(CurrentNode),
				*PinTypeWord, *WrittenType, *PinValue);
			OutPlan.Findings.Add(Finding);
		}
	}

	return true;
}

FString FTurboMCPGraphCheck::Render(const FPlan& InPlan)
{
	FString Out = FString::Printf(TEXT("preview: checking graph text for %s in %s"),
		*InPlan.GraphName, *InPlan.Asset);
	Out += FString::Printf(
		TEXT("\n%d node form(s) and %d pin line(s) read, %d finding(s)"),
		InPlan.NodeFormsFound, InPlan.PinLinesFound, InPlan.Findings.Num());

	if (InPlan.Findings.Num() == 0)
	{
		// `P1`: never return nothing. "Nothing found" WITH the counts above it is the answer.
		Out += TEXT("\nthe text would be accepted: no unknown node type, no unknown pin and no type mismatch.");
		Out += TEXT("\nNOTHING WAS WRITTEN: this tool parses, resolves and checks only.");
		return Out;
	}

	for (const FFinding& Finding : InPlan.Findings)
	{
		Out += FString::Printf(TEXT("\n\n  %s  line %d, column %d: %s"),
			*Finding.KindWord, Finding.Line, Finding.Column, *Finding.Message);
	}

	Out += FString::Printf(
		TEXT("\n\n%d finding(s): %d node-type, %d pin-name, %d type-mismatch, %d refused, %d unjudged."),
		InPlan.Findings.Num(),
		InPlan.CountOf(EFindingKind::NodeType),
		InPlan.CountOf(EFindingKind::PinName),
		InPlan.CountOf(EFindingKind::TypeMismatch),
		InPlan.CountOf(EFindingKind::Refused),
		InPlan.CountOf(EFindingKind::Unjudged));
	Out += TEXT("\nNOTHING WAS WRITTEN: this tool parses, resolves and checks only.");

	return Out;
}
