"""Run the isolated Designer navigation read fixture on the connected editor."""

import json
import sys

from tmcp import Session


AUTOMATION = "AutomationTestToolset.AutomationTestToolset"
TEST = "TurboMCP.Widget.NavigationRead"


def main():
    session = Session()
    session.connect()
    session.call(AUTOMATION, "DiscoverTests", {})
    report = json.loads(session.call(AUTOMATION, "RunTests", {"testNames": [TEST]}))
    entries = report["tests"]
    if (len(entries) != 1 or entries[0]["name"] != TEST
            or (report["passed"], report["failed"], report["skipped"], report["total"]) != (1, 0, 0, 1)
            or entries[0]["state"] != "Success" or entries[0]["errors"] or entries[0]["warnings"]):
        raise ValueError(json.dumps(report))
    print("RESULT: PASS (%.3f s native automation)" % entries[0]["duration"])
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (ValueError, KeyError, TypeError) as exc:
        print("RESULT: FAIL " + str(exc))
        sys.exit(1)
