# ACCEPTANCE ROW THIS SUITE CLAIMS TO PROVE: T-HELP-6 - it exercises the two PURE functions BEHIND R-CALL-COST with literal inputs, which is the same capability T-HELP-6 names, proved a level down. A row may be claimed by MORE THAN ONE SUITE, and one already is.
"""Exercise the two PURE functions behind `R-CALL-COST`, with LITERAL inputs.

WHY THIS SUITE EXISTS, AND ONE HALF OF THE GAP IS MEASURED RATHER THAN ASSUMED
----------------------------------------------------------------------------
`R-CALL-COST` totals calls by `FTurboMCPCallCost::NormaliseToolset` and attributes them by
`FTurboMCPCallCost::AssetFromArguments`.

THE HALF THAT IS GENUINELY UNREACHABLE IS `AssetFromArguments`. Measured on 2026-10-11 from the log
itself (`TestProject/Saved/Logs/TurboMCPCalls.jsonl`, 252 lines): EVERY entry is `source=engine`, and
an engine event carries NO `arguments` field at all. So no suite can hand that function a non-empty
input, and the only thing watching it is the sentence `R-CALL-COST` prints when the per-asset section
cannot fill. That is this suite's first job.

THE OTHER HALF IS REACHABLE, AND SAYING SO CORRECTS AN EARLIER RECORD RATHER THAN FLATTERING THIS
SUITE. `NormaliseToolset` IS watched by `Tools/test-call-cost.py`: against a build that demonstrably
compiled `return Trimmed;` in its body (`[1/5] Compile [x64] TurboMCPCallCost.cpp`, DLL relinked),
that suite reports `RESULT: FAIL (1) -> no tool row contains the engine's qualification path`. The log
really does carry the qualified spelling - 116 of its 252 lines do. A previously reported break test
that still passed is NOT reproducible, and the most likely cause is the stale-binary trap this project
has recorded seven times: the registration of the qualified form landed on 2026-10-09 (`08dda6e`) and
the caller's spelling constant has only ever held the qualified value (`2b643ad`, same day), so the
log's shape at that break test was the shape it has now. That run cannot be observed from here and is
not claimed to be.

SO WHAT THIS SUITE ADDS FOR THE NORMALISER IS NOT "A CHECK THAT CANNOT OTHERWISE FAIL" BUT "A CHECK
THAT CANNOT GO VACUOUS". The existing check bites only while the log happens to carry a QUALIFIED
name, which is a fact about the engine's hash resolver rather than about this code: an engine that
resolved the bare name, or a log holding only `(top-level)` rows, would leave it green with the
normaliser broken again. A literal input cannot drift, and it is the only shape that can ask for the
DOUBLY-qualified path a join applied twice produces, which is the shape the observed defect had.

THE COMMANDLET THAT WOULD HAVE DONE THIS WITHOUT A SUITE CANNOT RUN, and that is the ENGINE's
constraint, not this project's: `TurboMCP.Build.cs:61` depends on `Kismet`, so the process reaches
`FBlueprintEditorModule::StartupModule()` and dies on its first statement, `check(GEditor);`
(`Editor/Kismet/Private/BlueprintEditorModule.cpp:214`) - before any commandlet's `Main` is reached.
So the assertions run where every other check in this project runs: OVER MCP, FROM A SUITE.

WHAT THIS SUITE DOES, AND WHAT IT DOES NOT
------------------------------------------
It calls `R-PURE-CHECKS` with LITERAL toolset spellings and LITERAL arguments blobs, parses the values
back OUT of the answer, and compares them with literals of its own. It decides nothing from the shape
of a sentence: the collapse is read off a COUNT (`toolset distinct keys | 1`) and the asset answer is
read off a LENGTH (`asset length | N`), because a number beside a sentence is exactly what this
project has been burned by.

WHAT IT CANNOT PROVE, STATED RATHER THAN IMPLIED: it proves that the two FUNCTIONS implement the rule.
It does not prove that `R-CALL-COST` CALLS them on the fields the log actually carries, because that is
a fact about a different tool's body. `Tools/test-call-cost.py` remains the suite that watches that
wiring, and this one does not claim to replace it.

IT TOGGLES NOTHING AND RESTORES NOTHING, because `R-PURE-CHECKS` reads no log, loads no asset and
writes nothing. That is worth stating: it cannot race another suite, and it needs no fixture rebuild,
which is what stops the editor.
"""

import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from tmcp import READ_TOOLSET, Session  # noqa: E402

# The toolset names as the OTHER writer spells them, taken from `TurboMCPAnalyticsProvider.cpp:124-134`,
# which registers each of these under both names. A check over all four is what makes "one key" a
# statement about the rule rather than about one lucky input.
OWN_TOOLSETS = ("TurboMCPReadToolset", "TurboMCPLookToolset",
                "TurboMCPPreviewToolset", "TurboMCPEditToolset")

ASSET_LITERAL = "/Game/Tests/BP_NodeTest"

failures = []
checks_run = [0]


def check(name, ok, detail=""):
    # THE COUNT IS REPORTED RATHER THAN DESCRIBED. A verdict without a denominator says nothing, and
    # this project has recorded a runner that printed PASS while looking at nothing.
    checks_run[0] += 1
    print("  %-4s %s%s" % ("ok" if ok else "FAIL", name, ("  " + detail) if detail else ""))
    if not ok:
        failures.append(name)


def pure(session, toolsets="", arguments=""):
    """One call, with a LITERAL name and a LITERAL dict, which is what the coverage audit reads."""
    return session.call(READ_TOOLSET, "ReadTurboMCPPureChecks",
                        {"toolsets": toolsets, "arguments": arguments})


def value(text, label):
    """The value on the single line `label | value`, EXTRACTED rather than matched in place.

    Each label is anchored at the start of a line and the separator is the pipe the tool writes, so
    `toolset distinct keys` cannot be satisfied by the `toolset spelling(s) given` line: the label is
    matched whole. `None` means the line was ABSENT, which is reported as such rather than compared
    against a default - an unparsed line and an empty value are different findings.
    """
    match = re.search(r"(?m)^%s \| (.*)$" % re.escape(label), text)
    return match.group(1).strip() if match else None


def number(text, label):
    """The same, but PARSED AS A NUMBER, and `None` when the line is absent or is not one."""
    found = value(text, label)
    return int(found) if found is not None and re.fullmatch(r"-?\d+", found) else None


def listed(text, label):
    """Every value of the repeated two-space-indented lines `  label | value`, in order."""
    return re.findall(r"(?m)^  %s \| (.*)$" % re.escape(label), text)


def main():
    session = Session()
    session.connect()

    # ---------------------------------------------------------------------------------------------
    # THE TOOL ITSELF, BEFORE ANYTHING IS CONCLUDED FROM IT. A tool that refuses the call, or that
    # answers from a hard-coded string mentioning neither function, makes every check below
    # meaningless - so this is checked first and the failure names the input that produced it.
    # ---------------------------------------------------------------------------------------------
    print("=== the tool answers, and names the two functions it actually exercised ===")
    base = pure(session, "TurboMCP.TurboMCPReadToolset,TurboMCPReadToolset", "")
    print(base)
    print()
    check("the tool answers rather than refusing the call",
          not base.startswith("error:") and not base.startswith("!! "), base[:160])
    check("the answer names BOTH pure functions it exercises, so it is a report and not a constant",
          "FTurboMCPCallCost::NormaliseToolset" in base
          and "FTurboMCPCallCost::AssetFromArguments" in base,
          base[:300])

    # ---------------------------------------------------------------------------------------------
    # ASSERTION 1 AND 2: THE TWO SPELLINGS OF ONE TOOLSET COLLAPSE TO ONE KEY.
    #
    # THE TWO INPUTS ARE THE TWO WRITERS' REAL OUTPUT, not invented: `TurboMCPAnalyticsProvider.cpp:55`
    # stores each identifier exactly as given, so an ENGINE entry records `TurboMCP.TurboMCPReadToolset`
    # (measured in the log: 28 such lines), while this plugin's own `FScopedCall` records the bare
    # `TurboMCPReadToolset`.
    #
    # THE INPUT THAT MAKES EACH CHECK FAIL IS NAMED IN ITS OWN COMMENT, because a check whose failing
    # input nobody can name is a check nobody has shown can fail.
    # ---------------------------------------------------------------------------------------------
    print()
    print("=== the two spellings the log's two writers produce collapse to ONE key ===")
    given = listed(base, "in")
    keys = listed(base, "key")

    # FAILING INPUT: a tool that trims and re-emits, i.e. one that echoes the spellings without ever
    # calling the normaliser - the two lists would come back equal instead of the keys agreeing.
    check("both spellings are echoed back, so the answer is about the inputs that were given",
          given == ["TurboMCP.TurboMCPReadToolset", "TurboMCPReadToolset"], "in=%s" % given)

    # FAILING INPUT: `TurboMCP.TurboMCPReadToolset` with `NormaliseToolset` replaced by
    # `return Trimmed;` - the exact break that did not bite in `test-call-cost`, which would return
    # the qualified spelling unchanged and put the engine's qualification path in the key.
    check("the engine's QUALIFIED spelling collapses to the toolset's own name",
          len(keys) == 2 and keys[0] == "TurboMCPReadToolset", "keys=%s" % keys)

    # FAILING INPUT: the bare `TurboMCPReadToolset` with a normaliser that strips a fixed prefix
    # list - a spelling it does not know would come back sliced somewhere other than its own name.
    check("this plugin's BARE spelling is unchanged, because its last segment is itself",
          len(keys) == 2 and keys[1] == "TurboMCPReadToolset", "keys=%s" % keys)

    # FAILING INPUT: both spellings at once with any normaliser that returns its argument - the count
    # comes back 2, WHICH IS THE DEFECT THIS WHOLE CAPABILITY EXISTS TO PREVENT: one toolset reported
    # as two rows, each with a denominator of its own.
    check("the two spellings produce ONE key, so one toolset cannot become two rows with two denominators",
          number(base, "toolset distinct keys") == 1,
          "toolset distinct keys=%r" % number(base, "toolset distinct keys"))

    # ---------------------------------------------------------------------------------------------
    # ASSERTION 3: A SECOND TOOLSET, so the check above cannot be satisfied by a normaliser that
    # returns a CONSTANT. A constant would make the read toolset's pair agree and look correct.
    # ---------------------------------------------------------------------------------------------
    print()
    print("=== a SECOND toolset, so a normaliser that returns a constant cannot pass the check above ===")
    edit = pure(session, "TurboMCP.TurboMCPEditToolset,TurboMCPEditToolset", "")
    print(edit)
    print()
    edit_keys = listed(edit, "key")

    # FAILING INPUT: the edit toolset with a normaliser that returns a constant such as
    # `TurboMCPReadToolset` - the check above would still pass and THIS one goes red, which is the
    # only reason it is here.
    check("the edit toolset collapses to a DIFFERENT key, so the normaliser is not returning a constant",
          edit_keys == ["TurboMCPEditToolset", "TurboMCPEditToolset"], "keys=%s" % edit_keys)
    check("and it too is ONE key, which is the claim made for every toolset and not just one",
          number(edit, "toolset distinct keys") == 1,
          "toolset distinct keys=%r" % number(edit, "toolset distinct keys"))

    # ---------------------------------------------------------------------------------------------
    # THE DOUBLY-QUALIFIED PATH, WHICH IS THE SHAPE A JOIN APPLIED TWICE PRODUCES. The first version
    # of the normaliser stripped a leading `TurboMCP.`, and the tool's own first real run printed
    # `TurboMCPEditToolset.TurboMCP.TurboMCPEditToolset.EditTurboMCPCallLog` - so this input
    # distinguishes LAST SEGMENT from PREFIX STRIP, which the two spellings above cannot.
    # ---------------------------------------------------------------------------------------------
    print()
    print("=== a path qualified twice, which is the shape a join applied twice produces ===")
    doubled = pure(session, "TurboMCP.TurboMCPReadToolset.TurboMCP.TurboMCPReadToolset", "")
    print(doubled)
    print()

    # FAILING INPUT: the same string with a normaliser that strips a leading `TurboMCP.` - it returns
    # `TurboMCPReadToolset.TurboMCP.TurboMCPReadToolset`, which is the observed defect verbatim.
    check("a path qualified twice still collapses to the toolset's own name, so the rule is LAST SEGMENT and not PREFIX STRIP",
          listed(doubled, "key") == ["TurboMCPReadToolset"], "keys=%s" % listed(doubled, "key"))

    # ---------------------------------------------------------------------------------------------
    # ALL FOUR TOOLSETS THE ANALYTICS PROVIDER REGISTERS UNDER BOTH NAMES. One input cannot show a
    # rule; four inputs that all agree is the closest a single call can get to showing one.
    # ---------------------------------------------------------------------------------------------
    print()
    print("=== all four toolsets, each under both of the names it is registered by ===")
    offenders = []
    for name in OWN_TOOLSETS:
        pair = pure(session, "TurboMCP.%s,%s" % (name, name), "")
        got = listed(pair, "key")
        print("    %-24s -> %s" % (name, got))
        if got != [name, name] or number(pair, "toolset distinct keys") != 1:
            offenders.append("%s -> %s" % (name, got))

    # FAILING INPUT: ANY ONE of the four with a normaliser that mishandles it - a prefix list that
    # knows only `TurboMCPReadToolset`, or a rule that slices at the FIRST dot instead of the last.
    check("every one of the four toolsets collapses its two spellings to its own name",
          not offenders, "; ".join(offenders) if offenders else "all four agree")

    # ---------------------------------------------------------------------------------------------
    # ASSERTION 4: THE ASSET EXTRACTION. Three distinct emptinesses, because they take different code
    # paths and a suite that only tested one would not know the other two exist:
    #   (a) an EMPTY arguments field, which is what EVERY engine event carries - the early return;
    #   (b) non-empty arguments with no asset key at all - the key loop falling through;
    #   (c) a key that merely ENDS in `asset`, which a needle search that forgot its quotes would
    #       match and report an asset the caller never named.
    # ---------------------------------------------------------------------------------------------
    print()
    print("=== NO ARGUMENTS AT ALL, which is what every engine event in the real log carries ===")
    empty = pure(session, "TurboMCPReadToolset", "")
    print(empty)
    print()

    # FAILING INPUT: empty arguments with a function that returns a non-empty placeholder - the length
    # would stop being zero, which is the only reason the length is printed at all.
    check("empty arguments give an asset key of length ZERO, parsed as a number",
          number(empty, "asset length") == 0, "asset length=%r" % number(empty, "asset length"))
    check("an empty asset key is PRINTED rather than left as a blank line, so the line is not missing",
          value(empty, "asset key") == "(empty)",
          "asset key=%r" % value(empty, "asset key"))

    # FAILING INPUT: empty arguments with a function whose caller reads the absence as a zero - the
    # flag would say yes, and the per-asset totals would gain a row named after nothing.
    check("it states that NO asset is named, which is an ABSENT MEASUREMENT and not zero cost",
          value(empty, "asset names one") == "no", "asset names one=%r" % value(empty, "asset names one"))

    print("=== arguments that ARE given and still name no asset, which is a different code path ===")
    no_key = pure(session, "TurboMCPReadToolset", '{"toolset":"TurboMCPReadToolset"}')
    print(no_key)
    print()

    # FAILING INPUT: `{"toolset":"TurboMCPReadToolset"}` with a function that returns the whole blob
    # when no known key is found, instead of the empty string.
    check("non-empty arguments naming no asset ALSO give a key of length zero",
          number(no_key, "asset length") == 0, "asset length=%r" % number(no_key, "asset length"))
    check("and those arguments were really given, so this is the key loop falling through and not the early return",
          number(no_key, "arguments length") > 0,
          "arguments length=%r" % number(no_key, "arguments length"))

    print("=== a key that merely ENDS in `asset`, which must not be mistaken for the `asset` key ===")
    near = pure(session, "TurboMCPReadToolset", '{"myasset":"BP_X"}')
    print(near)
    print()

    # FAILING INPUT: `{"myasset":"BP_X"}` with a needle search that does not require the QUOTES around
    # the key - it would find `asset` inside `myasset` and report `BP_X`, an asset nobody named.
    check("a longer key ending in `asset` is not read as the `asset` key",
          number(near, "asset length") == 0, "asset length=%r" % number(near, "asset length"))

    # ---------------------------------------------------------------------------------------------
    # AND THE OTHER HALF OF ASSERTION 4: A LITERAL THAT NAMES ONE. Without this, every check above
    # would also pass on a function that always returns the empty string - which is precisely the
    # failure mode a silent one-way check has.
    # ---------------------------------------------------------------------------------------------
    print("=== a literal that DOES name an asset, so the empty answers above are a distinction ===")
    literal = pure(session, "TurboMCPReadToolset", '{"asset":"%s"}' % ASSET_LITERAL)
    print(literal)
    print()

    # FAILING INPUT: that literal with `AssetFromArguments` replaced by `return FString();` - the key
    # would be `(empty)` and the length zero, and every empty-answer check above would still pass.
    check("a literal naming an asset comes back as EXACTLY that asset",
          value(literal, "asset key") == ASSET_LITERAL,
          "asset key=%r" % value(literal, "asset key"))
    check("and its length is the number of characters in it, PARSED rather than read from a sentence",
          number(literal, "asset length") == len(ASSET_LITERAL),
          "asset length=%r expected=%d" % (number(literal, "asset length"), len(ASSET_LITERAL)))
    check("and it says an asset IS named, which is the opposite of the same tool's empty answer",
          value(literal, "asset names one") == "yes",
          "asset names one=%r" % value(literal, "asset names one"))

    # A PLURAL KEY IS TRIED BEFORE THE SINGULAR ONE, which the code states as its contract: a plural
    # key lists assets and the first is the honest answer for a per-asset total.
    print("=== a plural key is preferred to a singular one, which is the order the code states ===")
    plural = pure(session, "TurboMCPReadToolset", '{"assets":"BP_A","asset":"BP_B"}')
    print(plural)
    print()

    # FAILING INPUT: the same blob with the key list reversed - `asset` is tried first and answers
    # `BP_B`, so the check distinguishes a precedence rule from an accident of ordering.
    check("the `assets` key is read before the `asset` key",
          value(plural, "asset key") == "BP_A", "asset key=%r" % value(plural, "asset key"))

    # ---------------------------------------------------------------------------------------------
    # `P1` ON THIS TOOL'S OWN EMPTY INPUT: no toolset spelling at all is an ABSENT MEASUREMENT, and
    # a report that answered it with `toolset distinct keys | 0` would read as a toolset that had
    # collapsed to nothing.
    # ---------------------------------------------------------------------------------------------
    print("=== no toolset spelling at all, which is this tool's own empty answer ===")
    none_given = pure(session, "", "")
    print(none_given)
    print()

    # FAILING INPUT: an empty `toolsets` value with the `P1` branch deleted - the count of zero would
    # be the whole answer, and a zero with no reason is the shape `P1` forbids.
    check("no spelling given says so rather than reporting a bare count of zero",
          "NO TOOLSET SPELLING WAS GIVEN" in none_given, none_given[:400])
    check("and the count is still printed, so the absence has a denominator of its own",
          number(none_given, "toolset distinct keys") == 0,
          "toolset distinct keys=%r" % number(none_given, "toolset distinct keys"))

    print()
    if failures:
        print("RESULT: FAIL (%d) -> %s" % (len(failures), ", ".join(failures)))
        return 1
    print("RESULT: PASS (%d check(s) run)" % checks_run[0])
    return 0


if __name__ == "__main__":
    sys.exit(main())
