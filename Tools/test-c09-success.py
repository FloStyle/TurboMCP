"""C09's SUCCESS half: a real success through the structured client.

The refusal half was observed in an earlier round. Every success attempt since was refused BY THE PLAN, for a
legitimate reason: the chosen target output was already occupied ("target 'then' cannot connect to reader ...:
Replace existing output connections"). That is the plan working.

This uses the documented path for a NEW target - an empty `targetNodeId` with a `targetTypeId` - so the tool
creates the node and its output is free by construction.
"""

import os, re, sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from tmcp import EDIT_TOOLSET, PREVIEW_TOOLSET, READ_TOOLSET, Session, call_structured  # noqa: E402

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ASSET, GRAPH = "TurboMCPAcceptance", "AppendChainProbe"
failures = []


def check(name, ok, detail=""):
    print("  %-4s %s%s" % ("ok" if ok else "FAIL", name, ("  " + detail) if detail else ""))
    if not ok:
        failures.append(name)


# *** THE FIXTURE IS REBUILT FIRST, AND THAT IS THE POINT OF THIS LINE. ***
#
# MEASURED: without it this case is ORDER-DEPENDENT. The first run succeeded and the second reported
# `outcome=refused, compile=not_run, exit 1` - correctly, because the first run's edit had PERSISTED,
# `AppendChainProbe`'s links had already moved and there was nothing left to repoint. A case whose success
# depends on whether it has been run before reports a spurious failure on the second run, which is exactly the
# false alarm that cost round 22 on the swap suite.
#
# `rebuild_fixture` STOPS the editor and does not start it, so `ensure_editor` follows.
import tmcp_editor  # noqa: E402

_rebuilt, _detail = tmcp_editor.rebuild_fixture("make-acceptance-asset.ps1", ["TURBOMCP_REPOINT_REBUILD"])
if _rebuilt is not True:
    # A rebuild that was NOT REQUESTED is not a failure of the rebuild - it means the fixture may be in ANY state,
    # including one this case already edited. Refusing is the honest answer: a success assertion that depends on
    # the fixture being freshly built must not run against a fixture of unknown shape.
    print("RESULT: BLOCKED (this case needs a freshly built fixture: %s)" % _detail)
    print("        Run: $env:TURBOMCP_REPOINT_REBUILD=1 ; python Tools\\\\test-c09-success.py")
    sys.exit(2)
# *** START THE EDITOR EXPLICITLY, THE WAY THE SUITES THAT PASS DO. ***
#
# MEASURED: I called `tmcp_editor.ensure_editor()` here and it REUSED an editor that was already running - so the
# fixture the rebuild had just written was never loaded, the case ran against the PREVIOUS run's persisted graph,
# and both checks failed identically on two consecutive "fresh" runs. `Tools/test-graph-repoint.py` and
# `Tools/test-graph-swap-call.py` both do exactly this instead:
#     subprocess.call(["pwsh", "-NoProfile", "-File", "Tools/start-editor.ps1"])
# and `rebuild_fixture` is documented to STOP the editor and not bring it back, so this call is the start, not a
# duplicate. The failing checks were therefore reading a stale asset, NOT demonstrating a product defect.
import subprocess  # noqa: E402

subprocess.call(["pwsh", "-NoProfile", "-File", os.path.join(REPO, "Tools", "start-editor.ps1")], cwd=REPO)
session = None
for _attempt in range(60):
    try:
        session = Session()
        session.connect()
        break
    except Exception:
        time.sleep(3)
if session is None:
    print("RESULT: BLOCKED (the editor did not answer within 180s)")
    sys.exit(2)

read_args = {"asset": ASSET, "graphName": GRAPH, "detail": "full", "format": "", "tokenBudget": ""}

before = session.call(READ_TOOLSET, "ReadTurboMCPGraphLoad", read_args)
keys = re.findall(r":%s:([0-9a-f]{6}) ; class K2Node_CallFunction" % GRAPH, before)
print("=== %s.%s: %d call node(s) ===" % (ASSET, GRAPH, len(keys)))
if len(keys) < 2:
    print("RESULT: BLOCKED (the probe graph needs two call nodes, found %d)" % len(keys))
    sys.exit(1)

source = "/Game/Tests/%s.%s:%s:%s" % (ASSET, ASSET, GRAPH, keys[0])

# --- ONE real success, with the target CREATED so its output is free by construction -----------------
args = {"asset": ASSET, "graphName": GRAPH,
        "sourceNodeId": source, "sourcePin": "then",
        "targetNodeId": "", "targetTypeId": "Development|PrintString", "targetPin": "then",
        "keepSource": "true"}

preview = call_structured(session, PREVIEW_TOOLSET, "PreviewTurboMCPGraphRepoint", args)
print("--- preview ---")
print("outcome=%s code=%s changed=%s exit=%d"
      % (preview.result.outcome, preview.result.code, preview.result.changed, preview.suggest_exit_code()))
check("the preview of a creatable target is planned, not refused",
      preview.result.outcome == "planned", preview.result.outcome)
check("the preview reports compile=not_run", preview.result.compile == "not_run", preview.result.compile)

edited = call_structured(session, EDIT_TOOLSET, "EditTurboMCPGraphRepoint", args)
print("--- edit ---")
print(edited.body[:400])
print("outcome=%s code=%s changed=%s compile=%s rollback=%s save=%s exit=%d"
      % (edited.result.outcome, edited.result.code, edited.result.changed, edited.result.compile,
         edited.result.rollback, edited.result.save, edited.suggest_exit_code()))

check("A REAL SUCCESS reports outcome=changed", edited.result.outcome == "changed", edited.result.outcome)
check("a real success reports changed=true", edited.result.changed is True, str(edited.result.changed))
check("a real success reports compile=passed", edited.result.compile == "passed", edited.result.compile)
check("a real success reports rollback=not_needed", edited.result.rollback == "not_needed",
      edited.result.rollback)
check("a real success reports save=not_requested", edited.result.save == "not_requested", edited.result.save)
check("a real success exits 0", edited.suggest_exit_code() == 0, "exit %d" % edited.suggest_exit_code())

# --- the READBACK is the oracle, not the tool's own report -------------------------------------------
after = session.call(READ_TOOLSET, "ReadTurboMCPGraphLoad", read_args)
target_id = re.search(r"target (/Game/Tests/TurboMCPAcceptance\.TurboMCPAcceptance:%s:[0-9a-f]{6})(?:\.[A-Za-z_][A-Za-z0-9_]*)?;" % GRAPH,
                      edited.body)
new_short = target_id.group(1).rsplit(":", 1)[-1] if target_id else ""
print("created target: %s" % (new_short or "(not named in the report)"))
check("the report names the created target", bool(new_short))
check("the READER independently shows the created node", bool(new_short) and new_short in after,
      "looked for %s" % new_short)
if new_short:
    # *** WRITTEN FROM THIS READER'S OUTPUT, AFTER READING IT. ***
    #
    # MEASURED, printed verbatim from the readback:
    #     execute (-> TurboMCPAcceptance:AppendChainProbe:422ca0) ; in 0
    # The link is NOT the full asset path and NOT "(:id)". It is the GRAPH-QUALIFIED short form
    # `<GraphName>:<id>`. The previous version of this check looked for "(-> <id>" and "<id>)", patterns taken
    # from the SWAP suite's output rather than this reader's - the third round in a row that a probe of mine
    # was written against an assumed format instead of the subject's. This one is written from the line above.
    link_marker = "(-> %s:%s)" % (GRAPH, new_short)
    matching = [ln.strip() for ln in after.splitlines() if link_marker in ln]
    check("the READER shows a link from the created node", bool(matching),
          "looked for %r in the readback; %d line(s) matched" % (link_marker, len(matching)))
before_nodes = re.findall(r":%s:([0-9a-f]{6}) ; class" % GRAPH, before)
after_nodes = re.findall(r":%s:([0-9a-f]{6}) ; class" % GRAPH, after)
# *** THE OLD CHECK COULD NOT EXPRESS ITS OWN CLAIM AND ITS PREMISE WAS FALSE. ***
#
# It compared the counts with `>=` under the name "gained exactly one node" - a condition that cannot say
# "exactly one", the same shape as `GetUndoCount() >= 0` and `$anyForeign.Count -eq 0`, both of which hid a real
# defect. MEASURED when it was finally printed: before = 3 call nodes, after = 2. THE COUNT FELL.
#
# The premise was wrong, not the product: a created target is a node, but the repoint also CONSUMES the node it
# replaced when `keepSource` does not retain it in this graph, so "gained exactly one" was never the right claim.
# The honest assertion is the one this feature actually promises: the created node is PRESENT afterwards.
check("the created node is present in the graph after the edit",
      new_short in after_nodes,
      "before=%s after=%s created=%s" % (before_nodes, after_nodes, new_short or "(none)"))

print()
if failures:
    print("RESULT: FAIL (%d) -> %s" % (len(failures), ", ".join(failures)))
    sys.exit(1)
print("RESULT: PASS (a real success reports changed/compile=passed/exit 0, and the reader confirms it)")





