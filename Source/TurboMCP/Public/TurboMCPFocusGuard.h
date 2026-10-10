// Copyright (c) 2026 Florian ESCHALIER (FloStyle)
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once

#include "CoreMinimal.h"

/**
 * GIVES THE FOREGROUND BACK WHEN THIS EDITOR TAKES IT DURING AN UNATTENDED RUN.
 *
 * WHY THIS EXISTS, AND WHY IT LIVES IN THE PLUGIN RATHER THAN IN A LAUNCHER. The owner reported that starting the
 * editor for a test run kept taking their focus away from a game, and FOUR ATTEMPTS FROM OUTSIDE THE PROCESS WERE
 * EACH MEASURED AND EACH FAILED:
 *
 *   * `-nullrhi` (what `-Offscreen` adds) made the editor fail to start - `the editor process exited after 8s`,
 *     twice - while a plain launch reached `RESULT: PASS` in 16 seconds;
 *   * `ShowWindow(SW_HIDE)` on the main handle killed it, `exited after 25s`;
 *   * `ShowWindow(SW_SHOWMINNOACTIVE)` killed it too, `exited after 18s`;
 *   * `-LiveCoding=0` IS a real flag and DID suppress `LiveCodingConsole` - which calls `BringToFront` by design,
 *     `LiveCodingConsole.cpp:169,240` - but the editor then failed to reach a healthy state, twice.
 *
 * **A PROCESS THAT MANAGES ITS OWN MESSAGE LOOP TREATS AN EXTERNALLY HIDDEN OR MINIMISED MAIN WINDOW AS A
 * SHUTDOWN CONDITION**, which is why every external attempt killed it. FROM INSIDE, the editor is free to activate
 * itself normally and THE PLUGIN SIMPLY HANDS THE FOREGROUND BACK - which is a thing the editor's own window API is
 * allowed to do.
 *
 * *** AND IT EXISTS BECAUSE THE CORRECT FIX CANNOT BE THE ONE THE OWNER ASKED FOR. *** The request was to set up
 * Live Coding to save on editor startup. READING THE ENGINE SOURCE SHOWED THAT LIVE CODING IS THE CAUSE OF THE
 * INTERRUPTION - it is the component that brings itself to the front - SO THE FEATURE REQUESTED AS THE FIX IS WHAT
 * NEEDED REMOVING. **A FEATURE REQUEST CAN BE THE CAUSE OF THE COMPLAINT IT IS OFFERED AS THE FIX FOR.**
 *
 * IT IS GATED ON `FApp::IsUnattended()` (`App.cpp:278`), BECAUSE A HUMAN WHO LAUNCHES THE EDITOR WANTS TO TYPE IN
 * IT. This is for a machine running a suite, where nobody is looking at the window at all.
 */
class FTurboMCPFocusGuard
{
public:
	/** Installs the guard for this process. Safe to call more than once; the second call does nothing. */
	static void Install();

	/** Removes the guard and releases the handle it holds. */
	static void Remove();

	/**
	 * True when the guard is active, which is TRUE ONLY IN AN UNATTENDED RUN.
	 *
	 * Exposed so a suite can ASSERT ON IT rather than assume it: a test that depends on the editor not taking focus
	 * should be able to ask whether the guard is running, AND A GUARD NOBODY CAN OBSERVE IS A GUARD NOBODY CAN
	 * TELL APART FROM ONE THAT SILENTLY FAILED TO INSTALL.
	 */
	static bool IsActive();
};
