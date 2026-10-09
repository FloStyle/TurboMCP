// Copyright (c) 2026 Florian ESCHALIER (FloStyle)
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "TurboMCPTraps.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#include "Logging.h"

namespace
{
	/** The file shipped with the plugin. */
	const TCHAR* ShippedTrapsRelativePath = TEXT("Content/Traps/EngineToolTraps.json");

	/** The project's own notes, which win over the shipped file. */
	const TCHAR* ProjectTrapsRelativePath = TEXT("Config/TurboMCP/Traps.json");

	/**
	 * Reads the "tools" member of a traps document into OutTools.
	 *
	 * Kept separate from file loading so a parse failure and a missing file are
	 * reported differently, because they need different fixes.
	 */
	bool ParseTrapsDocument(
		const FString& InJson,
		TMap<FString, FTurboMCPTraps::FToolInfo>& OutTools,
		FString& OutError)
	{
		TSharedPtr<FJsonObject> Root;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(InJson);

		if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
		{
			OutError = TEXT("the file is not valid JSON");
			return false;
		}

		const TSharedPtr<FJsonObject>* ToolsObject = nullptr;
		if (!Root->TryGetObjectField(TEXT("tools"), ToolsObject) || ToolsObject == nullptr)
		{
			OutError = TEXT("the file has no 'tools' object");
			return false;
		}

		int32 ToolCount = 0;
		for (const TPair<FString, TSharedPtr<FJsonValue>>& Entry : (*ToolsObject)->Values)
		{
			const TSharedPtr<FJsonObject>* ToolObject = nullptr;
			if (!Entry.Value.IsValid() || !Entry.Value->TryGetObject(ToolObject) || ToolObject == nullptr)
			{
				continue;
			}

			FTurboMCPTraps::FToolInfo Info;

			// Parameter names an agent might guess, mapped to the real name.
			const TSharedPtr<FJsonObject>* ParamsObject = nullptr;
			if ((*ToolObject)->TryGetObjectField(TEXT("parameters"), ParamsObject) && ParamsObject != nullptr)
			{
				for (const TPair<FString, TSharedPtr<FJsonValue>>& Param : (*ParamsObject)->Values)
				{
					FString Value;
					if (Param.Value.IsValid() && Param.Value->TryGetString(Value))
					{
						Info.Parameters.Add(Param.Key, Value);
					}
				}
			}

			// Traps, as an array of sentences.
			const TArray<TSharedPtr<FJsonValue>>* TrapArray = nullptr;
			if ((*ToolObject)->TryGetArrayField(TEXT("traps"), TrapArray) && TrapArray != nullptr)
			{
				for (const TSharedPtr<FJsonValue>& Trap : *TrapArray)
				{
					FString Sentence;
					if (Trap.IsValid() && Trap->TryGetString(Sentence) && !Sentence.IsEmpty())
					{
						Info.Traps.Add(Sentence);
					}
				}
			}

			OutTools.Add(Entry.Key, MoveTemp(Info));
			++ToolCount;
		}

		if (ToolCount == 0)
		{
			OutError = TEXT("the file parsed but recorded no tools");
			return false;
		}

		return true;
	}
}

FTurboMCPTraps::FTurboMCPTraps()
{
	// Shipped notes first, so a project file can add to or correct them.
	const FString Shipped = FPaths::Combine(
		FPaths::ProjectPluginsDir(), TEXT("TurboMCP"), ShippedTrapsRelativePath);
	const FString Project = FPaths::Combine(FPaths::ProjectDir(), ProjectTrapsRelativePath);

	// The plugin also ships inside the engine, so try the engine's plugin path
	// when the project does not carry a copy. This is what a project that only
	// enables the plugin looks like.
	const FString EngineCopy = FPaths::Combine(
		FPaths::EnginePluginsDir(), TEXT("Marketplace"), TEXT("TurboMCP"), ShippedTrapsRelativePath);

	TArray<FString> Candidates;
	Candidates.Add(Shipped);
	if (!FPaths::IsSamePath(EngineCopy, Shipped))
	{
		Candidates.Add(EngineCopy);
	}
	Candidates.Add(Project);

	TArray<FString> Tried;
	for (const FString& Candidate : Candidates)
	{
		if (!FPaths::FileExists(Candidate))
		{
			Tried.Add(Candidate);
			continue;
		}

		FString Error;
		if (LoadFile(Candidate, Error))
		{
			LoadedFrom = Candidate;
		}
		else
		{
			// A file that exists but does not parse is worth a warning even when
			// an earlier file already provided notes, because the author will
			// otherwise never learn their file is being ignored.
			UE_LOG(LogTurboMCP, Warning, TEXT("Traps file '%s' ignored: %s"), *Candidate, *Error);
			Tried.Add(FString::Printf(TEXT("%s (%s)"), *Candidate, *Error));
		}
	}

	if (LoadedFrom.IsEmpty())
	{
		LoadError = FString::Printf(
			TEXT("no traps file could be read. Tried: %s"), *FString::Join(Tried, TEXT("; ")));
		UE_LOG(LogTurboMCP, Warning, TEXT("%s"), *LoadError);
	}
	else
	{
		UE_LOG(LogTurboMCP, Log, TEXT("Traps loaded from '%s' (%d tools)"),
			*LoadedFrom, Tools.Num());
	}
}

bool FTurboMCPTraps::LoadFile(const FString& InPath, FString& OutError)
{
	FString Json;
	if (!FFileHelper::LoadFileToString(Json, *InPath))
	{
		OutError = TEXT("the file could not be opened for reading");
		return false;
	}

	// Parse into a temporary so a broken later file cannot erase notes that an
	// earlier file already provided.
	TMap<FString, FToolInfo> Parsed;
	if (!ParseTrapsDocument(Json, Parsed, OutError))
	{
		return false;
	}

	for (TPair<FString, FToolInfo>& Entry : Parsed)
	{
		// A later file replaces a tool's entry outright. Merging would make it
		// impossible to remove a trap that no longer applies.
		Tools.Add(Entry.Key, MoveTemp(Entry.Value));
	}
	return true;
}

const FTurboMCPTraps& FTurboMCPTraps::Get()
{
	// Built once on first use. A function local static is thread safe in C++11,
	// which matters because tool calls can arrive on a worker thread.
	static const FTurboMCPTraps Instance;
	return Instance;
}

const FTurboMCPTraps::FToolInfo* FTurboMCPTraps::Find(const FString& InToolName) const
{
	return Tools.Find(InToolName);
}
