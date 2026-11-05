# NOV04-27: scoped PIE function invocation

Status: proposed contract; implementation and native evidence pending.

Field requirement: invoke a Blueprint-callable function on a local-player subsystem in a selected PIE world to trigger a runtime debugging event. Property writes and synthetic input do not satisfy that requirement. Arbitrary editor-console typing is not the intended implementation.

Resolve one live PIE world and one existing runtime object by exact identity. Never load an asset, use a class default object, guess a first world/player, or fall back to an editor object. Local-player subsystems must be explicitly supported, including their owning local player and world. Refuse stale targets and ambiguous worlds.

Separate signature discovery/preview from invocation. Describe exact parameter names, types, directions and accepted values; validate all arguments before ProcessEvent. Require explicit invocation intent. Report actual return/out parameters and distinguish dispatch from gameplay success. Calls can change runtime state and have no general undo guarantee. Reject unsupported parameter types, latent calls, network RPC routing and editor-only targets with precise reasons rather than silently approximating them. Document any scope restrictions before accepting the task as complete.

Focused fixture must include a local-player subsystem callable that records an event and returns observable state. Prove selected-world isolation, valid arguments, missing/extra/wrong-type refusal before dispatch, stale object refusal, no-PIE refusal and accurate output. Runtime object references in arguments require same-world validation. Do not infer that a successful synthetic call validates arbitrary project functions or the original private script.
