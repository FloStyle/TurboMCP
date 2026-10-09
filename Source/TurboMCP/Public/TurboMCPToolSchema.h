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

class FTurboMCPTraps;

/**
 * The registry's JSON schemas, turned into the compact help an agent can act on.
 *
 * `R-TOOL-SCHEMA` in `03_TOOLS.md` asks for a signature, one row per parameter, and the trap
 * notes, in under 2000 characters for any single tool. The schemas come from
 * `UToolsetRegistry`, so the ENGINE's own tools are covered by the same code path as this
 * plugin's - which is the point: an agent that has to learn two help systems will use neither.
 *
 * WHY THE OUTPUT IS CAPPED
 * ------------------------
 * `describe_toolset` returns about 70,000 characters for the Blueprint tools. An agent that
 * asks for help and receives that has been given a worse problem than the one it had, so the
 * budget is enforced rather than hoped for, and the result SAYS what was dropped and how to
 * see it (`P6`).
 *
 * TRAPS COME FROM DATA, NOT CODE
 * ------------------------------
 * `D-07` requires the trap notes to live in a file so they can be corrected without a rebuild.
 * `FTurboMCPTraps` already implements that and is read here rather than re-implemented: a
 * second loader would agree with the first until one of them was changed, which is this
 * project's recurring failure.
 */
class FTurboMCPToolSchema
{
public:
	/** One parameter, as the schema describes it. */
	struct FParam
	{
		FString Name;

		/** The schema's `type`, or `any` when it gave none. */
		FString Type;

		/** True when the schema lists it under `required`. */
		bool bRequired = false;

		/** The schema's `default`, when it has one. */
		FString Default;

		/** The OPENING SENTENCE of the schema's description, which is all that fits. */
		FString Opening;
	};

	/** One tool, as the schema describes it. */
	struct FTool
	{
		/**
		 * The BARE tool name, which is what `call_tool` takes.
		 *
		 * NOT the whole identifier: the registry publishes `Toolset.Function` and `call_tool`
		 * wants the function alone, so the bare name is taken from after the LAST dot. The
		 * engine's own toolset names contain dots, which is why it is the last one and not the
		 * first.
		 */
		FString Name;

		/**
		 * The whole identifier as the registry published it, for printing and for matching a
		 * caller who copied it out of `describe_toolset`.
		 */
		FString FullName;

		FString Description;
		TArray<FParam> Params;

		/** Why the schema is unusual, when it is. Empty for a schema fully understood. */
		FString OddSchemaNote;
	};

	/** One toolset, with everything it serves. */
	struct FToolset
	{
		FString Name;
		FString Description;
		TArray<FTool> Tools;
	};

	/** What the caller asked for, resolved. */
	struct FPlan
	{
		/** The name the caller wrote. */
		FString RequestedToolset;

		/** The toolset's real name, from the registry. */
		FString ResolvedToolset;

		/** The tool the caller named, resolved to its real name. Empty when none was named. */
		FString ResolvedTool;

		/** True when a bare name was given and the resolution took the FULL form. */
		bool bUsedShortForm = false;

		/** Every tool of the toolset, when no tool was named. */
		TArray<FTool> Tools;

		/** The one tool asked for, when one was named. */
		FTool Single;

		/** The schema's own name for the toolset, when it differs from the resolved name. */
		FString SchemaToolsetName;

		/** Every toolset the registry serves, for a "did you mean" list. */
		TArray<FString> AllToolsetNames;

		/** Plain sentences from the traps file, for the named tool. */
		TArray<FString> Traps;

		/** Real parameter names the traps file records, keyed by a guess. */
		TArray<FString> TrapParameterNotes;

		/** Why the traps are absent, when they are. Empty when they were read. */
		FString TrapsNote;
	};

	/** The cap `03_TOOLS.md` requires for a single tool's help. */
	static constexpr int32 MaxToolChars = 2000;

	/**
	 * Resolves a toolset and an optional tool, and reads what it needs.
	 *
	 * The toolset name is accepted in FULL (`ModelContextProtocol`-style, as the registry
	 * reports it) and in SHORT form (the bare name), and a name that fits nothing is refused
	 * with the CLOSEST REAL NAMES rather than with a bare "not found" (`P7`).
	 */
	static bool Plan(const FString& InToolsetName, const FString& InToolName,
		FPlan& OutPlan, FString& OutError);

	/** The help text for a plan, under `MaxToolChars` for a single tool. */
	static FString Render(const FPlan& InPlan);

	/**
	 * The closest real names to a guess, best first.
	 *
	 * ONE IMPLEMENTATION, because `03_TOOLS.md` says "the same matcher is used for did you
	 * mean in every other tool". A second copy would agree with this one until one was
	 * changed, which is how this project has broken formats before.
	 *
	 * @param InCandidates  The real names to score against.
	 * @param InLimit       How many to return. Ten unless the caller says otherwise.
	 */
	static TArray<FString> ClosestNames(const FString& InGuess,
		const TArray<FString>& InCandidates, int32 InLimit = 10);

	/**
	 * Normalises a name for matching: case folded, and spaces, underscores, brackets and
	 * the category part before the last `|` removed.
	 *
	 * Exposed because `P-NODE-TYPE` must normalise BOTH sides the same way, and the spec
	 * says so in as many words.
	 */
	static FString NormaliseForMatch(const FString& InText);

	/** The opening sentence of a description, which is all the budget allows. */
	static FString OpeningSentence(const FString& InText);

	/** Every toolset the registry serves, sorted. Empty when the registry is unavailable. */
	static TArray<FString> AllToolsetNames();
};
