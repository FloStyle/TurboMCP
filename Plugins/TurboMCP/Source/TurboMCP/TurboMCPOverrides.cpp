// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "Tools/TurboMCPToolsets.h"

#include "BlueprintEditorLibrary.h"
#include "Containers/UnrealString.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "UObject/Class.h"
#include "UObject/Object.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UnrealType.h"

#include "TurboMCPAssetResolver.h"

/**
 * R-OVERRIDES: what can be overridden.
 *
 * `03_TOOLS.md` asks for the functions and events of the parent chain and of the
 * implemented interfaces, with full signatures, inputs and outputs marked
 * correctly, and whether each one is already overridden here.
 *
 * THE ENGINE ALREADY COMPUTES THE SET, and this file uses it rather than
 * reproducing it. `UBlueprintEditorLibrary::ListEvents` and `ListFunctions` are
 * documented as listing "locally defined function graphs, overridable parent-class
 * functions, and functions declared on interfaces (both explicitly implemented and
 * inherited through a natively-implemented interface)", each carrying an
 * `bIsImplemented` flag.
 *
 * The editor set also includes local functions. Only entries with a resolved
 * parent/interface declaration belong in override counts. Unresolved entries are
 * reported separately rather than guessed to be overrides.
 *
 * What this file adds is what the spec asks for beyond the set: the signature, with
 * inputs and outputs marked, and the class that declares the original so a caller
 * can write a Super call.
 */
namespace
{
	/** How many functions are printed before the list caps. */
	constexpr int32 MaxFunctions = 200;

	/** How many parameters are printed per signature. */
	constexpr int32 MaxParams = 24;

	/** How many characters one result may hold. */
	constexpr int32 MaxResultChars = 30000;

	/** One entry, after the engine's set has been enriched. */
	struct FEntry
	{
		FName Name;
		bool bIsEvent = false;
		bool bIsImplemented = false;

		/** Null when the name resolves to nothing reflected, which is reported. */
		const UFunction* Function = nullptr;

		/** The class that declares it, so a Super call can be written against it. */
		const UClass* DeclaringClass = nullptr;
	};

	/**
	 * The class that DECLARES a function, searched from the parent chain.
	 *
	 * `GetOverrideFunctionClass` answers "which class would I write Super:: on",
	 * and once a Blueprint overrides a function it answers the Blueprint's own
	 * generated class. That is the wrong answer here: a caller reading
	 * `from: /Game/.../BP_NodeTest.BP_NodeTest_C` would write
	 * `BP_NodeTest_C::ReceiveBeginPlay`, which does not exist.
	 *
	 * A native interface is included in the walk, because an interface member's
	 * declaring class is the interface.
	 */
	const UClass* FindDeclaringClass(const UBlueprint* InBlueprint, FName InName)
	{
		for (const UClass* Class = InBlueprint->ParentClass; Class != nullptr;
			Class = Class->GetSuperClass())
		{
			// Only the class's own functions, so the nearest declaration wins.
			// FindFunctionByName takes the flag second, not first.
			if (Class->FindFunctionByName(InName, EIncludeSuperFlag::ExcludeSuper) != nullptr)
			{
				return Class;
			}
		}

		// Interfaces inherited through a native or Blueprint parent need not be
		// present in the Blueprint's directly implemented interface list.
		for (const UClass* Class = InBlueprint->ParentClass; Class != nullptr;
			Class = Class->GetSuperClass())
		{
			for (const FImplementedInterface& Interface : Class->Interfaces)
			{
				if (Interface.Class != nullptr)
				{
					if (const UFunction* Function = Interface.Class->FindFunctionByName(InName))
					{
						return Function->GetOwnerClass();
					}
				}
			}
		}

		for (const FBPInterfaceDescription& Interface : InBlueprint->ImplementedInterfaces)
		{
			if (Interface.Interface != nullptr
				&& Interface.Interface->FindFunctionByName(InName) != nullptr)
			{
				return Interface.Interface;
			}
		}

		return nullptr;
	}

	/** The reflected function for a name, or null. */
	const UFunction* ResolveFunction(const UBlueprint* InBlueprint, FName InName)
	{
		// The skeleton class first, because it is the one that exists while editing.
		if (InBlueprint->SkeletonGeneratedClass != nullptr)
		{
			if (const UFunction* Found =
					InBlueprint->SkeletonGeneratedClass->FindFunctionByName(InName))
			{
				return Found;
			}
		}
		if (const UClass* Declaring = FindDeclaringClass(InBlueprint, InName))
		{
			return Declaring->FindFunctionByName(InName);
		}
		return nullptr;
	}

	/** A function's parameters, with direction marked. */
	FString DescribeParameters(const UFunction* InFunction)
	{
		if (InFunction == nullptr)
		{
			// A name with no reflected function is not a failure: an interface member
			// can be declared without a Function entry. Saying so beats an empty pair
			// of brackets that looks like "takes nothing".
			return FString();
		}

		TArray<FString> Ins;
		TArray<FString> Outs;

		int32 Shown = 0;
		for (TFieldIterator<FProperty> It(InFunction); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
		{
			if (Shown >= MaxParams)
			{
				break;
			}
			++Shown;

			const FProperty* Property = *It;
			const FString Type = Property->GetCPPType();
			const FString Name = Property->GetName();

			if (Property->HasAnyPropertyFlags(CPF_ReturnParm | CPF_OutParm))
			{
				Outs.Add(FString::Printf(TEXT("%s %s"), *Type, *Name));
			}
			else
			{
				Ins.Add(FString::Printf(TEXT("%s %s"), *Type, *Name));
			}
		}

		FString Out = FString::Printf(TEXT("(%s)"), *FString::Join(Ins, TEXT(", ")));
		if (Outs.Num() > 0)
		{
			Out += FString::Printf(TEXT(" -> (%s)"), *FString::Join(Outs, TEXT(", ")));
		}
		return Out;
	}
}

FString UTurboMCPReadToolset::ReadTurboMCPOverrides(
	const FString& Asset,
	const FString& Include)
{
	if (Asset.IsEmpty())
	{
		return TEXT("error: no asset given. Pass a Blueprint in any path form, for ")
			TEXT("example BP_NodeTest.");
	}

	// The include filter is validated before anything loads, so a typo costs
	// nothing and the error names every accepted value.
	const FString Wanted = Include.TrimStartAndEnd().ToLower();
	int32 Mode = 0;   // 0 = all, 1 = overridden, 2 = available
	if (Wanted.IsEmpty() || Wanted == TEXT("all"))
	{
		Mode = 0;
	}
	else if (Wanted == TEXT("overridden"))
	{
		Mode = 1;
	}
	else if (Wanted == TEXT("available") || Wanted == TEXT("not-overridden"))
	{
		Mode = 2;
	}
	else
	{
		return FString::Printf(
			TEXT("error: '%s' is not an include filter. Accepted: all, overridden, ")
			TEXT("available. Leave it empty for all."), *Include);
	}

	FTurboMCPAssetResolver::FResolved Resolved;
	const FString ResolveError = FTurboMCPAssetResolver::Resolve(Asset, Resolved);
	if (!ResolveError.IsEmpty())
	{
		return ResolveError;
	}

	UBlueprint* Blueprint = Cast<UBlueprint>(
		LoadObject<UObject>(nullptr, *Resolved.ObjectPath));
	if (Blueprint == nullptr)
	{
		return FString::Printf(
			TEXT("error: %s is not a Blueprint, so it has no parent chain to ")
			TEXT("override."), *Resolved.ObjectPath);
	}

	// The engine's own set, from the editor's own implementation, so a
	// natively-implemented interface is covered without a rule written here.
	TArray<FEntry> Entries;
	TMap<FName, int32> IndexByName;

	auto AddInfo = [&Entries, &IndexByName](const FBlueprintFunctionInfo& Info, bool bIsEvent)
	{
		const int32* Existing = IndexByName.Find(Info.Name);
		if (Existing != nullptr)
		{
			// A name can appear in both lists, an event that is also callable being
			// the normal case. The event flag is kept, because it is the more
			// specific statement.
			if (bIsEvent)
			{
				Entries[*Existing].bIsEvent = true;
			}
			return;
		}

		FEntry Entry;
		Entry.Name = Info.Name;
		Entry.bIsEvent = bIsEvent;
		Entry.bIsImplemented = Info.bIsImplemented;
		IndexByName.Add(Info.Name, Entries.Num());
		Entries.Add(MoveTemp(Entry));
	};

	for (const FBlueprintFunctionInfo& Info : UBlueprintEditorLibrary::ListEvents(Blueprint))
	{
		AddInfo(Info, /*bIsEvent=*/true);
	}
	for (const FBlueprintFunctionInfo& Info : UBlueprintEditorLibrary::ListFunctions(Blueprint))
	{
		AddInfo(Info, /*bIsEvent=*/false);
	}

	// Enriched after collection, so a lookup never runs twice and never runs while
	// the list is being built.
	for (FEntry& Entry : Entries)
	{
		Entry.Function = ResolveFunction(Blueprint, Entry.Name);
		Entry.DeclaringClass = FindDeclaringClass(Blueprint, Entry.Name);
	}

	TArray<FString> ExcludedNames;
	Entries.RemoveAll([&ExcludedNames](const FEntry& Entry)
	{
		if (Entry.DeclaringClass != nullptr)
		{
			return false;
		}
		ExcludedNames.Add(Entry.Name.ToString());
		return true;
	});
	ExcludedNames.Sort();

	// Sorted by name so two reads of an unchanged Blueprint give the same text.
	Entries.Sort([](const FEntry& A, const FEntry& B)
	{
		return A.Name.ToString() < B.Name.ToString();
	});

	int32 Overridden = 0;
	for (const FEntry& Entry : Entries)
	{
		if (Entry.bIsImplemented)
		{
			++Overridden;
		}
	}

	// Counted before the filter, so a caller asking for `available` still learns how
	// many exist in total.
	const int32 Total = Entries.Num();

	FString Out = FString::Printf(TEXT("read: overridable functions and events of %s"),
		*Resolved.ObjectPath);

	Out += FString::Printf(TEXT("\nparent: %s"), Blueprint->ParentClass != nullptr
		? *Blueprint->ParentClass->GetPathName() : TEXT("(none)"));

	if (Blueprint->ImplementedInterfaces.Num() == 0)
	{
		Out += TEXT("\ninterfaces: none implemented directly");
	}
	else
	{
		TArray<FString> Names;
		for (const FBPInterfaceDescription& Interface : Blueprint->ImplementedInterfaces)
		{
			Names.Add(Interface.Interface != nullptr
				? Interface.Interface->GetPathName() : TEXT("(null)"));
		}
		Out += FString::Printf(TEXT("\ninterfaces: %s"),
			*FString::Join(Names, TEXT(", ")));
	}

	Out += TEXT("\n; the set comes from the editor's own ListEvents and ListFunctions, ");
	Out += TEXT("filtered to resolved parent/interface declarations.");
	if (!ExcludedNames.IsEmpty())
	{
		Out += FString::Printf(TEXT("\n; excluded from override counts (local or unresolved origin): %s"),
			*FString::Join(ExcludedNames, TEXT(", ")));
	}

	Out += FString::Printf(
		TEXT("\ncounts: %d overridable, %d already overridden here, %d still available"),
		Total, Overridden, Total - Overridden);

	int32 Shown = 0;
	int32 Matched = 0;

	for (const FEntry& Entry : Entries)
	{
		if (Mode == 1 && !Entry.bIsImplemented)
		{
			continue;
		}
		if (Mode == 2 && Entry.bIsImplemented)
		{
			continue;
		}
		++Matched;

		if (Shown >= MaxFunctions)
		{
			continue;
		}
		++Shown;

		Out += FString::Printf(TEXT("\n%s : %s%s"),
			*Entry.Name.ToString(),
			*DescribeParameters(Entry.Function),
			Entry.bIsEvent ? TEXT("  [event]") : TEXT(""));

		Out += FString::Printf(TEXT("\n    state: %s"),
			Entry.bIsImplemented ? TEXT("overridden here") : TEXT("available"));

		if (Entry.DeclaringClass != nullptr)
		{
			Out += FString::Printf(TEXT("\n    from: %s"),
				*Entry.DeclaringClass->GetPathName());
		}
		else
		{
			// Naming the gap rather than printing nothing, so a caller knows the
			// lookup ran and failed rather than that the field is unused.
			Out += TEXT("\n    from: unknown, no class in the chain declares this name");
		}
	}

	if (Mode != 0)
	{
		Out += FString::Printf(TEXT("\nshown: %d of %d matching the '%s' filter"),
			Shown, Matched, *Wanted);
	}

	if (Matched == 0)
	{
		// `P1`: a filter that matched nothing says so and says what would work,
		// rather than looking like a Blueprint with nothing to override.
		Out += FString::Printf(
			TEXT("\nNo function matched the '%s' filter. %d are overridable in ")
			TEXT("total; drop the filter to see them."),
			*Wanted, Total);
	}
	else if (Matched > Shown)
	{
		Out += FString::Printf(
			TEXT("\n(%d more, use the include filter to narrow the list)"),
			Matched - Shown);
	}

	if (Total == 0)
	{
		// A real answer about the parent chain rather than a failure, and worth
		// saying plainly so a zero is not read as a broken tool.
		Out += TEXT("\n; no parent/interface override declaration was resolved. A ");
		Out += TEXT("parent with no Blueprint-exposed events and no implemented ");
		Out += TEXT("interfaces is normal.");
	}

	if (Out.Len() > MaxResultChars)
	{
		const int32 Dropped = Out.Len() - MaxResultChars;
		Out = Out.Left(MaxResultChars);
		Out += FString::Printf(
			TEXT("\n\n; (%d more character(s) were dropped; use the include filter to ")
			TEXT("narrow the list)"), Dropped);
	}

	return Out;
}
