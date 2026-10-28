#include "Tools/TurboMCPToolsets.h"
// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
#include "Editor.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "Misc/OutputDevice.h"

namespace TurboMCPDiagnosticConsole
{
// Capture synchronous command output only. Subsequent log events belong to the log reader.
class FCapture final : public FOutputDevice
{
public:
    FString Text;
    bool Truncated = false;
    void Serialize(const TCHAR* Data, ELogVerbosity::Type, const FName&) override
    {
        const int32 Room = FMath::Max(0, 8000 - Text.Len());
        const int32 Size = FCString::Strlen(Data);
        Text.AppendChars(Data, FMath::Min(Room, Size));
        if (Size > Room) Truncated = true;
        if (Text.Len() < 8000) Text += TEXT("\n");
    }
};
struct FRequest
{
    UWorld* World = nullptr;
    IConsoleVariable* Variable = nullptr;
    FString Command, Before, Name, Value;
    bool Mutates = false, Log = false, Debugger = false;
};
struct FPending
{
    FString Token, Target, Command, Before;
    TWeakObjectPtr<UWorld> World;
    double Expires = 0;
};
// Tools run on the game thread; only the latest preview is valid. Tokens are single-use.
static FPending Pending;

bool Identifier(const FString& Text)
{
    if (Text.IsEmpty()) return false;
    for (TCHAR C : Text) if (!FChar::IsAlnum(C) && C != TCHAR('_') && C != TCHAR('.')) return false;
    return true;
}
FString LogState(UWorld* World, const FString& Name)
{
    FCapture Output;
    GEditor->Exec(World, *FString::Printf(TEXT("Log list %s"), *Name), Output);
    TArray<FString> Lines;
    Output.Text.ParseIntoArrayLines(Lines);
    for (const FString& Line : Lines)
    {
        TArray<FString> Words;
        Line.ParseIntoArrayWS(Words);
        if (Words.Num() >= 2 && Words[0].Equals(Name, ESearchCase::IgnoreCase)) return Words[1];
    }
    return FString();
}
FString Prepare(const FString& Target, const FString& Command, FRequest& Out)
{
    if (!IsInGameThread() || !GEditor) return TEXT("Diagnostic console requires the editor game thread.");
    int32 Matches = 0;
    for (const FWorldContext& Context : GEditor->GetWorldContexts())
    {
        const bool Match = (Target == TEXT("editor") && Context.WorldType == EWorldType::Editor)
            || (Context.WorldType == EWorldType::PIE && Target == FString::Printf(TEXT("pie:%d"), Context.PIEInstance));
        if (Match && Context.World()) { Out.World = Context.World(); ++Matches; }
    }
    if (Matches != 1) return TEXT("Target must identify exactly one live world: editor or pie:<instance number>.");
    if (Command.IsEmpty() || Command.Len() > 512) return TEXT("Command must contain 1..512 characters.");
    for (TCHAR C : Command)
        if (C < TCHAR(' ') || C == TCHAR(';') || C == TCHAR('|') || C == TCHAR('&'))
            return TEXT("Only one command is supported. Control characters and command chaining are refused.");
    TArray<FString> Words;
    Command.ParseIntoArrayWS(Words);
    if (Words.IsEmpty()) return TEXT("Command is empty.");
    Out.Command = FString::Join(Words, TEXT(" "));
    if (Words[0].Equals(TEXT("Log"), ESearchCase::IgnoreCase))
    {
        Out.Log = true;
        if ((Words.Num() == 2 || Words.Num() == 3) && Words[1].Equals(TEXT("list"), ESearchCase::IgnoreCase))
        {
            if (Words.Num() == 3 && !Identifier(Words[2])) return TEXT("Log category filter must be an identifier.");
            return FString();
        }
        if (Words.Num() != 3 || !Identifier(Words[1]) || Words[1].Equals(TEXT("global"), ESearchCase::IgnoreCase))
            return TEXT("Use Log list [category] or Log <exact existing category> <verbosity>. Global changes are refused.");
        const TArray<FString> Levels = {TEXT("off"),TEXT("error"),TEXT("warning"),TEXT("display"),TEXT("log"),TEXT("verbose"),TEXT("veryverbose"),TEXT("all")};
        if (!Levels.Contains(Words[2].ToLower())) return TEXT("Verbosity must be off, error, warning, display, log, verbose, veryverbose or all.");
        Out.Name = Words[1]; Out.Value = Words[2];
        Out.Before = LogState(Out.World, Out.Name);
        if (Out.Before.IsEmpty()) return TEXT("Log category was not found. Use Log list to discover registered categories.");
        Out.Mutates = true;
        return FString();
    }
    if (Out.Command == TEXT("SlateDebugger.Event.Start") || Out.Command == TEXT("SlateDebugger.Event.Stop"))
    {
        if (!IConsoleManager::Get().FindConsoleObject(*Out.Command)) return TEXT("Slate event debugger is unavailable in this build.");
        Out.Debugger = true; Out.Mutates = true;
        return FString();
    }
    if (Words.Num() > 2 || !Identifier(Words[0])) return TEXT("Unsupported diagnostic command. Use a registered CVar, Log, or SlateDebugger.Event.Start/Stop.");
    Out.Variable = IConsoleManager::Get().FindConsoleVariable(*Words[0]);
    if (!Out.Variable || Out.Variable->TestFlags(ECVF_Unregistered)) return TEXT("No registered CVar with this name. Arbitrary console commands are not supported.");
    Out.Name = Words[0]; Out.Before = Out.Variable->GetString();
    if (Out.Before.Len() > 2048) return TEXT("CVar value exceeds the supported 2048-character diagnostic bound.");
    Out.Mutates = Words.Num() == 2;
    if (Out.Mutates)
    {
        if (Out.Variable->TestFlags(ECVF_ReadOnly)) return TEXT("This CVar is read-only; no change was made.");
        Out.Value = Words[1];
        if (Out.Value.Contains(TEXT("\"")) || Out.Value.Contains(TEXT("'"))) return TEXT("CVar values must be one unquoted token.");
    }
    return FString();
}
FString Run(const FString& Target, const FString& Command, const FString& Confirmation, bool Preview)
{
    FRequest Request;
    const FString Error = Prepare(Target, Command, Request);
    if (!Error.IsEmpty()) return TEXT("REFUSED: ") + Error;
    const FString Scope = FString::Printf(TEXT("Target: %s\nScope: process-wide diagnostics; target selection does not isolate log, Slate or CVar effects.\n"), *Target);
    if (Preview)
    {
        Pending = FPending();
        if (!Request.Mutates)
            return Scope + TEXT("Query preview; no command executed. Current CVar value: ") + (Request.Variable ? Request.Before : TEXT("not applicable")) + TEXT("\nUse the edit tool with empty Confirmation to run this query.");
        Pending.Token = FGuid::NewGuid().ToString(EGuidFormats::Digits);
        Pending.Target = Target; Pending.Command = Request.Command; Pending.Before = Request.Before;
        Pending.World = Request.World; Pending.Expires = FPlatformTime::Seconds() + 60.0;
        return Scope + TEXT("Preview only; no change made. Previous value: ") + (Request.Before.IsEmpty() ? TEXT("unavailable") : Request.Before)
            + TEXT("\nChange: ") + Request.Command + TEXT("\nCVar callbacks or debugger changes may affect editor/play behavior. Restore previous settings after debugging; no config save is requested.\nConfirmation: ") + Pending.Token;
    }
    if (Request.Mutates)
    {
        if (Confirmation.IsEmpty() || Confirmation != Pending.Token || Target != Pending.Target || Request.Command != Pending.Command
            || Request.Before != Pending.Before || Pending.World.Get() != Request.World || FPlatformTime::Seconds() > Pending.Expires)
            return TEXT("REFUSED: Run a fresh preview and copy its confirmation token. Target, command or current state changed, or the token expired.");
        Pending = FPending(); // Consume before callbacks; failures must not be blindly retried.
    }
    else if (!Confirmation.IsEmpty()) return TEXT("REFUSED: Queries require empty Confirmation.");
    FCapture Output;
    bool Handled = false;
    if (Request.Variable)
    {
        if (Request.Mutates) Request.Variable->Set(*Request.Value, ECVF_SetByConsole);
        // A CVar callback may unregister its own variable. Reacquire before readback.
        IConsoleVariable* Current = IConsoleManager::Get().FindConsoleVariable(*Request.Name);
        if (!Current || Current->TestFlags(ECVF_Unregistered))
            return Scope + TEXT("Assignment/query completed, but the CVar is no longer registered. Effect is unknown; do not retry automatically.");
        const FString After = Current->GetString();
        if (After.Len() > 2048) return Scope + TEXT("Assignment/query completed, but actual value exceeds the 2048-character response bound. Do not retry automatically.");
        return Scope + TEXT("CVar: ") + Request.Name + TEXT("\nBefore: ") + Request.Before + TEXT("\nActual value: ") + After
            + TEXT("\n") + (Request.Mutates ? TEXT("Assignment requested; actual readback above is authoritative. Callbacks and gameplay effects are not verified.") : TEXT("Query only; no assignment requested."));
    }
    if (Request.Log) Handled = GEditor->Exec(Request.World, *Request.Command, Output);
    else Handled = IConsoleManager::Get().ProcessUserConsoleInput(*Request.Command, Output, Request.World);
    FString Result = Scope + FString::Printf(TEXT("Command handled: %s\nSynchronous output%s:\n"), Handled ? TEXT("yes") : TEXT("no"), Output.Truncated ? TEXT(" (truncated at 8000 characters)") : TEXT(""));
    Result += Output.Text.IsEmpty() ? TEXT("(none)\n") : Output.Text;
    if (Request.Log && Request.Mutates) Result += TEXT("Actual category verbosity: ") + LogState(Request.World, Request.Name) + TEXT("\n");
    Result += TEXT("Dispatch does not prove later effects. Use ReadTurboMCPEditorLog for subsequent events; no output here does not mean no effects.");
    return Result;
}
}
FString UTurboMCPPreviewToolset::PreviewTurboMCPDiagnosticConsole(const FString& Target, const FString& Command)
{
    return TurboMCPDiagnosticConsole::Run(Target, Command, FString(), true);
}
FString UTurboMCPEditToolset::EditTurboMCPDiagnosticConsole(const FString& Target, const FString& Command, const FString& Confirmation)
{
    return TurboMCPDiagnosticConsole::Run(Target, Command, Confirmation, false);
}
