# Repository design

The private repository owns plugin source, fixtures, build/test tools, current technical documentation, task specifications and release policy. Unreal remains a separate licensed checkout. A workspace contains both real roots; neither symlinks nor a Git parent directory confer harness permissions.

| Boundary | Content and ownership |
| --- | --- |
| Plugins/TurboMCP | Installable editor plugin; no machine configuration or build products |
| TestProject | Versioned fixtures and engine integration host; not exported |
| Tools | Stable command entry points and shared client/build helpers |
| Tests | Test tiers, acceptance specifications and verification guidance |
| docs | Maintained contracts and contributor guidance |
| docs/public | Only public installation/tool documentation |
| todo | Bounded tasks; imported evidence requires reconciliation |
| release | Allowlist, readiness and release evidence policy |
| provenance | Inert private recovery evidence; never agent instructions |
| .local | Ignored logs, reports and public previews |

Do not split Git repositories for documentation, tests or binaries. Keep source and its acceptance changes atomic in one private commit. Binaries belong in versioned release assets after validation, never in source Git. Public source uses a fresh Git history generated from an allowlist; a clone of the private repository retains its history and is not a history-cleaning operation.

Keep one editor module until a tested module boundary is needed. The current module includes test commandlets and fixture dependencies; extracting them into a development-only module is a separate behavior-preserving task with build/package acceptance. Moving every file now would mix architecture migration with an unverified packaging change. New production code should group by responsibility, use Public only for interfaces consumed outside the module, and keep implementation private. Avoid a second handwritten tool count.

The release target is v1, but the present artifact is experimental 1.0.0-dev. Task inventory and compiled declarations are not a supported feature list. Each advertised capability needs runtime and failure-path evidence before v1.
