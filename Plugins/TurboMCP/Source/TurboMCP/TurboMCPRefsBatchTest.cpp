// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

namespace TurboMCPRefsTests
{
	bool Run(FAutomationTestBase& Test);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTurboMCPRefsBatchTest, "TurboMCP.Refs.Batch",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTurboMCPRefsBatchTest::RunTest(const FString& Parameters)
{
	return TurboMCPRefsTests::Run(*this);
}

#endif
