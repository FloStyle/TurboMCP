# ACCEPTANCE ROW THIS SUITE CLAIMS TO PROVE: T-VCS-1
#
#   | T-VCS-1 | Checkout | With source control off: the answer says so per asset. With it on: the state is
#   |         |          | reported per asset, a checkout is attempted through the provider, and the provider's own
#   |         |          | answer is repeated. A headless run of the same call records what the provider did, because
#   |         |          | that is the case that already failed once |
#
# VERIFIED VERBATIM against `Spec/06_ACCEPTANCE.md:265`. `Spec/05_DECISIONS.md:381-389` (`D-57`) is the ordering
# decision this row obeys: `E-VCS-CHECKOUT` is built and tested before any write tool calls it.
"""`E-VCS-CHECKOUT`: state per asset, and a checkout through the project's own provider.

WHAT THIS SUITE IS HELD TO, AND IT IS THE WHOLE CAPABILITY: with source control off, the answer must SAY SO PER
ASSET; with it on, the state must be reported per asset, a checkout must be ATTEMPTED THROUGH THE PROVIDER, and
THE PROVIDER'S OWN ANSWER must be repeated.

*** IT MEASURES EVERY BRANCH RATHER THAN ONE, AND IT OWNS THE EDITOR TO DO IT. ***

  Phase A   the case `T-VCS-1` calls "source control off". NOTHING IN THIS REPOSITORY SETS A PROVIDER
            (`SourceControlSettings.cpp:58-65` reads `-SCCProvider=` off the command line or `Provider=` from
            `[SourceControl.SourceControlSettings]`, and no file here writes either), so this is what the
            harness normally hands over. If a provider IS already selected the suite restarts the editor
            WITHOUT one, so this phase runs in every invocation rather than being skipped.
  Phase B1  "with it on", GIT, by relaunching the editor with `-SCCProvider=Git`. This is the half the row-1.3
            measurement deliberately left unmeasured: `Spec/WORKING_SCROW.md` section 5 item 6 says a Git
            checkout was not run, so Git's checkout ANSWER was unknown and only `is_available=True` had been
            measured. THIS SUITE MEASURES THE ANSWER INSTEAD OF PREDICTING IT.
  Phase B2  PERFORCE, by relaunching with `-SCCProvider=Perforce`. This is the provider whose
            `UsesCheckout() == true` (`PerforceSourceControlProvider.cpp:622-625`) is the reason the capability
            exists, and it is the measured proof that BOTH questions are required: `IsEnabled()` is true while
            `IsAvailable()` is false.
  Phase B3  THE SAME THING READ BACK THROUGH A DIFFERENT TOOL. A tool's own report is not evidence, so the
            engine's OWN helper `USourceControlHelpers` (`SourceControlHelpers.h:244`, `ScriptName =
            "SourceControl"`) is driven through `PythonScriptCommandlet` and its answers are compared with this
            tool's. A third tool, the `git` CLI, then confirms nothing in the working tree was touched.
  Phase C   RESTORES the provider-less editor the suite was handed, and ASSERTS the restore by reading the
            setting back through the tool.

GIT AND PERFORCE ARE THE TWO PROVIDERS THIS MACHINE CAN REACH AT ALL: `Spec/WORKING_SCROW.md` section 1
measured Git as `is_enabled=True, is_available=True`, Perforce as `is_enabled=True, is_available=False` with a
`p4 set` port that does not answer, and `cm` and `svn` are not on `PATH`. Subversion and Plastic therefore
cannot be selected here in a way that proves anything, and the suite says so instead of pretending.
"""

import os
import re
import subprocess
import sys
import tempfile
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from tmcp import EDIT_TOOLSET, PREVIEW_TOOLSET, READ_TOOLSET, Session  # noqa: E402
import tmcp_editor  # noqa: E402

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ENGINE = "C:\\UnrealEngine"
PROJECT = os.path.join(REPO, "TestProject", "TurboMCPTest.uproject")
CMD_EXE = os.path.join(ENGINE, "Engine", "Binaries", "Win64", "UnrealEditor-Cmd.exe")
CHECK_SERVER = os.path.join(REPO, "Tools", "check-mcp-server.py")

# THE ONE FIXTURE THIS SUITE NAMES, and it is named only as an INPUT. `TA01_HandLaid` exists on disk and is
# tracked by git; `TA14_KitDataTable` exists on disk and is NOT tracked. NEITHER ONE'S STATE, SIZE, COUNT OR
# CONTENT IS ASSERTED - the pair is used because it makes PER ASSET observable, and every check below is
# written so that it holds whatever those two states happen to be.
FIXTURE = "TA01_HandLaid"
FIXTURE_FILE = os.path.join(REPO, "TestProject", "Content", "Tests", "Kit", "TA01_HandLaid.uasset")
ASSETS = "TA01_HandLaid,TA14_KitDataTable"

# THE ENGINE'S OWN REFUSAL STRINGS, REPRODUCED VERBATIM FROM ITS OWN GATE. They are what the tool must answer
# with, and they are written here so the suite fails if the tool ever paraphrases them.
#   SourceControlHelpers.cpp:67  "Revision control is not enabled."
#   SourceControlHelpers.cpp:76  "Revision control server is currently not available."
NO_PROVIDER_REFUSAL = "Revision control is not enabled."
NO_SERVER_REFUSAL = "Revision control server is currently not available."

# ---------------------------------------------------------------------------------------------------------
# THE CROSS-CHECK PROBE, WRITTEN TO A TEMP FILE AND RUN BY THE ENGINE'S OWN `PythonScriptCommandlet`.
#
# *** WHY THIS IS IN THE SUITE RATHER THAN IN A NOTE: A TOOL'S OWN REPORT IS NOT EVIDENCE. *** This reaches
# `USourceControlHelpers::CheckOutFile` (`SourceControlHelpers.cpp:430-477`) and therefore
# `SourceControlHelpersInternal::VerifySourceControl` - a DIFFERENT code path from `FTurboMCPSourceControl` -
# and its answers are compared with this tool's below.
#
# ITS RESULT GOES TO A FILE, AND THAT IS MEASURED RATHER THAN TIDY: `print` output from
# `PythonScriptCommandlet` does NOT survive a PowerShell redirect on this machine (a run captured only
# `LogPythonScriptCommandlet` lines and no probe line at all), so the answer is written where it cannot be lost.
# ---------------------------------------------------------------------------------------------------------
READBACK_SCRIPT = '''"""Read the source control answer back through the ENGINE'S OWN Python surface."""
import os

import unreal

RESULT = os.environ["TMCP_VCS_READBACK_RESULT"]
ASSET = os.environ["TMCP_VCS_READBACK_ASSET"]

with open(RESULT, "w") as handle:
    handle.write("")

SC = getattr(unreal, "SourceControl", None)
with open(RESULT, "a") as handle:
    handle.write("present = %r\\n" % (SC is not None))
    if SC is not None:
        handle.write("current_provider = %r\\n" % (SC.current_provider(),))
        handle.write("is_enabled = %r\\n" % (SC.is_enabled(),))
        handle.write("is_available = %r\\n" % (SC.is_available(),))
        handle.write("check_out_file_returned = %r\\n" % (SC.check_out_file(ASSET, True),))
        handle.write("last_error_msg = %r\\n" % (SC.last_error_msg(),))
    handle.write("done = True\\n")
'''

failures = []
checks_run = [0]


def check(name, ok, detail=""):
    checks_run[0] += 1
    print("  %-4s %s%s" % ("ok" if ok else "FAIL", name, ("  " + detail) if detail and not ok else ""))
    if not ok:
        failures.append(name)


def field(text, label):
    """The value of one `label : value` line, or None. PARSED, never matched as a sentence."""
    match = re.search(r"^\s*" + re.escape(label) + r"\s*:\s*(.+?)\s*$", text, re.MULTILINE)
    return match.group(1) if match else None


def rows(text):
    """Every per-asset block of the STATE section as (label, body), in order.

    THE STATE SECTION ONLY. The Edit answer appends a `=== checkout ... ===` section that prints its own `[n]`
    blocks, and parsing both at once would double the row count and make a count check meaningless.
    """
    head = text.split("=== checkout (E-VCS-CHECKOUT) ===")[0]
    out = []
    label = None
    body = []
    for line in head.splitlines():
        match = re.match(r"^  \[(\d+)\] (.+?)\s*$", line)
        if match:
            if label is not None:
                out.append((label, "\n".join(body)))
            label = match.group(2)
            body = []
        elif label is not None:
            body.append(line)
    if label is not None:
        out.append((label, "\n".join(body)))
    return out


def refusal_of(body):
    """The `REFUSED: ...` value of one asset block, or None."""
    return field(body, "REFUSED")


def row_named(text, name):
    for label, body in rows(text):
        if label.strip() == name:
            return body
    return None


# ---------------------------------------------------------------------------------------------------------
# THE PROVIDER LAUNCHER, AND WHY THE SUITE LAUNCHES THE EDITOR ITSELF RATHER THAN CALLING `start-editor.ps1`.
#
# THE SETTING HAS TO BE ON THE COMMAND LINE AND NOTHING IN THIS REPOSITORY PUTS IT THERE.
# `SourceControlSettings.cpp:58-61` reads the `SCCProvider` switch pair off the command line before anything
# else, and `:64-65` falls back to `Provider=` under `[SourceControl.SourceControlSettings]`; no
# `SourceControlSettings.ini` exists anywhere in this project, in `Engine/Config`, in `Engine/Saved/Config` or
# in the user's Unreal AppData (measured in `Spec/WORKING_SCROW.md` section 1). `Tools/start-editor.ps1` takes
# no pass-through arguments, so the editor is launched directly with the same argv that script uses.
#
# *** THE ARGUMENT IS ONE LIST ELEMENT, WHICH IS ALSO THE FIX FOR THE TRAP `Spec/WORKING_SCROW.md` SECTION 5
# ITEM 4 RECORDS: `-SCCProvider=Plastic SCM` CONTAINS A SPACE (`PlasticSourceControlProvider.cpp:34`) and needs
# quoting in any shell harness. A LIST OF ARGUMENTS HAS NO QUOTING TO GET WRONG. ***
# ---------------------------------------------------------------------------------------------------------
def launch_editor_with_provider(provider, timeout_seconds=240):
    """Start UnrealEditor with `-SCCProvider=<provider>` and wait for the MCP server. (ok, detail)."""
    exe = os.path.join(ENGINE, "Engine", "Binaries", "Win64", "UnrealEditor.exe")
    if not os.path.exists(exe):
        return False, "the editor binary is not at %s" % exe

    argv = [exe, PROJECT, "-nosplash", "-nosound", "-nopause", "-unattended", "-culture=en",
            "-SCCProvider=" + provider]
    print("    launching: %s" % " ".join(argv))
    try:
        proc = subprocess.Popen(argv, cwd=REPO, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    except OSError as error:
        return False, "the editor could not be launched: %s" % error

    started = time.time()
    while time.time() - started < timeout_seconds:
        time.sleep(5)
        if proc.poll() is not None:
            return False, "the editor process exited after %ds (code %s)" % (
                int(time.time() - started), proc.returncode)
        try:
            probe = subprocess.run([sys.executable, CHECK_SERVER], capture_output=True, text=True, cwd=REPO)
        except OSError as error:
            return False, "the server probe could not run: %s" % error
        if "PASS" in (probe.stdout or ""):
            return True, "the MCP server answers with -SCCProvider=%s after %ds" % (
                provider, int(time.time() - started))
    return False, "the MCP server never answered within %ds" % timeout_seconds


def engine_readback(provider):
    """Drive the ENGINE's own `USourceControlHelpers` through a commandlet. (ok, values, detail)."""
    script = os.path.join(tempfile.gettempdir(), "tmcp-vcs-readback.py")
    result = os.path.join(tempfile.gettempdir(), "tmcp-vcs-readback-%s.txt" % (provider or "none"))
    with open(script, "w") as handle:
        handle.write(READBACK_SCRIPT)
    if os.path.exists(result):
        os.remove(result)

    env = dict(os.environ)
    env["TMCP_VCS_READBACK_RESULT"] = result
    env["TMCP_VCS_READBACK_ASSET"] = FIXTURE_FILE.replace("\\", "/")
    argv = [CMD_EXE, PROJECT, "-run=PythonScriptCommandlet",
            "-Script=" + script.replace("\\", "/"), "-unattended", "-nullrhi", "-culture=en"]
    if provider:
        argv.append("-SCCProvider=" + provider)
    try:
        done = subprocess.run(argv, capture_output=True, text=True, cwd=REPO, env=env, timeout=300)
    except (OSError, subprocess.TimeoutExpired) as error:
        return False, {}, "the commandlet could not be run: %s" % error

    if not os.path.exists(result):
        return False, {}, "the commandlet wrote no result (exit %s)" % done.returncode
    values = {}
    with open(result) as handle:
        for line in handle:
            if " = " in line and line.strip() != "done = True":
                key, _, value = line.partition(" = ")
                values[key.strip()] = value.strip()
    return True, values, "exit %s, %d value(s)" % (done.returncode, len(values))


def git_porcelain(path):
    """What the `git` CLI says is modified at `path` - A THIRD TOOL, used only to confirm nothing moved."""
    try:
        done = subprocess.run(["git", "status", "--porcelain", "--", path],
                              capture_output=True, text=True, cwd=REPO)
    except OSError as error:
        return "the git CLI could not be run: %s" % error
    return (done.stdout or "").strip()


def main():
    session = Session()
    session.connect()

    # =====================================================================================================
    # PHASE A: "SOURCE CONTROL OFF" - THE REFUSAL, PER ASSET. GUARANTEED RATHER THAN HOPED FOR.
    # =====================================================================================================
    print("--- the environment as handed to this suite ---")
    initial = session.call(READ_TOOLSET, "ReadTurboMCPVcsState", {"assets": ""})
    print(initial[:600])
    print()
    if field(initial, "IsEnabled()") == "true":
        print("    a provider is ALREADY selected, so the refusal branch would not be reached;")
        print("    restarting the editor WITHOUT a provider so the 'source control off' half runs anyway")
        tmcp_editor.stop_editor()
        started_ok, first = tmcp_editor.ensure_editor()
        print("    restart: %s (%s)" % (first, "ok" if started_ok else "FAILED"))
        session = Session()
        session.connect()

    print("--- phase A: state with no assets given ---")
    bare = session.call(READ_TOOLSET, "ReadTurboMCPVcsState", {"assets": ""})
    print(bare)
    print()

    check("the module facts are printed rather than assumed",
          field(bare, "module loaded") in ("true", "false"), bare[:300])
    check("it names the provider actually in use",
          field(bare, "provider in use") is not None, bare[:300])
    check("it reports the FIRST question as a measured value",
          field(bare, "IsEnabled()") in ("true", "false"), bare[:300])

    # *** `P1` IN ONE LINE: THE SECOND QUESTION WAS NOT ASKED, AND THE ANSWER SAYS SO RATHER THAN PRINTING
    # `false`, WHICH WOULD READ AS A MEASURED "the server is down". *** FAILS if the tool prints false here.
    check("with no provider, the SECOND question is reported as NOT ASKED rather than as false",
          field(bare, "IsEnabled()") != "false"
          or (field(bare, "IsAvailable()") or "").startswith("NOT ASKED"),
          "IsAvailable(): %r" % field(bare, "IsAvailable()"))
    check("and it names the engine's own refusal for every asset, up front",
          NO_PROVIDER_REFUSAL in bare, bare[:600])
    check("and it names WHERE that string comes from instead of implying it is this tool's own",
          "SourceControlHelpers.cpp:67" in bare, bare[:600])
    check("and it says no provider was reached at all",
          "NO PROVIDER WAS REACHED" in bare, bare[:900])

    print("--- phase A: state per asset, with no provider ---")
    state = session.call(READ_TOOLSET, "ReadTurboMCPVcsState", {"assets": ASSETS})
    print(state)
    print()

    blocks = rows(state)
    check("one row per asset, and no row is missing or duplicated",
          len(blocks) == 2, "%d row(s) for 2 assets: %s" % (len(blocks), [b[0] for b in blocks]))

    refusals = [refusal_of(body) for _, body in blocks]
    check("EVERY asset is refused by name", all(r for r in refusals), repr(refusals))
    check("and the refusal is THE ENGINE'S OWN STRING, verbatim",
          all(r == NO_PROVIDER_REFUSAL for r in refusals), repr(refusals))
    check("and each refusal says nothing was asked of a provider",
          all("NOTHING WAS ASKED OF ANY PROVIDER" in body for _, body in blocks),
          repr([b[0] for b in blocks]))

    # *** THIS IS THE CHECK THAT CATCHES THE `DefaultSourceControlProvider` TRAP: `GetState` returns a NULL
    # state there (`DefaultSourceControlProvider.cpp:52-60`), and a reader that folded it in would print
    # "under source control: false" for a file it never measured. `P1` says an absent measurement is not a
    # zero. ***
    check("and NO state is reported for any of them, so no false 'not under source control' can be read",
          not any(("under source control:" in body) or ("out of date:" in body) for _, body in blocks),
          repr([b for _, b in blocks]))

    # -------------------------------------------------------------------------------------------------
    # A NAME THAT RESOLVES TO NOTHING IS REFUSED BY NAME, AND THE TWO REFUSALS DIFFER.
    #
    # THIS IS THE PER-ASSET CHECK THAT DEPENDS ON NOTHING BUT THE INPUT - not on the git index, not on any
    # shared asset's state - so it holds in every environment.
    # -------------------------------------------------------------------------------------------------
    mixed = session.call(READ_TOOLSET, "ReadTurboMCPVcsState",
                         {"assets": "TA01_HandLaid,NoSuchAsset_TurboMCPVcsProbe"})
    print(mixed)
    print()
    mixed_rows = rows(mixed)
    check("both inputs get their own row, so the refusal is decided per asset",
          len(mixed_rows) == 2, "%d row(s): %s" % (len(mixed_rows), [b[0] for b in mixed_rows]))

    good = row_named(mixed, "TA01_HandLaid")
    bogus = row_named(mixed, "NoSuchAsset_TurboMCPVcsProbe")
    check("the unresolvable name is refused by name",
          bogus is not None and refusal_of(bogus) is not None, repr(bogus)[:300])
    check("and that refusal names the INPUT, so it cannot be mistaken for the provider's refusal",
          bogus is not None and "NoSuchAsset_TurboMCPVcsProbe" in (refusal_of(bogus) or ""),
          repr(refusal_of(bogus))[:300])
    check("and the resolvable asset is NOT given the resolver's refusal",
          good is not None and (refusal_of(good) or "") != (refusal_of(bogus) or ""),
          repr((refusal_of(good), refusal_of(bogus)))[:400])

    # -------------------------------------------------------------------------------------------------
    # THE REFUSAL IS IN `Plan`, SO THE PREVIEW AND THE EDIT TWIN REFUSE THE SAME INPUT IN THE SAME WORDS.
    # *** ALL THREE TOOLS RUN ONE PLAN, SO THE REFUSAL TEXT MUST BE IDENTICAL, NOT MERELY SIMILAR. ***
    # -------------------------------------------------------------------------------------------------
    print("--- phase A: the preview and the edit twin refuse identically ---")
    preview = session.call(PREVIEW_TOOLSET, "PreviewTurboMCPVcsCheckout", {"assets": ASSETS})
    edit = session.call(EDIT_TOOLSET, "EditTurboMCPVcsCheckout", {"assets": ASSETS})
    print(preview[:400])
    print("...")
    print(edit[:400])
    print()

    preview_refusals = [refusal_of(body) for _, body in rows(preview)]
    edit_refusals = [refusal_of(body) for _, body in rows(edit)]
    read_refusals = [refusal_of(body) for _, body in blocks]

    check("the preview refuses each asset with the SAME string the read tool used",
          preview_refusals == read_refusals, "%r vs %r" % (preview_refusals, read_refusals))
    check("the edit twin refuses each asset with the SAME string the read tool used",
          edit_refusals == read_refusals, "%r vs %r" % (edit_refusals, read_refusals))
    check("the preview states what a checkout would do, per asset",
          preview.count("WHAT A CHECKOUT WOULD DO") >= 2,
          "%d occurrence(s)" % preview.count("WHAT A CHECKOUT WOULD DO"))
    check("the edit twin attempted NOTHING and says so",
          "NO CHECKOUT WAS ATTEMPTED AND NO PROVIDER WAS REACHED" in edit, edit[-800:])
    check("and it did not report a provider answer it could not have obtained",
          "provider answer, verbatim" not in edit, edit[-600:])

    git_edit = ""
    git_state = ""

    # =====================================================================================================
    # PHASE B1: GIT - "WITH IT ON", AND THE CHECKOUT ANSWER THE PREVIOUS WORKSTREAM COULD NOT MEASURE.
    # =====================================================================================================
    print()
    print("--- phase B1: relaunching the editor with -SCCProvider=Git ---")
    tmcp_editor.stop_editor()
    launched, detail = launch_editor_with_provider("Git")
    print("    %s" % detail)
    check("THE EDITOR COMES UP WITH -SCCProvider=Git, so the 'with it on' half is MEASURED and not skipped",
          launched, detail)

    if launched:
        session = Session()
        session.connect()

        git_state = session.call(READ_TOOLSET, "ReadTurboMCPVcsState", {"assets": ASSETS})
        print(git_state)
        print()

        check("the provider in use is 'Git', so -SCCProvider= took effect",
              field(git_state, "provider in use") == "'Git'",
              repr(field(git_state, "provider in use")))
        check("and the FIRST question is now true",
              field(git_state, "IsEnabled()") == "true", repr(field(git_state, "IsEnabled()")))
        # *** THE SECOND QUESTION IS ASKED NOW, AND THAT IS THE POINT OF ASKING BOTH. ***
        check("and the SECOND question IS asked and is true, which is why both must be asked",
              field(git_state, "IsAvailable()") == "true",
              repr(field(git_state, "IsAvailable()")))
        check("and the registered provider list is printed, so the name is the registry's own answer",
              "registered providers" in git_state, git_state[:500])

        # *** USESCHECKOUT IS THE FACT THAT DECIDES WHAT A CHECKOUT MEANS, AND IT IS REPORTED. ***
        check("it reports UsesCheckout(), the fact that decides whether a checkout can mean anything",
              field(git_state, "UsesCheckout()") == "false",
              repr(field(git_state, "UsesCheckout()")))
        check("and it SAYS a checkout is a no-op on this provider instead of implying a lock was taken",
              "A CHECKOUT ON THIS PROVIDER IS A NO-OP" in git_state, git_state[:900])

        git_blocks = rows(git_state)
        check("the state IS reported per asset, now that a provider answers",
              len(git_blocks) == 2 and all("under source control:" in body for _, body in git_blocks),
              "%d row(s); %s" % (len(git_blocks), [b[0] for b in git_blocks]))
        check("and none of them is refused any more",
              all(refusal_of(body) is None for _, body in git_blocks),
              repr([refusal_of(b) for _, b in git_blocks]))
        check("and each row carries its OWN checked-out / out-of-date answer rather than one repeated line",
              all(("checked out here:" in body and "out of date:" in body) for _, body in git_blocks),
              repr([b[0] for b in git_blocks]))
        # *** TWO ASSETS, ONE REPORTED AS SOURCE CONTROLLED AND ONE NOT, IS THE PER-ASSET CLAIM MADE
        # OBSERVABLE. The check does not say WHICH is which, so it does not pin a state onto a shared asset -
        # it asserts only that the two rows were answered INDEPENDENTLY, which is the property in question. ***
        check("and the two rows are answered INDEPENDENTLY rather than given one repeated state",
              len(set(field(body, "under source control") for _, body in git_blocks)) == 2,
              repr([field(body, "under source control") for _, body in git_blocks]))

        print("--- phase B1: the checkout, through the provider ---")
        git_edit = session.call(EDIT_TOOLSET, "EditTurboMCPVcsCheckout", {"assets": ASSETS})
        print(git_edit)
        print()

        check("the checkout is ATTEMPTED through the provider, and its own answer is repeated",
              "provider answer, verbatim: ECommandResult::" in git_edit, git_edit[-900:])
        check("and the answer says a no-op was expected HERE rather than claiming a lock was taken",
              "A CHECKOUT IS A NO-OP ON THIS PROVIDER" in git_edit, git_edit[-1400:])
        check("and it repeats the PROVIDER'S OWN message, not a paraphrase of it",
              "not supported by revision control provider 'Git'" in git_edit, git_edit[-1400:])
        check("and it states that no file was written by the tool",
              "NO FILE WAS WRITTEN BY THIS TOOL" in git_edit, git_edit[-1400:])
        check("and the preview refused none of this input, so preview and edit still agree",
              all(refusal_of(body) is None
                  for _, body in rows(session.call(PREVIEW_TOOLSET, "PreviewTurboMCPVcsCheckout",
                                                   {"assets": ASSETS}))),
              git_state[:200])
    else:
        check("the Git checkout answer was obtained", False, detail)

    # =====================================================================================================
    # PHASE B2: PERFORCE - THE PROVIDER WHOSE `UsesCheckout() == true` IS WHY THIS CAPABILITY EXISTS.
    #
    # IT CANNOT ROUND-TRIP HERE (`p4 set` points at a port that does not answer), AND THAT IS EXACTLY WHAT
    # MAKES IT VALUABLE: `IsEnabled()` is TRUE and `IsAvailable()` is FALSE, which is the measured reason BOTH
    # questions must be asked. THIS IS THE PROVIDER THAT CATCHES A TOOL WHICH ASKS ONLY THE FIRST.
    # =====================================================================================================
    print()
    print("--- phase B2: relaunching the editor with -SCCProvider=Perforce ---")
    tmcp_editor.stop_editor()
    launched_p4, detail_p4 = launch_editor_with_provider("Perforce")
    print("    %s" % detail_p4)
    check("THE EDITOR COMES UP WITH -SCCProvider=Perforce, so the 'selected but unusable' half is MEASURED",
          launched_p4, detail_p4)

    if launched_p4:
        session = Session()
        session.connect()

        p4_state = session.call(READ_TOOLSET, "ReadTurboMCPVcsState", {"assets": FIXTURE})
        print(p4_state)
        print()

        # *** THE TWO ANSWERS THAT ARE THE WHOLE REASON BOTH QUESTIONS EXIST. ***
        check("with Perforce selected, the FIRST question is TRUE",
              field(p4_state, "IsEnabled()") == "true", repr(field(p4_state, "IsEnabled()")))
        check("AND THE SECOND IS FALSE, which is why a tool asking only the first would report a working provider",
              field(p4_state, "IsAvailable()") == "false", repr(field(p4_state, "IsAvailable()")))
        check("and it reports UsesCheckout() == true, the property the spec's own sentence names",
              field(p4_state, "UsesCheckout()") == "true", repr(field(p4_state, "UsesCheckout()")))

        p4_blocks = rows(p4_state)
        check("EVERY asset is refused with THE SECOND ENGINE STRING, verbatim",
              len(p4_blocks) == 1 and refusal_of(p4_blocks[0][1]) == NO_SERVER_REFUSAL,
              repr([refusal_of(b) for _, b in p4_blocks]))
        check("and that refusal is attributed to the engine's own line rather than to this tool",
              "SourceControlHelpers.cpp:76" in p4_state, p4_state[:700])
        check("and no state is reported, because asking for one would be reaching a provider that has none",
              not any("under source control:" in body for _, body in p4_blocks), p4_state[:900])

        p4_edit = session.call(EDIT_TOOLSET, "EditTurboMCPVcsCheckout", {"assets": FIXTURE})
        check("and the edit twin refuses the same input, attempting nothing",
              "NO CHECKOUT WAS ATTEMPTED AND NO PROVIDER WAS REACHED" in p4_edit, p4_edit[-700:])

    # =====================================================================================================
    # PHASE B3: THE SAME QUESTIONS, READ BACK THROUGH A DIFFERENT TOOL.
    #
    # *** A TOOL'S OWN REPORT IS NOT EVIDENCE. *** The engine's own `USourceControlHelpers` answers the same
    # two questions and performs the same checkout, through `SourceControlHelpers.cpp`, which is not this
    # tool's code. Every claim this row makes about a provider is checked against it, and the `git` CLI then
    # confirms that neither call touched the working tree.
    # =====================================================================================================
    print()
    print("--- phase B3: the engine's own helper, read back through a DIFFERENT tool ---")
    tmcp_editor.stop_editor()

    ok_none, none_values, detail_none = engine_readback(None)
    ok_git, git_values, detail_git = engine_readback("Git")
    print("    no provider : %s  %s" % (detail_none, none_values))
    print("    git         : %s  %s" % (detail_git, git_values))
    print()

    check("the engine's own helper could be driven at all, so the cross-check happened",
          ok_none and ok_git, "%s / %s" % (detail_none, detail_git))

    if ok_none and ok_git:
        check("the engine's own helper names the SAME provider this tool named, with no provider set",
              none_values.get("current_provider") == "'None'", repr(none_values.get("current_provider")))
        check("and it refuses with THE SAME STRING this tool answers per asset",
              none_values.get("last_error_msg") == 'Text("%s")' % NO_PROVIDER_REFUSAL,
              repr(none_values.get("last_error_msg")))
        check("and it confirms this tool's Git answer to BOTH questions, through a different code path",
              git_values.get("current_provider") == "'Git'"
              and git_values.get("is_enabled") == "True"
              and git_values.get("is_available") == "True",
              repr(git_values))

        # ---------------------------------------------------------------------------------------------
        # *** AND HERE IS THE FINDING THE CROSS-CHECK EXISTS FOR, AND IT IS A TRAP RATHER THAN A
        # DISAGREEMENT. ***
        #
        # The ENGINE's own `CheckOutFile` answers TRUE on Git - and it never issues a checkout at all.
        # `USourceControlHelpers::CheckOutFile` returns true at `SourceControlHelpers.cpp:456-460` when the
        # state says `IsCheckedOut() || IsAdded()`, and Git reports `IsCheckedOut() == true` for every tracked
        # file (the state this tool prints for the same asset says exactly that). So the engine's `true` means
        # "the state says it is already checked out", NOT "a lock was taken" - and on a provider with no lock
        # command, a write tool that read that `true` as "checked out, safe to write" would be relying on a
        # lock that does not exist.
        #
        # THIS TOOL GOES THROUGH `ISourceControlProvider::Execute` DIRECTLY, so it records the OPERATION'S
        # answer: `Failed`, with the provider's own "Operation 'CheckOut' not supported" message. THE TWO
        # ANSWERS ARE NOT IN CONFLICT - THEY ANSWER DIFFERENT QUESTIONS - AND THE SUITE ASSERTS BOTH SO THAT
        # NEITHER CAN BE MISTAKEN FOR THE OTHER.
        # ---------------------------------------------------------------------------------------------
        check("THE ENGINE'S OWN CheckOutFile ANSWERS TRUE ON GIT, with no error text at all",
              git_values.get("check_out_file_returned") == "True"
              and git_values.get("last_error_msg") == 'Text("")',
              repr(git_values))
        check("and THIS TOOL records that the operation is not supported, which is the answer the engine's "
              "helper never obtains because it returns before issuing it",
              "ECommandResult::Failed" in git_edit
              and "not supported by revision control provider 'Git'" in git_edit,
              git_edit[-900:])
        check("and the state this tool printed for that asset says it is checked out, which is the very "
              "condition the engine's helper returns true on - so the two answers are explained, not at odds",
              "checked out here:      true" in git_state, git_state[-1400:])

    # -------------------------------------------------------------------------------------------------
    # AND NOTHING MOVED. THE `git` CLI IS A THIRD TOOL, AND IT ANSWERS A QUESTION NEITHER OF THE OTHER TWO
    # CAN: DID ANY OF THIS TOUCH A FILE.
    # -------------------------------------------------------------------------------------------------
    dirty = git_porcelain(FIXTURE_FILE)
    check("NEITHER call touched the working tree: the git CLI reports the fixture unmodified",
          dirty == "", "git status --porcelain said: %r" % dirty)

    # =====================================================================================================
    # PHASE C: RESTORE WHAT THE SUITE WAS HANDED, AND ASSERT THE RESTORE.
    #
    # THE FIXTURE HERE IS THE EDITOR ITSELF, and the state to restore is "running, with no provider selected",
    # which is what the harness and every other suite expect. A relaunch through the shared helper is the
    # restore; the ASSERTION is the read-back, because a restore nobody checks is a restore nobody knows worked.
    # =====================================================================================================
    print()
    print("--- phase C: restoring the provider-less editor, and asserting it ---")
    tmcp_editor.stop_editor()
    restored_ok, first = tmcp_editor.ensure_editor()
    print("    restore: %s (%s)" % (first, "ok" if restored_ok else "FAILED"))
    check("the editor is back up after the provider experiments", restored_ok, first)

    if restored_ok:
        restored = Session()
        restored.connect()
        back = restored.call(READ_TOOLSET, "ReadTurboMCPVcsState", {"assets": ""})
        check("AND THE RESTORE IS ASSERTED: the editor is back to no provider selected",
              field(back, "IsEnabled()") == "false", repr(field(back, "IsEnabled()")))

    # -------------------------------------------------------------------------------------------------
    # WHAT THIS SUITE CANNOT MEASURE HERE, SAID RATHER THAN SMOOTHED OVER.
    # -------------------------------------------------------------------------------------------------
    print()
    print("NOT MEASURED BY THIS SUITE, AND IT IS AN ENVIRONMENT FACT RATHER THAN AN OVERSIGHT:")
    print("  - PLASTIC SCM: `cm` is not on PATH, so its AVAILABLE branch is unreachable here. Its name is")
    print("    also the quoting trap (`-SCCProvider=Plastic SCM` contains a space).")
    print("  - SUBVERSION: `svn` is not on PATH, so its AVAILABLE branch is unreachable here too.")
    print("  - what a checkout does against a LIVE depot: no provider on this machine has a server that")
    print("    answers, so a real lock was never taken by anyone.")

    print()
    if failures:
        print("RESULT: FAIL (%d of %d) -> %s" % (len(failures), checks_run[0], ", ".join(failures)))
        return 1
    print("RESULT: PASS (%d check(s) run, no check skipped)" % checks_run[0])
    return 0


if __name__ == "__main__":
    sys.exit(main())
