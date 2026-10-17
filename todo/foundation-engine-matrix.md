# Establish installed-engine dependency and packaging matrix

ID: foundation-engine-matrix
Status: todo
Priority: P1
Readiness: ready
Owner: unassigned

## Problem
The foundation needs a verified implementation contract for this boundary.

## Scope
Inspect exact installed 5.8.3 dependencies and toolchain when available; keep source 5.8.3 pinned. Read docs/installed-engine.md. Do not claim 5.6 is a substitute.

## Dependencies
None for inspection and regression design. Shared engine access requires the integration owner.

## Acceptance
Document descriptor/header/library availability; BuildPlugin result and clean-project install for the exact target. If required engine distribution is unavailable, report blocked instead of support.

## Verification
Run python Tools/doctor.py for the baseline. Record exact RunUAT command, engine Build.version, package hash and clean-project runtime logs for the installed candidate.

## Handoff
Record results and remaining blockers before changing status.
