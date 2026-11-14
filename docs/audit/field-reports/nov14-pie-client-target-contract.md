# NOV05-15 explicit local PIE input targeting

Readiness: ready for the input-target slice; console remains separate and open.
Integration owner: Codex. Stock Unreal; DSH paused.

ReadTurboMCPPlayInputState(Target="list") shall list in-process local players
with opaque session-bound target tokens, world path, PIE instance, local-player
index and viewport readiness. Tokens bind weak world and local-player identity,
not only a reusable PIE instance number. Expired tokens refuse; no fallback to
another world. Dedicated servers without local players are not input targets.
Keep pie:0 as the existing single-world/single-player convenience alias; it
must continue to refuse ambiguity. Explicit tokens may select one player in
a multi-world session. Do not route by collection iteration order.

Resolve focus, controller, Slate user and viewport from that exact target.
Existing focus/activation guards remain; Slate delivery must not silently
reach another client's focused viewport. Scheduled input retains weak original
identities. Release must only release the requested target's tool-owned input,
and stale-target cleanup must never release another client's keys. Existing
global single-sequence ownership can remain explicit; concurrent synthetic
sequences are not added by this slice. State output identifies its target and
distinguishes global sequence ownership from the selected player's input.

Update read/input/menu-activation help to explain discovery, exact target
selection, expiry and unsupported external-process clients. No new PIE startup,
map load, console command capability or engine changes.

Focused acceptance: synthetic two-local-client discovery and exact resolution;
legacy alias ambiguity; token expiry after world teardown/restart; wrong-target
release refusal; one selected-client input/readback with the other unchanged.
Reuse existing focus/sequence checks where possible; no whole-suite repetition.
Record runtime evidence separately. Do not close NOV05-15 until the requested
console-target behavior has also been reconciled and implemented where supported.

Exact implementation scope: TurboMCPPlayInput.cpp, Public/Tools/TurboMCPToolsets.h,
and TurboMCPCommonPlayWorkflowTest.cpp under Plugins/TurboMCP/Source/TurboMCP.
Catalog and tracking are integrator-owned.
