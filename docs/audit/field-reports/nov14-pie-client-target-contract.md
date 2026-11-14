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

Integrator scope amendment: include TurboMCP.Build.cs to declare the ApplicationCore dependency required by the stock input-device mapper. Build ad07a0bce22d40148038fcd695c76497 reached linking and failed with an unresolved IPlatformInputDeviceMapper::Get symbol; no runtime success is claimed.

## Integration evidence

Candidate 91129b4c8541c202e015915ca3a1cd4909601caa passed the exact four-file scope gate from the trusted integration checkout. The focused native two-client probe passed in execution ea0cf7dac7444a32b96e7c36138661a2: two discovered targets, exact world resolution, ambiguous legacy alias refusal, selected-client keyboard delivery with the other unchanged, wrong-target release refusal and ended-session token refusal. The wrapper says CommonUI controller workflow, but this run explicitly selected TMCP_PIE_TARGET_ONLY=1: it is not the full CommonPlay or vision suite. Sixteen fixture files were restored.

Final output/help wording corrections compiled successfully in db0170ce4a6641318743d8e1435efcbd. Runtime behavior was unchanged after the focused pass, so the scenario was not repeated. Prior run 47be17cca20c492cb987949c65098029 compiled successfully but its validation was rejected because review documentation changed during execution; that failure is retained.

Limits: the probe covers keyboard input to one of two in-process worlds, not every gamepad/CommonUI routing configuration or split-screen device mapping. Cross-window gamepad forwarding refuses without changing preferences. General console execution is not added. Existing DiagnosticConsole uses pie:<instance number> and an allowlist whose effects remain process-wide. NOV05-15 stays open/partial for the broader console requirement; no full multiplayer certification.

Offline validation passed: .local/validation/79a291847cc74376b5c92540d7d62222/report.json. Implementation is complete for this bounded input slice; testing limits and remaining console work above remain explicit.
