// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "Tools/TurboMCPToolsets.h"

#include "Algo/Reverse.h"
#include "Algo/Unique.h"
#include "AssetRegistry/AssetRegistryState.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Dom/JsonObject.h"
#include "Engine/AssetManager.h"
#include "Misc/PackageName.h"
#include "Misc/PackagePath.h"
#include "Misc/SecureHash.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/PackageResourceManager.h"

#include "TurboMCPAssetResolver.h"

namespace
{
	using namespace UE::AssetRegistry;

	constexpr int32 MaxDepth = 6;
	constexpr int32 MaxSeeds = 5000;
	constexpr int32 MaxRecords = 250000;
	constexpr int32 MaxResultChars = 24000;
	constexpr int32 MaxPageSize = 500;

	enum class EKind : uint8 { Hard = 1, Soft = 2, SearchableName = 4, Management = 8 };
	constexpr uint8 AllKinds = 15;

	struct FKindName
	{
		EKind Kind = EKind::Hard;
		const TCHAR* Name;
	};
	constexpr FKindName KindNames[] = {
		{ EKind::Hard, TEXT("hard") }, { EKind::Soft, TEXT("soft") },
		{ EKind::SearchableName, TEXT("searchable-name") }, { EKind::Management, TEXT("management") }
	};

	const TCHAR* KindName(EKind Kind)
	{
		for (const FKindName& Entry : KindNames)
		{
			if (Entry.Kind == Kind)
			{
				return Entry.Name;
			}
		}
		return TEXT("unknown");
	}

	struct FOptions
	{
		bool bIn = false;
		bool bOut = false;
		bool bLegacy = false;
		bool bCompact = true;
		bool bIncludeSeeds = true;
		bool bRoots = false;
		int32 Depth = 1;
		uint8 Kinds = AllKinds;
	};

	FString ReadOptions(const FString& Direction, const FString& Kinds, int32 Limit, FOptions& Options)
	{
		const FString Wanted = Direction.TrimStartAndEnd().ToLower();
		const bool bBoth = Wanted.IsEmpty() || Wanted == TEXT("both");
		Options.bOut = bBoth || Wanted == TEXT("out") || (!Options.bLegacy && Wanted == TEXT("dependencies"));
		Options.bIn = bBoth || Wanted == TEXT("in") || (!Options.bLegacy && Wanted == TEXT("referencers"));
		if (!Options.bOut && !Options.bIn)
		{
			return Options.bLegacy ? TEXT("error: invalid direction. Accepted: out, in, both.")
				: TEXT("error: invalid direction. Accepted: dependencies, referencers, both, out, in.");
		}
		if (Options.Depth < 1 || Options.Depth > MaxDepth || Limit < 1 || Limit > MaxPageSize)
		{
			return TEXT("error: depth must be 1 to 6 and page size must be 1 to 500.");
		}

		const FString KindList = Kinds.TrimStartAndEnd().ToLower();
		if (KindList.IsEmpty() || KindList == TEXT("all"))
		{
			return FString();
		}
		Options.Kinds = 0;
		TArray<FString> Parts;
		KindList.ParseIntoArray(Parts, TEXT(","), false);
		for (FString& Part : Parts)
		{
			Part.TrimStartAndEndInline();
			bool bFound = false;
			for (const FKindName& Entry : KindNames)
			{
				if (Part == Entry.Name)
				{
					Options.Kinds |= static_cast<uint8>(Entry.Kind);
					bFound = true;
					break;
				}
			}
			if (!bFound)
			{
				return TEXT("error: kinds must be all or a comma-separated list of hard, soft, searchable-name, management.");
			}
		}
		return FString();
	}

	bool Includes(const FOptions& Options, EKind Kind)
	{
		return (Options.Kinds & static_cast<uint8>(Kind)) != 0;
	}

	bool IsLegacyNoise(FName Package)
	{
		if (Package.IsNone())
		{
			return true;
		}
		const FString Path = Package.ToString();
		return Path.StartsWith(TEXT("/Engine/")) || Path.StartsWith(TEXT("/Transient"))
			|| FPackageName::IsScriptPackage(Path) || FPackageName::IsTempPackage(Path);
	}

	struct FPackageInfo
	{
		TArray<FAssetData> Assets;
	};

	// The callbacks keep traversal independent of editor state and let the native test use an isolated registry.
	struct FRegistryReads
	{
		TFunction<bool(const FAssetIdentifier&, bool, EDependencyCategory, TArray<FAssetDependency>&)> Edges;
		TFunction<void(TArray<FName>&)> SavedPackages;
		TFunction<FPackageInfo(FName)> Package;
		TFunction<FString(FName)> DiskState;
		TFunction<FName(const FPrimaryAssetId&)> PrimaryPackage;
		TFunction<FPrimaryAssetId(FName)> RegisteredPrimaryId;
	};

	struct FEdge
	{
		FAssetIdentifier Target;
		FAssetIdentifier Via;
		EKind Kind = EKind::Hard;
		bool operator==(const FEdge& Other) const
		{
			return Target == Other.Target && Via == Other.Via && Kind == Other.Kind;
		}
	};

	struct FAdjacency
	{
		TArray<FEdge> Edges;
		int32 Filtered = 0;
		bool bAvailable = false;
	};

	class FReferenceReader
	{
	public:
		FReferenceReader(const FRegistryReads& InRegistry, const FOptions& InOptions)
			: Registry(InRegistry), Options(InOptions)
		{
		}

		const FPackageInfo& Package(FName Name)
		{
			if (const FPackageInfo* Found = Packages.Find(Name))
			{
				return *Found;
			}
			return Packages.Add(Name, Registry.Package(Name));
		}

		const FString& DiskState(FName Name)
		{
			if (const FString* Found = DiskStates.Find(Name))
			{
				return *Found;
			}
			return DiskStates.Add(Name, Registry.DiskState(Name));
		}

		FName PackageOf(const FAssetIdentifier& Id)
		{
			if (!Id.PackageName.IsNone() || !Id.GetPrimaryAssetId().IsValid())
			{
				return Id.PackageName;
			}
			const FPrimaryAssetId PrimaryId = Id.GetPrimaryAssetId();
			if (const FName* Found = PrimaryPackages.Find(PrimaryId))
			{
				return *Found;
			}
			const FName Name = Registry.PrimaryPackage(PrimaryId);
			PrimaryPackages.Add(PrimaryId, Name);
			return Name;
		}

		const FAdjacency& Adjacent(const FAssetIdentifier& Id, bool bIncoming)
		{
			auto& Cache = bIncoming ? Incoming : Outgoing;
			if (const FAdjacency* Found = Cache.Find(Id))
			{
				return *Found;
			}
			FAdjacency& Result = Cache.Add(Id);
			EDependencyCategory Categories = EDependencyCategory::None;
			if (Includes(Options, EKind::Hard) || Includes(Options, EKind::Soft))
			{
				Categories |= EDependencyCategory::Package;
			}
			if (Includes(Options, EKind::Management))
			{
				Categories |= EDependencyCategory::Manage;
			}
			if (Includes(Options, EKind::SearchableName))
			{
				Categories |= EDependencyCategory::SearchableName;
			}
			TArray<FAssetDependency> Raw;
			Result.bAvailable = Registry.Edges(Id, bIncoming, Categories, Raw);
			AddEdges(Raw, {}, Result);

			if (Id.IsPackage() && !bIncoming && Includes(Options, EKind::Management))
			{
				TSet<FPrimaryAssetId> PrimaryIds;
				const FPrimaryAssetId Registered = Registry.RegisteredPrimaryId(Id.PackageName);
				if (Registered.IsValid())
				{
					PrimaryIds.Add(Registered);
				}
				for (const FAssetData& Asset : Package(Id.PackageName).Assets)
				{
					const FPrimaryAssetId Tagged = Asset.GetPrimaryAssetId();
					if (Tagged.IsValid())
					{
						PrimaryIds.Add(Tagged);
					}
				}
				for (const FPrimaryAssetId& PrimaryId : PrimaryIds)
				{
					PrimaryPackages.FindOrAdd(PrimaryId) = Id.PackageName;
					Raw.Reset();
					const FAssetIdentifier Source(PrimaryId);
					Registry.Edges(Source, false, EDependencyCategory::Manage, Raw);
					AddEdges(Raw, Source, Result);
				}
			}

			if (Id.IsPackage() && bIncoming && Includes(Options, EKind::SearchableName))
			{
				IndexSearchableNames();
				if (const TArray<FEdge>* Named = SearchableIncoming.Find(Id.PackageName))
				{
					for (const FEdge& Edge : *Named)
					{
						AddEdge(Edge, Result);
					}
				}
			}

			Result.Edges.Sort([](const FEdge& A, const FEdge& B)
			{
				if (!(A.Target == B.Target))
				{
					return A.Target.LexicalLess(B.Target);
				}
				if (A.Kind != B.Kind)
				{
					return A.Kind < B.Kind;
				}
				return A.Via.LexicalLess(B.Via);
			});
			Result.Edges.SetNum(Algo::Unique(Result.Edges), EAllowShrinking::No);
			return Result;
		}

	private:
		void AddEdge(const FEdge& Edge, FAdjacency& Result)
		{
			if (Options.bLegacy && IsLegacyNoise(PackageOf(Edge.Target)))
			{
				++Result.Filtered;
			}
			else
			{
				Result.Edges.Add(Edge);
			}
		}

		void AddEdges(const TArray<FAssetDependency>& Raw, const FAssetIdentifier& Via, FAdjacency& Result)
		{
			for (const FAssetDependency& Dependency : Raw)
			{
				EKind Kind = EKind::Hard;
				switch (Dependency.Category)
				{
				case EDependencyCategory::Package:
					Kind = EnumHasAnyFlags(Dependency.Properties, EDependencyProperty::Hard) ? EKind::Hard : EKind::Soft;
					break;
				case EDependencyCategory::SearchableName: Kind = EKind::SearchableName; break;
				case EDependencyCategory::Manage: Kind = EKind::Management; break;
				default: continue;
				}
				if (Includes(Options, Kind))
				{
					AddEdge({ Dependency.AssetId, Via, Kind }, Result);
				}
			}
		}

		void IndexSearchableNames()
		{
			if (bSearchableIndexed)
			{
				return;
			}
			bSearchableIndexed = true;
			TArray<FName> Names;
			Registry.SavedPackages(Names);
			TArray<FAssetDependency> Raw;
			for (FName Name : Names)
			{
				Raw.Reset();
				Registry.Edges(FAssetIdentifier(Name), false, EDependencyCategory::SearchableName, Raw);
				for (const FAssetDependency& Dependency : Raw)
				{
					const FAssetIdentifier& Target = Dependency.AssetId;
					if (!Target.PackageName.IsNone() && !Target.IsPackage())
					{
						SearchableIncoming.FindOrAdd(Target.PackageName).Add(
							{ FAssetIdentifier(Name), Target, EKind::SearchableName });
					}
				}
			}
		}

		const FRegistryReads& Registry;
		const FOptions& Options;
		TMap<FName, FPackageInfo> Packages;
		TMap<FName, FString> DiskStates;
		TMap<FPrimaryAssetId, FName> PrimaryPackages;
		TMap<FAssetIdentifier, FAdjacency> Incoming;
		TMap<FAssetIdentifier, FAdjacency> Outgoing;
		TMap<FName, TArray<FEdge>> SearchableIncoming;
		bool bSearchableIndexed = false;
	};

	FRegistryReads MakeRegistryReads(IAssetRegistry& Registry)
	{
		return {
			[&Registry](const FAssetIdentifier& Id, bool bIn, EDependencyCategory Categories, TArray<FAssetDependency>& Out)
			{
				return bIn ? Registry.GetReferencers(Id, Out, Categories) : Registry.GetDependencies(Id, Out, Categories);
			},
			[&Registry](TArray<FName>& Out)
			{
				Registry.EnumerateAllPackages([&Out](FName Package, const FAssetPackageData&) { Out.Add(Package); });
			},
			[&Registry](FName Name)
			{
				FPackageInfo Info;
				if (Name.IsNone())
				{
					return Info;
				}
				Registry.GetAssetsByPackageName(Name, Info.Assets, false);
				Info.Assets.Sort([](const FAssetData& A, const FAssetData& B)
				{
					return A.GetSoftObjectPath().LexicalLess(B.GetSoftObjectPath());
				});
				return Info;
			},
			[](FName Name) -> FString
			{
				const FString Path = Name.ToString();
				FPackagePath MountedPath;
				if (FPackageName::IsScriptPackage(Path))
				{
					return TEXT("script");
				}
				if (Name.IsNone() || FPackageName::IsMemoryPackage(Path) || FPackageName::IsTempPackage(Path)
					|| Path.StartsWith(TEXT("/Transient")) || !FPackagePath::TryFromMountedName(Path, MountedPath))
				{
					return TEXT("virtual");
				}
				// Probe the package resource directly. A stale registry entry is not proof of a file.
				if (IPackageResourceManager::Get().DoesPackageExist(MountedPath))
				{
					return TEXT("present");
				}
				using ELocation = FPackageName::EPackageLocationFilter;
				return FPackageName::DoesPackageExistEx(MountedPath, ELocation::IoDispatcher) != ELocation::None
					? TEXT("virtual") : TEXT("missing");
			},
			[](const FPrimaryAssetId& Id)
			{
				const UAssetManager* Manager = UAssetManager::GetIfInitialized();
				return Manager ? Manager->GetPrimaryAssetPath(Id).GetLongPackageFName() : NAME_None;
			},
			[](FName Name)
			{
				const UAssetManager* Manager = UAssetManager::GetIfInitialized();
				return Manager ? Manager->GetPrimaryAssetIdForPackage(Name) : FPrimaryAssetId();
			}
		};
	}

	FString ReadyRegistry(IAssetRegistry*& Registry)
	{
		Registry = IAssetRegistry::Get();
		if (!Registry)
		{
			return TEXT("error: the asset registry is unavailable; the reference scan cannot run.");
		}
		Registry->WaitForCompletion();
		if (Registry->IsGathering())
		{
			return TEXT("error: the asset registry scan is incomplete; no reference counts were produced. Try again after mounted content finishes loading.");
		}
		return FString();
	}
}

namespace
{
	struct FSeed
	{
		FName Package;
		FString ObjectPath;
		FString ClassPath;
	};

	struct FRecord
	{
		int32 Seed = 0;
		bool bSeed = false;
		bool bIncoming = false;
		bool bRoot = false;
		int32 Depth = 0;
		int32 Parent = INDEX_NONE;
		FAssetIdentifier From;
		FEdge Edge{};
	};

	struct FTraversal
	{
		TArray<FRecord> Records;
		int32 Counts[2] = {};
		int32 Filtered[2] = {};
		int32 Unavailable[2] = {};
		bool bCapped = false;

		bool IsComplete() const
		{
			return !bCapped && Unavailable[0] == 0 && Unavailable[1] == 0;
		}
	};

	void TraverseSeed(FReferenceReader& Reader, const FOptions& Options, const FSeed& Seed, int32 SeedIndex,
		bool bIncoming, int32 SeedRecord, FTraversal& Traversal)
	{
		struct FPending
		{
			FAssetIdentifier Node;
			int32 Depth;
			int32 Record;
		};
		const FAssetIdentifier SeedId(Seed.Package);
		TArray<FPending> Pending = { { SeedId, 0, SeedRecord } };
		TSet<FAssetIdentifier> Seen = { SeedId };

		for (int32 Next = 0; Next < Pending.Num(); ++Next)
		{
			const FPending Current = Pending[Next];
			const FAdjacency& Adjacent = Reader.Adjacent(Current.Node, bIncoming);
			if (Current.Depth < Options.Depth)
			{
				Traversal.Unavailable[bIncoming] += !Adjacent.bAvailable;
				Traversal.Filtered[bIncoming] += Adjacent.Filtered;
			}
			if (bIncoming && Options.bRoots && Adjacent.bAvailable && Adjacent.Edges.IsEmpty()
				&& Reader.DiskState(Reader.PackageOf(Current.Node)) == TEXT("present"))
			{
				Traversal.Records[Current.Record].bRoot = true;
			}
			if (Current.Depth >= Options.Depth)
			{
				continue;
			}
			for (const FEdge& Edge : Adjacent.Edges)
			{
				if (Traversal.Records.Num() >= MaxRecords)
				{
					Traversal.bCapped = true;
					return;
				}

				const int32 Record = Traversal.Records.Add({ SeedIndex, false, bIncoming, false,
					Current.Depth + 1, Current.Record, Current.Node, Edge });
				++Traversal.Counts[bIncoming];
				if (Current.Depth + 1 >= Options.Depth && !(Options.bRoots && bIncoming))
				{
					continue;
				}

				// Keep the original identifier in the result, then traverse the owning package when it is known.
				const FName Package = Reader.PackageOf(Edge.Target);
				const FAssetIdentifier Node = Package.IsNone() ? Edge.Target : FAssetIdentifier(Package);
				bool bAlreadySeen = false;
				Seen.Add(Node, &bAlreadySeen);
				if (!bAlreadySeen)
				{
					Pending.Add({ Node, Current.Depth + 1, Record });
				}
			}
		}
	}

	FTraversal Traverse(FReferenceReader& Reader, const TArray<FSeed>& Seeds, const FOptions& Options)
	{
		FTraversal Traversal;
		if (!Options.bLegacy)
		{
			for (int32 Index = 0; Index < Seeds.Num(); ++Index)
			{
				FRecord& Record = Traversal.Records.AddDefaulted_GetRef();
				Record.bSeed = true;
				Record.Seed = Index;
			}
		}
		for (int32 Index = 0; Index < Seeds.Num() && !Traversal.bCapped; ++Index)
		{
			const int32 SeedRecord = Options.bLegacy ? INDEX_NONE : Index;
			if (Options.bOut)
			{
				TraverseSeed(Reader, Options, Seeds[Index], Index, false, SeedRecord, Traversal);
			}
			if (Options.bIn && !Traversal.bCapped)
			{
				TraverseSeed(Reader, Options, Seeds[Index], Index, true, SeedRecord, Traversal);
			}
		}
		return Traversal;
	}

	FString Serialize(const TSharedRef<FJsonObject>& Object)
	{
		FString Result;
		auto Writer = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Result);
		FJsonSerializer::Serialize(Object, Writer);
		return Result;
	}

	void HashField(FMD5& Digest, const FString& Field)
	{
		const FTCHARToUTF8 Utf8(*Field);
		const uint32 Length = Utf8.Length();
		Digest.Update(reinterpret_cast<const uint8*>(&Length), sizeof(Length));
		Digest.Update(reinterpret_cast<const uint8*>(Utf8.Get()), Length);
	}

	FString Revision(const TArray<FSeed>& Seeds, const FOptions& Options, const FTraversal& Traversal)
	{
		FMD5 Digest;
		HashField(Digest, FString::Printf(TEXT("refs-v1:%d:%d:%d:%d:%d:%d:%d:%d:%d:%d:%d:%d"),
			Options.bIn, Options.bOut, Options.bLegacy, Options.bCompact, Options.bRoots,
			Options.Depth, Options.Kinds, Traversal.Unavailable[0], Traversal.Unavailable[1],
			Traversal.Filtered[0], Traversal.Filtered[1], Traversal.bCapped));
		HashField(Digest, Options.bIncludeSeeds ? TEXT("seeds") : TEXT("edges-only"));
		for (const FSeed& Seed : Seeds)
		{
			HashField(Digest, Seed.Package.ToString());
			HashField(Digest, Seed.ObjectPath);
			HashField(Digest, Seed.ClassPath);
		}
		for (const FRecord& Record : Traversal.Records)
		{
			HashField(Digest, FString::Printf(TEXT("%d:%d:%d:%d:%d:%d"), Record.Seed,
				Record.bSeed, Record.bIncoming, Record.bRoot, Record.Depth, static_cast<int32>(Record.Edge.Kind)));
			if (!Record.bSeed)
			{
				HashField(Digest, Record.From.ToString());
				HashField(Digest, Record.Edge.Target.ToString());
				HashField(Digest, Record.Edge.Via.ToString());
			}
		}
		uint8 Bytes[16];
		Digest.Final(Bytes);
		return BytesToHex(Bytes, UE_ARRAY_COUNT(Bytes));
	}

	struct FPage
	{
		int32 Start = 0;
		int32 End = 0;
		FString Revision;

		bool ReadCursor(const FString& Cursor, int32 Total)
		{
			if (Cursor.IsEmpty() || Cursor == TEXT("first"))
			{
				return true;
			}
			FString Prefix;
			FString Offset;
			return Cursor.Split(TEXT(":"), &Prefix, &Offset) && Prefix == Revision
				&& LexTryParseString(Start, *Offset) && Start > 0 && Start < Total
				&& FString::FromInt(Start) == Offset;
		}

		FString NextCursor(int32 Total) const
		{
			return End < Total ? Revision + TEXT(":") + FString::FromInt(End) : FString();
		}
	};

	FString ShortClass(const FString& ClassPath)
	{
		return FPackageName::ObjectPathToObjectName(ClassPath);
	}

	void Describe(FReferenceReader& Reader, const FAssetIdentifier& Id, bool bCompact,
		FJsonObject& Row)
	{
		const FName Package = Reader.PackageOf(Id);
		const FPackageInfo& Info = Reader.Package(Package);
		Row.SetStringField(TEXT("package"), Package.IsNone() ? FString() : Package.ToString());
		const FString& DiskState = Reader.DiskState(Package);
		Row.SetStringField(TEXT("diskState"), DiskState);
		if (DiskState == TEXT("present") || DiskState == TEXT("missing"))
		{
			Row.SetBoolField(TEXT("missingOnDisk"), DiskState == TEXT("missing"));
		}

		const FAssetData* Matching = nullptr;
		for (const FAssetData& Asset : Info.Assets)
		{
			if (Id.IsPackage() || (!Id.PackageName.IsNone() && Id.ObjectName == Asset.AssetName)
				|| (Id.GetPrimaryAssetId().IsValid() && Id.GetPrimaryAssetId() == Asset.GetPrimaryAssetId()))
			{
				Matching = &Asset;
				break;
			}
		}
		Row.SetStringField(TEXT("class"), Matching ? (bCompact ? Matching->AssetClassPath.GetAssetName().ToString()
			: Matching->AssetClassPath.ToString()) : TEXT("unknown"));
		if (Id.IsPackage() && Info.Assets.Num() > 1)
		{
			Row.SetBoolField(TEXT("multipleAssets"), true);
		}
		if (Matching && !bCompact)
		{
			Row.SetStringField(TEXT("objectPath"), Matching->GetObjectPathString());
		}
	}

	TSharedRef<FJsonObject> EdgeObject(const FRecord& Record)
	{
		TSharedRef<FJsonObject> Edge = MakeShared<FJsonObject>();
		Edge->SetStringField(TEXT("from"), Record.From.ToString());
		Edge->SetStringField(TEXT("identifier"), Record.Edge.Target.ToString());
		Edge->SetStringField(TEXT("kind"), KindName(Record.Edge.Kind));
		if (Record.Edge.Via.IsValid())
		{
			Edge->SetStringField(TEXT("via"), Record.Edge.Via.ToString());
		}
		return Edge;
	}

	FString BatchRecord(FReferenceReader& Reader, const FSeed& Seed, const FRecord& Record,
		const FTraversal& Traversal, const FOptions& Options)
	{
		const TSharedRef<FJsonObject> Row = MakeShared<FJsonObject>();
		Row->SetStringField(TEXT("seed"), Seed.Package.ToString());
		if (Record.bSeed)
		{
			Row->SetStringField(TEXT("type"), TEXT("seed"));
			Describe(Reader, FAssetIdentifier(Seed.Package), Options.bCompact, *Row);
			Row->SetStringField(TEXT("class"), Options.bCompact ? ShortClass(Seed.ClassPath) : Seed.ClassPath);
			if (!Options.bCompact)
			{
				Row->SetStringField(TEXT("objectPath"), Seed.ObjectPath);
			}
		}
		else
		{
			Row->Values = MoveTemp(EdgeObject(Record)->Values);
			Row->SetStringField(TEXT("seed"), Seed.Package.ToString());
			Row->SetStringField(TEXT("direction"), Record.bIncoming ? TEXT("referencers") : TEXT("dependencies"));
			Row->SetNumberField(TEXT("depth"), Record.Depth);
			Describe(Reader, Record.Edge.Target, Options.bCompact, *Row);
		}
		if (Record.bRoot)
		{
			Row->SetBoolField(TEXT("registryRoot"), true);
			TArray<TSharedPtr<FJsonValue>> Chain;
			const FRecord* Hop = &Record;
			while (!Hop->bSeed)
			{
				Chain.Add(MakeShared<FJsonValueObject>(EdgeObject(*Hop)));
				Hop = &Traversal.Records[Hop->Parent];
			}
			Algo::Reverse(Chain);
			Row->SetArrayField(TEXT("rootChain"), MoveTemp(Chain));
		}
		return Serialize(Row);
	}

	FString LegacyRecord(FReferenceReader& Reader, const FRecord& Record)
	{
		const FAssetIdentifier& Id = Record.Edge.Target;
		const FPackageInfo& Info = Reader.Package(Reader.PackageOf(Id));
		FString Path = Id.ToString();
		FString Class = Info.Assets.IsEmpty() ? TEXT("not in the asset registry") : TEXT("unknown");
		for (const FAssetData& Asset : Info.Assets)
		{
			if (Id.IsPackage() || Id.ObjectName == Asset.AssetName)
			{
				Path = Id.IsPackage() ? Asset.GetObjectPathString() : Path;
				Class = Asset.AssetClassPath.GetAssetName().ToString();
				break;
			}
		}
		FString Row = FString::Printf(TEXT("\n  %s  [%s] (%s)"), *Path, *Class, KindName(Record.Edge.Kind));
		if (Id.IsPackage() && Info.Assets.Num() > 1)
		{
			Row += TEXT(" (package-level edge; first asset by path shown)");
		}
		if (Record.Edge.Via.IsValid())
		{
			Row += TEXT(" via ") + Record.Edge.Via.ToString();
		}
		return Row;
	}

	FString FormatPage(FReferenceReader& Reader, const TArray<FSeed>& Seeds, const FOptions& Options,
		const FTraversal& Traversal, const FString& Cursor, int32 PageSize)
	{
		FPage Page;
        // Filter presentation indices only: root-chain parents index the full traversal.
        TArray<int32> Visible;
        for (int32 Index = 0; Index < Traversal.Records.Num(); ++Index)
            if (Options.bIncludeSeeds || !Traversal.Records[Index].bSeed) Visible.Add(Index);
        const int32 Total = Visible.Num();
		if (!Cursor.IsEmpty() || Total > 0)
		{
			Page.Revision = Revision(Seeds, Options, Traversal);
		}
		if (!Page.ReadCursor(Cursor, Total))
		{
			return TEXT("error: cursor is invalid or the query results changed. Restart with an empty cursor.");
		}

		FString Rows[2];
		for (Page.End = Page.Start; Page.End < Total && Page.End - Page.Start < PageSize; ++Page.End)
		{
			const FRecord& Record = Traversal.Records[Visible[Page.End]];
			FString Row = Options.bLegacy ? LegacyRecord(Reader, Record)
				: BatchRecord(Reader, Seeds[Record.Seed], Record, Traversal, Options);
			if (!Options.bLegacy && !Rows[0].IsEmpty())
			{
				Row = TEXT(",") + Row;
			}
			// Leave room for the envelope and preserve complete records.
			if (Rows[0].Len() + Rows[1].Len() + Row.Len() > MaxResultChars - 3000)
			{
				break;
			}
			Rows[Options.bLegacy && Record.bIncoming] += Row;
		}
		if (Page.End == Page.Start && Page.End < Total)
		{
			return TEXT("error: a reference record exceeds the response budget. Try compact mode or omit root chains.");
		}

		const TSharedRef<FJsonObject> Metadata = MakeShared<FJsonObject>();
		Metadata->SetNumberField(TEXT("seedCount"), Seeds.Num());
		Metadata->SetBoolField(TEXT("seedRecordsIncluded"), !Options.bLegacy && Options.bIncludeSeeds);
		Metadata->SetNumberField(TEXT("total"), Total);
		Metadata->SetNumberField(TEXT("edgeCount"), Traversal.Counts[0] + Traversal.Counts[1]);
		Metadata->SetNumberField(TEXT("returned"), Page.End - Page.Start);
		Metadata->SetNumberField(TEXT("offset"), Page.Start);
		Metadata->SetStringField(TEXT("nextCursor"), Page.NextCursor(Total));
		Metadata->SetBoolField(TEXT("complete"), Traversal.IsComplete());
		Metadata->SetBoolField(TEXT("totalIsExact"), Traversal.IsComplete());
		Metadata->SetNumberField(TEXT("unavailableQueries"), Traversal.Unavailable[0] + Traversal.Unavailable[1]);
		Metadata->SetBoolField(TEXT("traversalCapped"), Traversal.bCapped);

		if (!Options.bLegacy)
		{
			Metadata->SetStringField(TEXT("scope"), TEXT("Registry edges only. Unsaved, dynamic and config references may be missing."));
			if (Options.bRoots)
			{
				Metadata->SetStringField(TEXT("rootScope"), TEXT("A root is a saved package with no incoming edges of the selected kinds in the registry. One shortest chain is shown per reached root and seed."));
			}
			return Serialize(Metadata).LeftChop(1) + TEXT(",\"records\":[") + Rows[0] + TEXT("]}");
		}

		FString Result = FString::Printf(TEXT("%s: references of %s"),
			Traversal.IsComplete() ? TEXT("read") : TEXT("partial"), *Seeds[0].ObjectPath);
		FString Counts[2] = { TEXT("not queried"), TEXT("not queried") };
		for (int32 Direction = 0; Direction < 2; ++Direction)
		{
			if (!(Direction ? Options.bIn : Options.bOut))
			{
				continue;
			}
			Counts[Direction] = Traversal.Unavailable[Direction] ? TEXT("unavailable") : FString::FromInt(Traversal.Counts[Direction]);
			Result += FString::Printf(TEXT("\n%s: %s"), Direction ? TEXT("used by (in)") : TEXT("uses (out)"), *Counts[Direction]);
			Result += Rows[Direction];
			if (!Traversal.Unavailable[Direction] && Traversal.Counts[Direction] == 0)
			{
				Result += Direction ? TEXT("\n  none. This asset may be unreferenced, or its users may not be saved yet.")
					: TEXT("\n  none. Try ReadTurboMCPBlueprintLoad for what the graphs touch.");
			}
			if (Traversal.Filtered[Direction] > 0)
			{
				Result += FString::Printf(TEXT("\n  (%d registry entries omitted by the legacy package filters)"),
					Traversal.Filtered[Direction]);
			}
		}
		Result += FString::Printf(TEXT("\ncounts: out=%s, in=%s"), *Counts[0], *Counts[1]);
		Result += TEXT("\nScope: direct registry edges. Engine, script, temporary and unresolved virtual packages are filtered. Unsaved or dynamic references may be missing; zero is not permission to delete an asset.");
		return Result + TEXT("\n; refs-page: ") + Serialize(Metadata);
	}

	FString ReadReferences(const FRegistryReads& Registry, const TArray<FSeed>& Seeds, const FOptions& Options,
		const FString& Cursor, int32 PageSize)
	{
		FReferenceReader Reader(Registry, Options);
		const FTraversal Traversal = Traverse(Reader, Seeds, Options);
		return FormatPage(Reader, Seeds, Options, Traversal, Cursor, PageSize);
	}
}

namespace
{
	FString FolderSeeds(IAssetRegistry& Registry, const FString& Folder, bool bRecursive, const FString& ClassFilter,
		TArray<FSeed>& Seeds)
	{
		FString Path = Folder;
		while (Path.EndsWith(TEXT("/")))
		{
			Path.LeftChopInline(1);
		}
		if (Path.IsEmpty() || !FPackageName::IsValidLongPackageName(Path + TEXT("/FolderProbe"), true)
			|| FPackageName::IsScriptPackage(Path) || FPackageName::IsMemoryPackage(Path))
		{
			return TEXT("error: folder must be a mounted content path, such as /Game/Art.");
		}
		TArray<FAssetData> Assets;
		if (!Registry.GetAssetsByPath(FName(*Path), Assets, bRecursive, false))
		{
			return TEXT("error: the asset registry could not enumerate the folder.");
		}
		Assets.Sort([](const FAssetData& A, const FAssetData& B)
		{
			return A.GetSoftObjectPath().LexicalLess(B.GetSoftObjectPath());
		});
		const FName ClassName(*ClassFilter);
		TSet<FName> Seen;
		for (const FAssetData& Asset : Assets)
		{
			if (!ClassFilter.IsEmpty() && Asset.AssetClassPath.GetAssetName() != ClassName
				&& !Asset.AssetClassPath.ToString().Equals(ClassFilter, ESearchCase::IgnoreCase))
			{
				continue;
			}
			bool bAlreadySeen = false;
			Seen.Add(Asset.PackageName, &bAlreadySeen);
			if (!bAlreadySeen)
			{
				Seeds.Add({ Asset.PackageName, Asset.GetObjectPathString(), Asset.AssetClassPath.ToString() });
			}
		}
		return FString();
	}
}

FString UTurboMCPReadToolset::ReadTurboMCPRefs(const FString& Asset, const FString& Direction,
	const FString& Kinds, int32 PageSize, const FString& Cursor)
{
	if (Asset.TrimStartAndEnd().IsEmpty())
	{
		return TEXT("error: no asset given. Pass an asset path or unique short name.");
	}
	FOptions Options;
	Options.bLegacy = true;
	const FString OptionError = ReadOptions(Direction, Kinds, PageSize, Options);
	if (!OptionError.IsEmpty())
	{
		return OptionError;
	}
	IAssetRegistry* Registry = nullptr;
	const FString RegistryError = ReadyRegistry(Registry);
	if (!RegistryError.IsEmpty())
	{
		return RegistryError;
	}
	FTurboMCPAssetResolver::FResolved Resolved;
	const FString ResolveError = FTurboMCPAssetResolver::Resolve(Asset, Resolved);
	if (!ResolveError.IsEmpty())
	{
		return ResolveError;
	}
	const TArray<FSeed> Seeds = { { FName(*Resolved.PackagePath), Resolved.ObjectPath, Resolved.AssetClass } };
	return ReadReferences(MakeRegistryReads(*Registry), Seeds, Options, Cursor, PageSize);
}

FString UTurboMCPReadToolset::ReadTurboMCPRefsBatch(const FString& Assets, const FString& Folder, bool bRecursive,
	const FString& ClassFilter, const FString& Direction, const FString& Kinds, int32 Depth, bool bCompact,
	bool bStopAtRoot, int32 PageSize, const FString& Cursor, bool IncludeSeeds)
{
	// The pinned tool schema treats a C++ default of TEXT("") as required.
	const auto OptionalText = [](const FString& Input)
	{
		const FString Value = Input.TrimStartAndEnd();
		return Value == TEXT("<none>") ? FString() : Value;
	};
	const FString AssetList = OptionalText(Assets);
	const FString FolderPath = OptionalText(Folder);
	const FString ClassPath = OptionalText(ClassFilter);
	if (AssetList.IsEmpty() == FolderPath.IsEmpty() || (FolderPath.IsEmpty() && !ClassPath.IsEmpty()))
	{
		return TEXT("error: provide either assets or a folder. Class filter applies only to folder selection.");
	}
	FOptions Options;
	Options.Depth = Depth;
	Options.bCompact = bCompact;
	Options.bRoots = bStopAtRoot;
	Options.bIncludeSeeds = IncludeSeeds;
	const FString OptionError = ReadOptions(Direction, Kinds, PageSize, Options);
	if (!OptionError.IsEmpty())
	{
		return OptionError;
	}
	if (bStopAtRoot && !Options.bIn)
	{
		return TEXT("error: stop at root requires referencers or both.");
	}

	IAssetRegistry* Registry = nullptr;
	const FString RegistryError = ReadyRegistry(Registry);
	if (!RegistryError.IsEmpty())
	{
		return RegistryError;
	}

	TArray<FSeed> Seeds;
	if (!FolderPath.IsEmpty())
	{
		const FString Error = FolderSeeds(*Registry, FolderPath, bRecursive, ClassPath, Seeds);
		if (!Error.IsEmpty())
		{
			return Error;
		}
	}
	else
	{
		const TArray<FString> Inputs = FTurboMCPAssetResolver::SplitList(AssetList);
		if (Inputs.IsEmpty() || Inputs.Num() > MaxSeeds)
		{
			return TEXT("error: assets must contain between 1 and 5000 entries.");
		}
		for (const FString& Input : Inputs)
		{
			FTurboMCPAssetResolver::FResolved Resolved;
			const FString Error = FTurboMCPAssetResolver::Resolve(Input, Resolved);
			if (!Error.IsEmpty())
			{
				return Error;
			}
			Seeds.Add({ FName(*Resolved.PackagePath), Resolved.ObjectPath, Resolved.AssetClass });
		}
	}

	Seeds.Sort([](const FSeed& A, const FSeed& B)
	{
		return A.Package == B.Package ? A.ObjectPath < B.ObjectPath : A.Package.LexicalLess(B.Package);
	});
	Seeds.SetNum(Algo::Unique(Seeds, [](const FSeed& A, const FSeed& B)
	{
		return A.Package == B.Package;
	}), EAllowShrinking::No);
	if (Seeds.Num() > MaxSeeds)
	{
		return FString::Printf(TEXT("error: folder selected %d packages, above the 5000-seed limit. Narrow the folder or class."), Seeds.Num());
	}
	return ReadReferences(MakeRegistryReads(*Registry), Seeds, Options, Cursor, PageSize);
}

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"

namespace TurboMCPRefsTests
{
	bool Run(FAutomationTestBase& Test)
	{
		FAssetRegistryState State;
		const FName A(TEXT("/Game/RefsTest/A"));
		const FName B(TEXT("/Game/RefsTest/B"));
		const FName C(TEXT("/Game/RefsTest/C"));
		const FName Root(TEXT("/Game/RefsTest/Root"));
		const FName NamedUser(TEXT("/Game/RefsTest/NamedUser"));
		const FName Missing(TEXT("/Game/RefsTest/Missing"));
		const FName Script(TEXT("/Script/RefsTest"));
		const FName Virtual(TEXT("/Memory/RefsTest"));
		const FName ManagerPackage(TEXT("/Game/RefsTest/Manager"));
		const FPrimaryAssetId Manager(FName(TEXT("Fixture")), FName(TEXT("Manager")));
		const FPrimaryAssetId ManagedOwner(FName(TEXT("Fixture")), FName(TEXT("A")));
		const FName Object(TEXT("Definition"));
		const FName Value(TEXT("Key"));
		TArray<FName> Saved = { A, B, C, Root, NamedUser, ManagerPackage };
		const FAssetDependency Outgoing[] = {
			FAssetDependency::PackageDependency(B, EDependencyProperty::Hard),
			FAssetDependency::PackageDependency(C, EDependencyProperty::None),
			FAssetDependency::PackageDependency(Missing, EDependencyProperty::Hard),
			FAssetDependency::PackageDependency(Script, EDependencyProperty::Hard),
			FAssetDependency::PackageDependency(Virtual, EDependencyProperty::Hard),
			{ FAssetIdentifier(B, Object, Value), EDependencyCategory::SearchableName }
		};
		State.AddDependencies(A, Outgoing);
		const FAssetDependency ToA[] = { FAssetDependency::PackageDependency(A, EDependencyProperty::Hard) };
		State.AddDependencies(B, ToA);
		const FAssetDependency ToB[] = { FAssetDependency::PackageDependency(B, EDependencyProperty::Hard) };
		State.AddDependencies(Root, ToB);
		const FAssetDependency Named[] = { { FAssetIdentifier(A, Object, Value), EDependencyCategory::SearchableName } };
		State.AddDependencies(NamedUser, Named);
		const FAssetDependency ManageA[] = { { FAssetIdentifier(A), EDependencyCategory::Manage, EDependencyProperty::Direct } };
		State.AddDependencies(FAssetIdentifier(Manager), ManageA);
		const FAssetDependency ManageC[] = { { FAssetIdentifier(C), EDependencyCategory::Manage, EDependencyProperty::Direct } };
		State.AddDependencies(FAssetIdentifier(ManagedOwner), ManageC);

		int32 RegistryQueries = 0;
		const FRegistryReads Registry = {
			[&](const FAssetIdentifier& Id, bool bIn, EDependencyCategory Category, TArray<FAssetDependency>& Out)
			{
				++RegistryQueries;
				return bIn ? State.GetReferencers(Id, Out, Category) : State.GetDependencies(Id, Out, Category);
			},
			[&](TArray<FName>& Out) { Out = Saved; },
			[&](FName Name)
			{
				FPackageInfo Info;
				if (Name == A)
				{
					FAssetDataTagMap Tags;
					Tags.Add(FPrimaryAssetId::PrimaryAssetTypeTag, TEXT("Fixture"));
					Tags.Add(FPrimaryAssetId::PrimaryAssetNameTag, TEXT("A"));
					Info.Assets.Emplace(A, FName(TEXT("/Game/RefsTest")), FName(TEXT("A")),
						FTopLevelAssetPath(TEXT("/Script/Engine.Texture2D")), MoveTemp(Tags));
				}
				return Info;
			},
			[&](FName Name) -> FString
			{
				if (Name == Script) return TEXT("script");
				if (Name == Virtual || Name.IsNone()) return TEXT("virtual");
				return Name == Missing ? TEXT("missing") : TEXT("present");
			},
			[&](const FPrimaryAssetId& Id) { return Id == Manager ? ManagerPackage : NAME_None; },
			[&](FName Name) { return Name == ManagerPackage ? Manager : FPrimaryAssetId(); }
		};

		const auto Parse = [&Test](const FString& Response) -> TSharedPtr<FJsonObject>
		{
			TSharedPtr<FJsonObject> Result;
			Test.TestTrue(TEXT("Response is JSON"), FJsonSerializer::Deserialize(
				TJsonReaderFactory<>::Create(Response), Result) && Result.IsValid());
			return Result;
		};

		const TArray<FSeed> Seeds = { { A, TEXT("/Game/RefsTest/A.A"), TEXT("/Script/Engine.Texture2D") } };
		FOptions Both;
		Both.bIn = Both.bOut = Both.bRoots = true;
		Both.Depth = 3;
		const FString Combined = ReadReferences(Registry, Seeds, Both, FString(), 100);
		const TSharedPtr<FJsonObject> CombinedJson = Parse(Combined);
		if (!CombinedJson)
		{
			return false;
		}
		Test.TestTrue(TEXT("Known cycle terminates"), CombinedJson->GetNumberField(TEXT("edgeCount")) < 32);
		Test.TestTrue(TEXT("Includes hard and soft package edges"),
			Combined.Contains(TEXT("\"kind\":\"hard\"")) && Combined.Contains(TEXT("\"kind\":\"soft\"")));
		Test.TestTrue(TEXT("Retains the object and value on an incoming searchable-name edge"),
			Combined.Contains(TEXT("\"via\":\"/Game/RefsTest/A.Definition::Key\"")));
		Test.TestTrue(TEXT("Uses the primary asset on the outgoing management edge"),
			Combined.Contains(TEXT("\"via\":\"Fixture:A\"")));
		Test.TestTrue(TEXT("Resolves the incoming management identifier to its package"),
			Combined.Contains(TEXT("\"identifier\":\"Fixture:Manager\""))
			&& Combined.Contains(TEXT("\"package\":\"/Game/RefsTest/Manager\"")));
		Test.TestTrue(TEXT("Separates missing files from script and virtual packages"),
			Combined.Contains(TEXT("\"missingOnDisk\":true")) && Combined.Contains(TEXT("\"diskState\":\"script\""))
			&& Combined.Contains(TEXT("\"diskState\":\"virtual\"")));
		Test.TestTrue(TEXT("Reports the seed class"), Combined.Contains(TEXT("\"class\":\"Texture2D\"")));
		Test.TestTrue(TEXT("Includes an evidence chain to an observed root"), Combined.Contains(TEXT("\"rootChain\":[{")));
		Test.TestFalse(TEXT("Unavailable registry node makes results partial"), CombinedJson->GetBoolField(TEXT("totalIsExact")));

		FOptions Hard;
		Hard.Kinds = static_cast<uint8>(EKind::Hard);
		Hard.bIn = true;
		const FName Hub(TEXT("/Game/RefsTest/Hub"));
		const FName Tail(TEXT("/Game/RefsTest/Tail"));
		const FAssetDependency ToHub[] = { FAssetDependency::PackageDependency(Hub, EDependencyProperty::Hard) };
		const FAssetDependency ToTail[] = { FAssetDependency::PackageDependency(Tail, EDependencyProperty::Hard) };
		State.AddDependencies(Hub, ToTail);
		TArray<FSeed> Scale;
		for (int32 Index = 0; Index < 700; ++Index)
		{
			const FString Path = FString::Printf(TEXT("/Game/RefsTest/Scale_%03d"), Index);
			const FName Name(*Path);
			State.AddDependencies(Name, ToHub);
			Scale.Add({ Name, Path, TEXT("/Script/Engine.Texture2D") });
		}

		const TArray<FSeed> HubSeed = { { Hub, Hub.ToString(), TEXT("/Script/Engine.Texture2D") } };
		TSet<FString> Identifiers;
		int32 Offset = 0;
		FString Cursor;
		do
		{
			const TSharedPtr<FJsonObject> Page = Parse(ReadReferences(Registry, HubSeed, Hard, Cursor, 500));
			if (!Page)
			{
				return false;
			}
			Test.TestEqual(TEXT("Stable total across pages"), static_cast<int32>(Page->GetNumberField(TEXT("total"))), 701);
			Test.TestEqual(TEXT("Page begins at the previous boundary"), static_cast<int32>(Page->GetNumberField(TEXT("offset"))), Offset);
			const auto& Records = Page->GetArrayField(TEXT("records"));
			if (Records.IsEmpty())
			{
				Test.AddError(TEXT("Expected records on every continuation"));
				return false;
			}
			for (const auto& Record : Records)
			{
				FString Id;
				if (Record->AsObject()->TryGetStringField(TEXT("identifier"), Id))
				{
					Test.TestFalse(TEXT("An edge is never repeated"), Identifiers.Contains(Id));
					Identifiers.Add(Id);
				}
			}
			Offset += Records.Num();
			Cursor = Page->GetStringField(TEXT("nextCursor"));
		}
		while (!Cursor.IsEmpty() && Offset < 701);
		Test.TestTrue(TEXT("All 700 incoming edges appear exactly once"), Identifiers.Num() == 700 && Cursor.IsEmpty());
		Test.TestTrue(TEXT("Rejects invalid cursors"), ReadReferences(Registry, HubSeed, Hard, TEXT("invalid"), 100).StartsWith(TEXT("error:")));
        FOptions EdgesOnly = Hard;
        EdgesOnly.bIncludeSeeds = false;
        TSet<FString> EdgeIds;
        FString EdgeCursor;
        do
        {
            const TSharedPtr<FJsonObject> Page = Parse(ReadReferences(Registry, HubSeed, EdgesOnly, EdgeCursor, 100));
            if (!Page) return false;
            Test.TestEqual(TEXT("Edges-only total excludes seed"), Page->GetNumberField(TEXT("total")), 700.0);
            Test.TestEqual(TEXT("Edges-only retains seed count"), Page->GetNumberField(TEXT("seedCount")), 1.0);
            for (const auto& Value : Page->GetArrayField(TEXT("records")))
            {
                const auto Row = Value->AsObject();
                FString RecordType;
                Test.TestFalse(TEXT("No seed rows"), Row->TryGetStringField(TEXT("type"), RecordType) && RecordType == TEXT("seed"));
                const FString Id = Row->GetStringField(TEXT("identifier"));
                Test.TestFalse(TEXT("Edges-only does not repeat edges"), EdgeIds.Contains(Id));
                EdgeIds.Add(Id);
            }
            EdgeCursor = Page->GetStringField(TEXT("nextCursor"));
            if (!EdgeCursor.IsEmpty())
                Test.TestTrue(TEXT("Changing seed mode invalidates cursor"),
                    ReadReferences(Registry, HubSeed, Hard, EdgeCursor, 100).StartsWith(TEXT("error:")));
        } while (!EdgeCursor.IsEmpty() && EdgeIds.Num() < 701);
        Test.TestTrue(TEXT("Edges-only returns all edges and terminates"), EdgeIds.Num() == 700 && EdgeCursor.IsEmpty());
		Hard.bLegacy = true;
		const auto LegacyPage = [&](const FString& LegacyCursor)
		{
			FString Metadata;
			const FString Legacy = ReadReferences(Registry, HubSeed, Hard, LegacyCursor, 100);
			Test.TestTrue(TEXT("Legacy reader publishes page metadata"), Legacy.Split(TEXT("\n; refs-page: "), nullptr, &Metadata));
			return Parse(Metadata);
		};
		const TSharedPtr<FJsonObject> FirstLegacy = LegacyPage(FString());
		if (!FirstLegacy)
		{
			return false;
		}
		Test.TestEqual(TEXT("Legacy first page goes beyond the old cap"), static_cast<int32>(FirstLegacy->GetNumberField(TEXT("returned"))), 100);
		Test.TestEqual(TEXT("Legacy total includes all incoming edges"), static_cast<int32>(FirstLegacy->GetNumberField(TEXT("total"))), 700);
		const FString LegacyCursor = FirstLegacy->GetStringField(TEXT("nextCursor"));
		Test.TestFalse(TEXT("Legacy reader publishes a continuation"), LegacyCursor.IsEmpty());
		const TSharedPtr<FJsonObject> SecondLegacy = LegacyPage(LegacyCursor);
		if (!SecondLegacy)
		{
			return false;
		}
		Test.TestEqual(TEXT("Legacy continuation starts after the first page"), static_cast<int32>(SecondLegacy->GetNumberField(TEXT("offset"))), 100);

		Hard.bLegacy = Hard.bIn = false;
		Hard.bOut = true;
		for (int32 Depth = 1; Depth <= 2; ++Depth)
		{
			Hard.Depth = Depth;
			RegistryQueries = 0;
			const double Start = FPlatformTime::Seconds();
			const FString Response = ReadReferences(Registry, Scale, Hard, FString(), 100);
			const double Seconds = FPlatformTime::Seconds() - Start;
			const TSharedPtr<FJsonObject> Page = Parse(Response);
			if (!Page)
			{
				return false;
			}
			Test.TestEqual(TEXT("All scale seeds were processed"), static_cast<int32>(Page->GetNumberField(TEXT("seedCount"))), 700);
			Test.TestEqual(TEXT("Scale edge count"), static_cast<int32>(Page->GetNumberField(TEXT("edgeCount"))), 700 * Depth);
			Test.TestEqual(TEXT("Each adjacency queried once per batch"), RegistryQueries, 699 + Depth);
			Test.TestTrue(TEXT("Scale call finished under 60 seconds"), Seconds < 60.0);
			UE_LOG(LogTemp, Display, TEXT("FT-166 isolated registry: 700 seeds, depth %d, %.6f seconds, %d adjacency queries, 1 batch invocation"),
				Depth, Seconds, RegistryQueries);
		}
		return true;
	}
}
#endif
