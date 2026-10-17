# Copyright (c) 2026 FloStyle
# SPDX-License-Identifier: MPL-2.0
"""Builds Release/CAPABILITIES.json from the reflected declarations in TurboMCPToolsets.h.

This is release bookkeeping, not a test. It reads the header as text, associates each AICallable
declaration with the class that owns it and the doc comment above it, and writes one row per
exposed operation.

`effects` and `evidence` are seeded honestly: an effect is recorded as `unknown` until the
implementation has been read or exercised, because a doc comment is a statement of intent and this
project has already found doc comments that describe behaviour the code does not have.
"""

from __future__ import annotations

import json
import os
import re
import sys

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
HEADER = os.path.join(REPO, "Source", "TurboMCP", "Public", "Tools", "TurboMCPToolsets.h")
OUT = os.path.join(REPO, "Release", "CAPABILITIES.json")

KINDS = {
    "UTurboMCPReadToolset": ("Read", False),
    "UTurboMCPLookToolset": ("Look", False),
    "UTurboMCPPreviewToolset": ("Preview", False),
    "UTurboMCPEditToolset": ("Edit", True),
}

# Tools whose effect class has been established by reading the implementation in this run or by the
# supplied audit. Anything absent stays `unknown`, which is the honest default.
EFFECTS = {
    "PreviewTurboMCPBatchEdit": ["none_planned_only", "reads_assets"],
    "EditTurboMCPBatchEdit": ["asset_memory", "graph_edit"],
}

DISPOSITION = {
    "PreviewTurboMCPBatchEdit": (
        "in_scope_repair_required",
        "ARC09 subset card. Current implementation parses and plans only; it does not mutate.",
    ),
    "EditTurboMCPBatchEdit": (
        "in_scope_repair_required",
        "ARC09 restricts the release batch to graphRepoint in one Blueprint graph. The current "
        "implementation does not yet refuse cross-asset or cross-graph batches, does not compile "
        "once at the end, and does not bound each operation against the state produced by its "
        "predecessors. Repairing it is step S16.",
    ),
}


def first_sentence(text: str) -> str:
    """The tool description the engine hands an agent: the doc comment's first sentence."""
    flat = re.sub(r"\s+", " ", text).strip()
    match = re.search(r"^(.*?\.)(\s|$)", flat)
    return (match.group(1) if match else flat).strip()


def parse(path: str):
    with open(path, "r", encoding="utf-8") as handle:
        lines = handle.read().splitlines()

    tools = []
    current_class = None
    doc: list[str] = []
    pending = False

    for index, line in enumerate(lines):
        class_match = re.match(r"^class\s+(U\w+)", line)
        if class_match:
            current_class = class_match.group(1)
            doc = []
            continue

        stripped = line.strip()

        if stripped.startswith("/**"):
            doc = []
            if stripped.endswith("*/"):
                doc.append(stripped[3:-2])
            else:
                doc.append(stripped[3:])
            continue
        if stripped.startswith("*") and not stripped.startswith("*/"):
            doc.append(stripped.lstrip("*").strip())
            continue
        if stripped.startswith("*/"):
            continue

        if "AICallable" in stripped:
            pending = True
            continue

        if pending:
            decl = re.match(r"^\s*static\s+FString\s+(\w+)\s*\(", line)
            if decl:
                name = decl.group(1)
                # A declaration may wrap over several lines: read forward to the terminating `);`.
                chunk = line[decl.end() - 1:]
                cursor = index
                while ");" not in chunk:
                    cursor += 1
                    if cursor >= len(lines):
                        raise RuntimeError("unterminated declaration for %s at line %d" % (name, index + 1))
                    chunk += " " + lines[cursor].strip()
                params = chunk[1:chunk.rindex(")")].strip()
                params = re.sub(r"\s+", " ", params)
                kind, writes = KINDS.get(current_class, ("Unknown", None))
                disposition, reason = DISPOSITION.get(
                    name,
                    ("included", "Existing declared capability. No repair identified for it yet."),
                )
                tools.append({
                    "toolset": "TurboMCP.%s" % current_class,
                    "kind": kind,
                    "classWrites": writes,
                    "tool": name,
                    "parameters": params,
                    "description": first_sentence(" ".join(doc)),
                    "headerLine": index + 1,
                    "effects": EFFECTS.get(name, ["unknown"]),
                    "releaseDisposition": disposition,
                    "dispositionReason": reason,
                    "requiredCases": [],
                    "evidence": [],
                })
                pending = False
            elif stripped and not stripped.startswith("//"):
                pending = False

    return tools


def main() -> int:
    tools = parse(HEADER)
    by_toolset: dict[str, int] = {}
    for tool in tools:
        by_toolset[tool["toolset"]] = by_toolset.get(tool["toolset"], 0) + 1

    document = {
        "schemaVersion": 1,
        "architectureRevision": 1,
        "purpose": (
            "Release capability manifest. One row per exposed TurboMCP operation, with its support "
            "boundary, effect class, release disposition and required evidence. Seeded from the "
            "declarations; `effects` stays `unknown` until the implementation has been read or "
            "exercised, and `evidence` stays empty until a required case passes."
        ),
        "generatedFrom": "Source/TurboMCP/Public/Tools/TurboMCPToolsets.h",
        "counts": {
            "total": len(tools),
            "byToolset": by_toolset,
        },
        "releaseBoundary": {
            "platform": "Windows, source build",
            "engine": "Unreal Engine 5.8.3 source revision 396c9f059903aed5fec78ecd3d437a40c6415368",
            "excludedPlatforms": ["Launcher", "Linux", "macOS", "adjacent engine versions"],
            "batchScope": "ARC09: existing graphRepoint operation kind, one Blueprint graph, one transaction, one final compile, no save",
        },
        "tools": tools,
    }

    with open(OUT, "w", encoding="utf-8") as handle:
        json.dump(document, handle, indent=2, ensure_ascii=False)

    print("wrote %s (%d tools)" % (OUT, len(tools)))
    for name in sorted(by_toolset):
        print("  %-34s %d" % (name, by_toolset[name]))
    return 0


if __name__ == "__main__":
    sys.exit(main())
