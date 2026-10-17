"""One-shot patch: parse the probe's answer at every E01 call site.

THE DEFECT, MEASURED. `owns(...)` returns `ps_json(...)`, i.e. `(exit_code, output)`. The call sites read

    payload, out = owns(ours, editor_exe, repo_project, engine)

so `payload` was the EXIT CODE - the integer 0 - and `out` was the JSON string. `field(payload, "Owned")`
therefore returned None for a correct `{"Owned": true}` sitting in `out`, and SEVEN assertions reported a
module defect that did not exist. Instrumenting the call site showed both values in one line:

    DBG2 rc= 0  raw= '{"Owned":true,"Reason":"process 40260 is this project\'s editor"}\n'
                    last= {'Owned': True, ...}
      ok   E01 this repository's editor (positional project) is owned

The fix is one line per call site: unpack `(rc, output)` and parse the output.
"""

import pathlib
import re

PATH = pathlib.Path(r"C:\Users\Example\Desktop\TurboMCP\Tools\test-ownership.py")
text = PATH.read_text(encoding="utf-8")

# `payload, out = owns(` ... `)` where the arguments may span lines.
pattern = re.compile(r"(?m)^(?P<ind>[ \t]*)payload, out = (?P<call>owns\(.*?\))[ \t]*$", re.DOTALL)

count = 0


def replace(match):
    global count
    count += 1
    indent = match.group("ind")
    call = match.group("call")
    return ("%s_owns_rc, out = %s\n%spayload = last_json(out)" % (indent, call, indent))


text, count = pattern.subn(replace, text)
PATH.write_text(text, encoding="utf-8")

print("patched %d call site(s)" % count)
for number, line in enumerate(text.splitlines(), start=1):
    if "_owns_rc" in line or "payload = last_json" in line:
        print("L%d: %s" % (number, line.strip()))
