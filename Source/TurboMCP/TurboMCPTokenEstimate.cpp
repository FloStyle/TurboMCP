// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "TurboMCPTokenEstimate.h"

int32 TurboMCPTokenEstimate(const FString& InText)
{
	int32 Total = 0;
	int32 RunLength = 0;

	// A run of letters and digits is counted when it ENDS, so the closing is done twice: once
	// when a non-alphanumeric character arrives and once at the end of the text.
	auto CloseRun = [&Total, &RunLength]()
	{
		if (RunLength > 0)
		{
			// `ceil(length / 4)`, which for integers is `(length + 3) / 4`.
			Total += (RunLength + 3) / 4;
			RunLength = 0;
		}
	};

	for (const TCHAR Character : InText)
	{
		if (FChar::IsAlnum(Character))
		{
			++RunLength;
			continue;
		}

		CloseRun();

		// Whitespace counts 0; every other visible character counts 1. `IsWhitespace` covers
		// space, tab, newline and carriage return, which are the ones that appear in these
		// outputs.
		if (!FChar::IsWhitespace(Character))
		{
			++Total;
		}
	}

	CloseRun();
	return Total;
}
