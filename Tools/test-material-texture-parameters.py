"""Focused proof for texture material parameters and sampler-safe instance editing."""

import os
import runpy
import re
import sys
import time

TOOLS = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, TOOLS)

from tmcp import EDIT_TOOLSET, READ_TOOLSET, Session  # noqa: E402
import tmcp_editor  # noqa: E402

ASSET = "M_TurboMCPFixture"
INSTANCE = "MI_TurboMCPFixture"

failures = []


def check(name, ok, detail=""):
    print("  %-4s %s%s" % ("ok" if ok else "FAIL", name, ("  " + detail) if detail else ""))
    if not ok:
        failures.append(name)


def connect():
    # Prefer a direct MCP handshake. This keeps the focused check independent of the lifecycle
    # helper's launcher-output decoding when an already-running editor is healthy.
    deadline = time.time() + 180
    while time.time() < deadline:
        try:
            session = Session()
            session.connect()
            return session
        except Exception:  # noqa: BLE001
            time.sleep(3)
    print("editor unavailable after direct MCP handshake")
    return None


def parameter_line(text, name):
    return next((line for line in text.splitlines() if line.startswith(name + " ")), "")


def main():
    runpy.run_path(os.path.join(TOOLS, "test-material-instance-quality.py"))
    session = connect()
    if session is None:
        return 1

    print("=== texture parameters in R-MAT-LOAD ===")
    read = session.call(READ_TOOLSET, "ReadTurboMCPMaterialLoad", {
        "asset": ASSET, "bIncludeInstanceOverrides": False, "tokenBudget": 4000,
    })
    print(read[:1800])
    check("the material reader answered", read.startswith("read:"), read[:180])

    srgb = parameter_line(read, "FixtureSRGBTexture")
    linear = parameter_line(read, "FixtureLinearTexture")
    print("  sRGB:   %s" % srgb)
    print("  linear: %s" % linear)
    check("the sRGB texture parameter is listed", bool(srgb), srgb)
    check("the linear texture parameter is listed", bool(linear), linear)
    check("the sRGB sampler type is printed", "SAMPLERTYPE_Color" in srgb, srgb)
    check("the linear sampler type is printed", "SAMPLERTYPE_LinearColor" in linear, linear)

    for name, line in (("sRGB", srgb), ("linear", linear)):
        default = re.search(r"\bdefault=(\S+)", line)
        handle = re.search(r"\bhandle=(\S+)", line)
        check("the %s default texture is printed" % name,
              default is not None and default.group(1) != "(none)", line)
        check("the %s texture handle is usable" % name,
              handle is not None and handle.group(1).startswith("/Engine/"), line)
        check("the %s handle identifies its default" % name,
              default is not None and handle is not None and default.group(1) == handle.group(1), line)

    print("=== incompatible named default is refused by the edit ===")
    refused = session.call(EDIT_TOOLSET, "EditTurboMCPMaterialInstanceSet", {
        "asset": INSTANCE,
        "overrides": ["FixtureLinearTexture = /Engine/EngineResources/DefaultTexture.DefaultTexture"],
    })
    print(refused[:900])
    check("the incompatible edit is refused", refused.startswith("error:"), refused[:200])
    check("the refusal names the parameter", "FixtureLinearTexture" in refused, refused[:300])
    check("the refusal names the incompatible texture", "DefaultTexture" in refused, refused[:300])
    check("the refusal states the sampler requirement", "SAMPLERTYPE_LinearColor" in refused and "linear" in refused.lower(), refused[:400])

    if failures:
        print("RESULT: FAIL (%d) -> %s" % (len(failures), ", ".join(failures)))
        return 1
    print("RESULT: PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
