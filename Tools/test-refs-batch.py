"""Exercise the live batch schema and the isolated 700-seed registry regression."""

import json
import sys

from tmcp import READ_TOOLSET, Session


BATCH = "ReadTurboMCPRefsBatch"
AUTOMATION = "AutomationTestToolset.AutomationTestToolset"
TEST = "TurboMCP.Refs.Batch"


def check(condition, message):
    if not condition:
        raise ValueError(message)
    print("ok  " + message)


def batch(session, **params):
    return session.call(READ_TOOLSET, BATCH, params)


def seed_paths(page):
    return {row["package"] for row in page["records"] if row.get("type") == "seed"}


def main():
    session = Session()
    session.connect()
    dirty_before = session.call(READ_TOOLSET, "ReadTurboMCPDirtyList", {"limit": "1000"})

    shallow = json.loads(batch(session, folder="/Game/Tests", direction="out", kinds="hard"))
    recursive = json.loads(batch(session, folder="/Game/Tests/", recursive=True,
                                 classFilter="Blueprint", direction="dependencies", kinds="hard"))
    shallow_paths = seed_paths(shallow)
    recursive_paths = seed_paths(recursive)
    check("/Game/Tests/BP_NodeTest" in shallow_paths
          and "/Game/Tests/Kit/BP_TestAlpha" not in shallow_paths,
          "non-recursive folder selection excludes descendant assets")
    check("/Game/Tests/Kit/BP_TestAlpha" in recursive_paths
          and "/Game/Tests/WBP_NodeTest" not in recursive_paths,
          "recursive folder selection includes descendants and applies the exact class filter")
    check(all(row["class"] == "Blueprint" for row in recursive["records"]
              if row.get("type") == "seed"),
          "every seed matches the requested class")

    params = {"assets": "/Game/Tests/BP_NodeTest|/Game/Tests/TA11_TestEnum",
              "kinds": "hard", "pageSize": 1}
    first = json.loads(batch(session, **params))
    second = json.loads(batch(session, **params, cursor=first["nextCursor"]))
    check(first["seedCount"] == second["seedCount"] == 2
          and first["total"] == second["total"] and second["offset"] == 1
          and first["records"] != second["records"],
          "explicit list and cursor work through the live schema")
    check(batch(session, assets="/Game/Tests/BP_NodeTest", depth=7).startswith("error:"),
          "depth over six is refused")
    check(batch(session, **params, depth=2, cursor=first["nextCursor"]).startswith("error:"),
          "cursor from a different query is refused")
    check(batch(session, assets="/Game/Tests/BP_NodeTest", folder="/Game/Tests").startswith("error:"),
          "combined seed selectors are refused")
    check(session.call(READ_TOOLSET, "ReadTurboMCPDirtyList", {"limit": "1000"}) == dirty_before,
          "batch reads leave the dirty-package list unchanged")

    session.call(AUTOMATION, "DiscoverTests", {})
    report = json.loads(session.call(AUTOMATION, "RunTests", {"testNames": [TEST]}))
    entries = report["tests"]
    check(len(entries) == 1 and entries[0]["name"] == TEST, "ran the exact native reference test")
    if entries[0]["state"] != "Success" or entries[0]["errors"]:
        raise ValueError(json.dumps(entries[0]))
    check((report["passed"], report["failed"], report["skipped"], report["total"]) == (1, 0, 0, 1),
          "native graph, pagination and timing assertions passed")
    print("RESULT: PASS")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (ValueError, KeyError, TypeError) as exc:
        print("RESULT: FAIL " + str(exc))
        sys.exit(1)
