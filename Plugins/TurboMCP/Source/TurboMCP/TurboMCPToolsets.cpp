// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "Tools/TurboMCPToolsets.h"
#include "TurboMCPModule.h"
#include "TurboMCPSettings.h"

#include "Containers/UnrealString.h"
#include "Editor.h"
#include "Misc/EngineVersion.h"
#include "Interfaces/IPluginManager.h"
#include "Modules/ModuleManager.h"
#include "HAL/FileManager.h"
#include "Editor/EditorEngine.h"
#include "Framework/Application/SlateApplication.h"
#include "IModelContextProtocolModule.h"
#include "ModelContextProtocol.h"
#include "ModelContextProtocolServer.h"
#include "ModelContextProtocolSettings.h"
#include "ToolsetRegistry/UToolsetRegistry.h"
#include "UObject/Class.h"
#include "Widgets/SWindow.h"

#include "Logging.h"
#include "TurboMCPCallRecorder.h"

/**
 * Toolset name an agent and the panel use, in the engine's own format.
 *
 * The engine builds `<Module>.<ClassName>` in
 * FFunctionLibraryToolset::GetToolsetClassName and does not strip the `U`, so
 * the same rule is repeated here. `07_ENGINE_NOTES.md` section 2.1 shows the
 * full name in `call_tool`, which is why a tool must be able to print it.
 */
namespace TurboMCPToolsets
{
	/** Every toolset class this plugin owns, in kind order: Read, Look, Preview, Edit. */
	const TArray<TSubclassOf<UToolsetDefinition>>& GetToolsetClasses()
	{
		static const TArray<TSubclassOf<UToolsetDefinition>> Classes = {
			UTurboMCPReadToolset::StaticClass(),
			UTurboMCPLookToolset::StaticClass(),
			UTurboMCPPreviewToolset::StaticClass(),
			UTurboMCPEditToolset::StaticClass(),
		};
		return Classes;
	}

	/**
	 * The `<Module>.<ClassName>` name the engine registers this class under.
	 *
	 * The leading `U` is stripped. This was verified against the live server
	 * rather than assumed: `list_toolsets` returns
	 * `TurboMCP.TurboMCPReadToolset`, not `TurboMCP.UTurboMCPReadToolset`.
	 *
	 * The reason is that the name actually comes from the `ScriptName` metadata
	 * that UnrealHeaderTool writes for the class, which drops the `U` prefix
	 * (`ToolsetRegistry.cpp`). `FFunctionLibraryToolset::GetToolsetClassName`
	 * builds a name from the raw C++ class name and keeps the `U`, so copying
	 * that function gives a name no toolset answers to.
	 *
	 * A name that no tool accepts is worse than no name: it costs the caller a
	 * failed call, which is what `P7` exists to prevent.
	 */
	FString GetToolsetName(const UClass* InClass)
	{
		if (InClass == nullptr)
		{
			return FString();
		}

		FString ClassName = InClass->GetName();
		if (ClassName.StartsWith(TEXT("U")))
		{
			ClassName.RightChopInline(1);
		}

		const UObject* Outer = InClass->GetOuter();
		if (Outer == nullptr)
		{
			return ClassName;
		}

		// Package name looks like "/Script/TurboMCP"; keep the last part.
		const FString PackageName = Outer->GetName();
		int32 LastSlashIndex = INDEX_NONE;
		if (PackageName.FindLastChar(TEXT('/'), LastSlashIndex))
		{
			return PackageName.RightChop(LastSlashIndex + 1) + TEXT(".") + ClassName;
		}

		return ClassName;
	}

	/** One line saying whether the MCP server is running, and where. */
	FString DescribeServerState()
	{
		// Build the address from the same settings the engine uses, so the line
		// cannot disagree with the real server. Defaults live in
		// ModelContextProtocol.h.
		const FString UrlPath = UE::ModelContextProtocol::GetServerUrlPath();
		const uint32 Port = UE::ModelContextProtocol::GetServerPortNumber();

		FString State;
		IModelContextProtocolModule* Module = IModelContextProtocolModule::Get();
		const bool bRunning = (Module != nullptr) && (Module->GetServer() != nullptr);

		if (bRunning)
		{
			State = FString::Printf(
				TEXT("server: running at http://localhost:%u%s"), Port, *UrlPath);
		}
		else
		{
			// P1: an empty or bare answer is not allowed, so say what to do next.
			State = TEXT("server: not running. Turn on Auto Start Server under ")
				TEXT("Project Settings, Model Context Protocol, or start it by hand.");
		}

		// Tool search mode changes what a client sees: three tools instead of every
		// tool (07_ENGINE_NOTES.md section 2.2), so a caller that skips this line
		// will think its tools are missing.
		const UModelContextProtocolSettings* Settings = GetDefault<UModelContextProtocolSettings>();
		if (Settings != nullptr)
		{
			State += Settings->bEnableToolSearch
				? TEXT(" tool search: on, so call_tool takes the toolset and bare tool name.")
				: TEXT(" tool search: off, so every tool is listed directly.");
		}

		if (Settings != nullptr && Settings->bEnableToolSearch)
		{
			State += TEXT("\ncall_tool arguments example: {\"toolset_name\":\"TurboMCP.TurboMCPReadToolset\",\"tool_name\":\"ReadTurboMCPStatus\",\"arguments\":{}}");
			State += TEXT("\nUse separate toolset_name and bare tool_name fields. Omitting toolset_name searches top-level MCP tools only; a joined Toolset.Tool name is not a substitute. For Tool not found, check these fields and discovery before restarting.");
		}
		State += TEXT("\nTurboMCP safety policy filters native DataTableTools.add_rows/set_rows/rename_rows, BlueprintTools.set_pin_value/compile_blueprint and ProgrammaticToolset.execute_tool_script. They may be absent from discovery; this is intentional.");
		State += TEXT("\nReplacements: Preview/EditTurboMCPDataTableRows, EditTurboMCPPinDefault (or Preview/EditTurboMCPGraphPatch), EditTurboMCPCompile and EditTurboMCPSave. For supported graph batches use Preview/EditTurboMCPBatchEdit; arbitrary scripts have no equivalent. Discover exact schemas before calling.");

		return State;
	}

	/** One line per toolset, saying whether the registry has it. */
	FString DescribeRegisteredToolsets()
	{
		FString Lines;
		for (const TSubclassOf<UToolsetDefinition>& Class : GetToolsetClasses())
		{
			const FString Name = GetToolsetName(Class.Get());
			const bool bRegistered = UToolsetRegistry::IsToolsetClassRegistered(Class);

			// `%s` with a TCHAR* is the correct specifier here. `%hs` means a
			// narrow char* in printf and would print one byte per character.
			Lines += FString::Printf(TEXT("\n%s %s"),
				bRegistered ? TEXT("registered:") : TEXT("MISSING:"),
				*Name);
		}
		return Lines;
	}

	/**
	 * Names of the live Slate windows, at most InLimit of them.
	 *
	 * Returns false when there is no live application, which is the honest
	 * answer in a commandlet rather than an empty list that reads like "no
	 * windows exist".
	 */
	bool GetLiveWindowNames(TArray<FString>& OutNames, int32 InLimit)
	{
		if (!FSlateApplication::IsInitialized())
		{
			return false;
		}

		TArray<TSharedRef<SWindow>> Windows;
		FSlateApplication::Get().GetAllVisibleWindowsOrdered(Windows);

		for (const TSharedRef<SWindow>& Window : Windows)
		{
			if (OutNames.Num() >= InLimit)
			{
				break;
			}
			OutNames.Add(Window->GetTitle().ToString());
		}
		return true;
	}
}

FString UTurboMCPReadToolset::ReadTurboMCPStatus()
{
	// `04_FORMATS.md` section 1: a Read result opens with what was read and how
	// big it is. The count is printed even when it is zero, so an agent can tell
	// "nothing registered" apart from "the check did not run" (P1).
	const int32 ToolsetCount = TurboMCPToolsets::GetToolsetClasses().Num();

	FString Result = FString::Printf(
		TEXT("read: TurboMCP status, %d toolsets\n"), ToolsetCount);

    const FTurboMCPModule* Module = FModuleManager::GetModulePtr<FTurboMCPModule>(TEXT("TurboMCP"));
    const bool Active = Module && Module->IsEnabledForSession();
    const UTurboMCPSettings* Preferences = UTurboMCPSettings::Get();
    const bool Requested = Preferences && Preferences->bEnableTurboMCP;
    Result += FString::Printf(TEXT("activation this session: %s\nper-user preference: %s\nrestart required: %s\npreferences: Editor Preferences > Plugins > TurboMCP\n"),
        Active ? TEXT("enabled") : TEXT("disabled"), Requested ? TEXT("enabled") : TEXT("disabled"), Active != Requested ? TEXT("yes") : TEXT("no"));
    if (!Active)
        return Result + TEXT("TurboMCP is disabled for this editor session. Its status tool is not exposed over MCP. Enable it in Editor Preferences and restart. Unreal MCP and other toolsets are unaffected.\n");
    if (const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("TurboMCP")))
    {
        const FPluginDescriptor& Descriptor = Plugin->GetDescriptor();
        Result += FString::Printf(TEXT("plugin descriptor version: %s (%d)\n"), *Descriptor.VersionName, Descriptor.Version);
    }
    else Result += TEXT("plugin descriptor version: unavailable\n");
    Result += TEXT("status code compiled: " __DATE__ " " __TIME__ " (compiler-local time; not a Git revision)\n");
    const FString Binary = FModuleManager::Get().GetModuleFilename(TEXT("TurboMCP"));
    Result += FString::Printf(TEXT("engine: %s\nbinary: %s\nbinary file timestamp (not revision proof): %s\nsource revision: unknown\n"),
        *FEngineVersion::Current().ToString(), *Binary, *IFileManager::Get().GetTimeStamp(*Binary).ToIso8601());
    Result += TEXT("safety: selected native DataTable/pin/compile/script routes are filtered; use TurboMCP replacements. Direct Python is outside this protection.\n");
    Result += TEXT("effects: ReadTurboMCPPlayCheck starts PIE and runs project code. Compile success does not prove execution reachability or disk persistence.\n");
	Result += TurboMCPToolsets::DescribeServerState();
	Result += TurboMCPToolsets::DescribeRegisteredToolsets();

	return Result;
}

FString GetTurboMCPStatusText()
{
	// The panel and the tool must never disagree, so the panel calls the tool's
	// code directly instead of rebuilding this text (P9).
	return UTurboMCPReadToolset::ReadTurboMCPStatus();
}

FString UTurboMCPLookToolset::LookTurboMCPToolset()
{
	/**
	 * How many window titles one result may list.
	 *
	 * P6 caps every list so a result cannot flood an agent's context. An editor
	 * can have dozens of windows open, and the agent needs to recognise a
	 * target, not read the whole layout.
	 */
	constexpr int32 MaxWindowLines = 6;

	TArray<FString> WindowNames;
	const bool bHasApplication = TurboMCPToolsets::GetLiveWindowNames(WindowNames, MaxWindowLines);

	// P1: the count is printed with its reason, so "no windows" never reads the
	// same as "the check did not run".
	FString Result = FString::Printf(
		TEXT("look: editing windows, %d visible, showing at most %d"),
		WindowNames.Num(), MaxWindowLines);

	if (!bHasApplication)
	{
		Result += TEXT("\nNo live Slate application, so there is nothing to capture. ")
			TEXT("This is what a commandlet sees, where no editor window exists.");
		return Result;
	}

	if (WindowNames.Num() == 0)
	{
		Result += TEXT("\nNo visible editor window. Open a graph or the level editor, ")
			TEXT("then call this again.");
		return Result;
	}

	for (const FString& Name : WindowNames)
	{
		Result += FString::Printf(TEXT("\nwindow: %s"), *Name);
	}

	// Say plainly what this tool did not do, so it is not mistaken for a capture.
	Result += TEXT("\nThis tool lists windows only. Use LookTurboMCPCapture to capture a Blueprint graph.");

	return Result;
}

FString UTurboMCPPreviewToolset::PreviewTurboMCPToolset()
{

	if (!UToolsetRegistry::IsAvailable())
	{
		// P1: say why the check did not run rather than returning a bare zero.
		return TEXT("nothing to preview: the toolset registry is not available, ")
			TEXT("so no schema could be read.");
	}

	const FString AllSchemas = UToolsetRegistry::GetAllToolsetJsonSchemas();
	const int32 TotalChars = AllSchemas.Len();

	// A Preview reports what it worked out. It never claims to have changed
	// anything, which is why this opens with "would change".
	FString Result = FString::Printf(
		TEXT("would change: nothing. Registry schemas for all registered toolsets: %d characters total"),
		TotalChars);

	for (const TSubclassOf<UToolsetDefinition>& Class : TurboMCPToolsets::GetToolsetClasses())
	{
		const FString Name = TurboMCPToolsets::GetToolsetName(Class.Get());
		const bool bRegistered = UToolsetRegistry::IsToolsetClassRegistered(Class);

		if (!bRegistered)
		{
			// A toolset the registry refused is invisible, and the editor still
			// looks healthy, so naming it here is the point of this tool.
			Result += FString::Printf(
				TEXT("\n%s is not registered, so an agent cannot see it at all"), *Name);
			continue;
		}

		const FString OwnSchema = UToolsetRegistry::GetToolsetJsonSchema(Class);
		Result += FString::Printf(TEXT("\n%s: %d schema characters"), *Name, OwnSchema.Len());
	}

	Result += TEXT("\nThese are whole-toolset schema sizes, not compact help sizes. The 2000-character limit applies to one ReadTurboMCPToolHelp response, not a toolset schema.");

	if (TotalChars == 0)
	{
		Result += TEXT("\nNo toolset returned a schema, which means the registry is ")
			TEXT("present but empty.");
	}

	return Result;
}

FString UTurboMCPEditToolset::ReadTurboMCPEditQueue()
{
	// D-34 refuses a save during play or simulate, because saves made then failed
	// or half worked. Every Edit tool needs the same guard, and a caller should
	// be able to ask before it plans anything.
	FString Result = TEXT("read: editor write readiness");

	if (!GIsEditor)
	{
		Result += TEXT("\nNot in the editor, so no tool can write. TurboMCP is editor only.");
		return Result;
	}

	// GEditor is null while the editor starts or shuts down.
	if (GEditor == nullptr)
	{
		Result += TEXT("\nThe editor is not available right now, so nothing can be written. ")
			TEXT("Try again once the editor has finished loading.");
		return Result;
	}

	// 06_ACCEPTANCE.md T-STRUCT-7 and T-STRUCT-8 depend on this distinction.
	// IsPlaySessionInProgress is used rather than IsPlayingSessionInEditor so a
	// session that is queued for the next tick also counts as "not now": a write
	// started in that gap would land in a world that is about to be replaced.
	// Both are the engine's own public checks on UEditorEngine, rather than
	// reading PlayInEditorSessionInfo from outside.
	const bool bPlaySessionInProgress = GEditor->IsPlaySessionInProgress();
	const bool bSimulating = GEditor->IsSimulateInEditorInProgress();

	Result += bPlaySessionInProgress
		? TEXT("\nplay in editor: running or queued, so a write would be refused (D-34)")
		: TEXT("\nplay in editor: not running");
	Result += bSimulating
		? TEXT("\nsimulate in editor: running, so a write would be refused (D-34)")
		: TEXT("\nsimulate in editor: not running");

	// Say which condition decided it, rather than only the verdict.
	const bool bCanOpenTransaction = !bPlaySessionInProgress && !bSimulating;
	Result += bCanOpenTransaction
		? TEXT("\nwrites allowed: yes, an Edit could open one transaction now")
		: TEXT("\nwrites allowed: no. Stop play or simulate, then ask again.");

	return Result;
}

void RegisterTurboMCPToolsets()
{
	for (const TSubclassOf<UToolsetDefinition>& Class : TurboMCPToolsets::GetToolsetClasses())
	{
		// The engine logs its own success and failure for this call, including
		// "invalid tool list" when a class has no AICallable functions. Repeating
		// that in our log would just duplicate engine output, so the result is
		// checked rather than logged here.
		UToolsetRegistry::RegisterToolsetClass(Class);

		if (!UToolsetRegistry::IsToolsetClassRegistered(Class))
		{
			// A toolset the registry refused is invisible to every agent, and the
			// editor still looks healthy, so this is worth a warning. The usual
			// cause is a toolset class with no `meta = (AICallable)` functions.
			UE_LOG(LogTurboMCP, Warning,
				TEXT("Toolset '%s' did not register. Check its AICallable functions."),
				*TurboMCPToolsets::GetToolsetName(Class.Get()));
		}
	}

	// *** AND THE RECORDING IS INSTALLED AFTER ALL FOUR ARE IN, WHICH IS THE ONE PLACE IT BELONGS. ***
	// The handler the engine builds for a class is replaced by a wrapper that writes the `start` record
	// before the call and the `end` record after it, so `T02`'s `done when` 1 holds on the agent's path
	// without an edit in any of the 68 tool bodies. See `TurboMCPCallRecorder.h`.
	InstallTurboMCPCallRecording();
}

void UnregisterTurboMCPToolsets()
{
	for (const TSubclassOf<UToolsetDefinition>& Class : TurboMCPToolsets::GetToolsetClasses())
	{
		UToolsetRegistry::UnregisterToolsetClass(Class);
	}

	// After the registry has let them go, so nothing is left holding an engine handler across a
	// module unload.
	RemoveTurboMCPCallRecording();
}
