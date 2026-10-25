# CommonUI runtime controller workflow

## Result and scope

Native run 83b71dfcc8764dff9cd82b7b212a8a46 passed the stock-engine build and vision suite. This is a limited, reproducible runtime scenario, not certification of every menu or controller. CommonUI dependencies and configuration live in TestProject only. Production TurboMCP has no new CommonUI dependency or engine patch.

## Tools and observed sequence

| Tool | Role |
| --- | --- |
| ReadTurboMCPPlayCheck | Own a bounded 30-second PIE session and report runtime warnings/errors. |
| ReadTurboMCPPlayInputState | Observe world, focus, held key and mapping contexts. CommonUI diagnostics remain explicitly unavailable. |
| EditTurboMCPPlayInput | Send timed digital/analog input through Slate. No direct action injection or callback invocation. |
| LookTurboMCPCapture | Capture game and overlay at native resolution; inspect visible results. |

1. Focus gameplay and send Gamepad_Special_Right. Enhanced Input opens a real CommonActivatableWidget pause screen.
2. Gamepad_FaceButton_Bottom activates the focused Settings CommonButton.
3. Gamepad_RightShoulder selects Video; Gamepad_LeftShoulder selects Audio. Assertions check both tab identity and linked switcher content.
4. Gamepad_FaceButton_Right routes CommonUI Back to the parent pause screen, then to gameplay.
5. Gamepad_RightX evaluates the gameplay look action after resume. Captures show the camera orientation changed.
6. Keyboard P opens Pause. Default editor Escape is refused before input. BackSpace routes the configured keyboard Back action and resumes gameplay.
7. The owned session ends with zero warnings and errors. Timed holds are released between steps, including while paused.

All actions go through the tools. Fixture setup uses stock APIs, and its independent state reads inspect the actual stack, tab, switcher content, pause flag and action counters. Fixture callbacks are normal application behavior, not test shortcuts. Ten captures preserve the sequence. The Video content and camera images were visually inspected.

## Fixes and failed attempts

- The first build needed the test module's GameplayTags dependency.
- Run 4bf5b122f0aa48fbb69497d4aab11f1f exposed default Escape stopping PIE before menu Back. The fixture then dereferenced its removed actor. PlayInput now checks configured editor PlayWorld shortcuts and refuses conflicts with the command name and next step; the fixture fails safely if PIE disappears. No editor shortcut is silently rebound.
- Run 5e44c60fcae4405e9fafe77981453055 passed state checks, but image review found unreadable selected buttons and missing linked content. This was not accepted as visual completion. The fixture now adds content to the switcher explicitly, uses readable contrast, and asserts actual displayed content.
- A fixture slot variable shadowed a UWidget member during that repair; it was renamed before the final passing build.

To use Escape as gameplay Back in PIE, explicitly change the editor's Stop Play Session shortcut first, or map another game key such as Backspace. This is an editor configuration choice, not a custom engine requirement.

## Remaining limits

Directional focus movement, focus loss during a held input, simultaneous axes/chords, modal/disabled controls, multiplayer and physical controller/device-specific prompt behavior remain unchecked. General CommonUI active-stack/device diagnostics are not yet a production tool feature. The fixture reads that state independently; do not claim ReadPlayInputState supplies it.

Full tool-review completion stays at 38/127. These two input tools have stronger scenario evidence, but the remaining cases are still tracked.

Reproduce through `python Tools/execute-tests.py --profile engine --suites test-vision-workflow --execute`. The suite includes prior vision checks and the new CommonPlay scenario. Local evidence is `TestProject/Saved/TurboMCP/Workflow/common-play.txt` plus the execution report.

Offline integration gate: 3058faa6d2674b67ad3cca6e07f9e35b passed all 26 checks.
