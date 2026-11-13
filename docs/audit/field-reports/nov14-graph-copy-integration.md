# November 14 integration review

Accepted submissions: NOV13-06 (72a0556c), NOV05-11/12 (721c8b06). Independent review covered the complete source/help/test diff. The committed tree digest 7cd8d3aa0555bbe7677508f18f65b99e562832f0e5c3261b7185c4f95eeabd91 matches supervised execution 965b204c8d5e4ce4a8a85040e359ad63.

## Behavior

BlueprintLoad help explains per-asset character budgets, splitting multi-asset calls and the correct bridge arguments. No declaration/default or runtime change; historical client failure cause remains unidentified.

GraphCopy Plan and Apply share pre-mutation refusal for cross-graph local-variable references. Matching local names/types do not imply matching identity. Format Text retains its structural refusal with explicit recreate/reconnect guidance. Mixed selections fail as a whole. These are safe refusals, not support for automatic local rebinding or Format Text copying. Those capabilities remain unsupported and must not be advertised as delivered.

## Verification

Stock pinned Unreal 5.8.4 doctor/build and focused test-graph-copy-review passed: one native test, no errors or warnings. Getter/setter rejection, stale-plan revalidation, graph/pin/default/link/dirty preservation and member-getter success were covered. Sixteen fixture files were restored. No repeated editor run during integration: reviewed evidence and exact digest were reused.

First execution 109baa2c9b964eb698b5b171263f9952 built successfully but could not start the suite because TurboMCP was disabled in disposable project preferences. The retry temporarily enabled it and restored preferences. This failure remains recorded.

Worker offline validation was 24/26: generated capability inventory stale and an existing DataAssetCreateTest Reloaded lookup warning. Inventory regenerated at integration; lookup warning remains unresolved (existing TestNotNull guard is not recognized by the static scanner). Legacy check-plugin also has obsolete 5.8.3 assumptions; its failure is not replaced by a claim of full validation. The two historical Append-suite failures remain unresolved; Append was not rerun.

Runtime evidence is limited to this synthetic GraphCopy scenario. No full plugin regression or release certification.
