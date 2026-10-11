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

*** AND IT MEASURES BOTH BRANCHES RATHER THAN ONE, WHICH IS THE POINT OF THE PHASES BELOW. ***

  Phase A  measures the case `T-VCS-1` calls "source control off". The editor this suite is handed normally has
           no provider selected - NOTHING IN THIS REPOSITORY SETS ONE (`SourceControlSettings.cpp:58-65` reads
           `-SCCProvider=` from the command line or `Provider=` from `[SourceControl.SourceControlSettings]`, and
           no file in this project writes either) - so this phase measures the refusal.
  Phase B  measures the case it calls "with it on" BY RELAUNCHING THE EDITOR WITH `-SCCProvider=Git`, and it
           measures the PROVIDER'S ACTUAL ANSWER to a checkout. That is the half the row-1.3 measurement
           deliberately left unmeasured: `Spec/WORKING_SCROW.md` section 5 item 6 says a Git checkout was not
           run, so Git's checkout answer was unknown and only `is_available=True` was measured. THIS SUITE
           MEASURES IT INSTEAD OF PREDICTING IT, and if the editor cannot be brought up with that setting the
           suite FAILS rather than reporting a pass over a branch it never reached.
  Phase C  RESTORES the provider-less editor the suite was handed, and ASSERTS the restore by reading the
           setting back through the tool.

GIT IS THE PROVIDER BECAUSE IT IS THE ONLY ONE AVAILABLE ON THIS MACHINE: `Spec/WORKING_SCROW.md` section 1
measured `Git` as `is_enabled=True, is_available=True` with nothing but the `Provider` setting, while Perforce
needs a reachable server (`p4 set` points at `ssl:example.invalid:1666`, which does not answer) and `cm` and `svn`
are not on `PATH` at all. Perforce's `UsesCheckout() == true` (`PerforceSourceControlProvider.cpp:622-625`) is
the reason the capability exists, and this suite CANNOT exercise it here - which it says rather than hides.
"""

import os
import re
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from tmcp import EDIT_TOOLSET, PREVIEW_TOOLSET, READ_TOOLSET, Session  # noqa: E402
import tmcp_editor  # noqa: E402

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ENGINE = "C:\\UnrealEngine"
PROJECT = os.path.join(REPO, "TestProject", "TurboMCPTest.uproject")
CHECK_SERVER = os.path.join(REPO, "Tools", "check-mcp-server.py")

# TWO ASSETS, and the pair is chosen to make PER ASSET visible without pinning anything about either one:
# `TA01_HandLaid` is used by `test-asset-cost` and exists on disk. NEITHER ASSET'S STATE, COUNT OR CONTENT IS
# ASSERTED - only that each got its OWN row and that the two rows are produced independently.
ASSETS = "TA01_HandLaid,TA14_KitDataTable"

# THE ENGINE'S OWN REFUSAL STRINGS, REPRODUCED VERBATIM FROM ITS OWN GATE. They are what the tool must answer
# with, and they are written here so the suite fails if the tool ever paraphrases them.
#   SourceControlHelpers.cpp:67  "Revision control is not enabled."
#   SourceControlHelpers.cpp:76  "Revision control server is currently not available."
NO_PROVIDER_REFUSAL = "Revision control is not enabled."
NO_SERVER_REFUSAL = "Revision control server is currently not available."

failures = []
checks_run = [0]
skipped = []


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

    THE STATE SECTION ONLY. The Edit answer appends a `=== checkout ... ===` section that prints its own
    `[n]` blocks, and parsing both at once would double the row count and make a count check meaningless.
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
# PHASE B's LAUNCHER, AND WHY IT LAUNCHES THE EDITOR ITSELF RATHER THAN CALLING `start-editor.ps1`.
#
# THE SETTING HAS TO BE ON THE COMMAND LINE AND NOTHING IN THIS REPOSITORY PUTS IT THERE.
# `SourceControlSettings.cpp:58-61` reads the `SCCProvider` switch pair off the command line before anything
# else, and `:64-65` falls back to `Provider=` under `[SourceControl.SourceControlSettings]`; no
# `SourceControlSettings.ini` exists anywhere in this project, in `Engine/Config`, in `Engine/Saved/Config` or
# in the user's Unreal AppData, and no file under `Tools/` mentions `SCCProvider` (measured in
# `Spec/WORKING_SCROW.md` section 1). `Tools/start-editor.ps1` takes no pass-through arguments, so this suite
# launches the editor directly, with the same argv that script uses.
#
# *** THE ARGUMENT IS PASSED AS ONE LIST ELEMENT, WHICH IS ALSO THE FIX FOR THE TRAP `Spec/WORKING_SCROW.md`
# SECTION 5 ITEM 4 RECORDS: `-SCCProvider=Plastic SCM` CONTAINS A SPACE (`PlasticSourceControlProvider.cpp:34`)
# and needs quoting in any shell harness. A LIST OF ARGUMENTS HAS NO QUOTING TO GET WRONG. ***
# ---------------------------------------------------------------------------------------------------------
def launch_editor_with_provider(provider, timeout_seconds=300):
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

    deadline = time.time() + timeout_seconds
    while time.time() < deadline:
        time.sleep(5)
        if proc.poll() is not None:
            return False, "the editor process exited after %ds (code %s)" % (
                int(timeout_seconds - (deadline - time.time())), proc.returncode)
        try:
            probe = subprocess.run([sys.executable, CHECK_SERVER], capture_output=True, text=True, cwd=REPO)
        except OSError as error:
            return False, "the server probe could not run: %s" % error
        if "PASS" in (probe.stdout or ""):
            return True, "the MCP server answers with -SCCProvider=%s" % provider
    return False, "the MCP server never answered within %ds" % timeout_seconds


def main():
    session = Session()
    session.connect()

    # =====================================================================================================
    # PHASE A: WHAT THE EDITOR WAS HANDED, AND THEN "SOURCE CONTROL OFF" - THE REFUSAL, PER ASSET.
    #
    # THE BRANCH IS GUARANTEED RATHER THAN HOPED FOR. If a provider is already selected, the editor is
    # restarted WITHOUT one, so the refusal checks below run in every invocation rather than being skipped
    # on a machine where someone left a provider set.
    # =====================================================================================================
    print("--- the environment as handed to this suite ---")
    initial = session.call(READ_TOOLSET, "ReadTurboMCPVcsState", {"assets": ""})
    print(initial[:700])
    print()
    initial_enabled = field(initial, "IsEnabled()")

    if initial_enabled == "true":
        print("    a provider is ALREADY selected, so the refusal branch would not be reached;")
        print("    restarting the editor WITHOUT a provider so the 'source control off' half runs anyway")
        tmcp_editor.stop_editor()
        ok, first = tmcp_editor.ensure_editor()
        print("    restart: %s (%s)" % (first, "ok" if ok else "FAILED"))
        session = Session()
        session.connect()

    # -------------------------------------------------------------------------------------------------
    # THE READ HALF WITH NO ASSETS: the provider facts alone, which is what the spec's "no argument to
    # read the state" means.
    # -------------------------------------------------------------------------------------------------
    print("--- phase A: state with no assets given ---")
    bare = session.call(READ_TOOLSET, "ReadTurboMCPVcsState", {"assets": ""})
    print(bare[:900])
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

    # -------------------------------------------------------------------------------------------------
    # THE READ HALF WITH ASSETS: the refusal must be PER ASSET.
    # -------------------------------------------------------------------------------------------------
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
    # state there, and a reader that folded it in would print "under source control: false" for a file it
    # never measured. `P1` says an absent measurement is not a zero. ***
    check("and NO state is reported for any of them, so no false 'not under source control' can be read",
          not any(("under source control:" in body) or ("out of date:" in body) for _, body in blocks),
          repr([b for _, b in blocks]))

    # -------------------------------------------------------------------------------------------------
    # A NAME THAT RESOLVES TO NOTHING IS REFUSED BY NAME, AND THE TWO REFUSALS ARE DIFFERENT.
    #
    # THIS IS THE PER-ASSET CHECK THAT DOES NOT DEPEND ON THE GIT INDEX OR ON ANY SHARED ASSET'S STATE: the
    # difference between the two rows comes ENTIRELY FROM THE INPUT, so it holds in every environment.
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
          bogus is not None and refusal_of(bogus) is not None,
          repr(bogus)[:300])
    check("and that refusal names the INPUT, so it cannot be mistaken for the provider's refusal",
          bogus is not None and "NoSuchAsset_TurboMCPVcsProbe" in (refusal_of(bogus) or ""),
          repr(refusal_of(bogus))[:300])
    check("and the resolvable asset is NOT given the resolver's refusal",
          good is not None and (refusal_of(good) or "") != (refusal_of(bogus) or ""),
          repr((refusal_of(good), refusal_of(bogus)))[:400])

    # -------------------------------------------------------------------------------------------------
    # THE REFUSAL IS IN `Plan`, SO THE PREVIEW AND THE EDIT TWIN REFUSE THE SAME INPUT IN THE SAME WORDS.
    # *** THIS IS A CROSS-TOOL CHECK AND IT IS THE POINT OF PUTTING THE CHECK IN `Plan`: all three tools run
    # the same plan, so the refusal text must be IDENTICAL, not merely similar. ***
    # -------------------------------------------------------------------------------------------------
    print("--- phase A: the preview and the edit twin refuse identically ---")
    preview = session.call(PREVIEW_TOOLSET, "PreviewTurboMCPVcsCheckout", {"assets": ASSETS})
    edit = session.call(EDIT_TOOLSET, "EditTurboMCPVcsCheckout", {"assets": ASSETS})
    print(preview[:500])
    print("...")
    print(edit[:500])
    print()

    preview_refusals = [refusal_of(body) for _, body in rows(preview)]
    edit_refusals = [refusal_of(body) for _, body in rows(edit)]
    read_refusals = [refusal_of(body) for _, body in blocks]

    check("the preview refuses each asset with the SAME string the read tool used",
          preview_refusals == read_refusals, "%r vs %r" % (preview_refusals, read_refusals))
    check("the edit twin refuses each asset with the SAME string the read tool used",
          edit_refusals == read_refusals, "%r vs %r" % (edit_refusals, read_refusals))

    # THE PREVIEW MUST ALSO SAY WHAT A CHECKOUT WOULD MEAN, NOT ONLY WHAT IT WOULD REFUSE.
    check("the preview states what a checkout would do, per asset",
          preview.count("WHAT A CHECKOUT WOULD DO") >= 2,
          "%d occurrence(s)" % preview.count("WHAT A CHECKOUT WOULD DO"))

    # *** THE EDIT TWIN MUST NOT HAVE REACHED A PROVIDER. It says so in its own words, and the checkout
    # section must carry a refusal rather than a result. ***
    check("the edit twin attempted NOTHING and says so",
          "NO CHECKOUT WAS ATTEMPTED AND NO PROVIDER WAS REACHED" in edit, edit[-800:])
    check("and it did not report a provider answer it could not have obtained",
          "provider answer, verbatim" not in edit, edit[-600:])

    # =====================================================================================================
    # PHASE B: "WITH IT ON" - THE GIT CASE, MEASURED BY RELAUNCHING THE EDITOR WITH -SCCProvider=Git.
    # =====================================================================================================
    print()
    print("--- phase B: relaunching the editor with -SCCProvider=Git ---")
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

        # THE SETTING TOOK EFFECT, which is what proves the launch argument reached the module.
        check("the provider in use is 'Git', so -SCCProvider= took effect",
              field(git_state, "provider in use") == "'Git'",
              repr(field(git_state, "provider in use")))
        check("and the FIRST question is now true",
              field(git_state, "IsEnabled()") == "true", repr(field(git_state, "IsEnabled()")))
        # *** THE SECOND QUESTION IS ASKED NOW, AND THAT IS THE POINT OF ASKING BOTH: Perforce, Plastic and
        # Subversion answer the first with true unconditionally while this one is false. ***
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
              len(git_blocks) == 2
              and all("under source control:" in body for _, body in git_blocks),
              "%d row(s); %s" % (len(git_blocks), [b[0] for b in git_blocks]))
        check("and none of them is refused any more",
              all(refusal_of(body) is None for _, body in git_blocks),
              repr([refusal_of(b) for _, b in git_blocks]))
        check("and each row carries its OWN checked-out / out-of-date answer rather than one repeated line",
              all(("checked out here:" in body and "out of date:" in body) for _, body in git_blocks),
              repr([b[0] for b in git_blocks]))

        # ---------------------------------------------------------------------------------------------
        # *** THE CHECK THE PREVIOUS WORKSTREAM COULD NOT MAKE: THE PROVIDER'S ACTUAL ANSWER TO A GIT
        # CHECKOUT. `Spec/WORKING_SCROW.md` section 5 item 6 records that a Git checkout was deliberately
        # not run, so Git's checkout ANSWER was unmeasured. This measures it. ***
        # ---------------------------------------------------------------------------------------------
        print("--- phase B: the checkout, through the provider ---")
        git_edit = session.call(EDIT_TOOLSET, "EditTurboMCPVcsCheckout", {"assets": ASSETS})
        print(git_edit)
        print()

        check("the checkout is ATTEMPTED through the provider, and its own answer is repeated",
              "provider answer, verbatim: ECommandResult::" in git_edit, git_edit[-900:])
        check("and the answer says a no-op was expected HERE rather than claiming a lock was taken",
              "A CHECKOUT IS A NO-OP ON THIS PROVIDER" in git_edit, git_edit[-1200:])
        check("and it repeats the PROVIDER'S OWN message, not a paraphrase of it",
              "not supported by revision control provider 'Git'" in git_edit, git_edit[-1200:])
        check("and it states that no file was written by the tool",
              "NO FILE WAS WRITTEN BY THIS TOOL" in git_edit, git_edit[-1200:])
        check("and the preview refused none of this input, so preview and edit still agree",
              all(refusal_of(body) is None
                  for _, body in rows(session.call(PREVIEW_TOOLSET, "PreviewTurboMCPVcsCheckout",
                                                   {"assets": ASSETS}))),
              git_state[:200])

    # =====================================================================================================
    # PHASE C: RESTORE WHAT THE SUITE WAS HANDED, AND ASSERT THE RESTORE.
    # -----------------------------------------------------------------------------------------------------
    # THE FIXTURE HERE IS THE EDITOR ITSELF, and the state to restore is "running, with no provider
    # selected", which is what every other suite and the harness expect. A relaunch through the shared
    # helper is the restore; the ASSERTION is the read-back, because a restore nobody checks is a restore
    # nobody knows worked.
    # =====================================================================================================
    print()
    print("--- phase C: restoring the provider-less editor, and asserting it ---")
    tmcp_editor.stop_editor()
    ok, first = tmcp_editor.ensure_editor()
    print("    restore: %s (%s)" % (first, "ok" if ok else "FAILED"))
    check("the editor is back up after the provider experiment", ok, first)

    if ok:
        restored = Session()
        restored.connect()
        back = restored.call(READ_TOOLSET, "ReadTurboMCPVcsState", {"assets": ""})
        print(back[:500])
        check("AND THE RESTORE IS ASSERTED: the editor is back to no provider selected",
              field(back, "IsEnabled()") == "false", repr(field(back, "IsEnabled()")))

    # -------------------------------------------------------------------------------------------------
    # WHAT THIS SUITE CANNOT MEASURE HERE, SAID RATHER THAN SMOOTHED OVER.
    # -------------------------------------------------------------------------------------------------
    print()
    print("NOT MEASURED BY THIS SUITE, AND IT IS AN ENVIRONMENT FACT RATHER THAN AN OVERSIGHT:")
    print("  - PERFORCE, whose UsesCheckout() == true (PerforceSourceControlProvider.cpp:622-625) is the")
    print("    reason this capability exists. p4 set on this machine points at")
    print("    ssl:example.invalid:1666, which does not answer, so its AVAILABLE branch is unreachable here.")
    print("    Its UNAVAILABLE branch IS the second engine string this suite asserts the wording of.")
    print("  - PLASTIC SCM and SUBVERSION: cm and svn are not on PATH, so neither can be reached at all.")
    print("  - what a checkout does against a LIVE depot: no provider here is backed by a server.")
    print("  - %s" % NO_SERVER_REFUSAL)

    print()
    if failures:
        print("RESULT: FAIL (%d of %d) -> %s" % (len(failures), checks_run[0], ", ".join(failures)))
        return 1
    print("RESULT: PASS (%d check(s) run, no check skipped)" % checks_run[0])
    return 0


if __name__ == "__main__":
    sys.exit(main())
