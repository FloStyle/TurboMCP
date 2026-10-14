// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Misc/AutomationTest.h"

#include "TurboMCPNodeId.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTurboMCPNodeIdRoundTripTest,
	"TurboMCP.NodeId.RoundTripAndKeyValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTurboMCPNodeIdRoundTripTest::RunTest(const FString& Parameters)
{
	const FGuid Guid(0x123456ab, 0xcdef0123, 0x456789ab, 0xcdef0123);
	const FString Id = TurboMCPNodeId::Make(
		TEXT("/Game/Tests/BP_NodeTest.BP_NodeTest"), TEXT("My:Graph"), Guid,
		false, TurboMCPNodeId::StandardKeyDigits);

	TestEqual(TEXT("a graph colon is quoted in the emitted ID"), Id,
		TEXT("BP_NodeTest:\"My:Graph\":123456"));

	const TurboMCPNodeId::FParsed Parsed = TurboMCPNodeId::Parse(Id);
	TestEqual(TEXT("the asset survives the round trip"), Parsed.AssetName,
		TEXT("BP_NodeTest"));
	TestEqual(TEXT("the graph colon survives the round trip"), Parsed.GraphName,
		TEXT("My:Graph"));
	TestEqual(TEXT("the node key survives the round trip"), Parsed.NodeKey,
		TEXT("123456"));
	TestTrue(TEXT("the parsed ID is complete"), Parsed.IsComplete());

	TestEqual(TEXT("normalisation trims and lowercases a full key"),
		TurboMCPNodeId::NormaliseKey(TEXT("  ABCDEF  ")), TEXT("abcdef"));
	TestTrue(TEXT("a short key is refused"),
		TurboMCPNodeId::NormaliseKey(TEXT("12345")).IsEmpty());
	TestTrue(TEXT("a non-hex key is refused"),
		TurboMCPNodeId::NormaliseKey(TEXT("12zz34")).IsEmpty());

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
