// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "TurboMCPTestAdmission.h"

#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

FORCENOINLINE bool TurboMCPTestAdmission::IsCppSupervisorAdmissionAvailable()
{
	return false;
}

#endif // WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
