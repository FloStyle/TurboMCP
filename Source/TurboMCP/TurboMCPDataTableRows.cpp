// Copyright (c) 2026 Florian ESCHALIER (FloStyle)
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "TurboMCPDataTableRows.h"

#include "DataTableEditorUtils.h"
#include "Engine/DataTable.h"
#include "Misc/OutputDevice.h"
#include "UObject/Class.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"
#include "TurboMCPAssetResolver.h"

namespace FTurboMCPDataTableRows
{
namespace
{
	/** The engine's own port flags for a copy, which is what the data table editor's paste path uses. */
	constexpr int32 CopyFlags = PPF_Copy;

	/**
	 * CAPTURES WHAT THE ENGINE'S IMPORTER SAYS, INSTEAD OF PRINTING IT TO THE LOG.
	 *
	 * MEASURED REASON: `UScriptStruct::ImportText` takes an `FOutputDevice*` for its errors
	 * (`Class.h:2457`). The obvious-looking `&GError` IS NOT A CAPTURING DEVICE - it is a global that writes to the
	 * log - so a refusal would arrive at the caller as a bare "the struct refused the text" with the engine's own
	 * wording lost in a log nobody reads. **A REFUSAL THAT DOES NOT SAY WHY IS THE DEFECT THIS PROJECT KEEPS
	 * RECORDING**, and this four-line device is what keeps the engine's own words.
	 */
	class FRowLogCapture : public FOutputDevice
	{
	public:
		FString Text;

		virtual void Serialize(const TCHAR* Message, ELogVerbosity::Type Verbosity, const FName& Category) override
		{
			if (!Text.IsEmpty())
			{
				Text += TEXT("; ");
			}
			Text += Message;
		}
	};

	/**
	 * SPLIT `(A=1,B=(C=2))` INTO ITS TOP-LEVEL `A=1` AND `B=(C=2)` PAIRS.
	 *
	 * A NESTED STRUCT'S VALUE CONTAINS COMMAS AND ITS OWN PARENTHESES, so a plain split on a comma would cut a
	 * struct field in half and the import would then fail on text this function had corrupted. Depth is tracked and
	 * only depth-1 commas separate fields. **THE NESTED CASE IS THE EXPECTED CASE RATHER THAN AN EDGE CASE, because
	 * `Spec/06_ACCEPTANCE.md` gives TA-14 four fields and says ONE OF THEM IS A STRUCT TYPE.**
	 */
	void SplitTopLevel(const FString& Body, TArray<FString>& Out)
	{
		int32 Depth = 0;
		bool bInQuotes = false;
		FString Current;

		for (int32 Index = 0; Index < Body.Len(); ++Index)
		{
			const TCHAR Ch = Body[Index];

			// A QUOTED STRING MAY CONTAIN COMMAS AND PARENTHESES THAT ARE DATA RATHER THAN STRUCTURE.
			if (Ch == TCHAR('"'))
			{
				bInQuotes = !bInQuotes;
				Current.AppendChar(Ch);
				continue;
			}

			if (!bInQuotes)
			{
				if (Ch == TCHAR('('))
				{
					++Depth;
				}
				else if (Ch == TCHAR(')'))
				{
					--Depth;
				}
				else if (Ch == TCHAR(',') && Depth == 0)
				{
					Out.Add(Current);
					Current.Reset();
					continue;
				}
			}

			Current.AppendChar(Ch);
		}

		if (!Current.IsEmpty())
		{
			Out.Add(Current);
		}
	}

	/** The name to the left of the first `=`, trimmed. Empty when there is no `=` at all. */
	FString PairName(const FString& Pair)
	{
		int32 Equals = INDEX_NONE;
		if (!Pair.FindChar(TCHAR('='), Equals))
		{
			return FString();
		}
		return Pair.Left(Equals).TrimStartAndEnd();
	}

	/** True when two field names are the same field. */
	bool SameField(const FString& A, const FString& B)
	{
		// `FName` COMPARISON IS CASE-INSENSITIVE. The struct's fields come back from the engine and the caller's
		// spelling comes from a person, so comparing case-sensitively would refuse `health` for a field the engine
		// calls `Health` - A REFUSAL ABOUT SPELLING RATHER THAN ABOUT THE REQUEST.
		return A.Equals(B, ESearchCase::IgnoreCase);
	}

	/** Every field name the struct declares, for a refusal that names the real ones. */
	FString FieldNames(const UScriptStruct* Struct)
	{
		TArray<FString> Names;
		for (TFieldIterator<FProperty> It(Struct); It; ++It)
		{
			Names.Add(It->GetName());
		}
		return FString::Join(Names, TEXT(", "));
	}

	/** True when the struct declares this field. */
	bool StructHasField(const UScriptStruct* Struct, const FString& Name)
	{
		for (TFieldIterator<FProperty> It(Struct); It; ++It)
		{
			if (SameField(It->GetName(), Name))
			{
				return true;
			}
		}
		return false;
	}

	/** The struct's own spelling of a field the caller may have spelled any way. */
	FString CanonicalField(const UScriptStruct* Struct, const FString& Name)
	{
		for (TFieldIterator<FProperty> It(Struct); It; ++It)
		{
			if (SameField(It->GetName(), Name))
			{
				return It->GetName();
			}
		}
		return Name;
	}

	/** The engine's text for one field of a row, so the preview can print what is there now. */
	FString OneFieldText(const UScriptStruct* Struct, const uint8* Row, const FString& FieldName)
	{
		if (Row == nullptr)
		{
			return FString();
		}

		for (TFieldIterator<FProperty> It(Struct); It; ++It)
		{
			if (SameField(It->GetName(), FieldName))
			{
				FString Text;
				It->ExportText_InContainer(0, Text, Row, nullptr, nullptr, CopyFlags);
				return Text;
			}
		}
		return FString();
	}

	/**
	 * THE WHOLE ROW AS THE ENGINE PRINTS IT, which is the base the merge works on.
	 *
	 * `UScriptStruct::ExportText` (`Class.h:2443`) is the matching half of the `ImportText` the values go through,
	 * so the text this function produces is text the engine is guaranteed to accept back - WHICH IS THE WHOLE REASON
	 * A FIELD THE CALLER DID NOT MENTION SURVIVES: it is in the export, so it is in the import.
	 */
	FString WholeRowText(const UScriptStruct* Struct, const uint8* Row, UObject* Owner)
	{
		FString Text;
		Struct->ExportText(Text, Row, nullptr, Owner, CopyFlags, nullptr);
		return Text;
	}

	/**
	 * The text for a row that does not exist yet: THE STRUCT'S OWN DEFAULTS, so an unmentioned field of a new row
	 * gets its default rather than an indeterminate value.
	 *
	 * *** AND THIS IS WHERE A GUESS WAS WRONG, WHICH IS WHY THE ENGINE IS READ RATHER THAN RECALLED. *** The first
	 * version called `UScriptStruct::GetDefaultInstance()`. MEASURED: THAT METHOD DOES NOT EXIST -
	 * `error C2039: 'GetDefaultInstance' is not a member of 'UScriptStruct'`. What DOES exist is
	 * `UStruct::InitializeStruct(void*, int32)` (`Class.h:754`) and `UStruct::DestroyStruct(void*, int32)`
	 * (`Class.h:763`), which apply and release the struct's defaults IN PLACE. So the buffer is allocated, the
	 * DEFAULTS ARE APPLIED TO IT rather than the buffer being assumed to hold them, it is exported, and it is
	 * destroyed - so a nested struct field gets its own defaults applied instead of being zeroed.
	 */
	FString DefaultRowText(const UScriptStruct* Struct)
	{
		const int32 Size = Struct->GetStructureSize();
		if (Size <= 0)
		{
			return FString();
		}

		uint8* Buffer = (uint8*)FMemory::Malloc(Size, Struct->GetMinAlignment());
		// THE DEFAULTS ARE APPLIED, NOT ASSUMED: `InitializeStruct` is what the engine itself calls to make a
		// struct instance valid, so this cannot disagree with what `AddRow` will produce.
		Struct->InitializeStruct(Buffer);

		FString Text;
		Struct->ExportText(Text, Buffer, nullptr, nullptr, CopyFlags, nullptr);

		// RELEASED WITH THE MATCHING CALL, so a struct holding a container does not leak it.
		Struct->DestroyStruct(Buffer);
		FMemory::Free(Buffer);

		return Text;
	}

	/** Every row name, for a refusal that names them. */
	FString RowNames(const TMap<FName, uint8*>& Rows)
	{
		TArray<FString> Names;
		for (const TPair<FName, uint8*>& Pair : Rows)
		{
			Names.Add(Pair.Key.ToString());
		}
		return Names.Num() > 0 ? FString::Join(Names, TEXT(", ")) : TEXT("(none)");
	}

	/** Merge the caller's `Field=Value` pairs into the engine's own export of the row. */
	void MergeIntoExport(const FString& Exported, const TArray<FFieldEdit>& Fields, FString& Out)
	{
		// The export is `(A=1,B=2)`. Strip ONE outer pair of parentheses; anything else is left alone and the
		// import will report it, which is better than this function guessing at a shape it did not expect.
		FString Body = Exported;
		if (Body.Len() >= 2 && Body[0] == TCHAR('(') && Body[Body.Len() - 1] == TCHAR(')'))
		{
			Body = Body.Mid(1, Body.Len() - 2);
		}

		TArray<FString> Pairs;
		SplitTopLevel(Body, Pairs);

		// A field the caller named is REPLACED IN PLACE, so the order the engine printed is preserved and the
		// preview's text and the written text cannot disagree about order.
		for (const FFieldEdit& Edit : Fields)
		{
			bool bReplaced = false;
			for (FString& Pair : Pairs)
			{
				if (SameField(PairName(Pair), Edit.Name))
				{
					Pair = FString::Printf(TEXT("%s=%s"), *Edit.Name, *Edit.Text);
					bReplaced = true;
					break;
				}
			}
			if (!bReplaced)
			{
				Pairs.Add(FString::Printf(TEXT("%s=%s"), *Edit.Name, *Edit.Text));
			}
		}

		Out = FString::Printf(TEXT("(%s)"), *FString::Join(Pairs, TEXT(",")));
	}

	/** Import text into an existing struct instance, returning the engine's own error text when it refuses. */
	bool ImportRow(const UScriptStruct* Struct, uint8* Row, UObject* Owner, const FString& Text, FString& OutError)
	{
		FRowLogCapture Capture;
		const TCHAR* After = Struct->ImportText(*Text, Row, Owner, CopyFlags, &Capture,
			Struct->GetPathName());

		if (After == nullptr)
		{
			OutError = FString::Printf(
				TEXT("the row struct refused '%s'. The accepted spelling is the struct's own export text, for ")
				TEXT("example (Health=100,Name=\"x\"). Its fields are: %s%s"),
				*Text, *FieldNames(Struct),
				Capture.Text.IsEmpty() ? TEXT("") : *FString::Printf(TEXT(". The engine said: %s"), *Capture.Text));
			return false;
		}
		return true;
	}
}

FString Parse(const TArray<FString>& Lines, TArray<FRowOp>& Out)
{
	if (Lines.Num() == 0)
	{
		return TEXT("error: no row operations given. Each line is `name = field=value, field=value` to set a row ")
			TEXT("or `- name` to remove one.");
	}

	for (const FString& Raw : Lines)
	{
		const FString Line = Raw.TrimStartAndEnd();
		if (Line.IsEmpty())
		{
			continue;
		}

		FRowOp Op;
		Op.Source = Line;

		// A LEADING `-` IS THE REMOVAL FORM, and it is spelled out in the refusal below so a caller who wrote
		// something else is told BOTH forms rather than only the one they missed.
		if (Line.StartsWith(TEXT("-")))
		{
			Op.Op = ERowOp::Remove;
			Op.RowName = Line.RightChop(1).TrimStartAndEnd();
			if (Op.RowName.IsEmpty())
			{
				return FString::Printf(TEXT("error: '%s' removes a row but names none."), *Line);
			}
			Out.Add(MoveTemp(Op));
			continue;
		}

		int32 Equals = INDEX_NONE;
		if (!Line.FindChar(TCHAR('='), Equals))
		{
			return FString::Printf(
				TEXT("error: '%s' is neither form. Write `name = field=value, field=value` to set a row, ")
				TEXT("or `- name` to remove one."), *Line);
		}

		Op.Op = ERowOp::Set;
		Op.RowName = Line.Left(Equals).TrimStartAndEnd();
		if (Op.RowName.IsEmpty())
		{
			return FString::Printf(TEXT("error: '%s' sets fields but names no row."), *Line);
		}

		const FString Body = Line.RightChop(Equals + 1).TrimStartAndEnd();
		if (Body.IsEmpty())
		{
			return FString::Printf(
				TEXT("error: '%s' names a row and gives no field. Naming no field changes nothing, which is not ")
				TEXT("something to ask for."), *Line);
		}

		TArray<FString> Pairs;
		SplitTopLevel(Body, Pairs);
		for (const FString& Pair : Pairs)
		{
			const FString Trimmed = Pair.TrimStartAndEnd();
			if (Trimmed.IsEmpty())
			{
				continue;
			}

			int32 FieldEquals = INDEX_NONE;
			if (!Trimmed.FindChar(TCHAR('='), FieldEquals))
			{
				return FString::Printf(
					TEXT("error: '%s' has no `=` between a field name and its value. Write `field=value`."),
					*Trimmed);
			}

			FFieldEdit Edit;
			Edit.Name = Trimmed.Left(FieldEquals).TrimStartAndEnd();
			Edit.Text = Trimmed.RightChop(FieldEquals + 1).TrimStartAndEnd();
			if (Edit.Name.IsEmpty())
			{
				return FString::Printf(TEXT("error: '%s' has a value and no field name."), *Trimmed);
			}
			Op.Fields.Add(MoveTemp(Edit));
		}

		Out.Add(MoveTemp(Op));
	}

	if (Out.Num() == 0)
	{
		return TEXT("error: every line was empty, so there is nothing to do.");
	}
	return FString();
}

FString Plan(UDataTable* Table, const TArray<FRowOp>& Ops, FPlan& Out)
{
	if (Table == nullptr)
	{
		return TEXT("error: no data table.");
	}

	const UScriptStruct* Struct = Table->GetRowStruct();
	if (Struct == nullptr)
	{
		return TEXT("error: the table has no row struct, which happens when the struct it was built from was ")
			TEXT("deleted. Its rows cannot be interpreted, so nothing can be written.");
	}

	const TMap<FName, uint8*>& Rows = Table->GetRowMap();

	// ROWS THIS CALL WOULD ITSELF ADD, so an op later in the same call may address one. THIS IS THE ONLY WAY A NAME
	// THAT IS NOT IN THE TABLE YET CAN BE LEGITIMATE, and keeping it as a SET OF NAMES rather than as a growing
	// table is what keeps the plan from depending on writes that have not happened.
	TSet<FString> PlannedAdditions;

	for (const FRowOp& Op : Ops)
	{
		Out.Ops.Add(Op);

		const FName RowName(*Op.RowName);
		const bool bExistsNow = Rows.Contains(RowName);
		const bool bSetEarlier = PlannedAdditions.Contains(Op.RowName);
		const bool bWillExist = bExistsNow || bSetEarlier;

		if (Op.Op == ERowOp::Remove)
		{
			if (!bWillExist)
			{
				Out.Refusals.Add(FString::Printf(
					TEXT("'%s' removes the row '%s', which the table does not have and this call does not add. ")
					TEXT("Its rows are: %s"),
					*Op.Source, *Op.RowName, *RowNames(Rows)));
				continue;
			}
			// BY NAME, NEVER A COUNT. `D-45`: a count says how much is gone and a list says what.
			Out.Removals.AddUnique(Op.RowName);
			continue;
		}

		// A FIELD THE STRUCT DOES NOT DECLARE IS REFUSED WITH THE REAL FIELD NAMES, which the spec requires.
		for (const FFieldEdit& Edit : Op.Fields)
		{
			if (!StructHasField(Struct, Edit.Name))
			{
				Out.Refusals.Add(FString::Printf(
					TEXT("'%s' names the field '%s', which the row struct does not declare. Its fields are: %s"),
					*Op.Source, *Edit.Name, *FieldNames(Struct)));
			}
		}

		for (const FFieldEdit& Edit : Op.Fields)
		{
			FFieldChange Change;
			Change.RowName = Op.RowName;
			Change.Field = CanonicalField(Struct, Edit.Name);
			Change.NewText = Edit.Text;

			if (bExistsNow)
			{
				const uint8* Row = Rows.FindChecked(RowName);
				Change.OldText = OneFieldText(Struct, Row, Edit.Name);
				// A SET THAT CHANGES NOTHING IS REPORTED AND FLAGGED rather than hidden, because a caller who
				// asked for it should be able to see that the table already agreed.
				Change.bUnchanged = Change.OldText.Equals(Edit.Text, ESearchCase::IgnoreCase);
			}
			else if (bSetEarlier)
			{
				Change.OldText = TEXT("(set earlier in this call)");
			}
			else
			{
				Change.OldText = TEXT("(new row)");
				Out.Additions.AddUnique(Op.RowName);
			}

			Out.Changes.Add(MoveTemp(Change));
		}

		PlannedAdditions.Add(Op.RowName);
	}

	return FString();
}

FString Apply(UDataTable* Table, const FPlan& Plan, FString& OutReport)
{
	if (Table == nullptr)
	{
		return TEXT("error: no data table.");
	}
	if (!Plan.bAcceptable())
	{
		return TEXT("error: the plan has refusals, so nothing was written.");
	}

	const UScriptStruct* Struct = Table->GetRowStruct();
	if (Struct == nullptr)
	{
		return TEXT("error: the table has no row struct.");
	}

	// *** THE RE-RESOLUTION, AND IT IS `D-45`'S REQUIREMENT RATHER THAN A PRECAUTION. *** Every name in the plan is
	// looked up AGAIN here, against the table as it is NOW. `Plan` and `Apply` are TWO CALLS and the table can
	// change between them - another editor, another tool - SO A PLAN THAT STORED A POINTER OR AN OFFSET WOULD WRITE
	// INTO WHATEVER NOW OCCUPIES THAT POSITION. A NAME CANNOT BE STALE IN THAT WAY.
	//
	// AND IT IS WHY THE INDEX SHIFT IS NOT A PROBLEM HERE: the storage is a `TMap<FName, uint8*>`, which has NO
	// ORDER TO SHIFT, and NOTHING IN THIS FILE CONSULTS A ROW'S POSITION. Removing a row cannot invalidate any
	// other row's identity because identity IS the name.
	TMap<FName, FString> FinalText;
	TArray<FString> Order;

	for (const FRowOp& Op : Plan.Ops)
	{
		if (Op.Op != ERowOp::Set)
		{
			continue;
		}

		const FName RowName(*Op.RowName);
		if (!Order.Contains(Op.RowName))
		{
			Order.Add(Op.RowName);
		}

		// EVERY SET IS FOLDED INTO ONE FINAL TEXT PER ROW, because a row named twice in one call must end with the
		// LAST value rather than with whichever write happened to run last.
		const FString Base = FinalText.Contains(RowName)
			? FinalText.FindChecked(RowName)
			: (Table->GetRowMap().Contains(RowName)
				? WholeRowText(Struct, Table->GetRowMap().FindChecked(RowName), Table)
				: DefaultRowText(Struct));

		FString Merged;
		MergeIntoExport(Base, Op.Fields, Merged);
		FinalText.Add(RowName, Merged);
	}

	const int32 RowsBefore = Table->GetRowMap().Num();

	// ONE TRANSACTION: the removals and the sets, with no early exit, so a failure part-way is REPORTED rather than
	// silently leaving half the request applied.
	Table->Modify();

	int32 Removed = 0;
	for (const FString& Name : Plan.Removals)
	{
		if (FDataTableEditorUtils::RemoveRow(Table, FName(*Name)))
		{
			++Removed;
		}
	}

	int32 Written = 0;
	for (const TPair<FName, FString>& Pair : FinalText)
	{
		uint8* Row = Table->GetRowMap().FindRef(Pair.Key);
		if (Row == nullptr)
		{
			// `AddRow` initialises the new row from the struct's OWN defaults, which is what the engine's editor
			// does, so an unmentioned field of a NEW row gets its default rather than an indeterminate value.
			Row = FDataTableEditorUtils::AddRow(Table, Pair.Key);
		}
		if (Row == nullptr)
		{
			return FString::Printf(TEXT("error: the engine refused to add the row '%s'."), *Pair.Key.ToString());
		}

		FString ImportError;
		if (!ImportRow(Struct, Row, Table, Pair.Value, ImportError))
		{
			return FString::Printf(TEXT("error: row '%s': %s"), *Pair.Key.ToString(), *ImportError);
		}
		++Written;
	}

	Table->MarkPackageDirty();

	// AND THE READ BACK, THROUGH THE TABLE rather than through this function's own variables, so the report is a
	// MEASUREMENT OF THE RESULT rather than a restatement of the request. The spec asks for exactly this: *"then
	// re-reads the row names and each changed value back from the table."*
	FString ReadBack;
	for (const FString& Name : Order)
	{
		const uint8* Row = Table->GetRowMap().FindRef(FName(*Name));
		ReadBack += FString::Printf(TEXT("\n  %s = %s"), *Name,
			Row != nullptr ? *WholeRowText(Struct, Row, Table) : TEXT("MISSING AFTER WRITE"));
	}

	OutReport = FString::Printf(
		TEXT("written: %d row(s) set, %d removed; the table had %d row(s) and now has %d")
		TEXT("\nread back from the table:%s"),
		Written, Removed, RowsBefore, Table->GetRowMap().Num(), *ReadBack);

	return FString();
}

} // namespace FTurboMCPDataTableRows
