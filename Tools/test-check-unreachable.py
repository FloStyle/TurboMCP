# Copyright (c) 2026 FloStyle
# SPDX-License-Identifier: MPL-2.0
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.
"""Check the dead-statement scanner against the shapes it must accept and refuse.

The first fixture is the shape v1.0.0 shipped: a fail-closed gate that returns
immediately, with the real body still live underneath it. The others are shapes
a naive line scan flags wrongly.
"""

import importlib.util
import pathlib
import sys

HERE = pathlib.Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location('check_unreachable', HERE / 'check-unreachable.py')
scanner = importlib.util.module_from_spec(spec)
spec.loader.exec_module(scanner)

OLD_RUNTEST = '''
bool FWidgetPersistenceTest::RunTest(const FString& Parameters)
{
	// FAIL-CLOSED gateway. A separately reviewed admission seam must replace it.
	AddError(TEXT("admission-not-implemented: C++ supervisor context verification is unavailable"));
	return false;

	// This test performs a global package unload.
	const FString RunnerOwned = FPlatformMisc::GetEnvironmentVariable(
		TEXT("TURBOMCP_RUNNER_OWNS_FIXTURES"));
	if (RunnerOwned.IsEmpty())
	{
		return false;
	}
	return true;
}
'''

FIXED_RUNTEST = '''
bool FWidgetPersistenceTest::RunTest(const FString& Parameters)
{
	if (!TurboMCPTestAdmission::IsCppSupervisorAdmissionAvailable())
	{
		AddError(TEXT("admission-not-implemented: C++ supervisor context verification is unavailable"));
		return false;
	}

	const FString RunnerOwned = FPlatformMisc::GetEnvironmentVariable(
		TEXT("TURBOMCP_RUNNER_OWNS_FIXTURES"));
	if (RunnerOwned.IsEmpty())
	{
		return false;
	}
	return true;
}
'''

SWITCH_RETURN_THEN_CASE = '''
int32 Classify(int32 Value)
{
	switch (Value)
	{
	case 0:
		return 0;
	case 1:
		return 1;
	default:
		return -1;
	}
}
'''

TRACE_NAMESPACE = '''
namespace TraceStore
{
	static FString Describe()
	{
		FString Out;
		if (GArmed.Num() == 0)
		{
			Out += TEXT("No hits were recorded.");
		}

		return Out;
	}

	TArray<FHit> Hits()
	{
	FScopeLock Lock(&GMutex);
	return GHits;
	}

	int32 HitCount()
	{
		FScopeLock Lock(&GMutex);
		return GHits.Num();
	}
}
'''

COMMENTS_AND_STRINGS = '''
// return false; then this comment must not count
/* return true;
   still a comment */
const TCHAR* Message = TEXT("return; break; continue;");
bool Gate()
{
	// return false;
	return Message != nullptr;
}
'''

FIXTURES = [
    ('old gate shape', OLD_RUNTEST, True),
    ('fixed gate shape', FIXED_RUNTEST, False),
    ('switch return then case', SWITCH_RETURN_THEN_CASE, False),
    ('namespace function shape', TRACE_NAMESPACE, False),
    ('comments and strings', COMMENTS_AND_STRINGS, False),
]


def main():
    failures = 0
    for label, text, must_fail in FIXTURES:
        findings = scanner.scan_text(text, label)
        if must_fail and not findings:
            failures += 1
            print('FAIL: %s was accepted' % label)
        elif not must_fail and findings:
            failures += 1
            print('FAIL: %s was refused at line %d' % (label, findings[0][0]))
        elif must_fail:
            terminator, statement = findings[0][0], findings[0][1]
            if not terminator < statement:
                failures += 1
                print('FAIL: %s reported terminator line %d after statement line %d'
                      % (label, terminator, statement))
    if failures:
        print('RESULT: FAIL (%d case(s))' % failures)
        return 1
    print('RESULT: PASS (old gate refused, fixed gate, switch, namespace and literals accepted)')
    return 0


if __name__ == '__main__':
    sys.exit(main())
