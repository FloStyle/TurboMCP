// Copyright FloStyle. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "CoreTypes.h"
#include "Templates/SharedPointer.h"
#include "UObject/StrongObjectPtr.h"

class UToolCallAsyncResultString;

/**
 * Calling any MCP tool from the editor panel, WITHOUT Slate.
 *
 * WHY THIS IS SEPARATE FROM THE PANEL
 * -----------------------------------
 * `QUESTIONS.md` Q-11 records a measured gap: this plugin has no Slate inspection tool, so the panel's
 * widget tree cannot be read and NO AUTOMATED CHECK COVERS ITS LAYOUT. If the tool-calling logic lived
 * in the panel, none of it could be tested either - the panel would be one large unverifiable object.
 *
 * So everything that can be plain C++ is plain C++: discovery, sample generation and execution live
 * here, and the panel is a thin shell that renders them. A test can drive this class directly. What
 * remains unverifiable is exactly the layout and the widget wiring, which is the smaller half.
 *
 * ONE CODE PATH WITH THE MCP (`P9`)
 * ---------------------------------
 * A call goes through `UToolsetRegistry::ExecuteTool`, which is the same entry point the MCP server
 * uses, rather than through a copy of a tool's body. Nothing here knows what any tool does.
 *
 * DISCOVERY IS FROM THE REGISTRY, NEVER A LIST
 * -------------------------------------------
 * The toolset and tool names come from `UToolsetRegistry::GetAllToolsetJsonSchemas`, so a tool added by
 * anyone appears in the panel without this file being touched. A hardcoded list would silently omit
 * every later tool, which is the failure mode the panel exists to avoid.
 *
 * THE RESULT IS ASYNCHRONOUS, AND THAT IS NOT OPTIONAL
 * ---------------------------------------------------
 * Read from `Engine/Plugins/Experimental/ToolsetRegistry/.../UToolsetRegistry.cpp:118-148`:
 * `ExecuteTool` creates the result, hands a lambda to a FUTURE, and returns BEFORE that lambda runs. It
 * also returns a POINTER to an object it no longer holds a strong reference to, so a caller that keeps
 * only the raw pointer can have it garbage collected between the call and the completion. The result is
 * therefore owned by a `TStrongObjectPtr` and polled.
 */
class FTurboMCPPanelToolRunner
{
public:
	/** One toolset the registry publishes. */
	struct FToolsetInfo
	{
		FString Name;

		/** The full tool names, as `<Toolset>.<Function>`. */
		TArray<FString> Tools;
	};

	/** One tool, with what a caller needs to try it. */
	struct FToolInfo
	{
		/** The full published name. */
		FString FullName;

		/** The bare name, which is what a call takes. */
		FString BareName;

		FString Description;

		/** True when the schema carried an `inputSchema.properties` object at all. */
		bool bHasSchema = false;

		/** The parameter names, in the schema's order. */
		TArray<FString> Parameters;

		/** A JSON object a caller can edit, built from the schema's own types. */
		FString SampleJson;

		/** True when some parameter had no usable type, so the sample is a guess for those. */
		bool bSampleIsPartial = false;
	};

	/** What a call produced, readable at any time. */
	struct FCallResult
	{
		/** True once the registry has answered, whether with a value or an error. */
		bool bIsComplete = false;

		/** True when the registry reported an error. */
		bool bIsError = false;

		/** The tool's own text, untouched. */
		FString Value;

		/** The error text, when there is one. */
		FString Error;

		/** What was called, for the pane's header. */
		FString ToolsetName;
		FString ToolName;
		FString Arguments;

		/** Milliseconds from the call to the answer. */
		double DurationMs = 0.0;

		/** Character count and token estimate of `Value`, which `T-PANEL-6` asks for. */
		int32 CharacterCount = 0;
		int32 TokenEstimate = 0;

		/** True when the registry was never reached, so the tool never ran. */
		bool bNeverRan = false;
	};

	/** True when the registry is answering, with the reason when it is not. */
	static bool IsRegistryAvailable(FString& OutWhy);

	/** Every toolset the registry publishes, sorted by name. */
	static bool ListToolsets(TArray<FToolsetInfo>& OutToolsets, FString& OutError);

	/** Every tool of one toolset, sorted by bare name, with its schema-derived sample. */
	static bool ListTools(const FString& InToolsetName, TArray<FToolInfo>& OutTools,
		FString& OutError);

	/** One tool, or false with the reason. */
	static bool FindTool(const FString& InToolsetName, const FString& InBareName,
		FToolInfo& OutTool, FString& OutError);

	/**
	 * Builds an editable JSON object from a tool's `inputSchema`.
	 *
	 * The types come from the schema, so a caller does not have to know one. A parameter with no usable
	 * type gets an empty string and sets `bOutIsPartial`, because a guess that looks certain is worse
	 * than one that is flagged.
	 */
	static FString SampleJsonForTool(const FString& InToolsetName, const FString& InBareName,
		bool& bOutIsPartial, FString& OutError);

	/**
	 * Calls a tool and returns the value, waiting for the registry to answer.
	 *
	 * WAITS FOR UP TO `InTimeoutSeconds`, pumping nothing itself. The editor's own tick drives the
	 * completion, so this must NOT be called from a thread that blocks the game thread, and a caller
	 * that cannot afford to wait should use `BeginCall` and `GetResult` instead. It exists because a
	 * test needs a synchronous answer and the panel can afford neither the complexity nor the risk of
	 * an unread result.
	 */
	FString CallAndWait(const FString& InToolsetName, const FString& InToolName,
		const FString& InArgumentsJson, double InTimeoutSeconds = 10.0);

	/**
	 * Starts a call without blocking, for the panel.
	 *
	 * The panel must not block the game thread, so it starts the call and reads `GetResult` on a later
	 * paint. The result object is held strongly, which is what stops it being collected before the
	 * registry answers.
	 */
	void BeginCall(const FString& InToolsetName, const FString& InToolName,
		const FString& InArgumentsJson);

	/** The call's state, readable at any time. */
	const FCallResult& GetResult() const { return Result; }

	/** True when a call has been started and has not yet answered. */
	bool IsCallPending() const { return bCallPending; }

private:
	/**
	 * Owns the result object.
	 *
	 * A `TStrongObjectPtr` and NOT a raw pointer. `UToolsetRegistry::ExecuteTool` returns a pointer to
	 * an object whose only strong reference is a local inside that function, so it can be collected
	 * before the future's lambda runs. Measured at `UToolsetRegistry.cpp:121-147`.
	 */
	TStrongObjectPtr<UToolCallAsyncResultString> AsyncResult;

	FCallResult Result;
	bool bCallPending = false;
	double CallStartedSeconds = 0.0;

	/** Reads the result object into `Result` when the registry has answered. */
	void ReadResultIfComplete();
};
