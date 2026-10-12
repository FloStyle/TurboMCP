"""Generate TOOLS.md from the tool declarations in the plugin header.

WHY THIS EXISTS: THE TOOLS ARE THE PRODUCT, AND FORTY-SEVEN OF THE NINETY-THREE WERE NAMED NOWHERE IN THE
DOCUMENTATION. Each one is a `static FString` carrying `meta = (AICallable)` in `TurboMCPToolsets.h`, the
engine turns each into an MCP tool at load time, and a reader of this repository had no way to find out what
they were or what to call them.

*** IT READS THE HEADER RATHER THAN A HAND-KEPT LIST, AND THAT IS THE WHOLE POINT. *** A hand-kept list of 93
names is wrong the moment somebody adds the ninety-fourth, and this project has already spent four rounds
correcting a plan whose status column had drifted from the tree. The declaration is the source of truth, so
the generator reads the declaration.

WHAT IT EXTRACTS, and where each piece comes from:

    toolset     the `class UTurboMCP*Toolset` the function sits under, by line range
    name        the function name, from `static FString <Name>(`
    summary     THE FIRST SENTENCE OF THE DOC COMMENT ABOVE IT, which is exactly what an agent sees as the
                tool's description - the engine reads that comment through reflection, so printing anything
                else here would document something the caller never receives
    line        the declaration's line, so a reader can check the generator rather than trust it

AND IT REFUSES RATHER THAN GUESSING. A `static FString` with no doc comment, or one whose comment is empty,
is reported at the end as a finding instead of being emitted with a blank description - because a tool an
agent cannot understand is a tool that will be called wrongly.
"""

import re
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
HEADER = REPO / "Source" / "TurboMCP" / "Public" / "Tools" / "TurboMCPToolsets.h"

# One entry per toolset class, in file order, with the kind a reader needs to know.
TOOLSET_KIND = {
    "UTurboMCPReadToolset": ("Read", "No"),
    "UTurboMCPLookToolset": ("Look", "No"),
    "UTurboMCPPreviewToolset": ("Preview", "No"),
    "UTurboMCPEditToolset": ("Edit", "**Yes**"),
}


def first_sentence(comment_lines):
    """The first sentence of a doc comment: what the engine hands an agent as the description.

    *** THE COMMENT MARKERS ARE STRIPPED PER LINE, BEFORE THE LINES ARE JOINED. *** Joining first and then
    stripping leaves the interior lines' own `*` behind, which is how the first generated draft described a
    tool as "* * Read the MCP server state" - the marker of the second line, still in the middle of the
    sentence. A reader would have seen the asterisk in the description an agent receives.
    """
    cleaned = []
    for line in comment_lines:
        text = line.strip()
        text = re.sub(r"^/\*\*+", "", text)   # the opening /**
        text = re.sub(r"^\*+/", "", text)     # a closing */
        text = re.sub(r"^\*+", "", text)      # a leading *
        text = re.sub(r"\*+/$", "", text)     # a trailing */
        cleaned.append(text.strip())

    text = re.sub(r"\s+", " ", " ".join(cleaned)).strip()
    if not text:
        return ""
    stop = re.search(r"(?<=[a-z0-9\)\]\"'`])\.\s", text + " ")
    sentence = text[: stop.end()].strip() if stop else text
    return sentence.rstrip(".")


def main() -> int:
    lines = HEADER.read_text(encoding="utf-8").split("\n")

    # The line each toolset class starts on, so a function can be attributed by range.
    # *** `TURBOMCP_API` IS DELIBERATELY NOT REQUIRED HERE. *** The first version of this generator asked for
    # it, the classes do not carry it, and the result was ZERO toolsets found while 93 tools were found - the
    # summary table was empty and every tool was attributed to "?". A pattern that requires a token the file
    # does not contain matches nothing and says nothing about it.
    classes = []
    for index, line in enumerate(lines, start=1):
        match = re.match(r"\s*class\s+(\w+Toolset)\s*:", line)
        if match:
            classes.append((index, match.group(1)))

    tools = []
    for index, line in enumerate(lines, start=1):
        if not re.search(r"static\s+FString\s+(\w+)\s*\(", line):
            continue
        name = re.search(r"static\s+FString\s+(\w+)\s*\(", line).group(1)

        # Walk back to the doc comment. *** `UFUNCTION(...)` SITS BETWEEN THE COMMENT AND THE DECLARATION, *** so
        # the first version of this generator stopped on that line, found no comment, and reported all 93 tools
        # as undocumented. The macro line is skipped, and only then is the comment collected.
        comment = []
        cursor = index - 2
        while cursor >= 0:
            stripped = lines[cursor].strip()
            if not stripped:
                cursor -= 1
                continue
            if stripped.startswith("UFUNCTION") or stripped.startswith("UPROPERTY"):
                cursor -= 1
                continue
            if stripped == "*/" or stripped.endswith("*/"):
                cursor -= 1
                continue
            if stripped.startswith("/**") or stripped.startswith("*") or stripped.startswith("//"):
                comment.insert(0, stripped)
                cursor -= 1
                continue
            break

        owner = classes[0][1] if classes else "?"
        for start, cls in classes:
            if start <= index:
                owner = cls

        tools.append(
            {
                "name": name,
                "owner": owner,
                "line": index,
                "summary": first_sentence(comment),
            }
        )

    without = [t for t in tools if not t["summary"]]

    out = []
    out.append("# TurboMCP tool catalogue")
    out.append("")
    out.append(
        "**This file is generated from the declarations in\n"
        "[`Source/TurboMCP/Public/Tools/TurboMCPToolsets.h`](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h)**\n"
        "by `Tools/make-tools-doc.py`, so it cannot drift from the code. The engine turns each of these into an\n"
        "MCP tool at load time, and **the first sentence of the doc comment above each function is the\n"
        "description an agent receives** - that is why the summaries below are extracted rather than written."
    )
    out.append("")
    out.append("| Toolset | Kind | Writes? | Tools |")
    out.append("|---|---|---|---|")
    for _, cls in classes:
        kind, writes = TOOLSET_KIND.get(cls, ("?", "?"))
        count = len([t for t in tools if t["owner"] == cls])
        out.append("| `%s` | %s | %s | **%d** |" % (cls, kind, writes, count))
    out.append("| **Total** | | | **%d** |" % len(tools))
    out.append("")
    out.append(
        "**A reader in `Read` or `Look` cannot write**, because those classes hold no editing code - it is a\n"
        "property of the type, not a convention."
    )
    out.append("")

    for _, cls in classes:
        kind, writes = TOOLSET_KIND.get(cls, ("?", "?"))
        out.append("## `%s` — %s, writes: %s" % (cls, kind, writes))
        out.append("")
        out.append("| Tool | Description | Line |")
        out.append("|---|---|---|")
        for tool in [t for t in tools if t["owner"] == cls]:
            summary = tool["summary"] or "*** NO DOC COMMENT - see the note at the end ***"
            out.append(
                "| `%s` | %s | [%d](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L%d) |"
                % (tool["name"], summary, tool["line"], tool["line"])
            )
        out.append("")

    out.append("## Calling one")
    out.append("")
    out.append(
        "The MCP server runs in the editor on **`http://localhost:8000/mcp`**. With tool search enabled - the\n"
        "engine default, kept here - a client sees three meta-tools and reaches everything else through the third:"
    )
    out.append("")
    out.append("- `list_toolsets` — what is registered, including these four;")
    out.append("- `describe_toolset` — one toolset's tools and their schemas;")
    out.append("- `call_tool` — takes `toolset_name` and the **bare** function name, with `arguments` as a **JSON object, not a string**.")
    out.append("")
    out.append(
        "So the tool above named `ReadTurboMCPStatus` is called with `toolset_name` = `TurboMCP.Read` and\n"
        "`tool_name` = `ReadTurboMCPStatus`. The full dotted name is what a listing prints; `call_tool` wants the\n"
        "bare one. Both facts are in [`Spec/07_ENGINE_NOTES.md`](Spec/07_ENGINE_NOTES.md), and they are repeated\n"
        "here because they are the two things a caller gets wrong first."
    )
    out.append("")
    out.append("## Adding a tool")
    out.append("")
    out.append(
        "**Three steps, and the third is the one that is always forgotten.** A tool that exists but that no\n"
        "suite names is a tool the coverage audit reports as uncalled, and the audit passes only at\n"
        "`uncalled: (none)`."
    )
    out.append("")
    out.append("### 1. Declare it, in the right toolset class")
    out.append("")
    out.append(
        "Put it in the class that matches what it does — `UTurboMCPReadToolset` for a reader,\n"
        "`UTurboMCPEditToolset` for a writer — in\n"
        "[`TurboMCPToolsets.h`](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h). **The class is the safety\n"
        "property**: a reader declared in Read holds no editing code, so \"this cannot modify your project\" is a\n"
        "fact about the type rather than a promise. A writer needs a `Preview` twin in\n"
        "`UTurboMCPPreviewToolset` that shares the same `Plan`, so what the plan reports and what the edit does\n"
        "cannot drift apart."
    )
    out.append("")
    out.append("```cpp")
    out.append("/**")
    out.append(" * One line saying what the tool does. THIS SENTENCE IS WHAT AN AGENT READS, because the")
    out.append(" * engine builds the tool description from this comment through reflection.")
    out.append(" *")
    out.append(" * Then the parameters, and every refusal by name with the engine line that decides it.")
    out.append(" */")
    out.append("UFUNCTION(meta = (AICallable))")
    out.append("static FString ReadTurboMCPYourThing(const FString& Asset);")
    out.append("```")
    out.append("")
    out.append("### 2. Write a suite that calls it **by its literal name**")
    out.append("")
    out.append(
        "Add `Tools/test-<capability>.py`. *** THE CALL SITE MUST SPELL THE TOOL NAME AS A LITERAL, EXACTLY AS\n"
        "THE DECLARATION DOES, BECAUSE `Tools/audit-tool-coverage.py` FINDS IT BY REGEX OVER THE SUITE FILES.\n"
        "*** A name built at run time, or interpolated, is a name the audit cannot see, and the audit will\n"
        "report the tool as uncalled even though the suite exercises it."
    )
    out.append("")
    out.append("```python")
    out.append("session.call(READ_TOOLSET, \"ReadTurboMCPYourThing\", {\"asset\": \"/Game/Tests/Thing\"})")
    out.append("```")
    out.append("")
    out.append(
        "**Every check you write must name an input that makes it fail.** `Tools/audit-suite-quality.py`\n"
        "**blocks the whole run** when a check cannot fail or cannot pass, so a check that only ever passes is\n"
        "a build failure here, not a stylistic complaint. And run the break test: change the code so the check\n"
        "should go red, and confirm it does **with the wrong value visible** — a check that matches a prefix\n"
        "cannot see a change after the prefix."
    )
    out.append("")
    out.append("### 3. Verify, and run the whole thing")
    out.append("")
    out.append("```powershell")
    out.append("python Tools\\audit-tool-coverage.py    # must print: uncalled: (none)")
    out.append("python Tools\\audit-suite-quality.py    # must print 0 candidates, and it BLOCKS")
    out.append("python Tools\\make-tools-doc.py         # regenerate this file, so it lists the new tool")
    out.append("Tools\\build.ps1")
    out.append("Tools\\run-suites.ps1                   # the signal that matters: passed N, failed 0")
    out.append("```")
    out.append("")
    out.append(
        "**AND `Tools/make-tools-doc.py` IS STEP 3 FOR A REASON: THIS FILE IS GENERATED FROM THE DECLARATIONS, SO\n"
        "THE ONLY WAY TO KEEP IT TRUE IS TO RE-RUN THE GENERATOR.** A hand edit here is lost the next time\n"
        "anybody regenerates, which is the same defect as a hand-kept list of 93 names."
    )
    out.append("")

    if without:
        out.append("## Tools with no doc comment")
        out.append("")
        out.append(
            "*** THESE DECLARATIONS CARRY NO USABLE DOC COMMENT, SO THE ENGINE HANDS AN AGENT NO DESCRIPTION\n"
            "FOR THEM. *** They are listed rather than hidden because a tool nobody can understand is a tool\n"
            "that will be called wrongly:"
        )
        out.append("")
        for tool in without:
            out.append("- `%s` (line %d, `%s`)" % (tool["name"], tool["line"], tool["owner"]))
        out.append("")

    (REPO / "TOOLS.md").write_text("\n".join(out) + "\n", encoding="utf-8")

    print("  tools found:            %d" % len(tools))
    print("  without a doc comment:  %d" % len(without))
    print("  wrote TOOLS.md")
    return 0


if __name__ == "__main__":
    sys.exit(main())
