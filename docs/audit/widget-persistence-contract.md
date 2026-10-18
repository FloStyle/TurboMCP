# Widget persistence contract (R04)

Status: source-only contract; no runtime evidence. This document specifies the
smallest save/unload/reload durability test for the accepted literal and
connected-data Widget roundtrip cases. It is not implementation permission.

## Decision

Run one compiled automation suite inside the disposable TestProject editor owned
by `run-suites.ps1`:

1. Build the accepted fixture at one unique path under
   `TestProject/Content/Tests`.
2. Observe `SaveOnCompile`, set it to `SoC_Never` in memory under a scoped guard,
   compile, save explicitly through the production save tool, and require a clean
   package.
3. Explicitly release test-owned strong references, unload the package with
   `UPackageTools::UnloadPackages`, and prove the old Blueprint object identity is
   invalid.
4. Reload from the saved file with `LoadPackage`/`LoadObject`; prove the new
   Blueprint weak identity and loaded state.
5. Read the reloaded graph and assert the intended literal/connected node, links,
   positions, defaults, widget tree/slots and sibling graph.
6. Restore/remove only the owned files on every path.

This is an in-process package reload, not a fresh editor-process test. It does
not certify save-on-compile, session persistence or user-asset durability.

## Containment and global side effects

`UPackageTools::UnloadPackages` is not a local package-only operation. In
`Engine/Source/Editor/UnrealEd/Private/PackageTools.cpp:352-630` it:

- clears the editor selection;
- calls `GEditor->Trans->Reset(...)` when `bResetTransBuffer` is true (`:457`);
- roots the packages temporarily, flushes async loading/compilation/rendering;
- broadcasts `OnUnloadPackagesDelegate`, `CleanupUnloadingObjects` and
  `OnBlueprintGeneratedClassUnloaded`;
- calls `CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS)` twice (`:609`, `:626`);
- mutates editor/package state regardless of whether the caller's package is
  unloaded.

Use `FUnloadPackageParams` with `bUnloadDirtyPackages=false` and
`bResetTransBuffer=false` to avoid discarding undo history. This does not avoid
the two GC passes or the callbacks. The test is therefore suitable **only** in
the disposable, supervisor-owned TestProject editor with no unrelated unsaved
assets. It makes no user-editor safety claim. If an unrelated dirty package or
foreign editor is present, refuse/block; do not unload.

If a retained package shell or rooted object prevents the Blueprint identity
from invalidating, report **blocked** with the package/object flags. Do not clear
unrelated references or force GC to manufacture absence. The minimum fallback is
a supervisor-owned two-pass editor restart, which `run-suites.ps1` does not have;
it is a separate prerequisite, not part of this contract.

## Unique synthetic package

Use exactly:

- package: `/Game/Tests/WidgetPersistenceContract/<guid>/WBP_<guid>`
- root: `TestProject/Content/Tests/WidgetPersistenceContract/<guid>/`
- asset file: `WBP_<guid>.uasset`
- manifest: `owned.json`
- quarantine: `WBP_<guid>.uasset.quarantine`

Before creation, refuse if `FPackageName::DoesPackageExist`, `FindPackage`,
`FindObject`, the computed file, the root directory or the manifest already
exists. The manifest records the package name and the exact owned relative paths.
Only this GUID directory is removable; no recursive delete outside it and no
pre-existing TestProject asset is touched.

## Compile and explicit save

`CompileBlueprint` can implicitly save under
`UBlueprintEditorSettings::SaveOnCompile` (`BlueprintEditorSettings.h:89,243`;
save path in `BlueprintCompilationManager.cpp:458-490`). Before compiling any
fixture:

- read `GetMutableDefault<UBlueprintEditorSettings>()->SaveOnCompile`;
- set it to `SoC_Never` in memory;
- restore the previous value with a scoped guard on every exit path;
- never call `SaveConfig`, never write `EditorPerProjectUserSettings.ini`.

If the effective value cannot be observed/controlled, stop; explicit-save proof
would be ambiguous. This is a test-project C++ seam only; there is no Python/MCP
settings API.

Save with `UTurboMCPEditToolset::EditTurboMCPSaveWithoutThumbnail(PackageName)`
(`TurboMCPToolsets.h:1650`, `TurboMCPCompile.cpp:364-533,663-666`). It uses
`UPackage::SavePackage` and `FSavePackageArgs`, avoiding the Widget thumbnail
path. Require `UPackage::IsDirty()==false`, a nonzero `WBP_<guid>.uasset`, and
the exact owned path. Do not treat the call return alone as proof.

## Unload and reload

Before unload, release all test-owned strong references to the package,
Blueprint, generated class, nodes, widget tree and slots; keep only weak
observers and stable IDs/values. This is normal ownership hygiene, not clearing
unrelated engine references or dirty flags to force success.

Call:

```cpp
FUnloadPackageParams Params({Package});
Params.bUnloadDirtyPackages = false;
Params.bResetTransBuffer = false;
const bool bUnloaded = UPackageTools::UnloadPackages(Params);
```

Require true, empty error, and invalid old `TWeakObjectPtr<UWidgetBlueprint>` and
old weak generated class. Do not dereference old raw pointers afterward.

Package-table absence is **not** guaranteed by `UnloadPackages`: it can leave a
retained package shell. Accept either:

- `FindPackage(nullptr, *PackageName) == nullptr`; or
- a found shell with `!Package->IsFullyLoaded()` and no `RF_WasLoaded`; record
  that shell as retained. A non-null shell is not by itself failure.

Reload with `LoadPackage(nullptr, *PackageName, LOAD_None)`
(`UObjectGlobals.h:693`) and `LoadObject<UWidgetBlueprint>`. Require:

- non-null new weak Blueprint and new weak generated class;
- old weak Blueprint/class remain invalid;
- new package/Blueprint is fully loaded;
- `UWidgetBlueprint`, non-null generated class and CDO, and `IsUpToDate()`.

Do not require `NewPackage != OldPackage`: address reuse is legal and a retained
shell may be reused. Identity evidence is old-weak invalidation plus new-weak
creation and loaded-from-file state.

## Post-reload assertions

Reacquire nodes only by GUIDs captured before unload; never compare UObject
pointers across reload.

Literal case:

- exact existing node GUIDs and positions;
- new `PrintString` GUID, planned position, `InString` literal and exec link;
- fresh reader form with literal and exact exec source;
- sibling graph node GUIDs/positions/links.

Connected case:

- exact existing/new node GUIDs and positions;
- new `PrintString` `InString` link to the existing source `ReturnValue`, and
  `execute` link to the original consumer `then`;
- fresh reader form with new identity, duration/literal field, source reference
  and pin, and exec source.

Widget/tree/slot values compare by name/class/parent, variable flag, text and
exact anchors/offsets/alignment/Z-order/auto-size. No widget UObject pointer
identity across reload.

## Negative controls (deterministic)

1. **Deterministic save-write failure:** create an owned directory at the exact
   `WBP_<guid>.uasset` path before saving. The production
   `UPackage::SavePackage` must fail because the target cannot become a file;
   the package must remain dirty and no valid `.uasset` may exist. Remove the
   directory in `finally`. Engine save errors are narrow expected-error
   assertions tied to this control; they are never blanket-suppressed. If the
   exact log text is unavailable, this control is blocked, not passed.
2. **Save refusal, no write:** create a transient package under `/Temp/` and call
   the production save tool; `TurboMCPCompile.cpp:481-490` refuses it before
   writing. Assert refusal, no file and no dirty-state change. This is a
   deterministic refusal, not an absent-package or read-only-file test.
3. **Missing reload:** after the positive readback, unload the saved package,
   move `WBP_<guid>.uasset` to the exact quarantine path with `IFileManager`, and
   call `LoadPackage`. Require null/failure. Restore the file in a `finally`
   guard, unload any failed partial package, reload, and reassert the positive
   state. Expected load errors are narrowly asserted and recorded; they must not
   be blanket-filtered. The wrapper still fails on unexpected test warnings or
   errors.
4. **Collision refusal:** pre-create the target root/file in a separate unique
   name and prove creation refuses before package/file write.

No dirty flag is reset to make a failed save or missing reload look clean.

## Cleanup and recovery

Use a scoped cleanup object and a `finally` equivalent on every path. Cleanup
uses only weak pointers and the owned manifest; it never dereferences a Blueprint
or graph pointer invalidated by unload or a failed save. If the first unload
succeeded, do not call `UnloadPackages` again; if it failed, attempt at most one
guarded retry and report failure rather than repeating global side effects.

- unload any package the test still owns;
- delete quarantine if present;
- delete any owned save temp file, then `WBP_<guid>.uasset`, then `owned.json`;
- delete the GUID directory only if empty;
- if any unexpected file remains, fail closed and leave it for the runner; do
  not delete outside the owned directory.

Outer recovery is conditional:

- `Tools/execution_recovery.py:37-85` snapshots/restores
  `TestProject/Content` only for engine-profile execution and only after its
  owned Job Object closes. Hard supervisor death requires the recovery journal
  and `Tools/recover-tests.py`; recovery is not unconditional.
- `Tools/run-suites.ps1:604-646` snapshots `TestProject/Content/Tests` and
  restores it at `:395-464`/`:713-728`; live restore requires a usable editor or
  a runner-owned stop. A crashed editor cannot guarantee disk restore until the
  runner's recovery path runs.

The test itself adds no GC call, but the required `UnloadPackages` call performs
the two global `CollectGarbage` passes and editor callbacks noted above. That is
why only the disposable, supervisor-owned TestProject editor is in scope; no
"no global GC" claim is made.

## Later implementation files and command

- `Plugins/TurboMCP/Source/TurboMCP/TurboMCPWidgetPersistenceTest.cpp` (new)
- `Tools/test-widget-persistence.py` (new)
- one engine catalog row `test-widget-persistence`, timeout 900.

Supervisor command (not worker):

```powershell
python -X utf8 Tools/execute-tests.py --profile engine --suites test-widget-persistence --max-seconds 900 --execute
```

Evidence: build success; one automation Success with no errors/warnings; exact
owned paths; clean package after explicit save; unload true plus old weak
invalidation; reloaded Blueprint weak identity and loaded state; graph/widget/
sibling assertions; negative-control outcomes; cleanup result; runner restore
count. Missing native evidence remains pending.

## Known vs unverified

Confirmed in source: snapshot/restore boundaries; production save and dirty-flag
success; `UnloadPackages` callbacks/transaction reset/two GC passes; `LoadPackage`
API; `SaveOnCompile` implicit-save path; owned path mapping.

Unverified: whether this Widget Blueprint can be unloaded without retained
references in the pinned editor; whether a package shell remains; post-load
`IsUpToDate`/CDO state; behavior of expected load errors under automation log
capture; exact timing. If unload cannot invalidate the old Blueprint, the minimum
prerequisite is the two-pass restart in
`docs/audit/widget-persistence-prerequisites.md`.

## Rollback

Remove this document and, if later implemented, the two new test files and the
catalog row. Restore the fixture snapshot. No production or shared change is
involved.
