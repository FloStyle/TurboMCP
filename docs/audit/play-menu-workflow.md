# Live play and menu workflow

Use this scenario to check what a model sees, which control it chooses, and what the game actually does. A click result is only input delivery; verify the visible result and an independent state when available.

## Tool roles and sequence

1. Start a bounded owned play session with `ReadTurboMCPPlayCheck`. Choose a duration long enough for the planned steps (maximum 600 seconds); return to normal editor frames between calls. Do not take ownership of an existing user session.
2. Capture `LookTurboMCPCapture` with `Mode="viewport"`, `Target="pie"`, and empty `Asset`, `GraphName` and `Size`. It includes the game and Slate/UMG overlay, excludes editor chrome, and reports world identity and actual pixel dimensions. It requires exactly one in-process PIE world. It does not resize or move the camera.
3. Use native SlateInspector `Snapshot` to identify the play window, then `Observe` its subtree and `Snapshot` it to discover visible control refs. Prefer a current visible control ref over guessed screen coordinates. The fixture uses a fresh snapshot before each action.
4. Call native `Click` for the intended button ref. Allow normal game ticks. Capture again and inspect the new state. If nothing changed, investigate focus, visibility and input routing; do not repeatedly click blindly.
5. Repeat for Settings, Back and Purchase. The expected final state is Purchase complete, with exactly one purchase callback.
6. Release the observer with `Unobserve`; poll `ReadTurboMCPPlayCheck` until the owned session has stopped and inspect its warnings/errors.

Input coordinates in native snapshots are desktop/widget coordinates, not coordinates relative to the PNG. Ref-based input avoids confusing the two. Screenshot freshness is not guaranteed by a successful write; confirm the next frame after transitions.

## Implemented scenario and evidence

Native run d64216f0036647cfaa62701131f443fd passed the build and full vision wrapper. The menu fixture uses native Slate buttons and real routed pointer events, not direct callback invocation. Settings and Back change the visible controls. Purchase fires exactly once. Four native-resolution game-layer images record the states. The owned 15-second session stops with zero warnings and errors. Absent PIE, missing target and an unsupported requested size refuse. Menu images were visually inspected.

The native automation framework tears down PIE when another test starts. The whole menu loop therefore runs in one latent test with normal frames between actions. Initial attempt eda5dec01a38401abcb0f079a50d050d exposed that harness behavior. Run 796bc29e2eaf4e0187c5e1735eb25517 passed the native actions but its Python wrapper failed to read UTF-16 evidence; evidence now uses explicit UTF-8. Neither failed run is counted as passed.

Reproduce with `python Tools/execute-tests.py --profile engine --suites test-vision-workflow --execute`. Fixtures are disposable and unsaved. The supervisor owns the editor and restores fixture files. Detailed local evidence includes `TestProject/Saved/TurboMCP/Workflow/play-menu.txt` and the fresh vision transcript.

## Remaining scenarios

- CommonUI activation stacks, CommonButton styles, controller focus, Back routing and input-mode changes.
- Text entry, disabled controls, modal dialogs, scrolling and localization.
- Multiple players, viewport/camera selection, standalone processes and high-DPI extremes.

This is a passed Slate menu scenario, not blanket acceptance of those cases. Existing tools are reused: unique TurboMCP workflow review coverage stays 38/125.

Offline integration gate: 368f20d3c50043e98a0bb737e06d8e55 passed all 26 checks.
