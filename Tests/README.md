# Verification layers

`python Tools/validate.py` runs the explicit offline tier from catalog.json, serially, with deadlines, logs and exit codes. It does not discover an engine or start an editor. PowerShell and Git are required by infrastructure tests. Tests still have stable Tools/test-* entry points to preserve existing runner imports and task references.

Use `python Tools/execute-tests.py --profile engine --suites <name> --execute` for the engine tier; the supervisor builds first and owns the editor and fixtures. A catalog entry is inventory, not evidence that a test passes. Tests/acceptance.json defines product release cases; its unmapped cases block a release. Source guards, mocked tests, a compile, and native smoke each prove different things.

The legacy `test-ownership.py --with-engine-build` path is not a supported standalone agent command: direct builds now require supervisor context. No routine offline test may conditionally start an engine.

For a v1 candidate: reconcile all required acceptance IDs to exact test selectors, record source and binary identity, run each required case, test a fresh public export in a clean project, then validate the release evidence. A skipped, blocked, timed-out or missing case does not pass.

See the [infrastructure review and execution gate](../docs/audit/infrastructure-review.md) for bounded local checks and remaining engine-dispatch requirements.
