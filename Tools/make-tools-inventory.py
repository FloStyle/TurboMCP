"""Generate the Tools/ inventory from the files, so no tool is undocumented.

WHY: `Tools/README.md` carries a CURATED table - what each tool is for, in the order they are used - and it
described about thirty-five tools. The directory has since grown past seventy, *** SO THIRTY-NINE OF THEM
WERE NAMED NOWHERE IN THE DOCUMENTATION. *** A curator's table is the wrong place to fix that: it is prose,
it is ordered by workflow, and it cannot be kept complete by hand without becoming a list.

*** SO THIS IS THE LIST, AND THE README KEEPS THE PROSE. *** It reads the directory, takes each script's own
first docstring line or leading comment as its description - **the description its author wrote, not one this
generator invents** - and groups by what the tool needs to run.

    nothing, static      reads files; safe anywhere
    the source tree      needs the plugin sources, not a running editor
    a running editor     drives the MCP server
    nothing else running owns the editor's life cycle, so it must be alone
    part of the plugin   consumed by the plugin at runtime rather than run by a person
"""

import pathlib
import re
import sys

REPO = pathlib.Path(r"C:\Users\Example\Desktop\TurboMCP")
TOOLS = REPO / "Tools"
OUT = TOOLS / "INVENTORY.md"

# What each script needs before it can run. Anything not named here is a suite or a maker, decided by name.
NEEDS = {
    "nothing, static": [
        "audit-tool-coverage.py", "audit-suite-quality.py", "check-includes.py", "check-plugin.py",
        "check-refs.py", "count-plan.py", "check-suite-shapes.py", "make-questions-index.py",
        "make-tools-doc.py", "apply-license-header.ps1", "common.ps1", "tmcp_paths.py",
        "license-header.txt",
    ],
    "a running editor": [
        "check-mcp-server.py", "tmcp.py", "tmcp_editor.py", "test-agent-help.py", "test-asset-tools.py",
    ],
    "nothing else running": [
        "run-suites.ps1", "start-editor.ps1", "build.ps1", "setup-engine.ps1", "test-suite-timeout.ps1",
    ],
    "part of the plugin": ["license-header.txt"],
}


def describe(path: pathlib.Path) -> str:
    """The first meaningful line of the file's own header: docstring, comment block, or `#` comment.

    *** IT SKIPS THE LICENCE NOTICE RATHER THAN TAKING THE FIRST LINE IT FINDS. *** The notice sits at the top
    of every file in this repository and its fourth line reads "License, v. 2.0. If a copy of the MPL was not
    distributed with this" - so a generator that takes the first line of the comment block describes every
    script as **its own licence**. The first draft did exactly that for `tmcp_paths.py`.
    """
    SKIP = ("Copyright", "SPDX", "This Source Code Form", "License, v.", "file, You can obtain",
            "Mozilla Public License", "file, You can", "#!")

    # A file that is not a script but IS a tool's input. It has no header to read, and saying "its header says
    # nothing" about a template is true and useless.
    NOT_A_SCRIPT = {
        "license-header.txt": "The MPL-2.0 notice itself, as text. `apply-license-header.ps1` reads it, so "
                              "the notice has one spelling rather than one per script.",
    }
    if path.name in NOT_A_SCRIPT:
        return NOT_A_SCRIPT[path.name]

    text = path.read_text(encoding="utf-8", errors="replace")
    if path.suffix == ".py":
        # *** THE DOCSTRING DOES NOT ALWAYS COME FIRST: THE LICENCE NOTICE DOES. *** Five files put the
        # notice above the module docstring, so a pattern anchored at the top of the file found the notice or
        # nothing. The search is therefore unanchored, and the notice lines are skipped below.
        m = re.search(r'"""(.*?)"""', text, re.DOTALL)
        if m:
            body = m.group(1)
        else:
            m = re.match(r"((?:\s*#[^\n]*\n)+)", text)
            body = m.group(1) if m else ""
    elif path.suffix == ".ps1":
        m = re.search(r"<#(.*?)#>", text, re.DOTALL)
        if m:
            body = m.group(1)
            syn = re.search(r"\.SYNOPSIS\s*\n(.+?)(?:\n\s*\n|\.DESCRIPTION)", body, re.DOTALL)
            body = syn.group(1) if syn else body
        else:
            body = "\n".join(l for l in text.split("\n")[:40] if l.strip().startswith("#"))
    else:
        return ""

    for line in body.split("\n"):
        line = line.strip().lstrip("#").strip().strip('"').strip()
        if not line or any(line.startswith(s) for s in SKIP):
            continue
        line = re.sub(r"\s+", " ", line)
        return line[:118]
    return ""


def needs_of(name: str) -> str:
    for group, members in NEEDS.items():
        if name in members:
            return group
    if name.startswith("test-"):
        return "a running editor"
    if name.startswith("make-"):
        return "nothing else running"
    return "nothing, static"


def main() -> int:
    # *** RECURSIVE, BECAUSE THE TOOLS MOVED. *** `Tools/checks/` holds the documentation checks, and they were
    # moved there on purpose - the suite runner discovers suites by NAME (`^(test|check|audit)-`) and absorbed
    # three of them as suites, which silently changed the documented suite count. **A non-recursive listing
    # then dropped them from the inventory too, so the tools that fix the documentation became the tools the
    # documentation did not mention.** The listing is now the whole directory.
    scripts = sorted(
        p for p in TOOLS.rglob("*")
        if p.is_file() and (p.suffix in (".py", ".ps1") or p.name.endswith(".txt"))
        and p.name not in ("README.md", "INVENTORY.md")
    )

    groups = {}
    for p in scripts:
        groups.setdefault(needs_of(p.name), []).append(p)

    order = ["nothing, static", "the source tree", "a running editor", "nothing else running", "part of the plugin"]
    lines = [
        "# Tools inventory",
        "",
        "*** GENERATED FROM THE DIRECTORY BY `Tools/make-tools-inventory.py`, SO NO TOOL CAN BE UNDOCUMENTED. *** "
        "The description on each line is the **first line of the script's own header** - what its author wrote, "
        "not a summary invented here. [`README.md`](README.md) keeps the prose: why each one exists and the "
        "order they are used in.",
        "",
    ]
    total = 0
    for group in order:
        members = groups.get(group)
        if not members:
            continue
        total += len(members)
        lines.append("## %s (%d)" % (group.capitalize(), len(members)))
        lines.append("")
        lines.append("| Tool | What its own header says |")
        lines.append("|---|---|")
        for p in members:
            d = describe(p) or "*** its header says nothing - see the file ***"
            lines.append("| `%s` | %s |" % (p.name, d.replace("|", "\\|")))
        lines.append("")

    lines.append("*** %d files, all listed. ***" % total)
    lines.append("")
    OUT.write_text("\n".join(lines), encoding="utf-8")

    print("  files: %d" % total)
    for group in order:
        if groups.get(group):
            print("  %-24s %d" % (group, len(groups[group])))
    undocumented = [p.name for p in scripts if not describe(p)]
    print("  headers that say nothing: %d" % len(undocumented))
    for n in undocumented:
        print("      " + n)
    return 0


if __name__ == "__main__":
    sys.exit(main())
