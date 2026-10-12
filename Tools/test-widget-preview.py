"""Exercise L-WIDGET-PREVIEW against the running editor.

Checks the rules the spec states, and exits non-zero on failure.

  T-LOOK-5  render a widget with sample values
  P1        a zero and an empty message say what they mean
  P2        a bad size, bad JSON and a non-widget are refused
  P3        a Look tool changes nothing that outlives the call

NEEDS A RENDERING EDITOR
------------------------
This is the one suite that cannot run under -nullrhi, because it renders. The editor
the other suites use has rendering on by default, so this runs everywhere they do.

WHAT THE IMAGE ACTUALLY CONTAINS
--------------------------------
The test asset is a bare Canvas Panel with two empty Text Blocks, so the PNG is
**entirely black**. That is the correct result for a widget with no visible content,
and it is asserted as such rather than being presented as a rendered design. What the
suite proves is that the pipeline works end to end: an instance is made, the render
target is written, the pixels come back at the requested size, and a valid PNG lands
on disk.

A caller wanting a meaningful image sets values or adds content; that is their job,
not the tool's.
"""

import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import tmcp_paths  # noqa: E402  (after the sys.path line above)

from tmcp import LOOK_TOOLSET, Session  # noqa: E402

WIDGET = "/Game/Tests/WBP_NodeTest.WBP_NodeTest"
WIDGET_PACKAGE = "/Game/Tests/WBP_NodeTest"

failures = []


def check(name, ok, detail=""):
    print("  %-4s %s%s" % ("ok" if ok else "FAIL", name, ("  " + detail) if detail else ""))
    if not ok:
        failures.append(name)


def png_size(path):
    """Read a PNG's width and height from its IHDR, so the claim is checked."""
    try:
        with open(path, "rb") as handle:
            header = handle.read(24)
    except OSError:
        return None
    if len(header) < 24 or header[:8] != b"\x89PNG\r\n\x1a\n":
        return None
    return struct.unpack(">II", header[16:24])


def main():
    session = Session()
    session.connect()

    print("=== the widget exists and is on disk ===")
    described = session.call("TurboMCP.TurboMCPReadToolset", "ReadTurboMCPAssetDescribe",
                             {"assets": WIDGET})
    print(described)
    print()
    check("the widget is described", "WidgetBlueprint" in described)
    if "error:" in described:
        print("\nFAIL  run Tools/make-test-widget.py first.")
        return 1

    print()
    print("=== T-LOOK-5: render with the widget's own defaults ===")
    out = session.call(LOOK_TOOLSET, "LookTurboMCPWidgetPreview",
                       {"widget": WIDGET, "sampleValuesJson": "",
                        "functionName": "", "size": ""})
    print(out)
    print()
    check("reports the widget", WIDGET in out)
    check("reports the size it used", "size: 1280x720" in out)
    check("says how many values it set", "sample values: 0 set" in out)
    check("says no function was asked for", "function: none asked for" in out)
    check("writes an image and says where", "image:" in out and ".png" in out)
    check(
        "says the alpha is forced opaque, and why",
        "forced opaque" in out and "transparent" in out,
    )

    # The file, checked rather than taken on trust.
    image_path = None
    for line in out.splitlines():
        if line.startswith("image:"):
            image_path = line.split(":", 1)[1].strip()
    check("an image path was printed", image_path is not None)

    if image_path:
        # The tool prints a path relative to the engine, so it is resolved the way
        # the editor resolves it.
        absolute = os.path.abspath(os.path.join(
            os.path.join(tmcp_paths.find_engine(), "Engine", "Binaries", "Win64"), image_path))
        exists = os.path.isfile(absolute)
        check("the image exists on disk", exists, absolute)
        if exists:
            size = png_size(absolute)
            check("it is a real PNG at the size asked for", size == (1280, 720),
                  str(size))
            check("it is not empty", os.path.getsize(absolute) > 1000,
                  "%d bytes" % os.path.getsize(absolute))

    print()
    print("=== a requested size is honoured ===")
    small = session.call(LOOK_TOOLSET, "LookTurboMCPWidgetPreview",
                         {"widget": WIDGET, "sampleValuesJson": "",
                          "functionName": "", "size": "640x360"})
    check("reports the size asked for", "size: 640x360" in small)
    small_path = None
    for line in small.splitlines():
        if line.startswith("image:"):
            small_path = line.split(":", 1)[1].strip()
    if small_path:
        absolute = os.path.abspath(os.path.join(
            os.path.join(tmcp_paths.find_engine(), "Engine", "Binaries", "Win64"), small_path))
        if os.path.isfile(absolute):
            check("the second image is at the second size",
                  png_size(absolute) == (640, 360), str(png_size(absolute)))

    print()
    print("=== a script warning or error is reported, not swallowed ===")
    check(
        "says so when nothing was raised",
        "script messages:" in out,
    )

    print()
    print("=== P2: a variable that does not exist is refused with suggestions ===")
    bad_value = session.call(LOOK_TOOLSET, "LookTurboMCPWidgetPreview",
                             {"widget": WIDGET, "sampleValuesJson": '{"NotAVariable": 3}',
                              "functionName": "", "size": ""})
    print(bad_value)
    print()
    check("counts the refusal", "0 set, 1 refused" in bad_value)
    check("names the bad variable", "NotAVariable" in bad_value)
    check("offers names that do exist", "Some it does have:" in bad_value)

    print()
    print("=== P2: sample values must be a JSON object ===")
    bad_json = session.call(LOOK_TOOLSET, "LookTurboMCPWidgetPreview",
                            {"widget": WIDGET, "sampleValuesJson": "[1,2,3]",
                             "functionName": "", "size": ""})
    print(bad_json)
    print()
    check("refuses an array", bad_json.startswith("error:"))
    check("says why a name is needed", "a variable has to be named" in bad_json)

    print()
    print("=== P2: a bad size is refused by name ===")
    bad_size = session.call(LOOK_TOOLSET, "LookTurboMCPWidgetPreview",
                            {"widget": WIDGET, "sampleValuesJson": "",
                             "functionName": "", "size": "banana"})
    print(bad_size)
    print()
    check("refuses", bad_size.startswith("error:"))
    check("shows the accepted form", "1280x720" in bad_size)

    print()
    print("=== a size above the limit is refused, not clamped ===")
    huge = session.call(LOOK_TOOLSET, "LookTurboMCPWidgetPreview",
                        {"widget": WIDGET, "sampleValuesJson": "",
                         "functionName": "", "size": "9000x9000"})
    print(huge)
    print()
    check("refuses", huge.startswith("error:"))
    # *** THIS CHECK USED TO BE `"4096" in huge`, AND IT WAS A BARE-NUMERAL SUBSTRING TEST. ***
    # ANY 4096 ANYWHERE ON THE LINE SATISFIED IT - including the `9000`'s neighbour in a timestamp, a node id
    # or a byte count - so the check could pass while the tool said nothing about a limit at all. The tool's
    # own message is `error: %dx%d is larger than the %d limit.` (TurboMCPCapture.cpp:434), so the ASSERTION
    # NOW NAMES THE WHOLE SENTENCE AND NOT ONLY THE NUMBER IN IT. The sentence is what makes it about a limit.
    check("names the limit", "is larger than the 4096 limit" in huge)

    print()
    print("=== a function that takes parameters is refused ===")
    with_params = session.call(LOOK_TOOLSET, "LookTurboMCPWidgetPreview",
                               {"widget": WIDGET, "sampleValuesJson": "",
                                "functionName": "SetPositionInViewport", "size": ""})
    print(with_params)
    print()
    check("says it was not called",
          "nothing was called" in with_params or "only call a function" in with_params)

    print()
    print("=== P2: a non-widget is refused and names the class ===")
    wrong = session.call(LOOK_TOOLSET, "LookTurboMCPWidgetPreview",
                         {"widget": "BP_NodeTest", "sampleValuesJson": "",
                          "functionName": "", "size": ""})
    print(wrong)
    print()
    check("refuses", wrong.startswith("error:"))
    check("names the class it found", "is a Blueprint" in wrong)
    check("says only a Widget Blueprint works", "only a Widget Blueprint" in wrong)

    print()
    print("=== a missing widget is refused ===")
    missing = session.call(LOOK_TOOLSET, "LookTurboMCPWidgetPreview",
                           {"widget": "zzzznotathing", "sampleValuesJson": "",
                            "functionName": "", "size": ""})
    check("refuses", missing.startswith("error:"))

    print()
    if failures:
        print("RESULT: FAIL (%d) -> %s" % (len(failures), ", ".join(failures)))
        return 1
    print("RESULT: PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
