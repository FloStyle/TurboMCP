# Copyright (c) 2026 FloStyle
# SPDX-License-Identifier: MPL-2.0
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.
"""Resolve paths from this checkout and an explicitly configured Unreal engine root.
The environment wins over ignored workspace.local.json. No launcher or drive discovery is performed.
"""

from __future__ import annotations

import os
import json
from typing import List, Optional

#: The engine version this project is built against. A fork on another engine changes this one value.
EXPECTED_ENGINE_VERSION = "5.8.3"

#: The repository root, derived from this file's own location. ``Tools/`` sits directly under it.
REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
with open(os.path.join(REPO, "engine.lock.json"), encoding="utf-8") as _lock_file:
    EXPECTED_ENGINE_VERSION = json.load(_lock_file)["version"]


def engine_version(engine: str) -> Optional[str]:
    """The engine's own version, read from ``Engine/Build/Build.version``, or ``None``."""
    path = os.path.join(engine, "Engine", "Build", "Build.version")
    if not os.path.isfile(path):
        return None
    try:
        import json

        with open(path, "r", encoding="utf-8") as handle:
            data = json.load(handle)
        return "%s.%s.%s" % (
            data.get("MajorVersion"),
            data.get("MinorVersion"),
            data.get("PatchVersion"),
        )
    except Exception:
        return None


def engine_candidates() -> List[str]:
    """Explicit environment, then ignored local workspace configuration; never guess a different engine."""
    import json
    for var in ("TURBOMCP_ENGINE", "UE_ROOT"):
        if os.environ.get(var):
            return [os.path.abspath(os.environ[var])]
    config = os.path.join(REPO, "workspace.local.json")
    if os.path.isfile(config):
        with open(config, encoding="utf-8-sig") as handle:
            value = json.load(handle).get("engineRoot")
        if value:
            return [os.path.abspath(os.path.join(REPO, value))]
    return []


def find_engine(any_version: bool = False) -> str:
    """The engine install, preferring the one whose version matches :data:`EXPECTED_ENGINE_VERSION`.

    Raises:
        RuntimeError: when nothing resolves, naming everything that was tried. A wrong engine produces a
            build failure in the engine's own files, which reads as a defect in this plugin, so guessing
            is worse than stopping.
    """
    candidates = [c for c in engine_candidates() if os.path.isdir(c)]

    if not any_version:
        for candidate in candidates:
            if engine_version(candidate) == EXPECTED_ENGINE_VERSION:
                return candidate

    if candidates:
        if not any_version:
            found = ", ".join(
                "%s (%s)" % (c, engine_version(c) or "unknown") for c in candidates
            )
            raise RuntimeError(
                "no installed engine is version %s, which is what this project targets. Found: %s\n"
                "Set TURBOMCP_ENGINE, or call find_engine(any_version=True) to accept one anyway."
                % (EXPECTED_ENGINE_VERSION, found)
            )
        return candidates[0]

    raise RuntimeError(
        "no Unreal Engine install could be found. Tried the TURBOMCP_ENGINE and UE_ROOT environment "
        "variables, the Epic launcher registry, and D:\\UnrealEngine and C:\\UnrealEngine.\n"
        "Copy workspace.example.json to workspace.local.json and set engineRoot, or set TURBOMCP_ENGINE."
    )
