# ACCEPTANCE ROW THIS SUITE CLAIMS TO PROVE: T-VCS-1, and D-57's ordering decision.
#
#   | T-VCS-1 | Checkout | With source control off: the answer says so per asset. With it on: the state is
#   |         |          | reported per asset, a checkout is attempted through the provider, and the provider's own
#   |         |          | answer is repeated. A headless run of the same call records what the provider did, because
#   |         |          | that is the case that already failed once |
#
# VERIFIED VERBATIM against `Spec/06_ACCEPTANCE.md:265`. `Spec/05_DECISIONS.md:382-388` (`D-57`) is the ordering
# decision this row obeys: `E-VCS-CHECKOUT` is built and tested before any write tool calls it - row 10.1 did
# that - and this suite is the half that WIRES THE CALL IN.
"""`E-SAVE` and `E-STRUCT-FIELDS`: the automatic checkout a write tool performs before it writes.

=== THE TRAP THIS SUITE EXISTS TO KEEP OUT, MEASURED BY ROW 10.1 ===

`USourceControlHelpers::CheckOutFile` ANSWERS `True` ON GIT AND NEVER ISSUES A CHECKOUT. It returns early at
`SourceControlHelpers.cpp:456-460` when the provider's state says `IsCheckedOut() || IsAdded()`, and GIT
REPORTS `IsCheckedOut() == true` FOR EVERY TRACKED FILE. So a write tool wired to that boolean would read
"checked out, safe to write" on a provider whose `UsesCheckout() == false` - exactly the "git will not
[refuse], and will happily leave a modified file for someone else to find" of `Spec/03_TOOLS.md:1348-1351`.

**THE PROOF THAT THIS SUITE IS NOT SATISFIED BY THAT BOOLEAN IS IN PHASE B, AND IT IS TWO ANSWERS TO ONE
QUESTION, OBTAINED FROM TWO DIFFERENT CODE PATHS IN THE SAME RUN.** The engine's own helper is driven through
`PythonScriptCommandlet` and answers `True` with an EMPTY error text on Git. The write tool, in the same
editor, answers `ECommandResult::Failed` with the provider's own "Operation 'CheckOut' not supported by
revision control provider 'Git'". Those two answers cannot come from one call: the helper returns BEFORE
issuing the operation, so it can never see that message.

=== WHAT IS MEASURED, AND WHAT IS NOT ===

MEASURED HERE
  - with NO provider: the write tool states the ENGINE's own refusal, reaches no provider, and the write still
    happens. That is `D-57` obeyed - a source control problem must not become a write failure.
  - with GIT: the tool reports `UsesCheckout() == false`, SAYS a checkout is a NO-OP there rather than
    implying a lock was taken, ISSUES the operation anyway, and repeats the provider's own refusal verbatim.
    The write still happens.
  - the writes are real: the files are hashed before and after, and the edited field name is found in the
    SAVED `.uasset` bytes - a different tool from the write tool's own report.
  - the Preview and the Edit produce the SAME decision, `decision:` line for `decision:` line, in both phases.
  - the source of the mechanism never calls `CheckOutFile`, which is checked statically as well as by the
    two-answers argument above.

NOT MEASURED HERE, AND IT IS AN ENVIRONMENT FACT RATHER THAN AN OVERSIGHT
  - **NO CHECKOUT AGAINST A LIVE DEPOT HAS EVER BEEN PERFORMED, BY ANYONE, IN THIS PROJECT.** `p4 set` points
    at `ssl:example.invalid:1666`, which does not answer; `cm` and `svn` are not on `PATH`. So every checkout
    answer below is the answer to a REFUSAL or to a no-op provider. The claim this suite makes is
    "the operation is issued, and the provider's own answer is what is reported" - NOT "an automatic checkout
    works".
  - `EditTurboMCPStructNew` (the creator) is NOT wired: it creates a path that does not exist yet, so there is
    nothing to check out. That is stated rather than assumed, and it is one of the writers listed as unwired in
    the row 10.2 section of `Spec/WORKING_SCROW.md`.

=== THIS SUITE OWNS THE EDITOR, AND THAT IS SAID OUT LOUD ===

Reaching both the provider-less and the Git branch means RESTARTING the editor with `-SCCProvider=`, which is
the only mechanism that sets it (`SourceControlSettings.cpp:58-65`) and which `Tools/start-editor.ps1` cannot
pass through. So this suite stops and starts the editor twice and RESTORES a provider-less one at the end,
asserting the restore rather than assuming it. Under `Tools/run-suites.ps1` the harness is subordinate to a
healthy editor, so the restarts are this suite's own and are reported in its output.
"""

import hashlib
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

# THE FIXTURE IS BUILT BY THIS SUITE, per run, for two reasons that are both recorded in `test-struct.py`:
# `E-STRUCT-NEW` REFUSES an existing path, and a package that exists in the RUNNING EDITOR is not removed by
# deleting its `.uasset`. A run id makes every assertion below about THIS run's asset.
_RUN = str(os.getpid() % 100000)
PACKAGE = "/Game/Tests/TA22_VcsWrite_R" + _RUN
OBJECT = PACKAGE + ".TA22_VcsWrite_R" + _RUN
FILE = os.path.join(REPO, "TestProject", "Content", "Tests", "TA22_VcsWrite_R" + _RUN + ".uasset")
TESTS_DIR = os.path.dirname(FILE)
FIRST_FIELDS = "VcsProbeAlpha: int, VcsProbeBeta: float"

# THE ENGINE'S OWN REFUSAL STRING, reproduced verbatim from its own gate, written here so the suite fails if
# the tool ever paraphrases it. `SourceControlHelpers.cpp:67`.
NO_PROVIDER_REFUSAL = "Revision control is not enabled."

# The marker the tool prints, so the block is found by what the tool actually emits rather than by a guess.
BLOCK_FORMAT = "=== automatic checkout before the write (D-57): %s ==="

# The source of the mechanism, read statically by two checks below.
MECHANISM_H = os.path.join(REPO, "Source", "TurboMCP", "TurboMCPSourceControlWrite.h")
MECHANISM_CPP = os.path.join(REPO, "Source", "TurboMCP", "TurboMCPSourceControlWrite.cpp")
STRUCT_TOOLS = os.path.join(REPO, "Source", "TurboMCP", "TurboMCPStructTools.cpp")
SAVE_TOOL = os.path.join(REPO, "Source", "TurboMCP", "TurboMCPCompile.cpp")

# ---------------------------------------------------------------------------------------------------------
# THE CROSS-CHECK PROBE: THE ENGINE'S OWN `USourceControlHelpers`, THROUGH THE ENGINE'S OWN COMMANDLET.
#
# *** A TOOL'S OWN REPORT IS NOT EVIDENCE. *** This reaches `SourceControlHelpers.cpp:430-477`, which is not
# this project's code, and asks it the same question about the same file. Its answer is what makes the trap
# visible: `check_out_file` returns True while the operation this project issues is not supported at all.
#
# THE ANSWER GOES TO A FILE: `print` output from `PythonScriptCommandlet` does not survive a PowerShell
# redirect on this machine (measured by row 10.1's suite), so the value is written where it cannot be lost.
# ---------------------------------------------------------------------------------------------------------
READBACK_SCRIPT = '''"""Read the source control answer back through the ENGINE'S OWN Python surface."""
import os

import unreal

RESULT = os.environ["TMCP_VCSCW_READBACK_RESULT"]
ASSET = os.environ["TMCP_VCSCW_READBACK_ASSET"]

SC = getattr(unreal, "SourceControl", None)
with open(RESULT, "w") as handle:
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


def block_of(text, tool):
    """The automatic-checkout block for `tool`, or an empty string.

    The block is APPENDED by every wired tool, so taking from the marker to the end of the answer is the
    whole block. A tool that prepended it would make its own first word stop being `changed:`, which is
    checked separately and deliberately.
    """
    marker = BLOCK_FORMAT % tool
    index = text.find(marker)
    return text[index:] if index >= 0 else ""


def decisions(text):
    """Every `decision:` line of a block, in order.

    THIS IS THE PREVIEW/EDIT AGREEMENT CHECK, AND IT IS PARSED RATHER THAN MATCHED: a sentence could be
    reworded and still agree, so what is compared is the tool's own value on each line.
    """
    return re.findall(r"^\s+decision:\s*(.+?)\s*$", text, re.MULTILINE)


def file_hash(path):
    """The file's SHA-256, or None. A DIFFERENT TOOL from the write tool: the filesystem itself."""
    try:
        with open(path, "rb") as handle:
            return hashlib.sha256(handle.read()).hexdigest()
    except OSError:
        return None


def file_contains(path, needle):
    """Whether the SAVED asset's bytes carry a name. The check that cannot be faked by a report."""
    try:
        with open(path, "rb") as handle:
            return needle in handle.read()
    except OSError:
        return False


def git_porcelain(path):
    """What the `git` CLI says about one path - A THIRD TOOL - and whether it really answered.

    THE PATH IS MADE REPOSITORY-RELATIVE, and the exit code is returned too, so an empty answer that means
    "git could not be asked" cannot be mistaken for an empty answer that means "nothing changed".
    """
    rel = os.path.relpath(path, REPO).replace("\\", "/")
    try:
        done = subprocess.run(["git", "status", "--porcelain", "--", rel],
                              capture_output=True, text=True, cwd=REPO)
    except OSError as error:
        return None, "the git CLI could not be run: %s" % error
    if done.returncode != 0:
        return None, "git exited %s: %s" % (done.returncode, (done.stderr or "").strip()[:200])
    return (done.stdout or "").strip(), "git answered for %s" % rel


def read_source(path):
    try:
        with open(path, "r", encoding="utf-8", errors="replace") as handle:
            return handle.read()
    except OSError:
        return ""


def cleanup_fixtures():
    """Remove this suite's leftovers from THIS and any earlier run, on the way IN as well as out."""
    removed = []
    if not os.path.isdir(TESTS_DIR):
        return removed
    for name in sorted(os.listdir(TESTS_DIR)):
        if name.startswith("TA22_VcsWrite_R"):
            try:
                os.remove(os.path.join(TESTS_DIR, name))
                removed.append(name)
            except OSError:
                pass
    return removed


def launch_editor_with_provider(provider, timeout_seconds=240):
    """Start UnrealEditor with `-SCCProvider=<provider>` and wait for the MCP server. (ok, detail).

    `Tools/start-editor.ps1` TAKES NO PASS-THROUGH ARGUMENTS - row 10.1 recorded that as a fact about this
    repository - so the editor is launched directly with the same argv, and the switch is ONE ELEMENT of an
    argv list, which is also the fix for the `-SCCProvider=Plastic SCM` quoting trap (it contains a space).
    """
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
    script = os.path.join(tempfile.gettempdir(), "tmcp-vcswrite-readback.py")
    result = os.path.join(tempfile.gettempdir(), "tmcp-vcswrite-readback-%s.txt" % (provider or "none"))
    with open(script, "w") as handle:
        handle.write(READBACK_SCRIPT)
    if os.path.exists(result):
        os.remove(result)

    env = dict(os.environ)
    env["TMCP_VCSCW_READBACK_RESULT"] = result
    env["TMCP_VCSCW_READBACK_ASSET"] = FILE.replace("\\", "/")
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


def main():
    print("--- what this suite is, and the editor it owns ---")
    print("    IT RESTARTS THE EDITOR: once to reach -SCCProvider=Git, once to restore a")
    print("    provider-less one. Tools/start-editor.ps1 cannot pass -SCCProvider= (row 10.1).")
    print("    NOT MEASURED ANYWHERE HERE: a checkout against a LIVE depot. No provider on this")
    print("    machine has a server that answers, so every answer below is a refusal or a no-op.")
    print()

    leftovers = cleanup_fixtures()
    if leftovers:
        print("    (removed %d leftover fixture(s) on the way in: %s)"
              % (len(leftovers), ", ".join(leftovers)))

    ok_editor, editor_detail = tmcp_editor.ensure_editor()
    if not ok_editor:
        print("RESULT: FAIL (no editor to measure: %s)" % editor_detail)
        return 1
    session = Session()
    session.connect()

    # =====================================================================================================
    # PHASE A: NO PROVIDER - THE REFUSAL, AND THE WRITE THAT STILL HAPPENS.
    # =====================================================================================================
    print("--- phase A: the environment as handed to this suite ---")
    initial = session.call(READ_TOOLSET, "ReadTurboMCPVcsState", {"assets": ""})
    print(initial[:500])
    print()
    if field(initial, "IsEnabled()") == "true":
        print("    a provider is ALREADY selected, so the provider-less half would not be reached;")
        print("    restarting the editor WITHOUT a provider so that half runs anyway")
        tmcp_editor.stop_editor()
        started_ok, first = tmcp_editor.ensure_editor()
        print("    restart: %s (%s)" % (first, "ok" if started_ok else "FAILED"))
        session = Session()
        session.connect()

    print()
    print("--- phase A: the fixture, built by this suite through the newest writer's own creator ---")
    created = session.call(EDIT_TOOLSET, "EditTurboMCPStructNew",
                           {"path": PACKAGE, "fields": FIRST_FIELDS})
    print(created)
    print()
    check("the fixture struct was created", created.startswith("changed:"), created[:160])

    print()
    print("--- phase A: E-SAVE on a brand-new asset, which has NO FILE for source control yet ---")
    first_save = session.call(EDIT_TOOLSET, "EditTurboMCPSave", {"assets": PACKAGE})
    print(first_save)
    print()
    save_block = block_of(first_save, "E-SAVE")
    check("E-SAVE still opens with changed:, so the block was APPENDED and not prepended",
          first_save.startswith("changed:"), first_save[:120])
    check("E-SAVE still reports its own save count", "1 saved" in first_save, first_save[:200])
    check("E-SAVE's answer carries the automatic-checkout block, and it names E-SAVE",
          save_block != "", first_save[-600:])
    check("and that block names the policy taken out of the three options",
          "AttemptAndReport" in save_block and "(b) of the three options" in save_block,
          save_block[:900])
    check("and it says plainly that D-57 does not answer the UsesCheckout() == false question",
          "does NOT answer" in save_block, save_block[:1200])
    check("with no provider it states the ENGINE's own refusal, verbatim",
          NO_PROVIDER_REFUSAL in save_block, save_block[:900])
    check("and it says no provider was reached at all",
          "NO PROVIDER WAS REACHED" in save_block, save_block[:900])
    check("and it prints NO provider answer it could not have obtained",
          "provider answer, verbatim" not in save_block, save_block[-900:])
    check("and the block never says saved, so it cannot be confused with the save count",
          "saved" not in save_block.lower(), save_block[-900:])
    check("the file really was written, so the refusal did not gate the save",
          os.path.isfile(FILE), FILE)
    check("and the new asset's row says WHY nothing was asked, rather than an empty state",
          field(save_block, "run refusal") is not None
          and "decision: DO NOT ATTEMPT" in save_block, save_block[-1200:])

    # -------------------------------------------------------------------------------------------------
    # HASH BEFORE: this is the baseline every "did anything write?" check below is measured against, and
    # it is the FILESYSTEM answering, not the tool.
    # -------------------------------------------------------------------------------------------------
    hash_before_preview = file_hash(FILE)
    check("the fixture file is readable, so the hash checks below have something to read",
          hash_before_preview is not None, str(FILE))

    print()
    print("--- phase A: the Preview and the Edit must decide the SAME thing (all checks in Plan) ---")
    preview_a = session.call(PREVIEW_TOOLSET, "PreviewTurboMCPStructFields",
                             {"struct": OBJECT, "changes": "VcsProbeGamma: int"})
    print(preview_a)
    print()
    preview_a_block = block_of(preview_a, "P-STRUCT-FIELDS")
    check("P-STRUCT-FIELDS prints the block and names itself",
          preview_a_block != "", preview_a[-600:])
    check("and the preview says it reached no provider with a checkout",
          "THIS PREVIEW REACHED NO PROVIDER WITH A CHECKOUT" in preview_a_block,
          preview_a_block[-900:])
    check("and the preview did not write the file - the hash is unchanged",
          file_hash(FILE) == hash_before_preview, "%s vs %s" % (file_hash(FILE), hash_before_preview))

    edited_a = session.call(EDIT_TOOLSET, "EditTurboMCPStructFields",
                            {"struct": OBJECT, "changes": "VcsProbeGamma: int"})
    print(edited_a)
    print()
    edit_a_block = block_of(edited_a, "E-STRUCT-FIELDS")
    check("E-STRUCT-FIELDS still opens with changed:, so the block was APPENDED there too",
          edited_a.startswith("changed:"), edited_a[:120])
    check("E-STRUCT-FIELDS prints the block and names itself",
          edit_a_block != "", edited_a[-600:])
    check("*** THE PREVIEW AND THE EDIT DECIDE IDENTICALLY, decision line for decision line ***",
          decisions(preview_a_block) != []
          and decisions(preview_a_block) == decisions(edit_a_block),
          "preview %r vs edit %r" % (decisions(preview_a_block), decisions(edit_a_block)))
    check("and with no provider that shared decision is DO NOT ATTEMPT",
          all(d.startswith("DO NOT ATTEMPT") for d in decisions(edit_a_block)),
          repr(decisions(edit_a_block)))
    check("and the write tool says the refusal did NOT refuse the write",
          "NEVER GATES THE WRITE" in edit_a_block, edit_a_block[-900:])
    check("and no provider call was made for that asset",
          "no provider call was made for this asset" in edit_a_block, edit_a_block[-900:])
    check("and the checkout section wrote no file - only marked the package dirty - hash unchanged",
          file_hash(FILE) == hash_before_preview,
          "%s vs %s" % (file_hash(FILE), hash_before_preview))

    print()
    print("--- phase A: the write lands, proven by the FILE and not by the tool's report ---")
    second_save = session.call(EDIT_TOOLSET, "EditTurboMCPSave", {"assets": PACKAGE})
    print(second_save)
    print()
    check("the second save opens with changed:", second_save.startswith("changed:"), second_save[:120])
    check("the file's bytes CHANGED across that save, so the save is not the tool's word for it",
          file_hash(FILE) != hash_before_preview,
          "%s vs %s" % (file_hash(FILE), hash_before_preview))
    check("and the field this phase added is IN THE SAVED ASSET",
          file_contains(FILE, b"VcsProbeGamma"), FILE)
    git_before, git_before_detail = git_porcelain(FILE)
    check("the git CLI really answered for the fixture, so the comparison in phase B is not vacuous",
          git_before is not None, git_before_detail)
    print("    git status --porcelain for the fixture after phase A: %r (%s)"
          % (git_before, git_before_detail))

    # =====================================================================================================
    # PHASE B: GIT - `UsesCheckout() == false`, AND THE OPERATION ISSUED ANYWAY.
    # =====================================================================================================
    print()
    print("--- phase B: relaunching the editor with -SCCProvider=Git ---")
    tmcp_editor.stop_editor()
    launched, detail = launch_editor_with_provider("Git")
    print("    %s" % detail)
    check("THE EDITOR COMES UP WITH -SCCProvider=Git, so the 'with source control on' half is MEASURED",
          launched, detail)

    if not launched:
        check("the Git phase produced a result at all", False, detail)
        print()
        print("RESULT: FAIL (%d of %d) -> %s" % (len(failures), checks_run[0], ", ".join(failures)))
        return 1

    session = Session()
    session.connect()
    git_facts = session.call(READ_TOOLSET, "ReadTurboMCPVcsState", {"assets": ""})
    print(git_facts[:500])
    print()
    check("the provider in use is 'Git', so the relaunch took effect (read through E-VCS-CHECKOUT)",
          field(git_facts, "provider in use") == "'Git'",
          repr(field(git_facts, "provider in use")))

    hash_before_git_edit = file_hash(FILE)
    preview_g = session.call(PREVIEW_TOOLSET, "PreviewTurboMCPStructFields",
                             {"struct": OBJECT, "changes": "VcsProbeDelta: int"})
    print(preview_g)
    print()
    preview_g_block = block_of(preview_g, "P-STRUCT-FIELDS")
    check("the preview's block reports the provider it actually found",
          field(preview_g_block, "provider in use") == "'Git'",
          repr(field(preview_g_block, "provider in use")))
    check("and the preview ISSUED NO checkout: the file hash is unchanged",
          file_hash(FILE) == hash_before_git_edit,
          "%s vs %s" % (file_hash(FILE), hash_before_git_edit))

    print()
    print("--- phase B: THE EDIT, WITH A PROVIDER WHOSE UsesCheckout() IS FALSE ---")
    edited_g = session.call(EDIT_TOOLSET, "EditTurboMCPStructFields",
                            {"struct": OBJECT, "changes": "VcsProbeDelta: int"})
    print(edited_g)
    print()
    git_block = block_of(edited_g, "E-STRUCT-FIELDS")
    check("*** THE WRITE STILL HAPPENED: a failed checkout did NOT fail the write (D-57) ***",
          edited_g.startswith("changed:"), edited_g[:200])
    check("and the block reports UsesCheckout() == false, the fact that decides what a checkout means",
          field(git_block, "UsesCheckout()") == "false", repr(field(git_block, "UsesCheckout()")))
    check("and it SAYS a checkout is a no-op on this provider instead of implying a lock was taken",
          "A CHECKOUT IS A NO-OP ON THIS PROVIDER" in git_block, git_block[:1400])
    check("AND THE OPERATION WAS STILL ISSUED, with the provider's own answer repeated",
          "provider answer, verbatim: ECommandResult::Failed" in git_block, git_block[-1200:])
    check("and the provider's OWN message is repeated, not paraphrased",
          "not supported by revision control provider 'Git'" in git_block, git_block[-1200:])
    check("and it never claims a lock was taken - the input that fails this is that sentence going away",
          "nothing here is a claim that a lock was taken" in git_block, git_block[-1600:])
    check("*** THE PREVIEW AND THE EDIT AGREE UNDER A PROVIDER TOO ***",
          decisions(preview_g_block) != [] and decisions(preview_g_block) == decisions(git_block),
          "preview %r vs edit %r" % (decisions(preview_g_block), decisions(git_block)))
    check("and the checkout section wrote no file: the hash is unchanged across it",
          file_hash(FILE) == hash_before_git_edit,
          "%s vs %s" % (file_hash(FILE), hash_before_git_edit))

    print()
    print("--- phase B: E-SAVE, and the file write that follows the failed checkout ---")
    git_save = session.call(EDIT_TOOLSET, "EditTurboMCPSave", {"assets": PACKAGE})
    print(git_save)
    print()
    git_save_block = block_of(git_save, "E-SAVE")
    check("E-SAVE opens with changed: with Git selected too", git_save.startswith("changed:"),
          git_save[:160])
    check("and its block carries the provider's own refusal",
          "not supported by revision control provider 'Git'" in git_save_block,
          git_save_block[-1200:])
    check("and the FILE changed across that save, so the write is the filesystem's answer",
          file_hash(FILE) != hash_before_git_edit,
          "%s vs %s" % (file_hash(FILE), hash_before_git_edit))
    check("and the field this phase added is IN THE SAVED ASSET",
          file_contains(FILE, b"VcsProbeDelta"), FILE)

    # -------------------------------------------------------------------------------------------------
    # *** THE TRAP, MEASURED IN ONE RUN: TWO ANSWERS TO ONE QUESTION FROM TWO CODE PATHS. ***
    # -------------------------------------------------------------------------------------------------
    print()
    print("--- phase B: THE CROSS-CHECK - the engine's OWN helper, read back through a DIFFERENT tool ---")
    tmcp_editor.stop_editor()
    ok_git, git_values, detail_git = engine_readback("Git")
    print("    git : %s  %s" % (detail_git, git_values))
    print()
    check("the engine's own helper could be driven, so the cross-check happened", ok_git, detail_git)
    if ok_git:
        check("THE ENGINE'S OWN CheckOutFile ANSWERS TRUE ON GIT, with no error text at all",
              git_values.get("check_out_file_returned") == "True"
              and git_values.get("last_error_msg") == 'Text("")', repr(git_values))
        check("while THIS project's write tool, on the same provider in the same run, recorded "
              "ECommandResult::Failed with the provider's 'not supported' message",
              "ECommandResult::Failed" in git_block
              and "not supported by revision control provider 'Git'" in git_block,
              git_block[-1200:])
        check("so the two answers CANNOT come from one call: the helper returns before issuing the "
              "operation it would have to issue to see that message",
              git_values.get("check_out_file_returned") == "True"
              and "not supported" in git_block,
              "helper %r vs tool %r" % (git_values.get("check_out_file_returned"),
                                        "not supported" in git_block))

    git_after, git_after_detail = git_porcelain(FILE)
    check("NOTHING ELSE MOVED: the git CLI reports the same thing for the fixture after phase B",
          git_after is not None and git_after == git_before,
          "%r vs %r (%s)" % (git_after, git_before, git_after_detail))

    # -------------------------------------------------------------------------------------------------
    # AND THE MECHANISM ITSELF, CHECKED STATICALLY SO THE TRAP CANNOT COME BACK UNNOTICED.
    # Each of these has one named input that makes it fail, given with it.
    # -------------------------------------------------------------------------------------------------
    print()
    print("--- the mechanism, read as source, so the trap is excluded by construction ---")
    mechanism_text = read_source(MECHANISM_H) + read_source(MECHANISM_CPP)
    check("it never CALLS CheckOutFile - the input that fails this is any 'CheckOutFile(' in it",
          re.search(r"CheckOutFile\s*\(", mechanism_text) is None,
          repr(re.search(r".{40}CheckOutFile\s*\(.{40}", mechanism_text)))
    check("it reads the capability flag instead - the input that fails this is removing UsesCheckout()",
          "UsesCheckout()" in mechanism_text, "UsesCheckout() absent")
    check("and it reads the OPERATION's own result - the input that fails this is removing Execute(",
          "Execute(" in mechanism_text and "GetResultInfo()" in mechanism_text,
          "Execute( or GetResultInfo() absent")
    check("it names all three options and which one was taken - the input is dropping that block",
          "(a) SKIP IT, AND SAY YOU SKIPPED." in mechanism_text
          and "(b) ATTEMPT IT AND REPORT THE PROVIDER ANSWER." in mechanism_text
          and "(c) REFUSE THE WRITE UNLESS THE CALLER OPTS IN." in mechanism_text
          and "*** CHOSEN ***" in mechanism_text,
          "the three-option block or the CHOSEN marker is missing from the mechanism")
    check("BOTH wired write tools call it - the input is removing either call",
          "EnsureCheckedOut" in read_source(STRUCT_TOOLS)
          and "EnsureCheckedOut" in read_source(SAVE_TOOL),
          "%s / %s" % ("EnsureCheckedOut" in read_source(STRUCT_TOOLS),
                       "EnsureCheckedOut" in read_source(SAVE_TOOL)))
    check("and the struct writer's PREVIEW does not reach a provider with a checkout",
          "EnsureCheckedOut" not in read_source(STRUCT_TOOLS).split("PreviewTurboMCPStructFields")[-1]
          .split("EditTurboMCPStructFields")[0],
          "the preview body calls EnsureCheckedOut")

    # =====================================================================================================
    # PHASE C: RESTORE WHAT THE SUITE WAS HANDED, AND ASSERT THE RESTORE.
    # =====================================================================================================
    print()
    print("--- phase C: restoring the provider-less editor, and asserting it ---")
    tmcp_editor.stop_editor()
    restored_ok, restored_detail = tmcp_editor.ensure_editor()
    print("    restore: %s (%s)" % (restored_detail, "ok" if restored_ok else "FAILED"))
    check("the editor is back up after the provider experiment", restored_ok, restored_detail)
    if restored_ok:
        restored = Session()
        restored.connect()
        back = restored.call(READ_TOOLSET, "ReadTurboMCPVcsState", {"assets": ""})
        check("AND THE RESTORE IS ASSERTED: no provider is selected again",
              field(back, "IsEnabled()") == "false", repr(field(back, "IsEnabled()")))

    removed = cleanup_fixtures()
    print()
    print("    (removed %d fixture file(s) on the way out)" % len(removed))
    check("this suite's fixture is gone, so the next run meets a clean path",
          not os.path.isfile(FILE), FILE)

    print()
    print("NOT MEASURED BY THIS SUITE, AND IT IS AN ENVIRONMENT FACT RATHER THAN AN OVERSIGHT:")
    print("  - A CHECKOUT AGAINST A LIVE DEPOT: no provider on this machine has a server that answers")
    print("    (`p4 set` points at ssl:example.invalid:1666, which does not answer; `cm` and `svn` are not")
    print("    on PATH). Every answer above is the answer to a REFUSAL or to a no-op provider, and NO")
    print("    CLAIM IS MADE THAT AN AUTOMATIC CHECKOUT WORKS.")
    print("  - E-STRUCT-NEW IS NOT WIRED: it creates a path that does not exist yet, so there is nothing")
    print("    for source control to have an opinion about. The writers left unwired are listed in the")
    print("    row 10.2 section of Spec/WORKING_SCROW.md.")

    print()
    if failures:
        print("RESULT: FAIL (%d of %d) -> %s" % (len(failures), checks_run[0], ", ".join(failures)))
        return 1
    print("RESULT: PASS (%d check(s) run, no check skipped)" % checks_run[0])
    return 0


if __name__ == "__main__":
    sys.exit(main())
