"""Check node reader output on a disposable fixture; no mutation calls."""
import re
from tmcp import Session, READ_TOOLSET

session = Session()
session.connect()
asset = "/Game/Tests/BP_NodeTest"
def call(name, **args):
    result = session.call(READ_TOOLSET, name, args)
    print(name + ": " + result[:500], flush=True)
    return result

def check(ok, message):
    if not ok:
        raise AssertionError(message)

found = call("ReadTurboMCPNodeFind", asset=asset, words="K2Node", graphName="")
ids = re.findall(r"^(/[^\s]+:[^:\s]+:[0-9a-fA-F]+)$", found, re.MULTILINE)
check(bool(ids), "fixture must expose a node ID: " + found)
node = ids[0]
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
oversized = call("ReadTurboMCPNodeResolve", nodeIds="|".join([node] * 26))
check(oversized.startswith("error:") and "at most 25" in oversized, oversized)
help_text = call("ReadTurboMCPToolHelp", toolsetName=READ_TOOLSET, toolName="ReadTurboMCPNodeResolve")
check("nodeIds:" in help_text and "may load assets" in help_text, help_text)
context = call("ReadTurboMCPNodeContext", asset=asset, graphName=graph, nodeId=node, nextHops=0)
check("read: static execution context" in context and "not proof of runtime execution" in context, context)
check("No edit, compile or save was requested" in context, context)
missing = call("ReadTurboMCPNodeContext", asset=asset, graphName=graph, nodeId="invalid", nextHops=0)
check(missing.startswith("error:") and "ReadTurboMCPNodeFind" in missing, missing)
bad_graph = call("ReadTurboMCPNodeContext", asset=asset, graphName="NoSuchGraph", nodeId=node, nextHops=0)
check("Available graphs:" in bad_graph and "NoSuchGraph" in bad_graph, bad_graph)
for depth in (-1, 33):
    refusal = call("ReadTurboMCPNodeContext", asset=asset, graphName=graph, nodeId=node, nextHops=depth)
    check(refusal.startswith("error:") and "0 to 32" in refusal, refusal)
empty = call("ReadTurboMCPNodeResolve", nodeIds="")
check(empty.startswith("error:") and "no node ID" in empty, empty)
miss = call("ReadTurboMCPNodeFind", asset=asset, words="NoSuchUniqueNodeText92871", graphName="")
check("Nothing matched" in miss and "broader terms" in miss, miss)
print("RESULT: PASS", flush=True)
