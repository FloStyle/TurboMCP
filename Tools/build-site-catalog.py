"""Build the public tool catalogue from documented AICallable declarations.

Run with --check in CI. This describes source declarations, not live registration
or accepted behavior. Runtime schema discovery remains authoritative for calls.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[1]
HEADER = "Plugins/TurboMCP/Source/TurboMCP/Public/Tools/TurboMCPToolsets.h"
OUTPUT = ROOT / "site/tools.json"
DECLARATION = re.compile(
    r"/\*\*((?:(?!\*/).)*)\*/\s*"
    r"UFUNCTION\s*\(meta\s*=\s*\(AICallable\)\)\s*"
    r"static\s+FString\s+(\w+)\s*\((.*?)\);", re.S
)


def arguments(signature):
    """Split C++ parameters without splitting strings, templates or TEXT()."""
    result, start, depth, quote, escape = [], 0, 0, False, False
    for index, char in enumerate(signature):
        if quote:
            if escape:
                escape = False
            elif char == "\\":
                escape = True
            elif char == '"':
                quote = False
        elif char == '"':
            quote = True
        elif char in "(<{[":
            depth += 1
        elif char in ")>}]":
            depth -= 1
        elif char == "," and depth == 0:
            result.append(signature[start:index].strip())
            start = index + 1
    if signature[start:].strip():
        result.append(signature[start:].strip())
    if depth or quote:
        raise ValueError("Unbalanced parameter declaration")
    return result


def catalogue():
    source = (ROOT / HEADER).read_text(encoding="utf-8")
    # Canonical text hash is independent of Git's LF/CRLF checkout conversion.
    source = source.replace("\r\n", "\n")
    rows = []
    for match in DECLARATION.finditer(source):
        comment, name, signature = match.groups()
        clean = "\n".join(re.sub(r"^\s*\* ?", "", s).strip()
                          for s in comment.splitlines()).strip()
        description = re.sub(r"\s+", " ", clean.split("@param", 1)[0]).strip()
        if not description:
            raise ValueError(f"Missing description: {name}")
        docs = {m.group(1): re.sub(r"\s+", " ", m.group(2)).strip()
                for m in re.finditer(r"@param\s+(\w+)\s+(.*?)(?=\n@param|\Z)", clean, re.S)}
        params = []
        for argument in arguments(signature):
            declaration, separator, default = argument.partition("=")
            item = re.fullmatch(r"\s*(.*?)\s+(\w+)\s*", declaration, re.S)
            if not item:
                raise ValueError(f"Cannot parse parameter: {name}: {argument}")
            kind, param = item.groups()
            params.append({"name": param, "type": re.sub(r"\s+", " ", kind),
                           "required": not separator, "default": default.strip() if separator else None,
                           "description": docs.get(param, "See the declaration and current tool help.")})
        group = next((g for g in ("Read", "Look", "Preview", "Edit") if name.startswith(g + "TurboMCP")), None)
        if not group:
            raise ValueError(f"Unexpected tool name: {name}")
        summary = re.split(r"(?<=[a-z0-9)])\.\s+(?=[A-Z])", description, maxsplit=1)[0].rstrip(".")
        rows.append({"name": name, "group": group,
                     "toolset": f"TurboMCP.TurboMCP{group}Toolset",
                     "summary": summary, "description": description, "parameters": params,
                     "sourceLine": source.count("\n", 0, match.start(2)) + 1})
    markers = len(re.findall(r"UFUNCTION\s*\(meta\s*=\s*\(AICallable\)\)", source))
    if not rows or len(rows) != markers or len({r['name'] for r in rows}) != len(rows):
        raise ValueError(f"Incomplete or duplicate catalogue: {len(rows)} rows, {markers} AICallable declarations")
    rows.sort(key=lambda row: row["name"])
    return {"schema": 1, "source": HEADER,
            "sourceTextSha256": hashlib.sha256(source.encode("utf-8")).hexdigest(),
            "count": len(rows),
            "counts": {g: sum(r["group"] == g for r in rows) for g in ("Read", "Look", "Preview", "Edit")},
            "boundary": "Source declarations, including Preview/Edit pairs. Not a live registration or release-acceptance result.",
            "tools": rows}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true", help="Fail when the checked-in catalogue differs")
    args = parser.parse_args()
    data = catalogue()
    result = json.dumps(data, indent=2, ensure_ascii=True) + "\n"
    if args.check:
        if not OUTPUT.is_file() or OUTPUT.read_text(encoding="utf-8") != result:
            raise ValueError("site/tools.json is stale. Run python Tools/build-site-catalog.py")
    else:
        OUTPUT.write_text(result, encoding="utf-8", newline="\n")
    print(f"PASS: {data['count']} documented AICallable declarations; catalogue {'current' if args.check else 'written'}")


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError) as error:
        print(f"FAIL: {error}", file=sys.stderr)
        sys.exit(1)
