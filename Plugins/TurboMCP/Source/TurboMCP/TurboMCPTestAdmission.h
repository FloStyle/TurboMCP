// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

namespace TurboMCPTestAdmission
{
	/**
	 * Returns true only when a reviewed C++ supervisor-context check admits a
	 * native fixture run. No such check exists, so this always returns false.
	 *
	 * The answer is defined out of line and FORCENOINLINE, so the compiler cannot
	 * prove the gated code unreachable and raise C4702 for the body the gate
	 * protects.
	 */
	bool IsCppSupervisorAdmissionAvailable();
}

#endif // WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
