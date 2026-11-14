# Data Asset fixture lookup guard

## Current review

`TurboMCPDataAssetCreateTest.cpp` loads an owned saved `UInputAction` after unloading the disposable package. `TestNotNull` gates every dereference, so this is a static-check false positive rather than an observed null dereference. `Tools/checks/check-lookup-guards.py` recognizes an explicit pointer comparison; the current `TestNotNull` condition does not satisfy that structural rule. The offline gate consequently fails in `test-lookup-guards`. This fixture is already run only through the supervised editor boundary.

## Approved repair

Change only `Plugins/TurboMCP/Source/TurboMCP/TurboMCPDataAssetCreateTest.cpp`. Keep the existing `TestNotNull` assertion and make the non-null branch an explicit comparison before dereferencing `Reloaded`. Preserve the unconditional owned-file cleanup after the branch, and preserve the save/unload/reload checks and expected error count. Do not change the checker or production Data Asset code.

The implementation assignment is `nov14-data-asset-lookup-guard` in the schema-2 scope manifest. It may run the offline guard check and `git diff --check`. The integrator owns the capability inventory refresh and any editor run. A passing static guard check proves only that the fixture's source is structurally guarded; it does not certify Data Asset persistence.
