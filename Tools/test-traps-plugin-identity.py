"""Static regression guard for renamed-plugin trap loading.

A plugin may be mounted below any directory name.  The loader must identify its
plugin by the TurboMCP module listed in the enabled plugin descriptor, then use
that plugin's base directory for EngineToolTraps.json.
"""

import json
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
TOOLS = ROOT / "Tools"
SOURCE = (ROOT / "Source/TurboMCP/TurboMCPTraps.cpp").read_text(encoding="utf-8")
BUILD = (ROOT / "Source/TurboMCP/TurboMCP.Build.cs").read_text(encoding="utf-8")
FIXTURE = json.loads((TOOLS / "renamed-plugin-traps.fixture.json").read_text(encoding="utf-8"))
failures = []


def check(label, condition):
    print("  %s %s" % ("ok  " if condition else "FAIL", label))
    if not condition:
        failures.append(label)


def main():
    mounted_folder = FIXTURE["mountedFolder"]
    module_names = {module["Name"] for module in FIXTURE["descriptor"]["Modules"]}
    expected_path = "%s/Content/Traps/EngineToolTraps.json" % mounted_folder
    check("renamed-plugin fixture keeps TurboMCP as the module identity",
          mounted_folder != "TurboMCP" and module_names == {"TurboMCP"})
    check("renamed-plugin fixture resolves traps below the alternate mount",
          FIXTURE["expectedShippedTrapsPath"] == expected_path)
    check("discovers enabled plugins instead of assuming a folder", "GetEnabledPlugins()" in SOURCE)
    check("matches the declaring module identity", "Module.Name == ThisModuleName" in SOURCE)
    check("loads shipped traps from the resolved plugin base directory",
          "OwningPlugin->GetBaseDir()" in SOURCE)
    check("does not retain project or engine TurboMCP folder assumptions",
          'ProjectPluginsDir(), TEXT("TurboMCP")' not in SOURCE
          and 'EnginePluginsDir(), TEXT("Marketplace"), TEXT("TurboMCP")' not in SOURCE)
    check("links the Projects module that owns IPluginManager", '"Projects"' in BUILD)
    print("RESULT: %s" % ("PASS" if not failures else "FAIL"))
    return 0 if not failures else 1


if __name__ == "__main__":
    raise SystemExit(main())
