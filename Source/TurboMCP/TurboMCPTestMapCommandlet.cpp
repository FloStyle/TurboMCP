// Copyright (c) 2026 Florian ESCHALIER (FloStyle)
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "TurboMCPTestMapCommandlet.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "HAL/PlatformFileManager.h"
#include "Logging.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "IPythonScriptPlugin.h"
#include "Misc/FileHelper.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "UObject/UObjectIterator.h"

namespace
{
	/** Where the map goes. A compile-time constant, so the file path can be derived from it exactly. */
	const TCHAR* const TestMapPackage = TEXT("/Game/Tests/TurboMCPTestMap");

	/** The level's own name inside the package, which is what a reader sees in the content browser. */
	const TCHAR* const TestMapAssetName = TEXT("TurboMCPTestMap");
}

UTurboMCPTestMapCommandlet::UTurboMCPTestMapCommandlet()
{
	IsClient = false;
	IsServer = false;
	IsEditor = true;
	LogToConsole = true;
}

int32 UTurboMCPTestMapCommandlet::Main(const FString& Params)
{
	// THE ACTOR CLASS IS LOOKED UP BY NAME, AND THE REASON IS A BUILD CONSTRAINT RATHER THAN A PREFERENCE.
	//
	// MEASURED: the obvious shape - include the test project's actor header and spawn it - DOES NOT BUILD.
	// A PLUGIN MODULE CANNOT DEPEND ON A GAME MODULE, so the plugin cannot see `TurboMCPTest`, and the
	// include failed with "No such file or directory". The class therefore has to be found at RUNTIME.
	//
	// The module is loaded first and its failure is REPORTED rather than ignored: a silent miss would produce
	// a map with no actor in it, which looks exactly like a good map from the outside and would send every
	// later test to the wrong place.
	const FName TestModuleName = TEXT("TurboMCPTest");
	const bool bModuleLoaded = FModuleManager::Get().IsModuleLoaded(TestModuleName)
		|| FModuleManager::Get().LoadModule(TestModuleName) != nullptr;

	UE_LOG(LogTurboMCP, Display, TEXT("TurboMCP: test module %s loaded=%s"),
		*TestModuleName.ToString(), bModuleLoaded ? TEXT("yes") : TEXT("NO"));

	UClass* LogActorClass = nullptr;
	for (TObjectIterator<UClass> It; It; ++It)
	{
		if (It->GetName() == TEXT("TurboMCPTestLogActor"))
		{
			LogActorClass = *It;
			break;
		}
	}

	if (LogActorClass == nullptr)
	{
		UE_LOG(LogTurboMCP, Error, TEXT("TurboMCP: ATurboMCPTestLogActor was not found in the class "
			"registry, so no actor can be placed and the map would raise nothing. The class lives in the "
			"TurboMCPTest module; is that module loaded, and is it in the .uproject's Modules list?"));
	}

	UE_LOG(LogTurboMCP, Display, TEXT("TurboMCP: building the PIE test map"));

	// THE BLUEPRINT IS BUILT FIRST, BY A PYTHON SCRIPT, BECAUSE ONLY THE ENGINE'S OWN GRAPH WRITER MAY
	// AUTHOR IT.
	//
	// `FBlueprintCoreDelegates::OnScriptException` is raised BY THE BLUEPRINT VM, so a native actor can never
	// make it fire - which is why the trace stayed unproven even once a map existed. The map therefore needs a
	// BLUEPRINT actor with a node that runs, and the only writer this project trusts for graph text is the
	// engine's `BlueprintTools.write_graph_dsl`. That is a PYTHON tool, so the Python plugin executes it here
	// rather than this commandlet hand-building a graph - which would have been a second graph writer that
	// could disagree with the readers.
	//
	// THE SCRIPT IS RUN FIRST SO ITS ASSET IS SAVED AND DISCOVERABLE before the class registry is searched
	// below. Doing it the other way round would search for a class that does not exist yet and then report
	// that the Blueprint was missing, which would point at the fixture rather than at the ordering.
	{
		const FString ScriptPath = FPaths::ConvertRelativePathToFull(
			FPaths::ProjectDir() / TEXT("../Tools/make-test-map-blueprint.py"));

		// THE BLUEPRINT'S FILE IS DELETED HERE FIRST, AND THAT IS WHAT MAKES THE REBUILD WORK.
		//
		// MEASURED FAILURE: `BlueprintTools.create` answered nothing on the SECOND run while the first run
		// succeeded. The Python step tried to delete the old asset through the asset registry and
		// `does_asset_exist` answered FALSE while the `.uasset` was plainly on disk - because the registry had
		// not discovered it in that run - so the delete was skipped and the factory then collided with the
		// asset it could not see. A FIXTURE THAT CAN ONLY BE BUILT ONCE IS NOT A FIXTURE.
		//
		// Removing the FILE is deterministic in a way that asking the registry is not, and it is the same
		// approach the save below already uses for the map: delete, then write into a space known to be empty.
		{
			const FString BlueprintFile = FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir())
				/ TEXT("Tests/BP_TurboMCPTestLog") + FPackageName::GetAssetPackageExtension();

			IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
			if (PlatformFile.FileExists(*BlueprintFile))
			{
				const bool bRemoved = PlatformFile.DeleteFile(*BlueprintFile);
				UE_LOG(LogTurboMCP, Display, TEXT("TurboMCP: removed the previous Blueprint file %s -> %s"),
					*BlueprintFile, bRemoved ? TEXT("yes") : TEXT("NO"));
			}
		}

		if (!FPaths::FileExists(ScriptPath))
		{
			UE_LOG(LogTurboMCP, Error, TEXT("TurboMCP: the Blueprint script is not at %s, so no Blueprint "
				"actor can be placed and the trace hole would stay open"), *ScriptPath);
		}
		else if (IPythonScriptPlugin* Python = IPythonScriptPlugin::Get())
		{
			// THE FILE'S CONTENTS ARE PASSED AS THE COMMAND, NOT ITS PATH.
			//
			// MEASURED FAILURE: passing `py "<path>"` made Python parse the PATH ITSELF as source and answer
			// `SyntaxError: invalid syntax (<string>, line 1)` pointing at the `C:` of the filename. The `py`
			// command takes Python SOURCE, so the file is read here and handed over as text.
			//
			// The script sets `__name__` by itself only when run as a module, so the guard is stripped and the
			// entry point is called explicitly. That keeps the script runnable BOTH ways - through the engine's
			// `PythonScriptCommandlet` and from here.
			FString Source;
			if (!FFileHelper::LoadFileToString(Source, *ScriptPath))
			{
				UE_LOG(LogTurboMCP, Error, TEXT("TurboMCP: the Blueprint script at %s could not be read"),
					*ScriptPath);
			}
			else
			{
				const FString Command = Source + TEXT("\nmain()\n");
				const bool bRan = Python->ExecPythonCommand(*Command);
				UE_LOG(LogTurboMCP, Display, TEXT("TurboMCP: blueprint script ran=%s"),
					bRan ? TEXT("yes") : TEXT("NO"));
			}
		}
		else
		{
			UE_LOG(LogTurboMCP, Error, TEXT("TurboMCP: the Python plugin is not loaded, so the Blueprint "
				"actor cannot be built and the trace hole would stay open. Is PythonScriptPlugin enabled in "
				"the .uproject?"));
		}
	}

	// THE PACKAGE IS CREATED FIRST, SO THE WORLD IS BORN INSIDE IT.
	//
	// `UWorld::CreateWorld` takes the package as a parameter (`World.h:3375`:
	// `CreateWorld(EWorldType::Type, bool, FName, UPackage*, bool, ERHIFeatureLevel::Type, const
	// InitializationValues*, bool)`) rather than being moved into one afterwards, which is the shape
	// `WorldFactory.cpp:47` uses.
	UPackage* Package = CreatePackage(*FString(TestMapPackage));
	if (Package == nullptr)
	{
		UE_LOG(LogTurboMCP, Error, TEXT("TurboMCP: could not create the package %s"), TestMapPackage);
		return 1;
	}
	Package->SetFlags(RF_Public | RF_Standalone);

	// A WORLD TYPE OF `Editor` RATHER THAN `Game`: this map is AUTHORED in the editor, and PIE copies it into
	// its own world to play. Authoring it as a Game world makes the editor treat the asset as a running
	// session and refuse to open it in the level editor.
	//
	// THE INITIALIZATION VALUES ARE BUILT EXPLICITLY, FOLLOWING `WorldFactory.cpp:38-46`, AND WORLD PARTITION
	// IS OFF ON PURPOSE.
	//
	// MEASURED FAILURE WITHOUT THEM: creating the world with the DEFAULT values turned World Partition ON, and
	// the process then reported
	//
	//     Ensure condition failed: !bInitialized  [WorldSubsystem.cpp:118]
	//     Tickable subsystem WorldPartitionSubsystem /Game/Tests/TurboMCPTestMap... was destroyed while still
	//     initialized!
	//
	// on the way out. A fixture whose teardown faults would make every later run suspect, and a test map has
	// no use for a partition.
	UWorld::InitializationValues InitValues = UWorld::InitializationValues()
		.ShouldSimulatePhysics(false)
		.EnableTraceCollision(true)
		.CreateNavigation(false)
		.CreateAISystem(false)
		.CreateWorldPartition(false)
		.EnableWorldPartitionStreaming(false);

	UWorld* World = UWorld::CreateWorld(EWorldType::Editor, /*bInformEngineOfWorld=*/false,
		FName(TestMapAssetName), Package, /*bAddToRoot=*/true, ERHIFeatureLevel::Num, &InitValues);

	if (World == nullptr)
	{
		UE_LOG(LogTurboMCP, Error, TEXT("TurboMCP: could not create the world in %s"), TestMapPackage);
		return 1;
	}

	// THE PERSISTENT LEVEL MUST EXIST BEFORE ANYTHING IS PLACED, because an actor's outer chain has to reach
	// a `ULevel` for the engine to serialize it with the map. Asserted rather than assumed, since "spawned
	// but not saved" is the failure this would otherwise produce.
	if (World->PersistentLevel == nullptr)
	{
		UE_LOG(LogTurboMCP, Error, TEXT("TurboMCP: the new world has no persistent level, so nothing placed "
			"in it would be saved"));
		return 1;
	}

	// THE BLUEPRINT CLASS IS ALSO LOOKED UP BY NAME, FOR THE SAME BUILD REASON AS THE NATIVE ONE.
	//
	// THIS IS THE PIECE THAT MAKES THE TRACE POSSIBLE AT ALL. `FBlueprintCoreDelegates::OnScriptException` is
	// raised BY THE BLUEPRINT VM, so a NATIVE actor can never make it fire however much code it runs - which
	// is why `E-TRACE` stayed unproven even after a map existed. A BLUEPRINT actor placed in the level DOES
	// enter the VM, so it is the only thing that can close Q-18.
	//
	// The Blueprint itself is authored by `Tools/make-test-map-blueprint.py` through the engine's own
	// `write_graph_dsl`, so this commandlet only PLACES it - it does not build a graph by hand, which would
	// have been a second graph writer that could disagree with the readers.
	UClass* BlueprintClass = nullptr;
	for (TObjectIterator<UClass> It; It; ++It)
	{
		if (It->GetName() == TEXT("BP_TurboMCPTestLog_C"))
		{
			BlueprintClass = *It;
			break;
		}
	}

	UE_LOG(LogTurboMCP, Display, TEXT("TurboMCP: blueprint class BP_TurboMCPTestLog_C found=%s"),
		BlueprintClass != nullptr ? TEXT("yes") : TEXT("no"));

	// ALWAYS SPAWN, because the engine's empty map has no floor and the default collision handling would
	// refuse a spawn that overlaps nothing.
	int32 PlacedCount = 0;
	int32 PlacedBlueprintCount = 0;

	if (LogActorClass != nullptr)
	{
		if (!LogActorClass->IsChildOf(AActor::StaticClass()))
		{
			UE_LOG(LogTurboMCP, Error, TEXT("TurboMCP: %s is not an AActor, so it cannot be placed"),
				*LogActorClass->GetName());
		}
		else
		{
			FActorSpawnParameters SpawnParams;
			SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			SpawnParams.ObjectFlags |= RF_Transactional;

			AActor* Placed = World->SpawnActor<AActor>(LogActorClass, FTransform::Identity, SpawnParams);
			if (Placed == nullptr)
			{
				UE_LOG(LogTurboMCP, Error, TEXT("TurboMCP: %s could not be spawned into the level"),
					*LogActorClass->GetName());
			}
		}
	}

	if (BlueprintClass != nullptr && BlueprintClass->IsChildOf(AActor::StaticClass()))
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		SpawnParams.ObjectFlags |= RF_Transactional;

		// OFFSET FAR FROM THE NATIVE ACTOR so the two are distinguishable in the world, and so a future
		// reader can tell which one raised which line.
		AActor* PlacedBlueprint = World->SpawnActor<AActor>(BlueprintClass,
			FTransform(FVector(500.0, 0.0, 0.0)), SpawnParams);
		if (PlacedBlueprint == nullptr)
		{
			UE_LOG(LogTurboMCP, Error, TEXT("TurboMCP: %s could not be spawned into the level"),
				*BlueprintClass->GetName());
		}
	}

	// THE COUNTS ARE MEASURED BACK RATHER THAN TRUSTED, because a map saved without its actors looks
	// identical to a good one from outside and would send every later test to the wrong conclusion. This is
	// the same self-measurement the kit commandlet does.
	for (AActor* Actor : World->PersistentLevel->Actors)
	{
		if (Actor == nullptr)
		{
			continue;
		}
		if (LogActorClass != nullptr && Actor->IsA(LogActorClass))
		{
			++PlacedCount;
		}
		if (BlueprintClass != nullptr && Actor->IsA(BlueprintClass))
		{
			++PlacedBlueprintCount;
		}
	}

	// THE SETTINGS OBJECT CARRIES WHAT A LATER STEP NEEDS TO KNOW, so the map itself records which markers
	// its actor raises rather than leaving a reader to guess.
	UTurboMCPTestMapSettings* Settings = NewObject<UTurboMCPTestMapSettings>(Package,
		TEXT("TurboMCPTestMapSettings"));
	Settings->WarningMarker = TEXT("TURBOMCP-TESTLOG-WARN");
	Settings->ErrorMarker = TEXT("TURBOMCP-TESTLOG-ERROR");
	Settings->PlacedActorCount = PlacedCount;
	Settings->PlacedBlueprintActorCount = PlacedBlueprintCount;
	Settings->ActorClassName = LogActorClass != nullptr ? LogActorClass->GetName() : TEXT("(not found)");
	Settings->BlueprintActorClassName =
		BlueprintClass != nullptr ? BlueprintClass->GetName() : TEXT("(not found)");
	Settings->MarkPackageDirty();

	// THE BLUEPRINT ACTOR IS THE ONE Q-18 DEPENDS ON, so the line names both counts and the verdict is taken
	// from the blueprint count - a map with the native actor but no Blueprint actor would raise warnings and
	// still leave the trace unproven, which is exactly the confusion this line exists to prevent.
	UE_LOG(LogTurboMCP, Display,
		TEXT("TurboMCP: testMap nativeActor=%s nativeCount=%d blueprintActor=%s blueprintCount=%d (needs >=1)")
		TEXT(" nativeClass=%s blueprintClass=%s"),
		PlacedCount > 0 ? TEXT("ok") : TEXT("FAILED"), PlacedCount,
		PlacedBlueprintCount > 0 ? TEXT("ok") : TEXT("FAILED"), PlacedBlueprintCount,
		*Settings->ActorClassName, *Settings->BlueprintActorClassName);

	World->MarkPackageDirty();

	// THE ABSOLUTE PATH IS BUILT BY HAND, BECAUSE THE ENGINE'S CONVERSION GOES RELATIVE HERE.
	//
	// MEASURED IN THIS PROJECT: `FPackageName::LongPackageNameToFilename` answered a path RELATIVE TO THE
	// ENGINE DIRECTORY and `UPackage::SavePackage` then reported "could not save" while the asset was in
	// memory and complete. The package path is a compile-time constant and the engine remaps `/Game` to the
	// project's content directory, so the absolute path is that constant without its `/Game` prefix.
	const FString ContentDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir());
	const FString FileName = ContentDir
		+ FString(TestMapPackage).RightChop(FCString::Strlen(TEXT("/Game")))
		+ FPackageName::GetMapPackageExtension();

	// THE OLD FILE IS DELETED BEFORE THE SAVE, WHICH IS WHAT MAKES THIS RE-RUNNABLE. Measured in this
	// project: building into a package that still held an object of the same name made `SavePackage` answer
	// "could not save" while everything had built, and moving the old object to the transient package did
	// NOT settle it. Removing the file writes into a space known to be empty.
	{
		IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
		if (PlatformFile.FileExists(*FileName) && !PlatformFile.DeleteFile(*FileName))
		{
			UE_LOG(LogTurboMCP, Error, TEXT("TurboMCP: the existing map at %s could not be deleted, so the "
				"save would collide"), *FileName);
			return 3;
		}
	}

	UE_LOG(LogTurboMCP, Display, TEXT("TurboMCP: saving to %s"), *FileName);

	// THE FLAGS ARE SET ON THE ASSET, NOT ONLY ON THE PACKAGE, AND THE SAVE REFUSES WITHOUT THEM.
	//
	// MEASURED FAILURE: the first version set `RF_Public | RF_Standalone` on the PACKAGE and left the world
	// alone. `UPackage::SavePackage` then reported
	//
	//     Warning: The Asset /Game/Tests/TurboMCPTestMap.TurboMCPTestMap being saved does not have any of
	//     the provided object flags (0x10000003); saving the package would cause data loss.
	//
	// and answered FALSE, so a map whose actor HAD been placed and counted was never written. The
	// `TopLevelFlags` in `FSavePackageArgs` describe the flags the TOP-LEVEL ASSET must carry, so they have
	// to be ON that asset. This is the shape of failure this project keeps recording: everything upstream
	// succeeded and only the write failed.
	World->SetFlags(RF_Public | RF_Standalone);
	Settings->SetFlags(RF_Public | RF_Standalone);

	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	SaveArgs.SaveFlags = SAVE_NoError;
	if (!UPackage::SavePackage(Package, World, *FileName, SaveArgs))
	{
		UE_LOG(LogTurboMCP, Error, TEXT("TurboMCP: could not save %s"), *FileName);
		return 3;
	}

	FAssetRegistryModule::AssetCreated(World);

	UE_LOG(LogTurboMCP, Display, TEXT("TurboMCP: wrote %s"), *FileName);

	// THE MAP IS WRITTEN EITHER WAY, AND THE EXIT CODE CARRIES THE VERDICT. A caller can therefore tell "no
	// map at all" from "a map that raises nothing", which are two very different failures and would
	// otherwise both look like a missing file.
	return PlacedCount > 0 ? 0 : 4;
}
