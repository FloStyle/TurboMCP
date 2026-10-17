"""Clean up after the previous patch, which was applied to two different tools by mistake.

1. `EditTurboMCPFunctionRemove` takes only `asset` and `name`; the previous patch added `inputs`/`outputs`
   there, which does not belong on that tool.
2. The signature probe used `FString`, and the shared type resolver REFUSES it -

       "input pin 'Label' has invalid type 'FString': 'FString' is not a type or a loadable class"

   so the case was asserting a type name the engine does not accept. It uses types the resolver supports.
"""

import pathlib

PATH = pathlib.Path(r"C:\Users\Example\Desktop\TurboMCP\Tools\test-function.py")
text = PATH.read_text(encoding="utf-8")

# 1. FunctionRemove does not take the signature parameters.
text = text.replace(
    '{"asset": ASSET, "name": signature_probe, "inputs": "", "outputs": ""})',
    '{"asset": ASSET, "name": signature_probe})')
text = text.replace(
    '{"asset": ASSET, "name": "NoSuchFunctionHere", "inputs": "", "outputs": ""})',
    '{"asset": ASSET, "name": "NoSuchFunctionHere"})')
text = text.replace(
    '{"asset": ASSET, "name": OWNED, "inputs": "", "outputs": ""})',
    '{"asset": ASSET, "name": OWNED, "inputs": "", "outputs": ""})')

# 2. A type the resolver really accepts.
text = text.replace('"inputs": "Score: float, Label: FString",',
                    '"inputs": "Score: float, Count: int",')
text = text.replace('"created inputs: Score, Label" in signed,',
                    '"created inputs: Score, Count" in signed,')

# Document why, at the case that learned it.
text = text.replace(
    "    # The signature syntax is `Name: Type`, comma-separated, from `ParseSignature` in\n"
    "    # TurboMCPFunction.cpp. This case supplies BOTH, then reads back the pin names the tool reports it\n"
    "    # created - and it removes the graph again, so the shared asset is still left as it was found.",
    "    # The signature syntax is `Name: Type`, comma-separated, from `ParseSignature` in\n"
    "    # TurboMCPFunction.cpp. THE TYPES ARE ONES THE RESOLVER ACCEPTS, WHICH `FString` IS NOT - MEASURED:\n"
    "    #   \"input pin 'Label' has invalid type 'FString': 'FString' is not a type or a loadable class\"\n"
    "    # so this case asserts what the engine supports rather than a type name that merely reads well.\n"
    "    # It supplies BOTH parameters, reads back the pin names the tool reports it created, and removes the\n"
    "    # graph again, so the shared asset is left as it was found.")

PATH.write_text(text, encoding="utf-8")

# Report the remove/add call sites so the correction is visible.
for number, line in enumerate(text.splitlines(), start=1):
    if "FunctionRemove" in line or "Score:" in line or "created inputs" in line:
        print("L%d: %s" % (number, line.strip()))
