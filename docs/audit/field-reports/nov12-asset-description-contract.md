# Asset description applicability

Readiness: ready. Owner: integrator. Task: NOV09-20.

Current review: DescribeAssetLine reports unknown generated-class metadata for every registry asset without a GeneratedClass tag, including non-Blueprint assets. This confuses missing evidence with an inapplicable property.

Use the loaded asset class through FAssetData::GetClass with its default non-loading resolution. If that class is known and is not a UBlueprint subclass, report generated class: not applicable. Preserve loaded Blueprint and registry GeneratedClass evidence precedence. Unknown/unloaded class identities must remain unknown; never infer applicability from class-name substrings or load an asset/class merely to improve wording. No mutation, compilation or save.

Exact implementation scope: TurboMCPAssets.cpp and Tools/test-asset-describe-metadata.py as listed in worker-scopes.json. Review existing Blueprint checks and add a representative non-Blueprint assertion. Batch with other source work before an incremental compile where practical. Record runtime evidence separately; source review is not runtime certification.
