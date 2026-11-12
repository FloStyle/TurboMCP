"""Check node reader output on a disposable fixture; no mutation calls."""
import re
from tmcp import Session, READ_TOOLSET

session = Session()
session.connect()
asset = "/Game/Tests/BP_NodeTest"
def call(name, **args):
    result = session.call(READ_TOOLSET, name, args)
    print(name + ": " + result, flush=True)
    return result

def check(ok, message):
    if not ok:
        raise AssertionError(message)

found = call("ReadTurboMCPNodeFind", asset=asset, words="K2Node", graphName="")
ids = re.findall(r"^(/[^\s]+:[^:\s]+:[0-9a-fA-F]+)$", found, re.MULTILINE)
check(bool(ids), "fixture must expose a node ID: " + found)
node = ids[0]

# A disjoint extra term must narrow ALL to zero but leave ANY unchanged.
any_found = call("ReadTurboMCPNodeFind", asset=asset, words="K2Node,NoSuchUniqueNodeText92871", graphName="", matchMode="any")
any_ids = re.findall(r"^(/[^\s]+:[^:\s]+:[0-9a-fA-F]+)$", any_found, re.MULTILINE)
check(any_ids == ids, "ANY must retain the original matching node identities")
all_found = call("ReadTurboMCPNodeFind", asset=asset, words="K2Node,NoSuchUniqueNodeText92871", graphName="", matchMode="all")
check("Nothing matched" in all_found, all_found)
bad_mode = call("ReadTurboMCPNodeFind", asset=asset, words="K2Node", graphName="", matchMode="approximate")
check(bad_mode.startswith("error:") and "all or any" in bad_mode, bad_mode)
class_miss = call("ReadTurboMCPNodeFind", asset=asset, words="K2Node", graphName="", nodeClass="NoSuchNodeClass92871")
check("Nothing matched" in class_miss, class_miss)
graph = node.rsplit(":", 2)[1]
resolved = call("ReadTurboMCPNodeResolve", nodeIds=node)
check("1 resolved, 0 refused" in resolved and "reference:" in resolved, resolved)
check("container-name=" in resolved and "direction-index=" in resolved and "array-index=" in resolved, resolved)
check("No edit, compile or save was requested" in resolved and "may load assets" in resolved, resolved)
check("writer support varies" in resolved, resolved)
check(resolved == call("ReadTurboMCPNodeResolve", nodeIds=node), "Resolution must be deterministic")
mixed = call("ReadTurboMCPNodeResolve", nodeIds=node + "|invalid")
check("1 resolved, 1 refused" in mixed and "All three parts" in mixed, mixed)
for key in ("xyzxyz", "12345"):
    bad = call("ReadTurboMCPNodeResolve", nodeIds=node.rsplit(":", 1)[0] + ":" + key)
    check("0 resolved, 1 refused" in bad and "invalid key" in bad, bad)
missing_key = call("ReadTurboMCPNodeResolve", nodeIds=node.rsplit(":", 1)[0] + ":" + "f" * 32)
check("0 resolved, 1 refused" in missing_key and "fresh ID" in missing_key, missing_key)
oversized = call("ReadTurboMCPNodeResolve", nodeIds="|".join([node] * 26))
check(oversized.startswith("error:") and "at most 25" in oversized, oversized)
help_text = call("ReadTurboMCPToolHelp", toolsetName=READ_TOOLSET, toolName="ReadTurboMCPNodeResolve")
check("nodeIds:" in help_text and "may load assets" in help_text, help_text)
context = call("ReadTurboMCPNodeContext", asset=asset, graphName=graph, nodeId=node, nextHops=0)
check("read: static execution context" in context and "not proof of runtime execution" in context, context)
check("No edit, compile or save was requested" in context, context)
inferred_context = call("ReadTurboMCPNodeContext", asset="", graphName="", nodeId=node, nextHops=0)
check(inferred_context == context, "A full reader ID must select the same context without redundant selectors")
conflict = call("ReadTurboMCPNodeContext", asset=asset, graphName="WrongGraph", nodeId=node, nextHops=0)
check(conflict.startswith("error:") and "conflicts" in conflict, conflict)

missing = call("ReadTurboMCPNodeContext", asset=asset, graphName=graph, nodeId="invalid", nextHops=0)
check(missing.startswith("error:") and "ReadTurboMCPNodeFind" in missing, missing)
bad_graph = call("ReadTurboMCPNodeContext", asset=asset, graphName="NoSuchGraph", nodeId=node, nextHops=0)
check("conflicts" in bad_graph, bad_graph)
for depth in (-1, 33):
    refusal = call("ReadTurboMCPNodeContext", asset=asset, graphName=graph, nodeId=node, nextHops=depth)
    check(refusal.startswith("error:") and "0 to 32" in refusal, refusal)
empty = call("ReadTurboMCPNodeResolve", nodeIds="")
check(empty.startswith("error:") and "no node ID" in empty, empty)
miss = call("ReadTurboMCPNodeFind", asset=asset, words="NoSuchUniqueNodeText92871", graphName="")
check("Nothing matched" in miss and "broader terms" in miss, miss)
# Small pages must preserve identities without repeats or dropped matches.
cursor = ""
paged_ids = []
seen_cursors = set()
for _ in range(100):
    page = call("ReadTurboMCPNodeFind", asset=asset, words="K2Node", graphName="", pageSize=1, cursor=cursor)
    paged_ids.extend(re.findall(r"^(/[^\s]+:[^:\s]+:[0-9a-fA-F]+)$", page, re.MULTILINE))
    next_match = re.search(r"^next_cursor: (.+)$", page, re.MULTILINE)
    check(next_match is not None, page)
    cursor = next_match.group(1).strip()
    if cursor == "none":
        check("complete=true" in page, page)
        break
    check(cursor not in seen_cursors, "Pagination repeated a cursor")
    seen_cursors.add(cursor)
else:
    raise AssertionError("Fixture pagination did not terminate")
check(paged_ids[:len(ids)] == ids and len(set(paged_ids)) == len(paged_ids), "Paged identities differ or repeat")
counted = call("ReadTurboMCPNodeFind", asset=asset, words="K2Node", graphName="", countOnly=True)
check("complete=true" in counted and f"matches_so_far={len(paged_ids)};" in counted, counted)
folder = call("ReadTurboMCPNodeFind", asset="/Game/Tests", scope="folder", words="NoSuchUniqueNodeText92871", graphName="")
check("eligible_assets=" in folder and "next_cursor:" in folder, folder)
if seen_cursors:
    stale = call("ReadTurboMCPNodeFind", asset=asset, words="DifferentQuery", graphName="", cursor=next(iter(seen_cursors)))
    check(stale.startswith("error:") and "Cursor" in stale, stale)
print("RESULT: PASS", flush=True)

# Focused relation check: unsaved native fixture, no persistent asset writes.
import runpy
from pathlib import Path
support = runpy.run_path(str(Path(__file__).with_name("test-widget-roundtrip.py")))
name = "TurboMCP.Read.ReferenceWrites"
session.call(support["AUTO_TOOLSET"], "DiscoverTests", {})
raw = session.call(support["AUTO_TOOLSET"], "RunTests", {"testNames": [name]})
print(raw)
entry, report = support["parse_run_tests_report"](raw, name)
if entry["warnings"]:
    raise RuntimeError("Unexpected reference-write warnings: " + repr(entry["warnings"]))
print("REFERENCE WRITES: PASS")
