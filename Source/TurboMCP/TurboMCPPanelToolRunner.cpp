// Copyright FloStyle. All Rights Reserved.

#include "TurboMCPPanelToolRunner.h"

#include "Containers/Map.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "Math/UnrealMathUtility.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "ToolsetRegistry/UToolsetRegistry.h"

#include "TurboMCPTokenEstimate.h"

namespace
{
	/**
	 * The registry's schema list, read the same way `TurboMCPAgentHelp.cpp:189` reads it.
	 *
	 * The shape is MEASURED rather than assumed: the root is a JSON ARRAY, each element an object with
	 * `name`, `description` and `tools`, and each tool an object with `name`, `description` and
	 * `inputSchema`. `TurboMCPAgentHelp.cpp` already parses it for `R-TOOL-INDEX` and
	 * `R-TOOL-HELP`, so this is one more reader of an understood format rather than a guess.
	 */
	bool ReadToolsetObjects(TArray<TSharedPtr<FJsonObject>>& OutObjects, FString& OutError)
	{
		OutObjects.Reset();

		if (!UToolsetRegistry::IsAvailable())
		{
			// `P1`: an empty list and a broken list must not look the same.
			OutError = TEXT("the toolset registry is not available, so no tool can be listed. It "
				"needs the editor, and it is registered once every module has finished loading.");
			return false;
		}

		const FString AllSchemas = UToolsetRegistry::GetAllToolsetJsonSchemas();

		TArray<TSharedPtr<FJsonValue>> Root;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(AllSchemas);
		if (!FJsonSerializer::Deserialize(Reader, Root))
		{
			OutError = FString::Printf(
				TEXT("the registry answered, but its schema list could not be read as JSON. The ")
				TEXT("first %d character(s) were: %.200s"),
				FMath::Min(200, AllSchemas.Len()), *AllSchemas.Left(200));
			return false;
		}

		for (const TSharedPtr<FJsonValue>& Value : Root)
		{
			const TSharedPtr<FJsonObject>* Object = nullptr;
			if (Value.IsValid() && Value->TryGetObject(Object) && Object != nullptr)
			{
				OutObjects.Add(*Object);
			}
		}

		if (OutObjects.Num() == 0)
		{
			// Deserialised, but nothing usable. Distinct from both the unavailable case above and
			// from a successful read that happens to be empty, because they call for different
			// actions.
			OutError = TEXT("the registry answered with JSON, but it held no toolsets. No module has "
				"registered one yet.");
			return false;
		}
		return true;
	}

	FString Field(const TSharedPtr<FJsonObject>& InObject, const TCHAR* InName)
	{
		FString Value;
		if (InObject.IsValid())
		{
			InObject->TryGetStringField(InName, Value);
		}
		return Value;
	}

	/** The bare name of a published `<Toolset>.<Function>` name. */
	FString BareName(const FString& InFullName)
	{
		int32 LastDot = INDEX_NONE;
		if (InFullName.FindLastChar(TEXT('.'), LastDot) && LastDot < InFullName.Len() - 1)
		{
			return InFullName.Mid(LastDot + 1);
		}
		return InFullName;
	}

	/**
	 * A JSON value a caller can replace, chosen from the schema's own declared type.
	 *
	 * WHY A SAMPLE AT ALL: a human trying a tool should not have to read a schema to find out that
	 * `asset` wants a package path. The sample is deliberately a VALID SHAPE rather than a useful
	 * value, because the useful value is the caller's to choose.
	 *
	 * An unrecognised type yields an empty string AND sets `bOutIsPartial`, so a guess that is a guess
	 * is visibly one. Silently emitting `0` for an unknown type would be worse than emitting nothing.
	 */
	FString SampleForType(const FString& InType, bool& bOutIsPartial)
	{
		if (InType == TEXT("string"))  { return TEXT("\"\""); }
		if (InType == TEXT("integer")) { return TEXT("0"); }
		if (InType == TEXT("number"))  { return TEXT("0"); }
		if (InType == TEXT("boolean")) { return TEXT("false"); }
		if (InType == TEXT("array"))   { return TEXT("[]"); }
		if (InType == TEXT("object"))  { return TEXT("{}"); }
		if (InType == TEXT("null"))    { return TEXT("null"); }

		bOutIsPartial = true;
		return TEXT("\"\"");
	}
}

bool FTurboMCPPanelToolRunner::IsRegistryAvailable(FString& OutWhy)
{
	OutWhy.Reset();
	if (UToolsetRegistry::IsAvailable())
	{
		return true;
	}
	OutWhy = TEXT("the toolset registry is not available. It needs the editor, and it registers "
		"once every module has finished loading.");
	return false;
}

bool FTurboMCPPanelToolRunner::ListToolsets(TArray<FToolsetInfo>& OutToolsets, FString& OutError)
{
	OutError.Reset();
	OutToolsets.Reset();

	TArray<TSharedPtr<FJsonObject>> Objects;
	if (!ReadToolsetObjects(Objects, OutError))
	{
		return false;
	}

	for (const TSharedPtr<FJsonObject>& Object : Objects)
	{
		FToolsetInfo Info;
		Info.Name = Field(Object, TEXT("name"));
		if (Info.Name.IsEmpty())
		{
			// A toolset with no name cannot be called, so it is skipped rather than listed as a
			// blank row a caller would try to use.
			continue;
		}

		const TArray<TSharedPtr<FJsonValue>>* Tools = nullptr;
		if (Object->TryGetArrayField(TEXT("tools"), Tools) && Tools != nullptr)
		{
			for (const TSharedPtr<FJsonValue>& ToolValue : *Tools)
			{
				const TSharedPtr<FJsonObject>* ToolObject = nullptr;
				if (ToolValue.IsValid() && ToolValue->TryGetObject(ToolObject)
					&& ToolObject != nullptr)
				{
					Info.Tools.Add(Field(*ToolObject, TEXT("name")));
				}
			}
		}
		Info.Tools.Sort();
		OutToolsets.Add(MoveTemp(Info));
	}

	OutToolsets.Sort([](const FToolsetInfo& A, const FToolsetInfo& B)
	{
		return A.Name < B.Name;
	});
	return true;
}

bool FTurboMCPPanelToolRunner::ListTools(const FString& InToolsetName,
	TArray<FToolInfo>& OutTools, FString& OutError)
{
	OutError.Reset();
	OutTools.Reset();

	TArray<TSharedPtr<FJsonObject>> Objects;
	if (!ReadToolsetObjects(Objects, OutError))
	{
		return false;
	}

	for (const TSharedPtr<FJsonObject>& Object : Objects)
	{
		if (!Field(Object, TEXT("name")).Equals(InToolsetName, ESearchCase::IgnoreCase))
		{
			continue;
		}

		const TArray<TSharedPtr<FJsonValue>>* Tools = nullptr;
		if (Object->TryGetArrayField(TEXT("tools"), Tools) && Tools != nullptr)
		{
			for (const TSharedPtr<FJsonValue>& ToolValue : *Tools)
			{
				const TSharedPtr<FJsonObject>* ToolObject = nullptr;
				if (!ToolValue.IsValid() || !ToolValue->TryGetObject(ToolObject)
					|| ToolObject == nullptr)
				{
					continue;
				}

				FToolInfo Info;
				Info.FullName = Field(*ToolObject, TEXT("name"));
				Info.BareName = BareName(Info.FullName);
				Info.Description = Field(*ToolObject, TEXT("description"));

				// The sample comes from the schema the registry published, so a tool added anywhere
				// gets one without this file knowing anything about it.
				const TSharedPtr<FJsonObject>* InputSchema = nullptr;
				FString Json = TEXT("{}");
				if ((*ToolObject)->TryGetObjectField(TEXT("inputSchema"), InputSchema)
					&& InputSchema != nullptr)
				{
					Info.bHasSchema = true;

					TArray<FString> Lines;
					const TSharedPtr<FJsonObject>* Properties = nullptr;
					if ((*InputSchema)->TryGetObjectField(TEXT("properties"), Properties)
						&& Properties != nullptr)
					{
						// THE MAP IS ITERATED DIRECTLY, because `TMapBase::GetKeys` is gone in 5.8 and
						// `Find` would not accept the `const FString&` a sorted key array hands it.
						// Iterating is both the supported call and the one that cannot have a
						// key/value type mismatch.
						//
						// SORTED, so the sample is stable between opens. A map's iteration order is
						// not the caller's, and a sample whose field order changes at random is one
						// nobody can compare.
						TArray<TPair<FString, TSharedPtr<FJsonValue>>> Sorted;
						for (const auto& Pair : (*Properties)->Values)
						{
							Sorted.Emplace(Pair.Key, Pair.Value);
						}
						Sorted.Sort([](const TPair<FString, TSharedPtr<FJsonValue>>& A,
									   const TPair<FString, TSharedPtr<FJsonValue>>& B)
						{
							return A.Key < B.Key;
						});

						for (const auto& Pair : Sorted)
						{
							const FString& Key = Pair.Key;
							const TSharedPtr<FJsonValue>& Property = Pair.Value;
							if (!Property.IsValid())
							{
								continue;
							}

							FString Type;
							const TSharedPtr<FJsonObject>* PropObject = nullptr;
							if (Property->TryGetObject(PropObject) && PropObject != nullptr)
							{
								Type = Field(*PropObject, TEXT("type"));
							}

							Info.Parameters.Add(Key);
							Lines.Add(FString::Printf(TEXT("  \"%s\": %s"),
								*Key, *SampleForType(Type, Info.bSampleIsPartial)));
						}
					}

					Json = Lines.Num() > 0
						? FString::Printf(TEXT("{\n%s\n}"), *FString::Join(Lines, TEXT(",\n")))
						: FString(TEXT("{}"));
				}
				else
				{
					// A tool with no schema takes no arguments, and `{}` is what its call wants. The
					// pane says which it is rather than leaving the caller to guess.
					Info.bHasSchema = false;
				}
				Info.SampleJson = Json;

				OutTools.Add(MoveTemp(Info));
			}
		}
		OutTools.Sort([](const FToolInfo& A, const FToolInfo& B)
		{
			return A.BareName < B.BareName;
		});
		return true;
	}

	OutError = FString::Printf(
		TEXT("'%s' is not a toolset the registry publishes."), *InToolsetName);
	return false;
}

bool FTurboMCPPanelToolRunner::FindTool(const FString& InToolsetName, const FString& InBareName,
	FToolInfo& OutTool, FString& OutError)
{
	TArray<FToolInfo> Tools;
	if (!ListTools(InToolsetName, Tools, OutError))
	{
		return false;
	}
	for (const FToolInfo& Tool : Tools)
	{
		if (Tool.BareName.Equals(InBareName, ESearchCase::IgnoreCase)
			|| Tool.FullName.Equals(InBareName, ESearchCase::IgnoreCase))
		{
			OutTool = Tool;
			return true;
		}
	}
	OutError = FString::Printf(TEXT("'%s' is not a tool of '%s'."),
		*InBareName, *InToolsetName);
	return false;
}

FString FTurboMCPPanelToolRunner::SampleJsonForTool(const FString& InToolsetName,
	const FString& InBareName, bool& bOutIsPartial, FString& OutError)
{
	bOutIsPartial = false;
	FToolInfo Tool;
	if (!FindTool(InToolsetName, InBareName, Tool, OutError))
	{
		return FString();
	}
	bOutIsPartial = Tool.bSampleIsPartial;
	return Tool.SampleJson;
}

void FTurboMCPPanelToolRunner::BeginCall(const FString& InToolsetName, const FString& InToolName,
	const FString& InArgumentsJson)
{
	Result = FCallResult();
	Result.ToolsetName = InToolsetName;
	Result.ToolName = InToolName;
	Result.Arguments = InArgumentsJson;

	FString Why;
	if (!IsRegistryAvailable(Why))
	{
		// The tool NEVER RAN, and the result says so rather than reporting an empty value that a
		// reader would take for the tool's own answer.
		Result.bIsComplete = true;
		Result.bIsError = true;
		Result.bNeverRan = true;
		Result.Error = Why;
		bCallPending = false;
		AsyncResult.Reset();
		return;
	}

	CallStartedSeconds = FPlatformTime::Seconds();

	// `ExecuteTool` is the SAME ENTRY POINT THE MCP SERVER USES (`P9`), so the panel and an agent
	// cannot take different paths through a tool.
	AsyncResult.Reset(UToolsetRegistry::ExecuteTool(InToolsetName, InToolName, InArgumentsJson));
	bCallPending = AsyncResult.IsValid();

	// THE RESULT MAY ALREADY BE IN. `UToolsetRegistry.cpp:135-146` hands the completion to a future
	// and returns, so a synchronous tool can have answered before this line - but the error paths at
	// :125-129 answer even sooner. Both are read here rather than waiting for a tick that may never
	// come for a call that is already done.
	ReadResultIfComplete();
}

const FTurboMCPPanelToolRunner::FCallResult& FTurboMCPPanelToolRunner::RefreshIfPending() const
{
	// `const` BECAUSE SLATE'S ATTRIBUTE GETTERS ARE. It mutates only the completion bookkeeping, never
	// the call. Casting away const in the PANEL instead would have hidden that a real read of shared
	// state happens during a paint, which is the kind of thing worth having in one place.
	//
	// The cast is safe here because the only member it changes is `bCallPending`, and the panel that
	// calls this is the same object that started the call.
	FTurboMCPPanelToolRunner* Self = const_cast<FTurboMCPPanelToolRunner*>(this);
	Self->ReadResultIfComplete();
	return Result;
}

void FTurboMCPPanelToolRunner::ReadResultIfComplete()
{
	if (!AsyncResult.IsValid() || Result.bIsComplete)
	{
		return;
	}
	if (!AsyncResult->bIsComplete)
	{
		return;
	}

	Result.DurationMs = (FPlatformTime::Seconds() - CallStartedSeconds) * 1000.0;
	Result.bIsError = !AsyncResult->Error.IsEmpty();
	Result.Error = AsyncResult->Error;

	if (Result.bIsError)
	{
		// An error is reported as the registry wrote it, and the value is left empty rather than
		// filled with a guess.
		Result.Value.Reset();
	}
	else
	{
		// THE VALUE IS READ FROM THE RESULT'S OWN FIELD, NOT DECODED FROM JSON.
		//
		// `UToolCallAsyncResultString` carries `Value` as a plain `FString` that IS the tool's text -
		// read from `ToolCallAsyncResultString.h:27`, `UPROPERTY(BlueprintReadOnly) FString Value;`.
		// The first version called `GetValueAsJsonString()` and then tried to deserialise it, which
		// failed to compile AND was the long way round: that call serialises the result into a JSON
		// document, so a tool returning `read: 3 nodes` arrives as a QUOTED, ESCAPED JSON string that
		// then has to be unescaped. The field is already what a human wants to read.
		//
		// The cast is checked rather than assumed: a tool whose result is not a string is a different
		// subclass, and its JSON is the honest rendering of it.
		if (UToolCallAsyncResultString* AsString = Cast<UToolCallAsyncResultString>(AsyncResult.Get()))
		{
			Result.Value = AsString->Value;
		}
		else
		{
			Result.Value = AsyncResult->GetValueAsJsonString();
		}
	}

	Result.CharacterCount = Result.Value.Len();
	Result.TokenEstimate = TurboMCPTokenEstimate(Result.Value);
	Result.bIsComplete = true;
	bCallPending = false;
}

FString FTurboMCPPanelToolRunner::CallAndWait(const FString& InToolsetName,
	const FString& InToolName, const FString& InArgumentsJson, double InTimeoutSeconds)
{
	BeginCall(InToolsetName, InToolName, InArgumentsJson);

	// The WAIT DOES NOT PUMP THE ENGINE. The editor's own tick completes the future, so this spins
	// only as long as the tick needs, and a caller on the game thread would deadlock rather than
	// block. It exists for a test, which runs while the editor ticks around it.
	const double Deadline = FPlatformTime::Seconds() + InTimeoutSeconds;
	while (!Result.bIsComplete && FPlatformTime::Seconds() < Deadline)
	{
		FPlatformProcess::Sleep(0.01f);
		ReadResultIfComplete();
	}

	if (!Result.bIsComplete)
	{
		// A TIMEOUT IS AN ANSWER, and a distinct one. Reporting an empty value would look like a tool
		// that returned nothing.
		Result.bIsComplete = true;
		Result.bIsError = true;
		Result.bNeverRan = false;
		Result.Error = FString::Printf(
			TEXT("the registry did not answer within %.1f seconds. The call may still be running; ")
			TEXT("its result was not read."),
			InTimeoutSeconds);
		bCallPending = false;
	}

	return Result.bIsError ? Result.Error : Result.Value;
}
