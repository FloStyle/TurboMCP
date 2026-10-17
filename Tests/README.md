# Verification layers

`python Tools/validate.py` runs the explicit offline tier from catalog.json, serially, with deadlines, logs and exit codes. It does not discover an engine or start an editor. PowerShell and Git are required by infrastructure tests. Tests still have stable Tools/test-* entry points to preserve existing runner imports and task references.

The engine tier uses `Tools/run-suites.ps1` after a build, with owned editor and fixture isolation. A catalog entry is inventory, not evidence that a test passes. Tests/acceptance.json defines product release cases; its unmapped cases block a release. Source guards, mocked tests, a compile, and native smoke each prove different things.

`test-ownership.py --with-engine-build` explicitly opts into the real missing-artifact build case. Never run it concurrently with a build or editor. No routine offline test may conditionally start an installed engine.

For a v1 candidate: reconcile all required acceptance IDs to exact test selectors, record source and binary identity, run each required case, test a fresh public export in a clean project, then validate the release evidence. A skipped, blocked, timed-out or missing case does not pass.
