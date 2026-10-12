#!/usr/bin/env python3
# Copyright (c) 2026 FloStyle
# SPDX-License-Identifier: MPL-2.0
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.
"""
Counts the comments that narrate a source file's own past, AND PRINTS THE SCOPE IT COUNTED OVER.

WHY THIS EXISTS, AND WHY IT PRINTS ITS SCOPE. The audit that first counted these reported 42, and the
number was wrong for a reason that could not be seen in the number itself: it had READ 99 of the 186
source files and presented the total as though it had all of them. `4b77de05` records the correction to
81, and 81 was ALSO wrong by the time it was written, because the fixes in between had already removed
some of the sites.

    *** A COUNT TAKEN FROM A PARTIAL READ IS A COUNT OF THE PART THAT WAS READ. ***

So this prints the file count it scanned, the file count it skipped, and the roots it walked. A reader can
then tell "63 sites" from "63 sites IN THE 203 FILES UNDER Source/" without having to ask.

THE PATTERNS ARE THE UNAMBIGUOUS ONES, DELIBERATELY. A broad filter (`used to `, `no longer`) over-counts:
"an input that no longer points where the plan said" DESCRIBES THE ENGINE, and "its result is only used to
call something" IS ORDINARY ENGLISH. Neither is a note about the document's own past, and a grep cannot tell
them apart from one. These four can:

    `first version`       covers `the first version`, `THE FIRST VERSION'S`
    `earlier version`     covers `an earlier version`, `AN EARLIER VERSION OF THIS`
    `was tried and`       covers `WAS TRIED AND`, `was tried and crashed`
    `measured failure`    covers `MEASURED FAILURE`, `MEASURED FAILURE WITHOUT THEM`

THE SWEEP IS COMPLETE WHEN THIS PRINTS ZERO. Until then it exits 1, because a counter that reports a
non-zero number and exits 0 is a counter nobody runs twice.

USAGE
    python Tools/checks/count-history-comments.py
    python Tools/checks/count-history-comments.py --list      # one line per site
"""

import argparse
import os
import re
import sys

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

# THE ROOTS WALKED, STATED SO THE COUNT CANNOT BE READ AS A TOTAL OVER SOMETHING ELSE.
ROOTS = ["Source"]
SUFFIXES = (".cpp", ".h")

PATTERNS = [
    ("first version", re.compile(r"first version", re.IGNORECASE)),
    ("earlier version", re.compile(r"earlier version", re.IGNORECASE)),
    ("was tried and", re.compile(r"was tried and", re.IGNORECASE)),
    ("measured failure", re.compile(r"measured failure", re.IGNORECASE)),
]


def scanned():
    found, skipped = [], 0
    for root in ROOTS:
        base = os.path.join(REPO, root)
        for dirpath, _dirnames, filenames in os.walk(base):
            for name in sorted(filenames):
                if name.endswith(SUFFIXES):
                    found.append(os.path.join(dirpath, name))
                else:
                    skipped += 1
    return found, skipped


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--list", action="store_true", help="print one line per site")
    args = parser.parse_args()

    files, skipped = scanned()
    sites = []
    for path in files:
        with open(path, "r", encoding="utf-8", errors="replace") as handle:
            for number, line in enumerate(handle, start=1):
                for label, pattern in PATTERNS:
                    if pattern.search(line):
                        sites.append((path, number, label, line.strip()))
                        break

    print("SCOPE")
    print("  roots walked      %s" % ", ".join(ROOTS))
    print("  suffixes          %s" % ", ".join(SUFFIXES))
    print("  files scanned     %d" % len(files))
    print("  files in the tree that were NOT scanned   %d" % skipped)
    print()

    print("PATTERNS, EACH COUNTED SEPARATELY")
    for label, pattern in PATTERNS:
        count = sum(1 for _p, _n, hit, _t in sites if hit == label)
        print("  %-18s %4d" % (label, count))
    print()

    print("BY FILE")
    by_file = {}
    for path, _number, _label, _text in sites:
        by_file.setdefault(path, 0)
        by_file[path] += 1
    for path in sorted(by_file, key=lambda p: (-by_file[p], p)):
        print("  %4d  %s" % (by_file[path], os.path.relpath(path, REPO)))
    print()

    if args.list:
        print("SITES")
        for path, number, label, text in sorted(sites):
            print("  %s:%d  [%s]" % (os.path.relpath(path, REPO), number, label))
            print("      %s" % text)
        print()

    print("DISTINCT LINES: %d" % len(sites))
    if sites:
        print()
        print("RESULT: FAIL (%d site(s) still narrate the file's own past)" % len(sites))
        print("; the sweep is complete when this prints 0. A HISTORICAL NOTE IS NOT THE PROBLEM - what is")
        print("; wanted is the REASON it carries, stated as a rule about the engine or the code, because")
        print("; deleting the sentence without keeping the reason deletes the justification with it.")
        return 1

    print()
    print("RESULT: PASS (no comment in the scanned tree narrates the file's own past)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
