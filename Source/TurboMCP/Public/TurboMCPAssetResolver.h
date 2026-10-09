// Copyright (c) 2026 Florian ESCHALIER (FloStyle)
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once

#include "Containers/UnrealString.h"

/**
 * Resolves any asset path form to one asset, or refuses with the reason.
 *
 * `P5` in `02_PRINCIPLES.md` requires every tool that takes an asset to accept a
 * short name, a package path, an object path and a generated class path, and to
 * find an asset that is in memory but never saved.
 *
 * `P2` requires a refusal, not a guess, when a short name fits more than one
 * asset. `D-10` in `05_DECISIONS.md` records why: a short name that fitted two
 * assets was silently resolved to the wrong one and two agents lost their work.
 *
 * Why one resolver: the engine's tools each want a different form, and each one
 * that guesses gets it wrong differently. One implementation means every
 * TurboMCP tool answers "which asset is this" the same way, which is what `P9`
 * asks for.
 */
class FTurboMCPAssetResolver
{
public:
	/** What a resolved asset is, in the forms other tools and the engine need. */
	struct FResolved
	{
		/** The object path, for example `/Game/UI/WBP_Card.WBP_Card`. */
		FString ObjectPath;

		/** The package path, for example `/Game/UI/WBP_Card`. */
		FString PackagePath;

		/** The asset's own name, for example `WBP_Card`. */
		FString AssetName;

		/**
		 * The asset's class path, for example `/Script/Engine.Blueprint`.
		 *
		 * Filled by both resolution routes so the answer cannot differ between
		 * path forms, which `T-ASSET-2` requires.
		 */
		FString AssetClass;

		/**
		 * The generated class path, filled only when the input named one.
		 *
		 * Left empty otherwise rather than being guessed from the asset name:
		 * inventing `Asset._C` for every asset made this differ between path
		 * forms. `R-ASSET-DESCRIBE` prints it under its own label.
		 */
		FString ClassPath;
	};

	/**
	 * Resolves one input string to exactly one asset.
	 *
	 * On success OutResolved is filled and an empty string is returned.
	 * On failure OutResolved is untouched and the returned string is a complete
	 * error line for the agent: it names what was refused, why, and the exact
	 * inputs that would work (`P2`, `P7`).
	 *
	 * @param InInput  Any accepted path form, or a short name.
	 * @param OutResolved  Filled only on success.
	 */
	static FString Resolve(const FString& InInput, FResolved& OutResolved);

	/**
	 * Resolves a list of inputs, one error line per input that failed.
	 *
	 * A list input accepts a JSON array AND one string split on `,` or `|`,
	 * because agents write both and both must work (`D-04`).
	 *
	 * @param InInputs  The raw inputs, in order.
	 * @param OutAssets  Appended to, one entry per input that resolved.
	 * @param OutErrors  Appended to, one line per input that failed.
	 */
	static void ResolveMany(
		const TArray<FString>& InInputs,
		TArray<FResolved>& OutAssets,
		TArray<FString>& OutErrors);

	/**
	 * Splits one string on `,` or `|` into trimmed, empty-free parts.
	 *
	 * Exposed because every list parameter in the plugin uses it, so the split
	 * rule cannot drift between tools.
	 */
	static TArray<FString> SplitList(const FString& InRaw);

	/**
	 * Every asset whose own name matches.
	 *
	 * Public because `04_FORMATS.md` 3.2 requires a caller that is given an
	 * ambiguous asset name to refuse it **and list every full path it fits**.
	 * Naming the ambiguity is only possible if the caller can ask for all the
	 * candidates, so this is not an implementation detail.
	 *
	 * @param InName         The name to match.
	 * @param bContainsMatch True for a substring match, false for a whole-name
	 *                       match. Substring matching is what `R-ASSET-SEARCH`
	 *                       uses; a node ID holds a whole name.
	 */
	static void FindAllByName(
		const FString& InName,
		TArray<FResolved>& OutMatches,
		bool bContainsMatch = false);

private:
	/**
	 * Tries the cheap forms first: an object that is already loaded, then an
	 * exact package or object path. Returns true when one asset was found.
	 */
	static bool TryDirectForms(const FString& InInput, FResolved& OutResolved);

	/**
	 * Fills OutResolved from a loaded object. This is the only path that finds
	 * an asset that exists in memory but was never saved.
	 *
	 * @param bWasClassPath  True when the caller's input named a generated class,
	 *        which is the only case where ClassPath is filled.
	 */
	static bool CompleteFromObject(
		const class UObject* InObject,
		FResolved& OutResolved,
		bool bWasClassPath = false);

	/**
	 * Fills the remaining fields of OutResolved from a package path, using the
	 * asset registry. Returns false when no single asset lives at that path.
	 */
	static bool CompleteFromPackagePath(const FString& InPackagePath, FResolved& OutResolved);
};
