# Launcher 5.8.4 package and clean-consumer evidence (R12, part 2)

This records one matrix row from [installed-engine.md](../installed-engine.md) for the installed
Epic Launcher distribution. It is evidence for the approved R12 slice, not a support claim:
no README, site or wiki text is changed by this record, and the package is not published.

## Matrix row

| Field | Value |
| --- | --- |
| Distribution | `installed-launcher` |
| Exact engine build | Unreal Engine 5.8.4, Changelist `58902052`, `IsPromotedBuild` 1, engine BuildId `55116800` |
| Platform | Win64 |
| Compiler | MSVC 14.50.35728 (VS2026 BuildTools 14.50.35717) with Windows SDK 10.0.22621.0 |
| Plugin revision | `997d502e4fea2d3e194ff7c4e7db50053ab50b8c` (tagged content is `ef1d1e1f`; this harness commit adds the Launcher identity path) |
| Package hash | `TurboMCP-1.0.0-Win64-UE5.8.4-997d502e4fea-501cb3bd7fbd435489350f585d6927aa.zip`, SHA-256 `980a4fcdeaeed01a810ecb64bc9c57679be31b8d207efd301091fff974cec47d`, 40,640,779 bytes |
| Packaged DLL | `Binaries/Win64/UnrealEditor-TurboMCP.dll`, SHA-256 `a6c6cb9b4788c868317c12ddced8e36510a99dec886186c837aac641328e15f8`, 9,569,280 bytes, `.modules` BuildId `55116800` |
| Clean-install result | Passed in run `693766e378cf45679a2d7b09a152caff`, both phases, 372 installed files each, every file matched the archive |
| Runtime acceptance | Passed in the same run: 4 toolsets registered, packaged DLL was the loaded DLL, create, duplicate refusal and read-back passed in both phases |

The package `.modules` BuildId equals the engine BuildId, so this binary is not tied to one
machine: a Launcher build ships one fixed BuildId that every installation of that build shares.
That is why this row can be a distribution claim where the source-build row cannot.

## Runs

| Run | Profile | Result | Log |
| --- | --- | --- | --- |
| `501cb3bd7fbd435489350f585d6927aa` | package | passed, 253 actions, no engine module compiled | `.local/executions/501cb3bd7fbd435489350f585d6927aa/01.log` |
| `693766e378cf45679a2d7b09a152caff` | consumer, bp-only | passed, 372 files, 0 build actions | `.local/executions/693766e378cf45679a2d7b09a152caff/00.log` |
| `693766e378cf45679a2d7b09a152caff` | consumer, cpp | passed, 372 files, 5 build actions | `.local/executions/693766e378cf45679a2d7b09a152caff/01.log` |

The Blueprint-only phase reports `build_actions` 0, so it needed no compiler. The C++ phase
compiled only its own module: `Default.rc2`, `PerModuleInline.gen.cpp`, the consumer source and
the two links.

## Blocked and failed runs, kept

| Run | Stage | Result | Cause |
| --- | --- | --- | --- |
| `b73dec4bd31747c5809108c70ba68a53` | package | failed | `doctor.py` ran `git` in the Launcher folder, which has no repository. Fixed by gating the package profile on the distribution (PR #43). |
| `e16dda80c3c2485b863e6590d562916d` | consumer, bp-only | blocked | The owned editor did not reach registration inside the 240 s readiness deadline on the first cold start; the MCP listener came up at 22:42:24. The rerun passed. |
| `b8e2093b93ae44e99c2b55c24f141bd6` | consumer, cpp | failed | The action proof read the engine's `Compile Resource [x64] Default.rc2` as unexpected. Fixed in PR #44 with a duplicate guard kept. |
| `516f05844a884d5792e6f5773c7e6d73` | package | passed, superseded | Built from `988ecb73`; superseded when the harness commit moved. |

## What this does not prove

- It does not publish or certify a Launcher release; publishing is a separate owner decision.
- It does not test a Launcher engine other than 5.8.4 Changelist `58902052`.
- It does not exercise the extended acceptance suites on the Launcher engine; only the
  V1-BP-CONSUMER and V1-CPP-CONSUMER checks were run.
- It does not change the plugin, the descriptor, the pin or any engine file.
