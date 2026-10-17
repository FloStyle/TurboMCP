# Build, testing and distribution design

## Identities and boundaries

A validation record identifies source commit, engine revision/distribution, configuration, binary hash, commands, exits and logs. A source guard, mocked regression, successful compile, native smoke and runtime acceptance are different evidence layers. Tests/catalog.json inventories script tiers; Tests/acceptance.json specifies required release cases. Neither file proves a pass.

## Execution

Tools/validate.py runs bounded offline checks serially and writes ignored evidence. Engine tests run only after building the selected checkout. A build executor owns the engine/editor/port and fixtures. Current leases are per checkout; cross-worktree atomic scheduling remains an explicit task. Until it is implemented, the integration owner must serialize engine work. Never kill an unowned editor.

Snapshot owned fixtures before mutation and restore exact bytes plus relevant loaded state afterward. Separate fixture generation from acceptance, and test the first run after generation; repeated passes must not conceal first-run failures. Any fixture intended to change in Git needs explicit review. A blocked, skipped or timed-out required case cannot pass.

## Packages

The complete source repository is one artifact. An installable plugin package is another. Development commandlets and fixture-only dependencies should move out of the shipping boundary only after production dependency users are identified and package tests cover the change. Run packaging against the exact target engine distribution and test in a fresh consumer project. Source-engine binaries do not prove installed-engine compatibility.

## Release closure

Release evidence must bind every required case to an executable selector and intact logs for the candidate. The complete-tree exporter preserves all tracked blobs and refuses unsafe content. A fresh-history copy must run the same offline checks and retain all documentation links. Publication and binary upload require explicit owner authorization. See [release process](../release.md).
