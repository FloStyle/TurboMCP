# Copyright (c) 2026 FloStyle
# SPDX-License-Identifier: MPL-2.0
"""Coordinate fixture restore with the kept-alive editor.

`prepare` asks the editor to release package write handles and delete assets that did
not exist at run start. `reload` makes the editor discard stale loaded UObjects after
the harness overwrites changed files. The plan is JSON so PowerShell and both MCP calls
agree on the exact package set.
"""

import argparse
import hashlib
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from tmcp import Session  # noqa: E402

TOOLSETS = (
    "init_unreal.FixtureIsolationToolset",
    # Compatibility with an editor started while this change was being measured.
    "init_unreal.TurboMCPFixtureIsolationToolset",
)


def files(root):
    result = {}
    for base, _, names in os.walk(root):
        for name in names:
            full = os.path.join(base, name)
            relative = os.path.relpath(full, root).replace("\\", "/")
            result[relative] = full
    return result


def digest(path):
    h = hashlib.sha256()
    with open(path, "rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()


def asset_path(relative):
    stem, extension = os.path.splitext(relative)
    if extension.lower() not in (".uasset", ".umap"):
        return None
    return "/Game/Tests/" + stem


def package_paths(relatives):
    return [path for path in (asset_path(relative) for relative in relatives) if path]


def call(name, arguments):
    session = Session()
    session.connect()
    for toolset in TOOLSETS:
        answer = session.call(toolset, name, arguments)
        text = answer if isinstance(answer, str) else json.dumps(answer, sort_keys=True)
        if "not found" in text.lower():
            continue
        if "error" in text.lower() or "failed" in text.lower():
            raise RuntimeError(text)
        return text
    raise RuntimeError("fixture isolation toolset is not registered")


def prepare(fixture, snapshot, plan_path):
    current = files(fixture)
    baseline = files(snapshot)
    changed = sorted(
        relative for relative in baseline
        if relative not in current or digest(current[relative]) != digest(baseline[relative]))
    extras = sorted(relative for relative in current if relative not in baseline)
    plan = {
        "changed": changed,
        "extras": extras,
        "asset_paths": package_paths(changed),
        "remove_paths": package_paths(extras),
    }
    # Always ask the live editor: a suite can leave a package dirty in memory
    # without saving it, in which case every disk hash still matches.
    answer = call("prepare_restore", {
        "asset_paths": plan["asset_paths"],
        "remove_paths": plan["remove_paths"],
        "baseline_paths": sorted(package_paths(baseline)),
    })
    print(answer)
    try:
        details = json.loads(answer)
    except (TypeError, json.JSONDecodeError):
        details = {}
    baseline_paths = set(package_paths(baseline))
    dirty_paths = [
        p for p in details.get("dirty_paths", "").split(",")
        if p and p in baseline_paths
    ]
    plan["asset_paths"] = sorted(set(plan["asset_paths"] + dirty_paths))
    with open(plan_path, "w", encoding="utf-8") as stream:
        json.dump(plan, stream, indent=2)
    print("PREPARED changed=%d dirty=%d removed=%d" %
          (len(changed), len(dirty_paths), len(extras)))


def reload(plan_path):
    with open(plan_path, encoding="utf-8") as stream:
        plan = json.load(stream)
    if plan["asset_paths"]:
        print(call("reload_restored", {"asset_paths": plan["asset_paths"]}))
    print("RELOADED assets=%d" % len(plan["asset_paths"]))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("action", choices=("prepare", "reload"))
    parser.add_argument("--fixture")
    parser.add_argument("--snapshot")
    parser.add_argument("--plan", required=True)
    args = parser.parse_args()
    if args.action == "prepare":
        if not args.fixture or not args.snapshot:
            parser.error("prepare requires --fixture and --snapshot")
        prepare(args.fixture, args.snapshot, args.plan)
    else:
        reload(args.plan)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
