"""Bounded type-discovery workflow: exact identities, word parsing and refusals."""
from tmcp import Session, READ_TOOLSET

s = Session()
s.connect()
def find(words, kind="struct"):
    result = s.call(READ_TOOLSET, "ReadTurboMCPTypeFind", {"words": words, "kind": kind})
    print(result, flush=True)
    return result

for query in ("CoreUObject Vector", " CoreUObject\tVector ", "CoreUObject\nVector"):
    text = find(query)
    assert "/Script/CoreUObject.Vector " in text, text
    assert "does not guarantee" in text and "Blueprint generated classes" in text, text
    rows = [line for line in text.splitlines() if line.startswith("/Script/")]
    assert rows and all("CoreUObject" in row and "Vector" in row for row in rows), rows
    assert len(rows) <= 30, rows
    assert text == find(query), "Repeated query ordering changed"
assert find(" \t\n").startswith("error:")
assert find("Vector", "banana").startswith("error:")
assert "Nothing matched" in find("NoSuchType987654"), "Missing type must not be guessed"
assert "/Script/CoreUObject.Vector " in find("CoreUObject Vector", "")
enum = find("TA11_TestEnum", "enum")
assert "/Game/" in enum and "[enum]" in enum, enum
help_text = s.call(READ_TOOLSET, "ReadTurboMCPToolHelp", {"toolsetName": READ_TOOLSET, "toolName": "ReadTurboMCPTypeFind"})
print(help_text)
assert "words:" in help_text and "kind:" in help_text and "writer support" in help_text, help_text
print("RESULT: PASS; type identities, whitespace, determinism, refusals and live help checked.")
