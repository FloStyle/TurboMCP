# Copyright (c) 2026 FloStyle
# SPDX-License-Identifier: MPL-2.0
"""Test-project-only tools used by the acceptance harness.

This module is loaded by Unreal's Python plugin when the test project starts.  It is not
part of the TurboMCP plugin and cannot ship with a game.
"""

import unreal
from toolset_registry import tool_call
from toolset_registry.registration import Registration


@unreal.uclass()
class FixtureIsolationToolset(unreal.ToolsetDefinition):
    """Release package handles and reload restored fixture bytes in one editor."""

    @tool_call
    @staticmethod
    def prepare_restore(asset_paths: list[str], remove_paths: list[str],
                        baseline_paths: list[str]) -> dict[str, str]:
        """Release package file handles before the baseline bytes are copied back.

        Restoring through source control was tried first, but the editor's Git provider does
        not expose a revert operation to Python. Unloading before the copy was tried second,
        but Blueprint generated classes kept the old package reachable. This third route uses
        the engine's documented handle-release API, then reload_restored replaces the loaded
        package after the runner copies the baseline file over it.
        """
        baseline_set = set(baseline_paths)
        dirty_paths = []
        dirty_extras = []
        for package in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages():
            name = package.get_name()
            if name == "/Game/Tests" or name.startswith("/Game/Tests/"):
                if name in baseline_set:
                    dirty_paths.append(name)
                else:
                    dirty_extras.append(name)

        restore_paths = sorted(set(asset_paths + dirty_paths))
        packages = []
        for path in restore_paths:
            asset = unreal.EditorAssetLibrary.load_asset(path)
            if asset is not None:
                packages.append(asset.get_outermost())

        # FullyLoadPackages is not an unload operation.  Release the package objects before
        # the runner copies baseline bytes, with a conservative fallback for engine minor versions.
        unload = getattr(unreal.EditorLoadingAndSavingUtils, "unload_packages", None)
        if packages and unload is None and dirty_paths:
            raise RuntimeError("the engine has no package-unload API for dirty fixture packages")
        if packages and unload is not None:
            try:
                unload(packages)
            except TypeError:
                failed = []
                unload(packages, failed)
                if failed:
                    raise RuntimeError("could not unload fixture packages: " + ", ".join(str(p) for p in failed))
            packages.clear()

        removed = []
        # Deleting the changed package after unload is the final handle-release fallback. The
        # runner immediately copies the exact baseline file back, so this is not a logical deletion.
        for path in sorted(set(asset_paths)):
            if unreal.EditorAssetLibrary.does_asset_exist(path):
                if not unreal.EditorAssetLibrary.delete_asset(path):
                    raise RuntimeError(f"could not release changed fixture asset through the editor: {path}")
        for path in sorted(set(remove_paths + dirty_extras)):
            if unreal.EditorAssetLibrary.does_asset_exist(path) or unreal.find_package(path) is not None:
                if not unreal.EditorAssetLibrary.delete_asset(path):
                    raise RuntimeError(f"could not delete fixture asset through the editor: {path}")
                removed.append(path)
        return {
            "prepared": str(len(packages)),
            "dirty_paths": ",".join(dirty_paths),
            "removed": ",".join(removed),
        }

    @tool_call
    @staticmethod
    def reload_restored(asset_paths: list[str]) -> dict[str, str]:
        """Reload copied bytes and prove every restored fixture resolves in memory."""
        # Material editing can temporarily remove an asset from the registry while its
        # package still exists on disk. Rescan the fixture mount before resolving the
        # package that was just copied back.
        registry = unreal.AssetRegistryHelpers.get_asset_registry()
        registry.scan_paths_synchronous(["/Game/Tests"], True, True)

        packages = []
        missing = []
        for path in asset_paths:
            asset = unreal.EditorAssetLibrary.load_asset(path)
            if asset is None:
                # Some material edits temporarily remove AssetData while the loaded
                # UObject still exists. Resolve that object directly so ReloadPackages
                # can restore both the package and its registry entry.
                object_name = path + "." + path.rsplit("/", 1)[-1]
                asset = unreal.find_object(None, object_name)
            if asset is None:
                package = unreal.find_package(path)
                if package is None:
                    missing.append(path)
                else:
                    packages.append(package)
            else:
                packages.append(asset.get_outermost())
        if missing:
            raise RuntimeError("restored fixture assets did not load: " + ", ".join(missing))

        any_reloaded = False
        error = ""
        if packages:
            result = unreal.EditorLoadingAndSavingUtils.reload_packages(
                packages, unreal.ReloadPackagesInteractionMode.ASSUME_POSITIVE)
            if isinstance(result, tuple):
                any_reloaded = bool(result[0])
                if len(result) > 1:
                    error = str(result[1])
            else:
                any_reloaded = bool(result)
        if error:
            raise RuntimeError("fixture reload failed: " + error)
        if packages and not any_reloaded:
            raise RuntimeError("fixture reload did not replace any package")

        packages.clear()
        del asset
        unreal.SystemLibrary.collect_garbage()
        registry.scan_paths_synchronous(["/Game/Tests"], True, True)
        unresolved = [
            p for p in asset_paths
            if unreal.EditorAssetLibrary.load_asset(p) is None and unreal.find_package(p) is None
        ]
        if unresolved:
            raise RuntimeError("fixture assets unresolved after reload: " + ", ".join(unresolved))
        return {"reloaded": str(len(asset_paths)), "resolved": str(len(asset_paths))}


_registration = Registration([FixtureIsolationToolset])
_registration.register()
