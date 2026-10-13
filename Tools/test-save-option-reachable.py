# ACCEPTANCE ROW THIS SUITE CLAIMS TO PROVE: NONE (this proves `done when` 3's REACHABILITY, which no row names)
"""`EditTurboMCPSave` EXPOSES the thumbnail option, so a caller can actually ask for it.

WHY THIS SUITE EXISTS, AND WHY IT IS NOT THE SAME AS THE OTHER ONE
    `Tools/test-save-without-thumbnail.py` calls the save with the option and FAILED: the result said `saved` - so the
    success branch ran - and it did NOT name the mechanism, so `WithoutThumbnail` arrived EMPTY. *** THE CODE COMPILED
    AND THE ARGUMENT DID NOT REACH IT, WHICH ARE TWO DIFFERENT THINGS: COMPILING IS NOT REACHING. ***

    *** AND THE CAUSE IS A CONVENTION I INFERRED INSTEAD OF MEASURING: I took the lower-camel argument spelling from
    `test-json.py`'s calls and concluded `WithoutThumbnail` would appear as `withoutThumbnail`. THE ENGINE DID NOT
    ACCEPT IT - OR DROPPED IT SILENTLY, WHICH IS WORSE. *** This suite asks the engine what the arguments ARE, rather
    than guessing, AND IT FAILS IF THE OPTION IS NOT EXPOSED AT ALL - because an option no caller can reach is not an
    option, however well it compiles.

WHAT IT CHECKS
    1. the schema for `EditTurboMCPSave` can be read at all.
    2. *** IT LISTS AN ARGUMENT WHOSE NAME MENTIONS THE THUMBNAIL, IN ANY SPELLING. *** The check is on the name rather
       than on the exact string, because the point is reachability and the spelling is what is being measured.
    3. and it lists `assets`, so the schema read is the right tool's and not an empty answer.
"""

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from tmcp import EDIT_TOOLSET, READ_TOOLSET, Session  # noqa: E402

TOOL = "EditTurboMCPSave"

failures = []


def check(name, ok, detail=""):
    print("  %-4s %s%s" % ("ok" if ok else "FAIL", name, ("  " + detail) if detail else ""))
    if not ok:
        failures.append(name)


def main():
    session = Session()
    schema = session.call(READ_TOOLSET, "ReadTurboMCPToolSchema",
                          {"toolset": EDIT_TOOLSET, "tool": TOOL})
    print("    schema, first 300 characters:")
    print("      " + (schema[:300].replace("\n", "\n      ") if schema else "(none)"))

    check("the schema for the save tool can be read", bool(schema.strip()), schema[:120])
    check("and it is that tool's schema, not an empty answer", "assets" in schema.lower(), schema[:200])
    # 2. The measurement this suite exists for.
    check("THE OPTION IS EXPOSED TO A CALLER", "thumb" in schema.lower(),
          "no argument mentioning the thumbnail appears in the schema")

    print("")
    if failures:
        print("RESULT: FAIL (%d check(s) failed: %s)" % (len(failures), ", ".join(failures)))
        return 1
    # *** THE SPELLING IS PUT IN THE RESULT LINE ON PURPOSE: THE SUITE RUNNER PASSES `RESULT:` LINES THROUGH AND
    # SWALLOWS THE REST OF THE SUITE'S STDOUT, AND THE ARGUMENT NAME IS THE ONE THING THIS SUITE EXISTS TO READ. A
    # MEASUREMENT THAT CANNOT LEAVE THE RUNNER IS NOT A MEASUREMENT. ***
    flat = " ".join(schema.split())
    thumb_at = flat.lower().find("thumb")
    around = flat[max(0, thumb_at - 90):thumb_at + 40] if thumb_at >= 0 else flat[:160]
    print("RESULT: PASS (the option is reachable; the schema says: ...%s...)" % around)
    return 0


if __name__ == "__main__":
    sys.exit(main())
