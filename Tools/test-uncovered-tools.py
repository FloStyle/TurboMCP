# ACCEPTANCE ROW THIS SUITE CLAIMS TO PROVE: NONE - it exercises the tools that NOTHING has ever called, which is COVERAGE, and coverage is Tools/audit-tool-coverage.py's question rather than any acceptance row's.
"""Exercise the tools that NOTHING has ever called.

WHY THIS SUITE EXISTS
---------------------
`Tools/audit-tool-coverage.py` reads the tool declarations out of the header and the calls out of every
suite, and reports the tools no suite calls. Its first honest run found FIVE:

    EditTurboMCPCompile        static FString EditTurboMCPCompile(const FString& Assets);
    LookTurboMCPToolset        static FString LookTurboMCPToolset();
    PreviewTurboMCPToolset     static FString PreviewTurboMCPToolset();
    ReadTurboMCPEditQueue      static FString ReadTurboMCPEditQueue();
    ReadTurboMCPNodeResolve    static FString ReadTurboMCPNodeResolve(const FString& NodeIds);

A tool that REGISTERS and answers nothing when called looks finished in `R-TOOL-INDEX` and in the
panel's tool list, and nothing contradicts it. That is the failure this suite closes, and it is the
half of "every MCP feature is testable" that a panel does not cover: a panel lets a human TRY a tool,
notices nothing about a tool nobody tried.

WHAT EACH CHECK PROVES
----------------------
- every tool ANSWERS rather than erroring on a valid call;
- every tool ANSWERS rather than returning an empty string, which is `P1`;
- the tools that take no argument work with none, because that is how a caller will use them;
- the toolset descriptions name their own toolset and list its tools, because a description that
  describes the wrong toolset is worse than none;
- the edit queue and the compile report their state rather than assuming one;
- `R-NODE-RESOLVE` refuses an id that names nothing and gives a real node back for one that does.
"""

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from tmcp import EDIT_TOOLSET, LOOK_TOOLSET, PREVIEW_TOOLSET, READ_TOOLSET, Session  # noqa: E402

ASSET = "BP_NodeTest"
GRAPH = "EventGraph"

failures = []


def check(name, ok, detail=""):
    print("  %-4s %s%s" % ("ok" if ok else "FAIL", name, ("  " + detail) if detail else ""))
    if not ok:
        failures.append(name)


def main():
    session = Session()
    session.connect()

    print("=== the two tools whose NAMES look like toolset descriptions but are not ===")
    # THE FIRST VERSION OF THIS SUITE ASSERTED THE WRONG THING and went red on two tools that were
    # right. It assumed a tool named `<Toolset>Toolset` describes its toolset. Measured, neither does:
    #
    #   LookTurboMCPToolset     lists the live Slate windows a capture could frame
    #   PreviewTurboMCPToolset  measures this plugin's help output against the 2000 character budget
    #
    # So the checks assert what each tool SAYS it does in its own doc comment, which is the only
    # contract it has. A check must assert what the CALL promises, not what its name suggests.
    look = session.call(LOOK_TOOLSET, "LookTurboMCPToolset", {})
    print(look[:500])
    print()
    check("the window lister answers rather than erroring",
          not look.startswith("error:"), look[:120])
    check("it names the window count or says the list is empty",
          "window" in look.lower() and "visible" in look.lower(), look[:200])
    # THE OUTPUT IS ASSERTED, NOT THE DOC COMMENT, and the first version of this check got that
    # wrong. It demanded the sentence "it does not capture an image yet" because that is what the
    # header says - but `L-CAPTURE` arrived in phase 3 and the caption now reads
    # `look: editing windows, N visible`. The HEADER comment is stale; the CODE is right. A check
    # written from a doc comment tests the comment.
    check("it shows the windows one per line or says there are none",
          look.count("\n") >= 1, look[:200])

    preview = session.call(PREVIEW_TOOLSET, "PreviewTurboMCPToolset", {})
    print(preview[:600])
    print()
    check("the help-size tool answers rather than erroring",
          not preview.startswith("error:"), preview[:120])
    check("it reports characters and the budget",
          "char" in preview.lower() and "2000" in preview, preview[:300])
    check("it names at least one TurboMCP toolset",
          "TurboMCP" in preview, preview[:300])

    print()
    print("=== the edit queue, which nothing had ever called ===")
    queue = session.call(READ_TOOLSET, "ReadTurboMCPEditQueue", {})
    print(queue[:600])
    print()
    check("the edit queue answers rather than erroring",
          not queue.startswith("error:"), queue[:120])
    check("it says something even when there is nothing queued",
          len(queue.strip()) > 20, repr(queue[:80]))

    print()
    print("=== R-NODE-RESOLVE, which nothing had ever called ===")
    # A REAL node key, taken from the graph rather than pinned, because a pinned key would break when
    # the graph is rebuilt.
    import re as _re  # noqa: PLC0415
    text = session.call(READ_TOOLSET, "ReadTurboMCPGraphLoad",
                        {"asset": ASSET, "graphName": GRAPH, "detail": "full",
                         "format": "", "tokenBudget": ""})
    keys = []
    for match in _re.finditer(r"%s:(\w{6})\s*;" % _re.escape(GRAPH), text):
        if match.group(1) not in keys:
            keys.append(match.group(1))
    print("    node keys in the graph: %s" % ", ".join(keys[:4]))
    check("the graph has a node to resolve", len(keys) > 0)

    if keys:
        resolved = session.call(READ_TOOLSET, "ReadTurboMCPNodeResolve",
                                {"nodeIds": keys[0]})
        print(resolved[:600])
        print()
        check("a real node key resolves rather than erroring",
              not resolved.startswith("error:"), resolved[:120])
        check("the answer names the key it resolved", keys[0] in resolved, resolved[:200])

    # A key that names nothing is REFUSED, with the real ones listed (`P7`). This is the check that
    # distinguishes a resolver from a lookup that silently answers nothing.
    missing = session.call(READ_TOOLSET, "ReadTurboMCPNodeResolve", {"nodeIds": "ffffff"})
    print(missing[:400])
    print()
    check("a key that names nothing is refused or reported",
          missing.startswith("error:") or "not" in missing.lower() or "no" in missing.lower(),
          missing[:160])
    check("an unknown key does not silently answer nothing",
          len(missing.strip()) > 10, repr(missing[:60]))

    print()
    print("=== E-COMPILE, which nothing had ever called ===")
    # COMPILING IS A WRITE, so it runs against a REAL asset the suites already share, and its own
    # result is checked for the two shapes the spec names: clean, or the warnings and errors. An
    # asset that is already compiled must report clean rather than reporting a change.
    compiled = session.call(EDIT_TOOLSET, "EditTurboMCPCompile", {"assets": ASSET})
    print(compiled[:800])
    print()
    check("compile answers rather than erroring", not compiled.startswith("error:"),
          compiled[:120])
    check("it reports a per-asset line", ASSET in compiled, compiled[:200])
    check("it says whether the asset still has unsaved changes",
          "unsaved" in compiled.lower() or "saved" in compiled.lower(), compiled[:300])

    # A name that resolves to nothing is refused by NAME rather than silently compiling nothing.
    unknown = session.call(EDIT_TOOLSET, "EditTurboMCPCompile", {"assets": "NoSuchAssetAtAll"})
    check("compiling something that does not exist is refused",
          unknown.startswith("error:") or "not" in unknown.lower(), unknown[:160])

    print()
    print("=== the audit that found these agrees they are covered now ===")
    # The audit is re-run rather than trusted, because the point of this suite is to make the audit
    # pass and the audit is what proves it.
    import subprocess  # noqa: PLC0415
    repo = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    audit = subprocess.run([sys.executable, os.path.join(repo, "Tools", "audit-tool-coverage.py")],
                           capture_output=True, text=True, cwd=repo)
    uncalled = ""
    for line in (audit.stdout or "").splitlines():
        if "uncalled:" in line:
            uncalled = line.split("uncalled:")[1].strip()
    print("    the audit now reports uncalled: %s" % (uncalled or "(none)"))
    check("the audit no longer lists any tool as uncalled",
          uncalled in ("", "(none)"), uncalled)

    print()
    if failures:
        print("RESULT: FAIL (%d) -> %s" % (len(failures), ", ".join(failures)))
        return 1
    print("RESULT: PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
