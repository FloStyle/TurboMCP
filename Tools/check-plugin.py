"""Pre-build health check for the TurboMCP plugin.

Run before any build. It catches in seconds the mistakes that otherwise only
appear hours into a compile:

  1. The engine tree is the expected version and complete.
  2. The plugin is mounted where UBT can discover it.
  3. Every ``#include`` resolves to a real file.
  4. Every engine API the plugin calls exists in the real headers.
  5. The UHT-generated reflection is newer than the plugin headers, so the
     generated code cannot be stale. This is the check that caught a real
     failure: headers were edited after UHT ran, and the generated code still
     named the old functions.
  6. Every AICallable function in a header has a matching definition, and every
     toolset class has at least one. The registry silently refuses a toolset
     with no tools, so this is the difference between working tools and an
     editor that looks fine and serves nothing.

Usage:
    python Tools/check-plugin.py            # full check
    python Tools/check-plugin.py --quiet    # only print problems
"""

import argparse
import os
import re
import sys

REPO = r"<REPO>"
ENGINE = r"C:\UnrealEngine"
PLUGIN_SRC = os.path.join(REPO, "Source", "TurboMCP")
UHT_OUT = os.path.join(
    REPO, "Intermediate", "Build", "Win64", "UnrealEditor", "Inc", "TurboMCP", "UHT"
)
MOUNT = os.path.join(ENGINE, "Engine", "Plugins", "Marketplace", "TurboMCP")
BUILD_VERSION = os.path.join(ENGINE, "Engine", "Build", "Build.version")

EXPECTED_VERSION = (5, 8, 3)

INCLUDE_RE = re.compile(r'^\s*#include\s+"([^"]+)"', re.MULTILINE)
AICALLABLE_RE = re.compile(
    r'UFUNCTION\s*\(\s*meta\s*=\s*\(\s*AICallable\s*\)\s*\)\s*'
    r"static\s+\w+\s+(\w+)\s*\(",
    re.MULTILINE,
)
UCLASS_RE = re.compile(r"^UCLASS\(\)\s*\nclass\s+(\w+)\s*:\s*public\s+UToolsetDefinition",
                       re.MULTILINE)

# Engine APIs the plugin depends on: (label, header path under ENGINE, pattern).
ENGINE_APIS = [
    ("UToolsetRegistry::IsAvailable",
     r"Engine\Plugins\Experimental\ToolsetRegistry\Source\ToolsetRegistry\Public\ToolsetRegistry\UToolsetRegistry.h",
     r"IsAvailable"),
    ("UToolsetRegistry::RegisterToolsetClass",
     r"Engine\Plugins\Experimental\ToolsetRegistry\Source\ToolsetRegistry\Public\ToolsetRegistry\UToolsetRegistry.h",
     r"RegisterToolsetClass"),
    ("UToolsetRegistry::UnregisterToolsetClass",
     r"Engine\Plugins\Experimental\ToolsetRegistry\Source\ToolsetRegistry\Public\ToolsetRegistry\UToolsetRegistry.h",
     r"UnregisterToolsetClass"),
    ("UToolsetRegistry::IsToolsetClassRegistered",
     r"Engine\Plugins\Experimental\ToolsetRegistry\Source\ToolsetRegistry\Public\ToolsetRegistry\UToolsetRegistry.h",
     r"IsToolsetClassRegistered"),
    ("UToolsetRegistry::GetAllToolsetJsonSchemas",
     r"Engine\Plugins\Experimental\ToolsetRegistry\Source\ToolsetRegistry\Public\ToolsetRegistry\UToolsetRegistry.h",
     r"GetAllToolsetJsonSchemas"),
    ("UToolsetRegistry::GetToolsetJsonSchema",
     r"Engine\Plugins\Experimental\ToolsetRegistry\Source\ToolsetRegistry\Public\ToolsetRegistry\UToolsetRegistry.h",
     r"GetToolsetJsonSchema"),
    ("IModelContextProtocolModule::Get",
     r"Engine\Plugins\Experimental\ModelContextProtocol\Source\ModelContextProtocol\Public\IModelContextProtocolModule.h",
     r"static IModelContextProtocolModule\* Get"),
    ("IModelContextProtocolModule::GetServer",
     r"Engine\Plugins\Experimental\ModelContextProtocol\Source\ModelContextProtocol\Public\IModelContextProtocolModule.h",
     r"GetServer"),
    ("MCP::GetServerUrlPath",
     r"Engine\Plugins\Experimental\ModelContextProtocol\Source\ModelContextProtocolEngine\Public\ModelContextProtocolSettings.h",
     r"GetServerUrlPath"),
    ("MCP::GetServerPortNumber",
     r"Engine\Plugins\Experimental\ModelContextProtocol\Source\ModelContextProtocolEngine\Public\ModelContextProtocolSettings.h",
     r"GetServerPortNumber"),
    ("UModelContextProtocolSettings::bEnableToolSearch",
     r"Engine\Plugins\Experimental\ModelContextProtocol\Source\ModelContextProtocolEngine\Public\ModelContextProtocolSettings.h",
     r"bEnableToolSearch"),
    ("FCoreDelegates::OnAllModuleLoadingPhasesComplete",
     r"Engine\Source\Runtime\Core\Public\Misc\CoreDelegates.h",
     r"OnAllModuleLoadingPhasesComplete"),
    ("UEditorEngine::IsPlaySessionInProgress",
     r"Engine\Source\Editor\UnrealEd\Classes\Editor\EditorEngine.h",
     r"IsPlaySessionInProgress"),
    ("UEditorEngine::IsSimulateInEditorInProgress",
     r"Engine\Source\Editor\UnrealEd\Classes\Editor\EditorEngine.h",
     r"IsSimulateInEditorInProgress"),
    ("FSlateApplication::GetAllVisibleWindowsOrdered",
     r"Engine\Source\Runtime\Slate\Public\Framework\Application\SlateApplication.h",
     r"GetAllVisibleWindowsOrdered"),
    ("FSlateApplication::IsInitialized",
     r"Engine\Source\Runtime\Slate\Public\Framework\Application\SlateApplication.h",
     r"IsInitialized"),
    ("IAssetRegistry::GetAssetsByPackageName",
     r"Engine\Source\Runtime\AssetRegistry\Public\AssetRegistry\IAssetRegistry.h",
     r"GetAssetsByPackageName"),
    ("IAssetRegistry::GetAssetsByPath",
     r"Engine\Source\Runtime\AssetRegistry\Public\AssetRegistry\IAssetRegistry.h",
     r"GetAssetsByPath"),
    ("IAssetRegistry::GetAssetByObjectPath",
     r"Engine\Source\Runtime\AssetRegistry\Public\AssetRegistry\IAssetRegistry.h",
     r"GetAssetByObjectPath"),
    ("FAssetData::IsValid",
     r"Engine\Source\Runtime\CoreUObject\Public\AssetRegistry\AssetData.h",
     r"bool IsValid"),
    ("FAssetData::GetObjectPathString",
     r"Engine\Source\Runtime\CoreUObject\Public\AssetRegistry\AssetData.h",
     r"GetObjectPathString"),
    ("FPackageName::TryConvertToMountedPath",
     r"Engine\Source\Runtime\CoreUObject\Public\Misc\PackageName.h",
     r"TryConvertToMountedPath"),
    ("FGlobalTabmanager::RegisterNomadTabSpawner",
     r"Engine\Source\Runtime\Slate\Public\Framework\Docking\TabManager.h",
     r"RegisterNomadTabSpawner"),
]


class Report:
    def __init__(self, quiet):
        self.quiet = quiet
        self.failures = []
        self.passes = 0

    def ok(self, message):
        self.passes += 1
        if not self.quiet:
            print("  ok    %s" % message)

    def fail(self, message):
        self.failures.append(message)
        print("  FAIL  %s" % message)

    def section(self, title):
        if not self.quiet:
            print()
            print(title)


def plugin_files(extension):
    for base, _dirs, files in os.walk(PLUGIN_SRC):
        for name in files:
            if name.endswith(extension):
                yield os.path.join(base, name)


def check_engine(report):
    report.section("engine")
    if not os.path.isdir(ENGINE):
        report.fail("engine not found at %s" % ENGINE)
        return
    report.ok("engine tree at %s" % ENGINE)

    if not os.path.isfile(BUILD_VERSION):
        report.fail("Build.version missing - engine not set up")
        return

    import json

    with open(BUILD_VERSION, "r", encoding="utf-8-sig") as handle:
        version = json.load(handle)
    actual = (
        version.get("MajorVersion"),
        version.get("MinorVersion"),
        version.get("PatchVersion"),
    )
    if actual == EXPECTED_VERSION:
        report.ok("Build.version is %d.%d.%d" % actual)
    else:
        report.fail(
            "Build.version is %d.%d.%d but the plugin targets %d.%d.%d"
            % (actual + EXPECTED_VERSION)
        )

    for rel in (
        r"Engine\Plugins\Experimental\ToolsetRegistry\ToolsetRegistry.uplugin",
        r"Engine\Plugins\Experimental\ModelContextProtocol\ModelContextProtocol.uplugin",
    ):
        if os.path.isfile(os.path.join(ENGINE, rel)):
            report.ok("engine plugin present: %s" % os.path.basename(rel))
        else:
            report.fail("engine plugin missing: %s" % rel)


def check_mount(report):
    report.section("plugin discovery")
    if not os.path.isdir(MOUNT):
        report.fail(
            "plugin is not mounted at %s. UBT cannot see a plugin outside "
            "Engine/Plugins. Create it with:\n"
            "        New-Item -ItemType Junction -Path '%s' -Target '%s'"
            % (MOUNT, MOUNT, REPO)
        )
        return
    report.ok("mounted at %s" % MOUNT)
    if os.path.isfile(os.path.join(MOUNT, "TurboMCP.uplugin")):
        report.ok("TurboMCP.uplugin reachable through the mount")
    else:
        report.fail("TurboMCP.uplugin is not reachable through the mount")


def check_includes(report):
    report.section("includes")
    index = set()
    for root in (
        os.path.join(ENGINE, "Engine", "Source"),
        os.path.join(ENGINE, "Engine", "Plugins"),
    ):
        for base, _dirs, files in os.walk(root):
            for name in files:
                index.add(name.lower())

    checked = 0
    missing = []
    for path in list(plugin_files(".h")) + list(plugin_files(".cpp")):
        with open(path, "r", encoding="utf-8", errors="replace") as handle:
            text = handle.read()
        for include in INCLUDE_RE.findall(text):
            checked += 1
            if include.endswith(".generated.h"):
                continue
            if os.path.basename(include).lower() not in index:
                missing.append((os.path.basename(path), include))

    if missing:
        for owner, include in missing:
            report.fail("%s includes unknown header %s" % (owner, include))
    else:
        report.ok("all %d includes resolve" % checked)


def check_apis(report):
    report.section("engine APIs")
    missing = 0
    for label, rel, pattern in ENGINE_APIS:
        path = os.path.join(ENGINE, rel)
        if not os.path.isfile(path):
            report.fail("%s: header not found (%s)" % (label, rel))
            missing += 1
            continue
        with open(path, "r", encoding="utf-8", errors="replace") as handle:
            if not re.search(pattern, handle.read()):
                report.fail("%s: pattern not found in %s" % (label, rel))
                missing += 1
    if not missing:
        report.ok("all %d engine APIs present" % len(ENGINE_APIS))


UHT_MARKER_RE = re.compile(
    r"\bUCLASS\b|\bUSTRUCT\b|\bUENUM\b|\bUFUNCTION\b|\bUPROPERTY\b|\bGENERATED_BODY\b"
)


def check_uht_freshness(report):
    """Only headers that carry reflection matter here.

    UnrealHeaderTool reads a header for `UCLASS`, `USTRUCT`, `UENUM`, `UFUNCTION`
    and `UPROPERTY`. A header with none of those is invisible to it, so changing
    one cannot make the generated code stale and cannot force a re-plan.

    This check used to flag **every** header, which made it wrong rather than
    cautious: it failed on TurboMCPGraphText.h and TurboMCPNodeId.h, which carry
    no reflection at all, and a check that cries wolf on a correct change is worse
    than no check, because it trains the reader to ignore it.
    """
    report.section("UHT freshness")
    generated = os.path.join(UHT_OUT, "TurboMCPToolsets.generated.h")
    if not os.path.isfile(generated):
        report.ok("no generated code yet (build has not run UHT)")
        return

    generated_time = os.path.getmtime(generated)
    stale = []
    reflected = 0
    for path in plugin_files(".h"):
        if os.path.basename(path).endswith(".generated.h"):
            continue

        with open(path, "r", encoding="utf-8", errors="replace") as handle:
            text = handle.read()
        if not UHT_MARKER_RE.search(text):
            continue

        reflected += 1
        if os.path.getmtime(path) > generated_time:
            stale.append(os.path.basename(path))

    if stale:
        report.fail(
            "these reflection headers are newer than the generated code: %s. "
            "UHT must re-run, or the build will use stale reflection and fail. "
            "A header change also makes UBT re-plan, discarding completed actions."
            % ", ".join(stale)
        )
    else:
        report.ok("generated code is newer than all %d reflection header(s)" % reflected)


def check_tools(report):
    report.section("toolsets and tools")
    headers = list(plugin_files(".h"))
    sources = [path for path in plugin_files(".cpp")]

    definitions = set()
    for path in sources:
        with open(path, "r", encoding="utf-8", errors="replace") as handle:
            for match in re.finditer(r"^FString\s+(\w+)::(\w+)\s*\(", handle.read(),
                                     re.MULTILINE):
                definitions.add(match.group(2))

    for path in headers:
        with open(path, "r", encoding="utf-8", errors="replace") as handle:
            text = handle.read()

        classes = UCLASS_RE.findall(text)
        declared = AICALLABLE_RE.findall(text)
        if not classes and not declared:
            continue

        name = os.path.basename(path)
        if not classes:
            report.fail("%s declares AICallable tools but no UToolsetDefinition class"
                        % name)
            continue

        if not declared:
            report.fail(
                "%s declares %s but no AICallable function. The registry refuses a "
                "toolset with no valid tools, and the editor still looks healthy."
                % (name, ", ".join(classes))
            )
            continue

        missing = [tool for tool in declared if tool not in definitions]
        if missing:
            report.fail("%s declares but does not define: %s" % (name, ", ".join(missing)))
        else:
            report.ok("%s: %s with %d tool(s)"
                      % (name, ", ".join(classes), len(declared)))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--quiet", action="store_true")
    args = parser.parse_args()

    report = Report(args.quiet)

    check_engine(report)
    check_mount(report)
    check_includes(report)
    check_apis(report)
    check_uht_freshness(report)
    check_tools(report)

    print()
    if report.failures:
        print("RESULT: FAIL (%d problem(s), %d check(s) passed)"
              % (len(report.failures), report.passes))
        return 1
    print("RESULT: PASS (%d checks)" % report.passes)
    return 0


if __name__ == "__main__":
    sys.exit(main())
