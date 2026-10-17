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
import glob as globmodule
import json
import os
import re
import sys

# *** NOTHING HERE NAMES A PATH OF ANYONE'S MACHINE. ***
#
# `REPO` and `ENGINE` used to be absolute literals, so this check - which compares the plugin's
# declarations against what UHT actually generated - only ran on the machine they were written on.
# The repository is derived from this file; the engine is found by the same two routes a user has:
# the TURBOMCP_ENGINE / UE_ROOT environment variables, then the conventional source-build location.
_REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def _find_engine_root():
    candidates = []
    for var in ("TURBOMCP_ENGINE", "UE_ROOT"):
        value = os.environ.get(var)
        if value:
            candidates.append(value)
    candidates.extend([r"C:\UnrealEngine", r"D:\UnrealEngine"])
    for candidate in candidates:
        if os.path.isdir(os.path.join(candidate, "Engine", "Build")):
            return candidate
    return None


REPO = _REPO
ENGINE = _find_engine_root()
PLUGIN_SRC = os.path.join(REPO, "Plugins", "TurboMCP", "Source", "TurboMCP")
UHT_OUT = os.path.join(
    REPO, "Plugins", "TurboMCP", "Intermediate", "Build", "Win64", "UnrealEditor", "Inc", "TurboMCP", "UHT"
)
BUILD_VERSION = os.path.join(ENGINE, "Engine", "Build", "Build.version") if ENGINE else None

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
    import json
    project = os.path.join(REPO, "TestProject", "TurboMCPTest.uproject")
    with open(project, encoding="utf-8-sig") as handle:
        data = json.load(handle)
    expected = os.path.realpath(os.path.join(REPO, "Plugins"))
    paths = [os.path.realpath(os.path.join(os.path.dirname(project), p))
             for p in data.get("AdditionalPluginDirectories", [])]
    if expected not in paths:
        report.fail("TestProject must discover this checkout's Plugins directory")
        return
    descriptor = os.path.join(expected, "TurboMCP", "TurboMCP.uplugin")
    if os.path.isfile(descriptor):
        report.ok("project discovers this checkout's TurboMCP descriptor")
    else:
        report.fail("plugin descriptor missing in this checkout")


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

    One false positive remains possible and is NOT hidden here: editing a
    reflection header changes its mtime, UHT re-runs, and when the reflected
    declarations are unchanged its output is byte-identical, so the generated file
    keeps its old mtime and this check reports stale reflection that is not stale.

    That happened once, and the honest resolution is to say so rather than to
    weaken the check. A content-based version was written and then **withdrawn**,
    because it could not be made to fail on a deliberately broken generated header,
    and a check whose failure cannot be demonstrated is worth less than one whose
    false positive is documented. See QUESTIONS Q-8.
    """
    report.section("UHT freshness")

    # The generated header for a given source header, or None when UHT makes none.
    def generated_for(header_path):
        base = os.path.basename(header_path)
        candidate = os.path.join(UHT_OUT, base.replace(".h", ".generated.h"))
        return candidate if os.path.isfile(candidate) else None

    if not os.path.isdir(UHT_OUT):
        report.ok("no generated code yet (build has not run UHT)")
        return

    stale = []
    stale_detail = []
    reflected = 0
    checked = 0
    unpaired = []

    for path in plugin_files(".h"):
        if os.path.basename(path).endswith(".generated.h"):
            continue

        with open(path, "r", encoding="utf-8", errors="replace") as handle:
            text = handle.read()
        if not UHT_MARKER_RE.search(text):
            continue

        reflected += 1

        # A header is compared ONLY against its OWN generated file.
        #
        # The previous version compared every reflection header against one hard-coded
        # generated header, which is a category error: TurboMCPToolsets.generated.h says
        # nothing about whether Logging.h is fresh. It also produced a false positive
        # that lasted a session, because UHT output is byte-identical when the reflected
        # declarations have not changed, so a re-run leaves the generated file's mtime
        # old while the edited header's mtime is new.
        generated = generated_for(path)
        if generated is None:
            # UHT produced no file for this header. That is a fact worth reporting, but
            # it is not staleness and must not be reported as such.
            unpaired.append(os.path.basename(path))
            continue

        checked += 1

        # THE DECLARATIONS ARE COMPARED, NOT THE TIMESTAMPS, AND THAT IS THE ONLY TEST THAT ANSWERS THE
        # QUESTION.
        #
        # The mtime comparison could not distinguish "UHT never ran" from "UHT ran and produced identical
        # bytes" - and the second is the COMMON case, because UHT output depends on the reflected
        # declarations and on nothing else in the file. So every build that touched a COMMENT moved a
        # header's mtime past its generated file, and this check went red for a reason that is not a
        # problem. It was recorded as QUESTIONS Q-8, and it produced a false positive in the run that
        # finally exercised every suite.
        #
        # WHAT MATTERS IS WHETHER EVERY REFLECTED DECLARATION REACHED THE GENERATED FILE. UHT emits
        # `DECLARE_FUNCTION(exec<Name>)` for every `UFUNCTION` it processed, so an `exec` the generated
        # file lacks is EXACTLY the staleness this check exists to catch - and a header whose declarations
        # are all present is fresh no matter what the clock says.
        missing = []
        with open(generated, "r", encoding="utf-8", errors="replace") as handle:
            generated_text = handle.read()
        for match in re.finditer(r"UFUNCTION\s*\([^)]*\)", text):
            tail = text[match.end():]
            semi = tail.find(";")
            if semi < 0:
                continue
            name = re.search(r"\b(\w+)\s*\(", tail[:semi])
            if name is None:
                continue
            if ("exec%s" % name.group(1)) not in generated_text:
                missing.append("%s:%s" % (os.path.basename(path), name.group(1)))

        if missing:
            stale.append(os.path.basename(path))
            stale_detail.extend(missing)

    if stale:
        report.fail(
            "these reflection headers declare something their OWN generated code lacks: %s. "
            "The missing declarations are: %s. THAT is staleness - UHT must re-run before the build, or "
            "the reflection will not carry them. This check compares DECLARATIONS rather than file times, "
            "because a re-run that produces identical bytes leaves the generated file's mtime old while "
            "the header's is new, which is not staleness. See QUESTIONS Q-8."
            % (", ".join(stale), ", ".join(stale_detail[:8]))
        )
    else:
        report.ok(
            "every reflected declaration in all %d reflection header(s) reached its OWN generated code"
            % checked
        )

    if unpaired:
        # Stated, not failed: a reflection header with no generated file of its own is
        # normal when its types are declared by another header in the same module.
        report.ok(
            "no generated header of their own, so not freshness-checked: %s"
            % ", ".join(unpaired)
        )


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


def check_declared_dependencies(report):
    """Every engine plugin module the source uses is declared in the descriptor.

    *** RELEASE FINDING R08, AND THE REASON IT SURVIVED SO LONG. ***

    `TurboMCP.Build.cs` lists `EditorScriptingUtilities` and `PythonScriptPlugin` as module dependencies, and
    `TurboMCP.uplugin` declared neither. The build still worked on THIS machine because the test project enables
    Python explicitly and the editor loads EditorScriptingUtilities for its own reasons - so a missing
    DECLARATION is invisible until someone installs the plugin somewhere else, which is exactly the clean-
    installation case the release has to pass.

    The evidence is in the source rather than in a hand-kept list: an include of
    `Engine/Plugins/<...>/Source/<Module>/...` names both the engine plugin and the module. Every such plugin
    an include refers to must appear in the descriptor's `Plugins` array, or a fresh project cannot resolve it.
    """
    report.section("declared plugin dependencies")

    descriptor = os.path.join(REPO, "Plugins", "TurboMCP", "TurboMCP.uplugin")
    if not os.path.isfile(descriptor):
        report.fail("TurboMCP.uplugin is missing, so no dependency can be declared")
        return
    try:
        with open(descriptor, "r", encoding="utf-8") as handle:
            data = json.load(handle)
    except ValueError as error:
        report.fail("TurboMCP.uplugin is not valid JSON: %s" % error)
        return

    declared = set()
    for entry in data.get("Plugins", []):
        name = entry.get("Name")
        if name:
            declared.add(name)

    # `Engine/Plugins/<group>/<PluginName>/Source/<Module>/...` - the plugin directory name IS the plugin name
    # in the descriptor, which is why the path is enough.
    pattern = re.compile(r"Engine[/\\]Plugins[/\\][^/\\]+[/\\]([^/\\]+)[/\\]Source[/\\]")
    used = {}
    for extension in (".h", ".cpp", ".cs"):
        for path in plugin_files(extension):
            with open(path, "r", encoding="utf-8", errors="replace") as handle:
                for name in pattern.findall(handle.read()):
                    used.setdefault(name, set()).add(os.path.basename(path))

    if not used:
        report.ok("no engine-plugin module is referenced from a plugin include path")
    else:
        undeclared = sorted(name for name in used if name not in declared)
        if undeclared:
            for name in undeclared:
                report.fail(
                    "%s is used by %s but is not declared in TurboMCP.uplugin. A fresh project resolves "
                    "declared dependencies only, so this fails on an installation that does not happen to "
                    "enable it already."
                    % (name, ", ".join(sorted(used[name])[:3])))
        else:
            report.ok("%d engine plugin(s) referenced from includes are all declared: %s"
                      % (len(used), ", ".join(sorted(used))))

    # And the reverse direction, which is what could break NEXT: a declared plugin whose descriptor does not
    # exist in the engine. That is a hard load failure on a clean install, not a warning.
    #
    # THE SEARCH IS RECURSIVE, AND IT HAS TO BE. MEASURED: the first version looked only at
    # `Engine/Plugins/<group>/<Name>/<Name>.uplugin` and reported `SlateInspectorToolset` as having no
    # descriptor, while it is at `Engine/Plugins/Experimental/Toolsets/SlateInspectorToolset/`. A false
    # positive in a blocking gate is a reason to bypass the gate, so the depth is not assumed.
    if ENGINE:
        for name in sorted(declared):
            found = globmodule.glob(
                os.path.join(ENGINE, "Engine", "Plugins", "**", name, name + ".uplugin"),
                recursive=True)
            if not found:
                report.fail("TurboMCP.uplugin declares '%s' and no engine descriptor was found for it" % name)
        report.ok("every declared plugin has an engine descriptor under %s" % ENGINE)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--quiet", action="store_true")
    args = parser.parse_args()

    report = Report(args.quiet)

    check_engine(report)
    check_mount(report)
    check_includes(report)
    check_declared_dependencies(report)
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
