# ACCEPTANCE ROW THIS SUITE CLAIMS TO PROVE: NONE (an instrument over the tree, not a capability on a fixture)
"""`check-lookup-guards.py` reports 0 unguarded on the real tree, AND reports 1 on a tree that has one.

WHY THE SECOND HALF IS THE POINT
    The checker's first three runs each reported unguarded sites that were guarded - twelve, then three, then none.
    Every one was a false positive of the pattern, in the direction that would have produced edits to correct code.
    *** SO A RUN THAT SAYS "0 UNGUARDED" IS WORTH NOTHING UNTIL THE SAME RUN IS SHOWN TO SAY "1" WHEN THERE IS ONE. ***
    This suite writes both fixtures itself, so the failure path is exercised on every run rather than trusted.

WHAT IT CHECKS
    1. the real tree: exit 0, and a NON-ZERO site count - *** A CHECKER THAT FOUND NOTHING TO CHECK WOULD ALSO EXIT 0,
       AND THAT IS THE PASS IT MUST NOT BE ABLE TO GET. ***
    2. a fixture directory holding one unguarded lookup: exit 1, and the file name in its output.
    3. a fixture directory holding the same lookup with a guard: exit 0.
"""

import os
import pathlib
import shutil
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]
CHECKER = ROOT / "Tools" / "checks" / "check-lookup-guards.py"

UNGUARDED = """\
void F()
{
	UObject* Loaded = LoadObject<UObject>(nullptr, TEXT("/Game/Thing"));
	int32 Count = Loaded->GetNameLength();
}
"""

GUARDED = """\
void F()
{
	UObject* Loaded = LoadObject<UObject>(nullptr, TEXT("/Game/Thing"));
	if (Loaded == nullptr)
	{
		return;
	}
	int32 Count = Loaded->GetNameLength();
}
"""


def run(target):
    """The checker's return code and output for a directory."""
    done = subprocess.run(
        [sys.executable, str(CHECKER), str(target)],
        capture_output=True, text=True,
    )
    return done.returncode, done.stdout + done.stderr


def main():
    failures = []

    # 1. The real tree, and it must have found sites to check.
    code, out = run(ROOT / "Source" / "TurboMCP")
    if code != 0:
        failures.append("the real tree reports unguarded lookups:\n{0}".format(out))
    if "lookup sites checked: 0" in out:
        failures.append("the checker found no sites at all, so its clean report means nothing:\n{0}".format(out))
    print("real tree: exit {0}".format(code))
    for line in out.splitlines():
        if line.startswith(("lookup sites checked", "guarded:")):
            print("  {0}".format(line))

    # 2 and 3. The failure path and the pass path, on fixtures this suite writes.
    # DSH gives temporary child directories owner-only ACLs that block their own
    # grandchildren. Use two direct workspace directories instead of nesting fixtures.
    suffix = str(os.getpid())
    unguarded_dir = ROOT / (".lookup-guards-unguarded-" + suffix)
    guarded_dir = ROOT / (".lookup-guards-guarded-" + suffix)
    work = (unguarded_dir, guarded_dir)
    try:
        unguarded_dir.mkdir()
        guarded_dir.mkdir()
        (unguarded_dir / "Fixture.cpp").write_text(UNGUARDED, encoding="utf-8")
        (guarded_dir / "Fixture.cpp").write_text(GUARDED, encoding="utf-8")

        code, out = run(unguarded_dir)
        print("fixture with an unguarded lookup: exit {0}".format(code))
        if code != 1:
            failures.append("an unguarded lookup was NOT reported, so the check cannot fail:\n{0}".format(out))
        elif "Fixture.cpp:3" not in out:
            failures.append("the unguarded lookup was reported without its file and line:\n{0}".format(out))

        code, out = run(guarded_dir)
        print("fixture with the same lookup guarded: exit {0}".format(code))
        if code != 0:
            failures.append("a guarded lookup was reported as unguarded:\n{0}".format(out))
    finally:
        for directory in work:
            shutil.rmtree(directory, ignore_errors=True)

    if failures:
        print("")
        for problem in failures:
            print("  {0}".format(problem))
        print("RESULT: FAIL ({0} problem(s))".format(len(failures)))
        return 1

    print("RESULT: PASS (the real tree is clean, and a fixture with one unguarded lookup is not)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
