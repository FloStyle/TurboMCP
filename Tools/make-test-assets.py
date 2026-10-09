"""Create a minimal test asset in the project, to prove scripted asset
creation works before relying on it for the acceptance kit.

Run headless:
    UnrealEditor.exe <project> -run=PythonScriptCommandlet -Script=<this file> -unattended -nullrhi
"""

import unreal

CONTENT = "/Game/Tests"

print("[turbomcp-probe] python is running")


def ensure_dir(path):
    if not unreal.EditorAssetLibrary.does_directory_exist(path):
        unreal.EditorAssetLibrary.make_directory(path)
        print("[turbomcp-probe] made directory %s" % path)


ensure_dir(CONTENT)

# A user enum is the simplest asset that exercises the asset registry and the
# save path. TA-11 in Spec/06_ACCEPTANCE.md needs exactly this.
enum_path = CONTENT + "/TA11_TestEnum"
if unreal.EditorAssetLibrary.does_asset_exist(enum_path):
    print("[turbomcp-probe] %s already exists" % enum_path)
else:
    factory = unreal.EnumFactory()
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    new_enum = asset_tools.create_asset(
        asset_name="TA11_TestEnum",
        package_path=CONTENT,
        asset_class=unreal.UserDefinedEnum,
        factory=factory,
    )
    print("[turbomcp-probe] created %s -> %s" % (enum_path, new_enum))

    if new_enum:
        # Four values, which is what TA-11 asks for.
        for name in ("Alpha", "Beta", "Gamma", "Delta"):
            try:
                new_enum.add_enum_value(name)
            except Exception as exc:  # noqa: BLE001
                print("[turbomcp-probe] add_enum_value(%s) failed: %s" % (name, exc))

    saved = unreal.EditorAssetLibrary.save_asset(enum_path)
    print("[turbomcp-probe] saved: %s" % saved)

print("[turbomcp-probe] done")
