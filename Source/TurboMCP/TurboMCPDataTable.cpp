// Copyright FloStyle. All Rights Reserved.
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "Tools/TurboMCPToolsets.h"

#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetIdentifier.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Containers/UnrealString.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "Engine/Blueprint.h"
#include "Engine/DataTable.h"
#include "K2Node_GetDataTableRow.h"
#include "UObject/Class.h"
#include "UObject/Object.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UnrealType.h"

#include "TurboMCPAssetResolver.h"

/**
 * R-DATATABLE: describe a data table.
 *
 * `03_TOOLS.md` asks for the row struct with its fields and types, the row count,
 * the row names capped, and the graphs in loaded Blueprints that read the table.
 * **Row values are explicitly out of scope**, and are not printed.
 *
 * The readers are found in two steps rather than one:
 *
 * 1. the asset registry's referencers give every asset that depends on the table,
 *    which is complete and does not need the asset loaded;
 * 2. those that are loaded Blueprints are then searched for
 *    `UK2Node_GetDataTableRow`, which is what turns an asset into the exact graphs.
 *
 * Only the second step can name a graph, and only the first is complete, so both
 * run and the result says which was used. A referencer that is not a loaded
 * Blueprint is still reported, because "something uses this" is a fact even when
 * the graph cannot be named.
 */
namespace
{
	/** How many row names are printed before the list caps. */
	constexpr int32 MaxRowNames = 60;

	/** How many struct fields are printed before the list caps. */
	constexpr int32 MaxFields = 80;

	/** How many reader graphs are printed before the list caps. */
	constexpr int32 MaxReaders = 40;

	/** How many characters one result may hold. */
	constexpr int32 MaxResultChars = 24000;

	/** A reader of the table, once found. */
	struct FReader
	{
		FString AssetPath;
		FString ClassName;

		/** Empty when the graph could not be named, which is reported. */
		FString GraphName;

		/** True when the asset is loaded, so its graphs could be searched. */
		bool bWasSearched = false;
	};

	/** One field of the row struct. */
	struct FRowField
	{
		FString Name;
		FString Type;
	};

	/** True for a package this tool should not list. */
	bool IsNoise(const FString& InPackageName)
	{
		return InPackageName.StartsWith(TEXT("/Script/"))
			|| InPackageName.StartsWith(TEXT("/Engine/"))
			|| InPackageName.StartsWith(TEXT("/Temp/"))
			|| InPackageName.StartsWith(TEXT("/Transient"));
	}
}

FString UTurboMCPReadToolset::ReadTurboMCPDataTable(
	const FString& Asset,
	const FString& Include)
{
	if (Asset.IsEmpty())
	{
		return TEXT("error: no asset given. Pass a DataTable in any path form, for ")
			TEXT("example DT_Items.");
	}

	// The include filter is validated before anything loads, so a typo costs
	// nothing and the error names every accepted value.
	const FString Wanted = Include.TrimStartAndEnd().ToLower();
	int32 Mode = 0;   // 0 = all, 1 = readers only, 2 = schema only
	if (Wanted.IsEmpty() || Wanted == TEXT("all"))
	{
		Mode = 0;
	}
	else if (Wanted == TEXT("readers"))
	{
		Mode = 1;
	}
	else if (Wanted == TEXT("schema"))
	{
		Mode = 2;
	}
	else
	{
		return FString::Printf(
			TEXT("error: '%s' is not an include filter. Accepted: all, readers, ")
			TEXT("schema. Leave it empty for all."), *Include);
	}

	FTurboMCPAssetResolver::FResolved Resolved;
	const FString ResolveError = FTurboMCPAssetResolver::Resolve(Asset, Resolved);
	if (!ResolveError.IsEmpty())
	{
		return ResolveError;
	}

	UObject* Loaded = LoadObject<UObject>(nullptr, *Resolved.ObjectPath);
	UDataTable* Table = Cast<UDataTable>(Loaded);
	if (Table == nullptr)
	{
		return FString::Printf(
			TEXT("error: %s is a %s, and only DataTables can be described by this ")
			TEXT("tool."),
			*Resolved.ObjectPath,
			Loaded != nullptr ? *Loaded->GetClass()->GetName() : TEXT("missing object"));
	}

	FString Out = FString::Printf(TEXT("read: data table %s"), *Resolved.ObjectPath);

	if (Mode != 1)
	{
		// The row struct, with its fields and types. `GetRowStruct` can be null on a
		// table whose struct was deleted, so that is reported rather than left as an
		// empty section.
		const UScriptStruct* RowStruct = Table->GetRowStruct();
		if (RowStruct == nullptr)
		{
			Out += TEXT("\nrow struct: none. The table has no row struct, which happens ");
			Out += TEXT("when the struct it was built from was deleted. Its rows cannot ");
			Out += TEXT("be interpreted until one is set.");
		}
		else
		{
			Out += FString::Printf(TEXT("\nrow struct: %s"),
				*RowStruct->GetPathName());

			TArray<FRowField> Fields;
			for (TFieldIterator<FProperty> It(RowStruct); It; ++It)
			{
				const FProperty* Property = *It;
				FRowField Field;
				Field.Name = Property->GetName();
				Field.Type = Property->GetCPPType();
				Fields.Add(MoveTemp(Field));
			}

			Out += FString::Printf(TEXT("\nfields: %d"), Fields.Num());
			if (Fields.Num() == 0)
			{
				// A struct with no fields is a real answer, not a missing one.
				Out += TEXT("\n  (the row struct declares no fields)");
			}
			int32 ShownFields = 0;
			for (const FRowField& Field : Fields)
			{
				if (ShownFields >= MaxFields)
				{
					Out += FString::Printf(TEXT("\n  (%d more field(s) not shown)"),
						Fields.Num() - ShownFields);
					break;
				}
				++ShownFields;
				Out += FString::Printf(TEXT("\n  %s : %s"), *Field.Name, *Field.Type);
			}
		}

		// Row names, capped. The count is read from the map so it is exact even when
		// the list is cut.
		const TMap<FName, uint8*>& RowMap = Table->GetRowMap();
		Out += FString::Printf(TEXT("\nrows: %d"), RowMap.Num());

		if (RowMap.Num() == 0)
		{
			Out += TEXT("\n  (the table has no rows)");
		}

		TArray<FName> RowNames;
		RowNames.Reserve(RowMap.Num());
		for (const TPair<FName, uint8*>& Pair : RowMap)
		{
			RowNames.Add(Pair.Key);
		}
		// Sorted so two reads of an unchanged table give the same text.
		RowNames.Sort([](const FName& A, const FName& B)
		{
			return A.ToString() < B.ToString();
		});

		int32 ShownRows = 0;
		for (const FName& Name : RowNames)
		{
			if (ShownRows >= MaxRowNames)
			{
				Out += FString::Printf(TEXT("\n  (%d more row name(s) not shown)"),
					RowNames.Num() - ShownRows);
				break;
			}
			++ShownRows;
			Out += FString::Printf(TEXT("\n  %s"), *Name.ToString());
		}

		// Row VALUES are out of scope by the spec, and saying so stops a caller
		// asking again.
		Out += TEXT("\n; row values are not printed. The engine's own data table ");
		Out += TEXT("tools read and write them.");
	}

	if (Mode != 2)
	{
		// Readers, in two steps: the registry for completeness, a node scan for the
		// graph names.
		IAssetRegistry* Registry = IAssetRegistry::Get();
		if (Registry == nullptr)
		{
			Out += TEXT("\nreaders: the asset registry is not available yet, so no ");
			Out += TEXT("reader could be looked up. Try again once the editor has ");
			Out += TEXT("finished loading.");
			return Out;
		}

		TArray<FAssetDependency> Referencers;
		Registry->GetReferencers(FAssetIdentifier(FName(*Resolved.PackagePath)), Referencers);

		TArray<FReader> Readers;
		int32 SkippedNoise = 0;

		for (const FAssetDependency& Referencer : Referencers)
		{
			const FString PackageName = Referencer.AssetId.PackageName.ToString();
			if (PackageName.IsEmpty() || IsNoise(PackageName))
			{
				++SkippedNoise;
				continue;
			}

			FReader Reader;
			Reader.AssetPath = PackageName;

			// The registry's own class, so this agrees with every other tool.
			TArray<FAssetData> Found;
			Registry->GetAssetsByPackageName(FName(*PackageName), Found,
				/*bIncludeOnlyOnDiskAssets=*/false);
			if (Found.Num() > 0)
			{
				Reader.AssetPath = Found[0].GetObjectPathString();
				Reader.ClassName = Found[0].AssetClassPath.GetAssetName().ToString();
			}

			// Only a loaded Blueprint can have its graphs searched. An unloaded one is
			// still reported, because "something uses this table" is a fact.
			if (UBlueprint* Blueprint = Cast<UBlueprint>(
					FindObject<UObject>(nullptr, *Reader.AssetPath)))
			{
				Reader.bWasSearched = true;

				// The graphs themselves, in one list, rather than names looked up
				// again per node. The first version searched by name inside the node
				// loop, which did the same work once per node.
				TArray<UEdGraph*> Graphs;
				auto AddGraph = [&Graphs](const TArray<TObjectPtr<UEdGraph>>& InGraphs)
				{
					for (const TObjectPtr<UEdGraph>& Graph : InGraphs)
					{
						if (Graph != nullptr)
						{
							Graphs.Add(Graph);
						}
					}
				};
				AddGraph(Blueprint->UbergraphPages);
				AddGraph(Blueprint->FunctionGraphs);
				AddGraph(Blueprint->MacroGraphs);
				AddGraph(Blueprint->EventGraphs);

				// The graphs that actually hold a read of THIS table.
				TArray<FString> ReadingGraphs;
				for (const UEdGraph* Graph : Graphs)
				{
					for (const UEdGraphNode* Node : Graph->Nodes)
					{
						const UK2Node_GetDataTableRow* RowNode =
							Cast<UK2Node_GetDataTableRow>(Node);
						if (RowNode == nullptr)
						{
							continue;
						}

						// The pin names the table. Matching the asset path is what
						// distinguishes a read of THIS table from a read of another.
						const UEdGraphPin* TablePin = RowNode->GetDataTablePin();
						if (TablePin == nullptr)
						{
							continue;
						}

						const FString Referenced = TablePin->DefaultObject != nullptr
							? TablePin->DefaultObject->GetPathName()
							: TablePin->DefaultValue;
						if (TablePin->DefaultObject == Table
							|| Referenced == Resolved.ObjectPath
							|| Referenced == Resolved.PackagePath)
						{
							ReadingGraphs.AddUnique(Graph->GetName());
						}
					}
				}

				Reader.GraphName = FString::Join(ReadingGraphs, TEXT(", "));
			}

			Readers.Add(MoveTemp(Reader));
		}

		// Sorted so two reads give the same text.
		Readers.Sort([](const FReader& A, const FReader& B)
		{
			return A.AssetPath < B.AssetPath;
		});

		Out += FString::Printf(TEXT("\nreaders: %d asset(s) depend on this table"),
			Readers.Num());

		if (Readers.Num() == 0)
		{
			// `P1`: a zero says what it means and names the caveat, because the
			// registry only records edges for saved packages.
			Out += TEXT("\n  none. The registry records dependencies when packages are ");
			Out += TEXT("saved, so a reader that has never been saved will not appear.");
		}

		int32 ShownReaders = 0;
		int32 NamedGraphs = 0;
		for (const FReader& Reader : Readers)
		{
			if (ShownReaders >= MaxReaders)
			{
				Out += FString::Printf(TEXT("\n  (%d more reader(s) not shown)"),
					Readers.Num() - ShownReaders);
				break;
			}
			++ShownReaders;

			Out += FString::Printf(TEXT("\n  %s  [%s]"),
				*Reader.AssetPath, *Reader.ClassName);

			if (!Reader.bWasSearched)
			{
				// Naming why a graph is missing, so a caller can load the asset and
				// ask again rather than reading the silence as "no graph reads it".
				Out += TEXT("\n      graph: not searched, this asset is not loaded");
			}
			else if (Reader.GraphName.IsEmpty())
			{
				Out += TEXT("\n      graph: none. This Blueprint depends on the table ");
				Out += TEXT("but no GetDataTableRow node in it names this table.");
			}
			else
			{
				Out += FString::Printf(TEXT("\n      graph: %s"), *Reader.GraphName);
				++NamedGraphs;
			}
		}

		if (SkippedNoise > 0)
		{
			Out += FString::Printf(
				TEXT("\n  (%d engine or script referencer(s) were not listed: they are ")
				TEXT("not content)"), SkippedNoise);
		}

		if (Readers.Num() > 0)
		{
			Out += FString::Printf(
				TEXT("\n; %d of %d reader(s) had their graphs searched. A reader that is ")
				TEXT("not loaded cannot be searched, so it is reported without a graph ")
				TEXT("rather than being omitted."),
				NamedGraphs, Readers.Num());
		}
	}

	if (Out.Len() > MaxResultChars)
	{
		const int32 Dropped = Out.Len() - MaxResultChars;
		Out = Out.Left(MaxResultChars);
		Out += FString::Printf(
			TEXT("\n\n; (%d more character(s) were dropped; use the include filter ")
			TEXT("to read the schema or the readers alone)"), Dropped);
	}

	return Out;
}
