# NOV05-18 member rename: integration review

Candidate `99322b8b6cd9189b1a3005a99fe827d1ca725a10` is accepted for the
bounded repair contract. Exact three-file scope check passed against approved
base `640490eb16312f6a156cd6a398ae87a84f0a2067`. General field validation remains pending.

The focused native workflow passed on stock Unreal 5.8.4 in local execution
`6e5dc4c65bd0431aaf1ecf3717218ddf`: build exit 0, one native test successful,
zero errors and warnings. It checks owner, child and external getter/setter
references, same-named unrelated members and locals, identity, defaults,
replication association, links, positions, stale confirmation and refusals.
The later compile checks the renamed effective default. This is synthetic
in-memory evidence, not save/reload or general project acceptance.

Earlier failed executions `c06f6378e1d7471db428f2fb939d4d32` and
`ed6c1df8638e45b2983408c89a5eec77` remain evidence. Discovery confused resident
registry entries with saved metadata for a newly created Blueprint. The repair
queries disk-only metadata before treating a package as new. Case-only rename
handling was corrected separately. Help now exposes compilation guidance in its
first sentence, which is the summary returned to callers.

Repository structure validation and the lookup-guard audit pass. Tool routing
now includes both new declarations; inventory records 196 declared tools, not
196 reviewed tools.

Source review found split child pins retained the old member prefix. The
candidate now renames the complete pin subtree in place, with identity and
collision checks before mutation. Focused execution
`216da5df7dac4b66a3e33ecd7c97637c` passed build and the extended native fixture
with zero warnings or errors: a split vector getter/setter keeps its pin IDs,
connection and literal value before the separate compile succeeds. This adds
split-vector evidence; it does not certify every nested struct shape.

Stock `MarkBlueprintAsStructurallyModified` regenerates the skeleton, marks the
Blueprint modified and broadcasts change callbacks. The tool reports this
effect and requires a separate full compile before saving; it does not promise
callback isolation or undo safety across compilation. The fixture verifies
unrelated member/local references, not arbitrary project callback behavior.
Different child default overrides,
unsupported bindings/reference nodes and live instances explicitly refuse;
these refusals must not be described as supported migration capabilities.
These documented limitations remain explicit rather than being counted as
migration capabilities. NOV05-18 implementation is done for the reviewed scope.

Offline gate `76f8245bf9ba4ec6af910c7066830d38` completed with 25 checks passing
and one failure: stale generated capability inventory. Regeneration followed by
`python Tools/validate-repo.py` passed. The original failed gate is retained;
no whole-suite rerun is used to hide it.
