# Widget persistence contract (R04)

Status: source-only contract; no runtime evidence. This document specifies the
smallest save/unload/reload durability test for the accepted literal and
connected-data Widget roundtrip cases. It does not implement the test.

## Decision

Use one compiled automation suite inside the existing supervisor-owned editor:

1. Build the accepted disposable Widget fixture at a unique path under
   `TestProject/Content/Tests`.
2. Compile it, save it with the production `EditTurboMCPSaveWithoutThumbnail`
   path, and require the package to become clean.
3. Unload the package, prove it is absent from the package table and that the
   old package/object pointers are invalid.
4. Load the package again from disk and prove the new package/object identity is
   different.
5. Read the reloaded graph with the production reader and assert the intended
   literal/connected node, links, positions, defaults, widget tree/slots and
   sibling graph.
6. Restore/remove only the owned unique files on every exit path.

No editor restart is proposed. This is an in-process **package** reload test, not
a fresh-editor process test. It does not certify save-on-compile, session
persistence, or user-asset durability.

## Existing containment

- `Tools/execution_recovery.py:37-85` snapshots and restores
  `TestProject/Content` for each engine-profile execution and removes files not
  present at run start. This is the outer recovery boundary.
- `Tools/run-suites.ps1:604-605` sets the fixture root to
  `TestProject/Content/Tests`; `:637-646` snapshots it and marks the runner as
  fixture owner; `:395-464` restores it byte-for-byte, including the live-editor
  path; `:713-728` restores after each suite.
- `Tools/fixture-isolation.py:70-111` releases package handles and reloads
  assets through `init_unreal.FixtureIsolationToolset` before a live bypass, or
  the runner stops its own editor first.
- The suite must not stop, start or signal an editor. It runs inside the editor
  owned by `run-suites.ps1`; no new process boundary is needed for package
  unload/reload.
- `run-suites.ps1` has no two-pass/editor-restart mode. If `UnloadPackages`
  cannot release the fixture because of a live reference, this contract cannot
  prove a fresh disk load; the minimum prerequisite is a separate
  supervisor-owned process-restart mode. That is not authorized or implemented
  here.

## Unique synthetic package

Use:

- package: `/Game/Tests/WidgetPersistenceContract/<guid>/WBP_<guid>`
- file: `TestProject/Content/Tests/WidgetPersistenceContract/<guid>/WBP_<guid>.uasset`

`<guid>` is a lower-case 32-digit GUID created by the test. Both the package and
the file must be absent before creation. Refuse if any of these is true:

- `FPackageName::DoesPackageExist(PackageName)`.
- `FindPackage(nullptr, *PackageName)` is non-null.
- the computed file exists.
- the asset registry already reports the object path.

Refusal happens before creating a package or file. Only the owned GUID directory
may be removed; no recursive delete outside it and no workplace asset is
touched.

## Save success

Use the production tool directly from C++:

`UTurboMCPEditToolset::EditTurboMCPSaveWithoutThumbnail(PackageName)`
(`Public/Tools/TurboMCPToolsets.h:1650`,
`TurboMCPCompile.cpp:364-533,663-666`).

This path writes the package itself with `UPackage::SavePackage` and
`FSavePackageArgs`, avoiding the Widget thumbnail/design-time path. Success is
proved by the package's dirty flag becoming false (`TurboMCPCompile.cpp:537-541`)
and by the computed `.uasset` existing with nonzero size. Do not treat the
engine call's return value alone as success.

## Unload and reload

Use the existing editor API:

- `UPackageTools::UnloadPackages(const TArray<UPackage*>&, FText&, bool)`
  (`Engine/Source/Editor/UnrealEd/Public/PackageTools.h:96`;
  implementation `PackageTools.cpp:352-...`). Pass the saved package and
  `bUnloadDirtyPackages=false`; require true and empty `OutErrorMessage`.
- Immediately verify `FindPackage(nullptr, *PackageName) == nullptr` and keep a
  `TWeakObjectPtr<UPackage>`/`TWeakObjectPtr<UWidgetBlueprint>` from before the
  unload; both must be invalid.
- Reload with `LoadPackage(nullptr, *PackageName, LOAD_None)`
  (`UObjectGlobals.h:693`). Require a non-null result and
  `NewPackage != OldPackage`.
- Load the Blueprint object from the new package and require
  `UWidgetBlueprint`, a non-null `GeneratedClass`, a non-null generated CDO,
  and `IsUpToDate()`.
- The old/new package distinction plus absence from the package table is the
  primary proof that the object came from the saved bytes, not the still-loaded
  object. A file digest is supplementary and is recorded before unload and after
  reload.

If unload or the absence check fails, stop and report a blocked/unverified
result. Do not clear references or dirty flags to force success.

## Assertions after reload

Reacquire nodes only by stable GUIDs captured before unload; never dereference
pre-unload pointers or require pointer identity across reload.

Literal case:

- Exact existing node GUIDs and positions.
- New `PrintString` GUID, planned position, `InString` literal, and exec link
  from the original consumer.
- Fresh production-reader form carrying the literal and the exact exec source.
- Sibling graph GUIDs/positions/links and widget tree/slot values.

Connected-data case:

- Exact existing and new node GUIDs/positions.
- New `PrintString` `InString` link to the existing source's `ReturnValue`, and
  `execute` link to the original consumer's `then`.
- Fresh reader form carrying the new identity, literal/duration field, source
  reference and pin, and exec source.
- No extra/missing node or link relative to the pre-save delta.

Widget/tree/slot assertions compare by name/class/parent, variable flag, text,
and exact anchors/offsets/alignment/Z-order/auto-size. Do not compare widget
UObject pointers after reload.

## Negative controls

1. **Failed save:** attempt the production save on a deliberately absent package
   name (or a unique package made read-only) and require an explicit failure;
   verify the saved trial's bytes and clean/dirty state are unchanged.
2. **Missing reload:** after the positive readback, unload the saved package,
   move its file to a quarantine name inside the owned GUID directory, call
   `LoadPackage`, and require null/failure. Restore the file, reload, and verify
   the positive state again. This proves the test is not satisfying reload from
   the still-loaded package.
3. **Collision refusal:** pre-create the package/file check condition in a
   separate disposable name and prove creation refuses before writing.

Every negative control restores its own state and must not mask the positive
case. Do not reset a dirty flag after a failed save to make it look clean.

## Cleanup

Use a scoped cleanup object with a `Retire`/`finally` equivalent:

- Unload any package the test still holds.
- Delete only `<owned GUID>.uasset` and, if empty, its owned directory.
- Remove the transient package/objects from memory; do not run global GC.
- On cleanup failure, report it and leave the file for the runner's fixture
  restore. The outer snapshot is the fallback, not an excuse to skip cleanup.
- Never save, delete or move a pre-existing TestProject asset.

The outer `execute-tests.py` snapshot (`TestProject/Content`) and the
`run-suites.ps1` fixture snapshot (`TestProject/Content/Tests`) both cover this
path. They restore it after normal failure and after a hard run failure; they do
not prove a fresh editor process.

## Implementation files and commands (later, separately authorized)

New files only:

- `Plugins/TurboMCP/Source/TurboMCP/TurboMCPWidgetPersistenceTest.cpp`
- `Tools/test-widget-persistence.py`
- one engine catalog row `test-widget-persistence`, timeout 900.

The wrapper follows `Tools/test-widget-roundtrip.py` strict
`AutomationTestToolset` parsing and must also fail on warnings.

Proposed supervisor command:

```powershell
python -X utf8 Tools/execute-tests.py --profile engine --suites test-widget-persistence --max-seconds 900 --execute
```

Expected evidence: build succeeds; one automation Success with no errors or
warnings; unique package/file paths; save dirty=false; unload true plus absent
package; reload new package identity; fresh reader assertions; negative-control
results; cleanup and runner restore count. Missing native evidence is pending,
not passed.

## Source-confirmed vs unverified

Confirmed in source: snapshot/restore boundaries; production save path and
dirty-flag success; `UnloadPackages`/`LoadPackage` APIs; package path mapping;
fixture ownership; runner fixture restore.

Unverified: whether a loaded Widget Blueprint and its generated class can be
unloaded cleanly in this editor (references/asset registry may refuse); the
post-load `IsUpToDate`/GeneratedClass state; save failure behavior under a
read-only package; file quarantine/reload failure behavior; exact runtime
timing and editor-side restore result.

If `UnloadPackages` refuses the loaded Widget fixture, the minimum prerequisite
is the supervisor-owned two-pass restart described in
`docs/audit/widget-persistence-prerequisites.md`; that is a separate approval,
not part of this contract.

## Rollback

Remove the document and, if later implemented, the two new test files and the
catalog row. Restore the fixture snapshot; no production or shared change is
involved.
