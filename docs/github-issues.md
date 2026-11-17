# GitHub issue map

[Open issues](https://github.com/FloStyle/TurboMCP/issues) | [1.0.0 milestone](https://github.com/FloStyle/TurboMCP/milestone/1)

GitHub holds discussion and new evidence. The linked source plans preserve report IDs, scope and historical evidence. This map does not dispatch work or mark a fix verified.

| Record | GitHub issue |
| --- | --- |
| `NOV04-01` | [#2 Investigate stock-engine asset deletion crash](https://github.com/FloStyle/TurboMCP/issues/2) |
| `NOV04-14` | [#3 Reproduce integer zero default refusal](https://github.com/FloStyle/TurboMCP/issues/3) |
| `NOV04-37` | [#4 Track engine PIE stop state versus script errors](https://github.com/FloStyle/TurboMCP/issues/4) |
| `NOV04-38` | [#5 Track engine asset lookup and referencer failures](https://github.com/FloStyle/TurboMCP/issues/5) |
| `NOV05-02` | [#6 Place compact loop blocks with nested Completed continuations](https://github.com/FloStyle/TurboMCP/issues/6) |
| `NOV05-03` | [#7 Report meaningful layout regressions and distinguish intended handoffs](https://github.com/FloStyle/TurboMCP/issues/7) |
| `NOV05-15` | [#8 Target a specific local PIE client for input and console requests](https://github.com/FloStyle/TurboMCP/issues/8) |
| `NOV05-16` | [#9 Replace placed widget class while preserving compatible structure](https://github.com/FloStyle/TurboMCP/issues/9) |
| `NOV10-02` | [#10 Investigate client-specific policy-unavailable refusals](https://github.com/FloStyle/TurboMCP/issues/10) |
| `NOV10-11` | [#11 Investigate malformed self-pin read evidence](https://github.com/FloStyle/TurboMCP/issues/11) |
| `NOV13-03` | [#12 Broaden bulk scan coverage without loading maps accidentally](https://github.com/FloStyle/TurboMCP/issues/12) |
| `ASSET-EXACT` | [#13 Resolve unloaded exact object paths in multi-asset packages](https://github.com/FloStyle/TurboMCP/issues/13) |
| `REL-SCOPE` | [#14 Review open findings and agree the 1.0.0 release scope](https://github.com/FloStyle/TurboMCP/issues/14) |
| `REL-ACCEPT` | [#15 Complete exact-candidate 1.0.0 acceptance](https://github.com/FloStyle/TurboMCP/issues/15) |
| `REL-PACKAGE` | [#16 Verify clean installation and versioned release packages](https://github.com/FloStyle/TurboMCP/issues/16) |
| `REL-HOST` | [#17 Finish repository protections and release publication controls](https://github.com/FloStyle/TurboMCP/issues/17) |
| `DEV-CLAIM` | [#18 Make fresh-checkout task claim setup and output reliable](https://github.com/FloStyle/TurboMCP/issues/18) |
| `UE59-PREP` | [#19 Prepare and verify Unreal 5.9 compatibility separately](https://github.com/FloStyle/TurboMCP/issues/19) |
| `CI-MAINT` | [#20 Update pinned actions to supported runtime versions](https://github.com/FloStyle/TurboMCP/issues/20) |
| `V1-GRAPH` | [#27 Require complete edit exports and explicit graph deletions](https://github.com/FloStyle/TurboMCP/issues/27) |
| `V1-LOG` | [#28 Report disk logging failures and bound rotated records](https://github.com/FloStyle/TurboMCP/issues/28) |
| `DOC-GUIDE` | [#23 Add interactive technical guide and presentation mode](https://github.com/FloStyle/TurboMCP/issues/23) |

The eleven November field records are open investigations or partial work. They are not eleven newly confirmed defects. The multi-asset resolver and fresh-checkout claim fixes merged in PR #22 and their issues are closed with focused verification evidence. Release checks and Unreal 5.9 preparation are separate.

Older FIELD-03 through FIELD-10 and the remaining roadmap records are linked from REL-SCOPE for reconciliation. Keep duplicate findings under one owner rather than copying every historical row into a new issue.

When an issue closes, link the fix and evidence; keep this map as a stable ID index. A daily source release does not close any issue automatically.

The [17 November reconciliation](audit/v1-remaining-reconciliation-2026-11-17.json)
links current issues, older FIELD records and proposed repairs. The separate
[acceptance mapping proposal](audit/v1-acceptance-mapping-2026-11-17.json) covers
97 required cases: 20 existing-equivalent candidates, 59 partial checks and
18 missing checks. These are coverage classifications, not test results.
Graph replacement (#27) remains in review after independent source findings.
The call-log disk repair (#28) passed a supervised build and focused native
fixture on 30937018; its source integration is ready for main. The CI runtime
maintenance (#20) is merged and closed after validation, Pages and daily
source release passed. These results do not complete the V1 release gates.
