# Read-only mode: design draft

**Design draft, not implementation permission.** This document specifies a
read-only mode for the TurboMCP server. It proposes a setting, two enforcement
points, the tools it must exclude, the refusal text, and the tests that would
prove it. No code exists for it yet, and this document does not authorize any.

[Documentation](../README.md) | [Tool reference](../user/tools.md) | [Architecture](../architecture.md) | [Tool controls](tool-controls.md)

## Why

`README.md:22` states the position plainly: "Read, view and edit are workflow
groups, not permission levels. Some calls execute project code or change editor
state." A studio that connects an agent to a shared project cannot rely on the
agent choosing only preview calls. The current controls are binary and coarse:

- `bEnableTurboMCP` (`Plugins/TurboMCP/Source/TurboMCP/Public/TurboMCPSettings.h:37`)
  is off by default and gates everything.
- `DisabledToolDomains` lets a user switch off a domain such as `Blueprint` or
  `Vision` (`TurboMCPSettings.h:29`), and `ToolOverrides` allows or denies one
  exact tool (`TurboMCPSettings.h:33`), both at
  `TurboMCPToolPolicy.cpp:24`–`35`.
- The policy already blocks an Edit call when its matching Preview is disabled
  (`TurboMCPToolPolicy.cpp:38`–`42`).

There is no way to say "this session may look and plan, but must not change
anything and must not run project code". That is the read-only mode.

## The setting

Two shapes fit the existing settings object, which is per user and per project
and requires a restart (`UCLASS(config = EditorPerProjectUserSettings)`,
`TurboMCPSettings.h:19`).

**Option A, recommended: one boolean.**

```cpp
UPROPERTY(EditAnywhere, config, Category = "Tool controls",
          meta = (ConfigRestartRequired = true, DisplayName = "Read-only session"))
bool bReadOnlyMode = false;
```

**Option B: an allow-list of workflow groups**, `AllowedWorkflowGroups`
(`Read`, `Look`, `Preview`), which generalizes better but immediately needs a
second exception list for the Preview and Read tools that still execute code
(section below), so it is not simpler.

Either way the value is read once at module start next to
`bEnabledForSession` (`TurboMCPModule.cpp:35`) and passed into
`TurboMCPToolPolicy::Initialize` (`TurboMCPModule.cpp:109`).

## Where it is enforced

The policy layer already has exactly the two points needed, and both must be
used. One is not enough: registration decides what a client can discover, and
the call-time check decides what it can still invoke by name.

**1. Registration.** `TurboMCPToolPolicy::Initialize`
(`TurboMCPToolPolicy.cpp:24`) builds the denial map that the registry consults
for tool discovery. Read-only mode adds one rule there, before the existing
domain and override rules so that a per-tool override cannot open a hole:

- every `EditTurboMCP*` name is denied;
- every tool listed in the exclusion table below is denied;
- `ToolOverrides` may not re-enable a denied name in read-only mode. The
  existing code lets an override win (`TurboMCPToolPolicy.cpp:31`–`33`); read-only
  mode must invert that order.

**2. Call time.** `TurboMCPCallRecorder` re-checks the policy on every call
(`TurboMCPCallRecorder.cpp:316`) and refuses before dispatch, with the existing
shape `"unchanged: " + PolicyReason + " This call did not execute."`
(`TurboMCPCallRecorder.cpp:318`). Read-only mode reuses it: the denial reason
comes from the policy, so a name that skipped discovery but reached the call
path is still refused, and the refusal happens before any asset load, graph
copy, transaction, render or save.

The same check exists in the batch path (`TurboMCPCallRecorder.cpp:231`) and in
bulk export (`TurboMCPBulkExport.cpp:138`), so both inherit the mode without new
code.

## Tools that must be excluded from read-only mode

The Preview and Look groups are plans and observations, but not all of them are
side-effect free. This table lists every Preview and Look tool whose contract
records that it runs project code, executes a console or process-wide command,
starts a play session, saves, or changes editor or selection state. Citations are
the tool contract in
`Plugins/TurboMCP/Source/TurboMCP/Public/Tools/TurboMCPToolsets.h`.

| Tool | Line | What it does that read-only must not allow | Citation | Disposition |
| --- | --- | --- | --- | --- |
| `LookTurboMCPWidgetPreview` | 681 | Constructs the widget and may call a widget function: runs project code. Writes a PNG under Saved. | 666–673, esp. 672 "allow permits project code" | **Excluded** |
| `LookTurboMCPCapture` | 704 | "May load assets and change editor view, focus or selection"; `select-nodes` changes selection; `viewport` reads a live PIE world. | 694–695, 699 | **Excluded** |
| `LookTurboMCPLayoutDoneCheck` | 633 | "may open editor UI and writes an image file" (Saved folder). | 625–626 | **Excluded** (file write, not an asset save) |
| `LookTurboMCPLayoutReport` | 622 | Loads the Blueprint and measures Slate geometry. | 612 | Allowed; asset load only |
| `LookTurboMCPGraphPositions` | 646 | Loads the Blueprint and renders to measure sizes. | 636 | Allowed; asset load only |
| `PreviewTurboMCPAssetDelete` | 728 | "loading and garbage collection may run". | 724 | Allowed, with the load caveat |
| `PreviewTurboMCPDataAssetCreate` | 746 | Preview validates without allocation; `Save` and source control belong to Edit. | 738–743 | Allowed; must refuse `Save=true` in read-only |
| `PreviewTurboMCPPlayInvoke` | 758 | Validates a scoped PIE call **without executing it**. | 751 | Allowed; it needs a running PIE world but does not call into it |
| `PreviewTurboMCPDiagnosticConsole` | 776 | Previews only; the Edit twin dispatches Log/SlateDebugger/CVar. "No shell, file operations or arbitrary console commands." | 771–773 | Allowed; the Edit twin is excluded by group |
| `PreviewTurboMCPFunctionRename` | 834 | "Dependencies are loaded; load callbacks may run." | 828 | Allowed, with the load caveat |
| `PreviewTurboMCPNodePins` | 1124 | "constructing a temporary node; loads assets and executes editor probe code". | 1116 | Allowed, with the editor-probe caveat |
| `PreviewTurboMCPNodePinGrowth` | 1413 | "Loading may execute project callbacks." | 1407 | Allowed, with the load caveat |
| `PreviewTurboMCPVariableFlags` | 1423 | "Asset loading may run project callbacks." | 1416 | Allowed, with the load caveat |
| `PreviewTurboMCPWidgetEvent` | 1432 | "Loading can execute project callbacks." | 1426 | Allowed, with the load caveat |
| `PreviewTurboMCPBlueprintReparent` | 1439 | "loading may execute project callbacks"; its Edit compiles (never saves). | 1435 | Allowed, with the load caveat |
| `PreviewTurboMCPDelegateRetarget` | 1448 | "Loading may execute project callbacks." | 1442 | Allowed, with the load caveat |
| `PreviewTurboMCPComment` | 1068 | "may measure transient graph widgets". | 1056 | Allowed; transient widgets only |
| `PreviewTurboMCPGraphTidy` | 1178 | Measures native geometry, no live edit. | 1130 | Allowed |

**Read-group exceptions that a read-only mode must also close.** Granting the
whole Read group would leave two holes:

- `ReadTurboMCPPlayCheck` (line 491) "starts PIE and runs project code"
  (`TurboMCPToolsets.h:25`). It must be denied in read-only mode even though it
  is a Read tool.
- `ReadTurboMCPPlayFunction` (line 53) inspects a runtime function without
  invoking it; `ReadTurboMCPPlayInputState` (line 83) reads input state. Both are
  allowed.

The "load caveat" rows matter for honesty, not for the gate: loading a project
package can run engine and project callbacks (PostLoad, construction). Read-only
mode cannot prevent that without refusing asset loads entirely, which would
leave nothing to look at. The document records the caveat instead of pretending
otherwise.

## Refusal format

Read-only refusals reuse the existing style, which is a single line that starts
with what did not change and ends with what did not run:

```
unchanged: <reason> This call did not execute.
```

That shape is built at `TurboMCPCallRecorder.cpp:318` and is what the policy
already produces for a disabled tool (`TurboMCPToolPolicy.cpp:33`–`35`). The
read-only reason follows the same voice as the existing denials, which name the
cause and where to change it (`TurboMCPToolPolicy.cpp:71`–`80`):

```
unchanged: This turboMCP session is read-only. EditTurboMCPGraphTidy changes the
project, so it is refused. Clear Read-only session in Editor Preferences >
Plugins > TurboMCP and restart the editor. This call did not execute.
```

One sentence of cause, one naming the tool's effect, one saying where the
setting lives, and the closing `This call did not execute.` that every policy
refusal already carries.

## Tests that prove an Edit call is refused before any change

Each test must show both halves: the refusal, and that nothing changed.

1. **Policy unit test** in the existing style of
   `TurboMCPToolPolicyTest.cpp:41`–`60`: initialize with `bReadOnlyMode = true`
   and assert that `EditTurboMCPGraphTidy`, `PreviewTurboMCPAssetDelete`'s edit
   twin, `LookTurboMCPWidgetPreview` and `ReadTurboMCPPlayCheck` are denied,
   while `ReadTurboMCPStatus` and `PreviewTurboMCPGraphTidy` are allowed.
2. **Call-path test**: with read-only on, call an Edit tool through the same
   entry point used by the recorder and assert the exact refusal string and that
   the undo buffer count and the asset's package dirty flag are unchanged before
   and after.
3. **No-side-effect test for the execution exception**: call
   `LookTurboMCPWidgetPreview` with `Execution=allow` in read-only mode and
   assert the refusal contains `read-only` and that no file was added under the
   project's Saved screenshot folder.
4. **Default path**: with read-only off, the same calls behave as today, so the
   mode cannot change existing behaviour when it is not set.

A passing test must assert the refusal *and* the absence of the change; asserting
only the refusal would not prove the ordering.

## Interaction with the default-off preference

`bEnableTurboMCP` is false by default (`TurboMCPSettings.h:37`). Read-only mode
is a restriction *inside* an enabled session, so:

- master off wins: every tool is denied with the existing "TurboMCP is disabled
  for this editor session." (`TurboMCPToolPolicy.cpp:34`), and read-only never
  appears in the message;
- read-only on with master off leaves the session exactly as it is today, which
  is closed;
- read-only on with master on denies the edit and execution set above and keeps
  the reading and planning set;
- both settings require a restart (`ConfigRestartRequired`), and the existing
  pending-change line in `Describe` (`TurboMCPToolPolicy.cpp:83`) already
  reports that a preference change needs a restart;
- the panel's Tool controls area should show the effective mode read-only, as
  it does for domains today, without adding a second switch that can disagree.

## Open questions for the owner

1. **Name and scope.** Is `bReadOnlyMode` the term studios expect, or should it
   be a profile ("Read-only", "Plan and preview", "Full")?
2. **`LookTurboMCPCapture`.** It is the most useful Look tool and the one that
   changes view and selection. Exclude it, or allow only the graph capture modes
   and refuse `select-nodes`, `viewport` and `screen`?
3. **`ReadTurboMCPPlayCheck`.** Read-only mode starting PIE is clearly wrong,
   but the tool is the only Play verification. Deny it in read-only mode, or
   allow it only when a PIE session already exists?
4. **File writes.** `LookTurboMCPLayoutDoneCheck` and
   `LookTurboMCPWidgetPreview` write images under Saved. Is a Saved-folder write
   acceptable in a read-only session, or must read-only mean "no file writes
   either"?
5. **Enforcement of asset loads.** Should read-only mode warn once when a call
   loads a package, since load callbacks can run project code?
6. **Per-project default.** Should the mode be settable in shared project config
   so a studio can enforce it, given that settings are currently per user
   (`TurboMCPSettings.h:19`)?
7. **Client-visible discovery.** Should read-only mode hide the excluded tools
   from `list_toolsets` and schemas, or list them as refused so an agent learns
   why they are missing?

## Out of scope

- No implementation, no engine run, no test execution.
- Edit-group tools are excluded wholesale; their individual behaviour is already
  documented in the tool reference.
- Imported feedback or asset content that carries instructions is a separate
  concern; read-only mode limits damage but is not a content filter.
