# Copyright (c) 2026 FloStyle
# SPDX-License-Identifier: MPL-2.0
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.
"""Finds the repository root and the Unreal Engine install, for every Python tool in this directory.

*** THIS MODULE EXISTS BECAUSE THE TOOLS HERE USED TO HARD-CODE PATHS OF ONE MACHINE. ***

`check-includes.py` and `check-plugin.py` named ``C:\\Users\\<user>\\...`` and ``C:\\UnrealEngine`` as
literals, and `test-vcs.py` and `test-vcs-write.py` named the engine the same way. A clone anywhere else
failed on the first invocation with a message about the author's disk rather than about the repository,
and two of those files are SUITES that the runner executes on every full run.

SO EVERYTHING IS DERIVED, AND THE ENGINE IS MATCHED BY VERSION RATHER THAN BY LOCATION:

    REPO      the parent of this file's directory - nothing is configured, so nothing is misconfigured
    ENGINE    every candidate is collected, then the one whose ``Engine/Build/Build.version`` MATCHES
              ``EXPECTED_ENGINE_VERSION`` is preferred

*** AND THE VERSION MATTER IS NOT A NICETY. *** This machine has UnrealEngine **5.8.3** at
``C:\\UnrealEngine`` AND an Epic launcher install of **5.6**. The first version of the PowerShell
equivalent resolved the engine by DISCOVERY and returned **5.6** - honestly, because 5.6 has an editor -
AND IT WAS STILL THE WRONG ENGINE, which would have produced a build failure inside the engine's own
headers and read as a defect in this plugin. **A path that resolves is not the same as a path that is
right**, and that is the same class of defect these tools exist to find.

WHEN NOTHING RESOLVES, :func:`find_engine` RAISES with the list of what it tried. It never falls back to
a guess, because a wrong engine is a misdiagnosis and an absent one is a clear message.
"""

from __future__ import annotations

import os
from typing import List, Optional

#: The engine version this project is built against. A fork on another engine changes this one value.
EXPECTED_ENGINE_VERSION = "5.8.3"

#: The repository root, derived from this file's own location. ``Tools/`` sits directly under it.
REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


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
    """Every directory that could be an engine install, in the order a user would expect.

    The order matters only for the tie-break, because :func:`find_engine` prefers a MATCHING VERSION
    wherever it was found. It is still stable so that a run is reproducible.
    """
    candidates: List[str] = []

    for var in ("TURBOMCP_ENGINE", "UE_ROOT"):
        value = os.environ.get(var)
        if value:
            candidates.append(value)

    # Epic's launcher installs, if the registry can be read. `winreg` is import-guarded because this
    # module is imported by tools that may run on a machine where it does not exist.
    try:
        import winreg  # type: ignore

        root = winreg.OpenKey(winreg.HKEY_LOCAL_MACHINE, r"SOFTWARE\EpicGames\Unreal Engine")
        index = 0
        while True:
            try:
                name = winreg.EnumKey(root, index)
            except OSError:
                break
            index += 1
            try:
                with winreg.OpenKey(root, name) as key:
                    installed, _ = winreg.QueryValueEx(key, "InstalledDirectory")
                if installed:
                    candidates.append(installed)
            except OSError:
                continue
    except Exception:
        pass

    candidates.extend([r"D:\UnrealEngine", r"C:\UnrealEngine"])
    return candidates


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
        "Set TURBOMCP_ENGINE to the engine root."
    )
