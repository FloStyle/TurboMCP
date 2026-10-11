// Copyright (c) 2026 Florian ESCHALIER (FloStyle)
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once

#include "Containers/Array.h"
#include "Containers/Map.h"
#include "Containers/UnrealString.h"
#include "CoreTypes.h"
#include "Misc/Guid.h"

class UUserDefinedStruct;

/**
 * `P/E-STRUCT-NEW` and `P/E-STRUCT-FIELDS`: user struct assets.
 *
 * *** THE MEASUREMENT THAT REVERSES `03_TOOLS.md`:910-914 AND `D-51`, WITH THE FILE AND LINE. ***
 *
 * Both spec passages say these tools *"cannot be done from a commandlet"* because *"`AddVariable` needs a
 * `UUserDefinedStructEditorData` that only the editor factory installs"*. THE MEASURED CRASH THAT PRODUCED THAT
 * SENTENCE IS REAL AND IS NOT IN DISPUTE - `TurboMCPKitCommandlet.cpp:1420-1451` records it:
 *
 *     CastChecked<UUserDefinedStructEditorData, UObject>()
 *     FMemberVariableNameHelper::Generate()                     StructureEditorUtils.cpp:256
 *     FStructureEditorUtils::AddVariable()                      StructureEditorUtils.cpp:304
 *
 * WHAT THE ENGINE ACTUALLY OFFERS, READ RATHER THAN RECALLED. `FStructureEditorUtils::CreateUserDefinedStruct()`
 * (`Engine/Source/Editor/UnrealEd/Private/Kismet2/StructureEditorUtils.cpp:42-64`) is a PUBLIC STATIC that does
 * the whole job in the same file that crashes without it:
 *
 *     48:  Struct = NewObject<UUserDefinedStruct>(InParent, Name, Flags);
 *     49:  UUserDefinedStructEditorData* EditorData =
 *              NewObject<UUserDefinedStructEditorData>(Struct, NAME_None, RF_Transactional);
 *     50:  Struct->EditorData = EditorData;
 *     59:  AddVariable(Struct, FEdGraphPinType(...PC_Boolean...));
 *
 * SO THE EDITOR FACTORY'S ONE LOAD-BEARING ACTION IS ONE `NewObject`, AND THE PUBLIC UTILITY PERFORMS IT. The
 * commandlet's crash came from calling `NewObject<UUserDefinedStruct>` DIRECTLY and then reaching for the field
 * list - it stopped ONE CALL SHORT of the API that works. The refusal was recorded on the wrong subject.
 *
 * *** AND THE SECOND QUESTION, WHICH IS THE ONE THAT MATTERS: DOES THIS NEED AN OPEN ASSET EDITOR OR MERELY AN
 * EDITOR PROCESS? MERELY AN EDITOR PROCESS, AND NOTHING ELSE. *** Read, not assumed:
 *
 *   - `AddVariable` (`StructureEditorUtils.cpp:288-318`) reads `Struct->EditorData`, opens a `FScopedTransaction`,
 *     and calls `OnStructureChanged`. No `GEditor`, no asset editor tab, no Slate.
 *   - `GetVarDesc` (`:587-591`) is `CastChecked<UUserDefinedStructEditorData>(Struct->EditorData)->VariablesDescriptions`
 *     and nothing more.
 *   - `UserDefinedStructEnabled()` (`:544-548`) is a `FBoolConfigValueHelper` over the config key
 *     `[/Script/UnrealEd.UserDefinedStructure] bUseUserDefinedStructure` - a config read, not an editor state.
 *   - `CompileStructure` (`:559-567`) goes through the `KismetCompiler` module interface.
 *
 * NONE OF THOSE READS AN OPEN EDITOR. What the pair actually needs is the `UnrealEd` module - already a
 * `PrivateDependencyModuleNames` entry in `TurboMCP.Build.cs:74` - and a LIVE EDITOR PROCESS, which is exactly what
 * every python suite in `Tools/` already talks to. THAT IS THE CLAIM THIS CAPABILITY MAKES, and `Tools/test-struct.py`
 * is what tests it.
 *
 * WHY THIS DOES NOT REPEAT THE `check(GEditor)` FAILURE. `LaunchEngineLoop.cpp:3343` guards
 * `FSlateApplication::Get().InitializeRenderer(...)` with `!IsRunningCommandlet()`, so Slate never initializes in a
 * commandlet, and `BlueprintEditorModule.cpp:214` is an unconditional `check(GEditor)`. Two commandlets in this
 * plugin cannot run for that reason. This pair touches NEITHER Slate NOR `GEditor`, so that limit does not bind
 * here - and this file claims the tools work AGAINST A LIVE EDITOR, not from a commandlet.
 *
 * THE PLAN/APPLY SPLIT FOLLOWS `FTurboMCPMaterialBlocks`, WHICH IS THE MOST RECENT WRITER. Every check lives in
 * `Plan`, so the Preview and its Edit twin refuse the same input (`D-02`), and `Apply` RE-RESOLVES EVERY FIELD BY
 * KEY rather than trusting a plan from an earlier call, which is `D-45`.
 *
 * A FIELD IS IDENTIFIED BY ITS DISPLAY NAME, AND THAT IS THE KEY. `FStructureEditorUtils` addresses a member by
 * `FGuid`, but a GUID is generated inside the engine and a caller can never name one; the display name is the only
 * key a caller has and the only one the spec's `Name: Type` syntax carries. So a plan stores DISPLAY NAMES, and
 * `Apply` re-resolves each one against the struct as it is AT APPLY TIME - a field that no longer exists is
 * REFUSED BY NAME rather than skipped, because silently skipping it would make the edit differ from the preview
 * that authorised it.
 */
class FTurboMCPStruct
{
public:
	/** One field of a struct, as a caller describes it and as the engine reports it back. */
	struct FField
	{
		/** The display name, which is the caller's key for this field. */
		FString Name;

		/** The type text exactly as the caller wrote it. */
		FString TypeText;

		/** The default as text, or empty. Set through `ChangeVariableDefaultValue`. */
		FString Default;

		/** The tooltip, or empty. Set through `ChangeVariableTooltip`. */
		FString Tooltip;

		/** The category, or empty. Written onto the description's `Category`. */
		FString Category;
	};

	/** One loaded asset that would be damaged by removing a field. */
	struct FReader
	{
		/** The asset's full object path, so the report names a thing rather than a count. */
		FString Path;

		/** Its class, so a reader of an unexpected kind is visible rather than averaged in. */
		FString Class;

		/** How many of this reader's own members or properties read the field being removed. */
		int32 Uses = 0;
	};

	/** What one field edit does. */
	enum class EFieldOp : uint8
	{
		Add,
		Change,
		Reorder,
		Remove,
	};

	/** One field edit, with everything both the report and the write need. */
	struct FFieldChange
	{
		EFieldOp Op = EFieldOp::Add;

		/** The field to add, change or remove. For a reorder this is the field being MOVED. */
		FField Field;

		/** For a reorder: the field it is moved relative to. */
		FString Anchor;

		/** For a reorder: true to land above the anchor, false for below. */
		bool bAbove = false;

		/**
		 * For a removal: the loaded-reader count the caller is authorising.
		 *
		 * `INDEX_NONE` means `force=` was not passed at all, which is a different thing from `force=0` and is
		 * reported differently. The spec refuses a removal *unless `force` names that count*, so a caller that
		 * guesses a wrong number is refused with the real one.
		 */
		int32 ForceCount = INDEX_NONE;
	};

	/** Everything needed to create a struct, decided before anything is created. */
	struct FPlanNew
	{
		/** The package path, for example `/Game/Tests/MyStruct`. */
		FString PackagePath;

		/** The asset name, taken from the path. */
		FString AssetName;

		/** The fields, in the order they will be created. */
		TArray<FField> Fields;
	};

	/** Everything needed to edit one struct's fields, decided before anything is changed. */
	struct FPlanFields
	{
		/** The resolved object path of the struct, so `Apply` loads the same asset the plan named. */
		FString ObjectPath;

		/** The fields the struct has now, in order, read while planning. */
		TArray<FField> CurrentFields;

		/** The edits, in the order the caller gave them. */
		TArray<FFieldChange> Changes;

		/** The loaded readers of each field a change removes, keyed by the field's display name. */
		TMap<FString, TArray<FReader>> ReadersByRemovedField;
	};

	/**
	 * Decide whether a struct can be created, and what it would contain. Creates nothing.
	 *
	 * Refuses a path that is not absolute, a path with no asset name, no fields at all, a field name that is not a
	 * usable identifier, a duplicate field name, an unknown field option, an unbalanced container type, and a type
	 * that resolves to nothing.
	 */
	static bool PlanNew(const FString& InPath, const FString& InFields,
		FPlanNew& OutPlan, FString& OutError);

	/** The preview text for `P-STRUCT-NEW`. States whether the path already exists, because that decides the refusal. */
	static FString RenderPreviewNew(const FPlanNew& InPlan);

	/** Create the asset. Refuses an existing path rather than overwriting it. */
	static FString ApplyNew(const FPlanNew& InPlan);

	/**
	 * Decide whether a struct's fields can be edited, and what would change. Changes nothing.
	 *
	 * Every check is here rather than in `Apply`, so the Preview and the Edit refuse the same input. The loaded
	 * readers of every field a change removes are measured HERE, so the count the caller is asked to authorise is
	 * the count the Edit will check against.
	 */
	static bool PlanFieldEdits(const FString& InStruct, const FString& InChanges,
		FPlanFields& OutPlan, FString& OutError);

	/** The preview text for `P-STRUCT-FIELDS`. */
	static FString RenderPreviewFields(const FPlanFields& InPlan);

	/** Apply the plan. One transaction; re-resolves every field by key. */
	static FString ApplyFields(const FPlanFields& InPlan);

	/**
	 * The loaded readers of a struct, listed by path and class.
	 *
	 * PUBLIC BECAUSE IT IS THE FACT THE REFUSAL IS BUILT ON, and a caller that wants to know why a removal was
	 * refused should be able to ask without attempting the removal. A count on its own is not an answer: `D-45`
	 * says a writer that removes anything *"lists each removal by key with its class and title, never a count"*.
	 *
	 * THE COUNT IS PER STRUCT AND NOT PER FIELD. Every field of a struct shares one shape and one set of
	 * readers, so there is no field name in this signature - and that is deliberate: an earlier version took one
	 * and never read it, which made the signature claim a per-field count the body did not compute.
	 */
	static void FindReaders(const UUserDefinedStruct* InStruct, TArray<FReader>& OutReaders);

	/** The fields a user struct currently declares, in order, read from its editor data. */
	static void ReadFields(const UUserDefinedStruct* InStruct, TArray<FField>& OutFields);

	/** Parse a field list into fields, without touching any asset. Shared by both plans. */
	static bool ParseFields(const FString& InFields, TArray<FField>& OutFields, FString& OutError);
};
