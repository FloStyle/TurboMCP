# Copyright (c) 2026 FloStyle
# SPDX-License-Identifier: MPL-2.0
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.
"""Check the dead-statement scanner against the shapes it must accept and refuse.

The failure cases are the shipped X1 shape and the reviewer probes that got past
the first version: a raw string with an unbalanced parenthesis that hid the rest
of the file, and a dead statement whose identifier looks like a label. The pass
cases are shapes a naive scan flags wrongly.
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

RAW_STRING_THEN_OLD_GATE = '''
static void Report()
{
	const TCHAR* Text = TEXT(R"DSL(node (A
  (more
)DSL");
	(void)Text;
}

bool FWidgetPersistenceTest::RunTest(const FString& Parameters)
{
	AddError(TEXT("admission-not-implemented"));
	return false;
	g();
}
'''

RAW_STRING_WITH_RETURN = '''
void F()
{
	const char* s = R"(
line } return;
end)";
	g();
}
'''

GOTO_LABEL_AFTER_RETURN = '''
void F()
{
	if (!Ready())
	{
		goto done;
	}
	return;
done:
	g();
}
'''

CASE_AFTER_RETURN = '''
int32 Pick(int32 Value)
{
	switch (Value)
	{
	case 0:
		return 0;
	case 1:
		g();
		return 1;
	}
	return -1;
}
'''

DEAD_STD_STRING = '''
void F()
{
	return;
	std::string s;
}
'''

FIXTURES = [
    ('old gate shape', OLD_RUNTEST, 1),
    ('fixed gate shape', FIXED_RUNTEST, 0),
    ('switch return then case', SWITCH_RETURN_THEN_CASE, 0),
    ('namespace function shape', TRACE_NAMESPACE, 0),
    ('comments and strings', COMMENTS_AND_STRINGS, 0),
    ('raw string with unbalanced paren then old gate', RAW_STRING_THEN_OLD_GATE, 1),
    ('statement-level raw string with return', RAW_STRING_WITH_RETURN, 0),
    ('goto label after return', GOTO_LABEL_AFTER_RETURN, 0),
    ('case after return inside switch', CASE_AFTER_RETURN, 0),
    ('dead std::string after return', DEAD_STD_STRING, 1),
]


def main():
    failures = 0
    for label, text, expected in FIXTURES:
        findings = scanner.scan_text(text, label)
        if len(findings) != expected:
            failures += 1
            print('FAIL: %s reported %d finding(s), expected %d: %r'
                  % (label, len(findings), expected, findings))
            continue
        if expected and not findings[0][0] < findings[0][1]:
            failures += 1
            print('FAIL: %s reported terminator line %d after statement line %d'
                  % (label, findings[0][0], findings[0][1]))
    if failures:
        print('RESULT: FAIL (%d case(s))' % failures)
        return 1
    print('RESULT: PASS (%d fixtures: old gate, fixed gate, switch, namespace, literals, '
          'raw string then gate, raw string with return, goto label, case after return, '
          'dead std::string)' % len(FIXTURES))
    return 0


if __name__ == '__main__':
    sys.exit(main())
