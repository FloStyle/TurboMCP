# NOV13-01/02 build warning repair

Readiness: ready. Integration owner: integrator.

Current review: AssetSearch declares a local Path that shadows its path parameter. Rename only the candidate package-path local and its filter use. AssetDelete calls the deprecated boolean GetObjectsWithOuter overload. Stock 5.8.4 UObjectHash.h lines 128-133 forwards true to EGetObjectsFlags::IncludeNestedObjects; use that exact enum, preserving all exclusion defaults and nested strong-reference checks.

Scope: TurboMCPAssets.cpp and TurboMCPAssetDelete.cpp only. No behavior, tool schema, help, deletion policy or engine changes. Architecture safety contracts remain unchanged. Verify the exact diff, offline gate and one supervised build; no editor regression is needed for these equivalent changes. Retain all unrelated failures. Mark implementation and build evidence separately.
