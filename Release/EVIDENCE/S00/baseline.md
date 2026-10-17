# S00 baseline record

Step: S00 — Record baseline and reconcile scope.
Date of record: 2026-10-16 (host clock), recorded by the sequential release agent.

## Repository identity

| Item | Value |
|---|---|
| Repository | `C:\Users\Example\Desktop\TurboMCP` |
| Active branch | `main`, tracking `private/main` |
| HEAD commit | `f045c1313f1db04ddcac269d6910be9ad6befb11` |
| HEAD subject | `Add graph repoint regression coverage` (2026-10-16 18:45:07 +0200) |
| Remotes | `origin` = FloStyle/TurboMCP, `private` = FloStyle/TurboMCP-Development |
| Stash list | empty |
| Engine | `C:\UnrealEngine`, `Engine/Build/Build.version` = 5.8.3, commit `396c9f059903aed5fec78ecd3d437a40c6415368` (`5.8.3 release`, 2026-09-22) |

## Pre-existing dirty state (preserved, not reverted)

Tracked modifications, SHA-256 of the working-tree content:

| File | SHA-256 |
|---|---|
| `Source/TurboMCP/Public/Tools/TurboMCPToolsets.h` | `101431B4D61D041C07BC8EFB0DD90EB7A1273E9559194DE9A0123E92F4178463` |
| `Source/TurboMCP/Public/TurboMCPGraphRepoint.h` | `71C488038E74C8BA68CA8C0A28D842AF5FC8E6DB4082A10E3FF86BFE295615DF` |
| `Source/TurboMCP/Public/TurboMCPGraphSwapCall.h` | `FD879809A74AB97B77DEC513C39356B81A5961A8A41A2AD2FFCAB1FE62203F89` |
| `Source/TurboMCP/TurboMCPGraphRepoint.cpp` | `CC0855284F3620C2D8E971ED48EF33C931B239C18364B91DE716482FB6B5DD89` |
| `Source/TurboMCP/TurboMCPGraphSwapCall.cpp` | `F105F70A42CA8A7CB051B2B2643850D7B52A63A929A97013ECF6E25F987B6158` |

Untracked source (preserved):

| File | SHA-256 |
|---|---|
| `Source/TurboMCP/TurboMCPBatchEditTools.cpp` | `C4915B9F2F949335B8D3ECAA7674E01762001E248025A50C01C57895254E9276` |

Untracked release documents (preserved): `RELEASE_AGENT.md`, `Release/`.

Combined tracked-diff fingerprint (`sha256` over `git diff` text):
`767249a0f23e94ebb3ac0af84c39a805f7e4af059b46f2b8059cddd04473fef2`

The original audit reported six dirty source files. Five tracked modifications plus one untracked
source file are present, so all six are accounted for. No file was reverted, stashed, or cleaned.

## Artifact identity at record time

| Item | Value |
|---|---|
| Plugin DLL | `Binaries\Win64\UnrealEditor-TurboMCP.dll`, 3 679 744 bytes, last write 2026-10-16 18:54:58 |
| Plugin PDB | `Binaries\Win64\UnrealEditor-TurboMCP.pdb`, 94 433 280 bytes, same timestamp |
| Modules file | `Binaries\Win64\UnrealEditor.modules`, BuildId `252a08be-7e88-4572-a625-5aaa7cb13b6b` |
| Newest source file | `Source/TurboMCP/Public/TurboMCPGraphSwapCall.h`, 2026-10-16 18:54:49 |

The DLL is later than every source file, so the recorded binary appears to contain the dirty
working tree. This is an observation, not proof: step S00 is an L0 step and no build or engine run
was performed here. The DLL identity is re-established by the first real build in S01/S03.

## Owned processes

No `UnrealEditor`, `UnrealEditor-Cmd`, `UnrealTraceServer`, `LiveCodingConsole`, `dotnet`,
`MSBuild` or `cl` process was running at record time. No MCP server answered on port 8000.
The only listener observed in the 19387/8080/8081/13377/3000/8088 set is the DSH Web GUI itself on
`127.0.0.1:19387`. Nothing owned by this run was stopped; nothing foreign was terminated.

## Declaration recount (measured, not historical)

`Source/TurboMCP/Public/Tools/TurboMCPToolsets.h`, current working tree:

| Toolset | Declared tools |
|---|---|
| `UTurboMCPReadToolset` | 30 |
| `UTurboMCPLookToolset` | 4 |
| `UTurboMCPPreviewToolset` | 29 |
| `UTurboMCPEditToolset` | 38 |
| **Total** | **101** |

Every one of the 101 declarations has a matching out-of-class definition in a `.cpp` file
(no orphan declaration). `TOOLS.md` still reports the pre-change total of 94, because it is a
generated file and has not been regenerated since the untracked batch tools were added. That drift
is a release defect for S17 (Z01), not an S00 blocker.

## Suite recount

| Item | Count |
|---|---|
| `Tools/test-*.py` | 68 |
| `Tools/check-*.py` | (selected by the runner's `^(test\|check\|audit)-` filter) |
| `Tools/audit-*.py` | (same filter) |
| `NoEditorSuites` in `run-suites.ps1` | 10 |
| Slow suites | 1 (`test-capture`) |
| PowerShell suite-shaped scripts | 1 (`test-suite-timeout.ps1`) |

## Ledger reconciliation

- Findings: 47, all disposition `open`, all mapped to a roadmap step by the supplied ledger.
- Board cards: 111 (`backlog` 20, `todo` 59, `done` 32, `failed` 0; 99 archived).
- Board revision observed live: 1742 (supplied snapshot revision 1632 is stale).
- Board disposition mapping and canonical card links: `Release/Audit/BOARD_MAP.json`.
- Acceptance cases: 97, all status `not_mapped`; capability manifest: `Release/CAPABILITIES.json`.

## Not done in S00

No build, no editor start, no suite run. S00 is an L0 step and this record is an inventory, not
implementation evidence.
