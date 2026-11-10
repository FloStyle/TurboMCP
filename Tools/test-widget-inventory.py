"""Validate the live Widget Inventory schema and run its isolated native fixture."""

import json
import sys

from tmcp import READ_TOOLSET, Session


TOOL = "ReadTurboMCPWidgetInventory"
AUTOMATION = "AutomationTestToolset.AutomationTestToolset"
TEST = "TurboMCP.Widget.Inventory"


def require(condition, message):
    if not condition:
        raise ValueError(message)


def main():
    session = Session()
    session.connect()
    help_text = session.call(READ_TOOLSET, "ReadTurboMCPToolHelp", {
        "toolsetName": READ_TOOLSET,
        "toolName": TOOL,
    })
    require(TOOL in help_text and "assetFolder" in help_text and "nextCursor" in help_text, help_text)

    # Exercise the registered function with no candidate loads before the fixture.
    response = session.call(READ_TOOLSET, TOOL, {
        "rules": '{"class":"/Script/UMG.Widget"}',
        "folder": "/Game/Tests",
        "pageSize": 1,
        "loadUnloaded": False,
    })
    page = json.loads(response)
    require(page["pageScanned"] <= 1 and page["loadAttempts"] == 0, repr(page))

    session.call(AUTOMATION, "DiscoverTests", {})
    report = json.loads(session.call(AUTOMATION, "RunTests", {"testNames": [TEST]}))
    entries = report["tests"]
    require(len(entries) == 1 and entries[0]["name"] == TEST, repr(entries))
    require((report["passed"], report["failed"], report["skipped"], report["total"]) == (1, 0, 0, 1),
            json.dumps(report))
    require(entries[0]["state"] == "Success" and not entries[0]["errors"] and not entries[0]["warnings"],
            json.dumps(entries[0]))
    print(f"RESULT: PASS ({entries[0]['duration']:.3f} s native automation)")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (ValueError, KeyError, TypeError) as exc:
        print("RESULT: FAIL " + str(exc))
        sys.exit(1)
