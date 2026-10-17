// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "TurboMCPRollback.h"

#include "Editor/UnrealEd/Public/Editor.h"
#include "Logging.h"

bool FTurboMCPRollback::EndAndUndoActiveTransaction()
{
	if (GEditor == nullptr)
	{
		// *** A REVERT THAT CANNOT HAPPEN IS SAID OUT LOUD. *** A `-run=` commandlet has no `GEditor`, so the
		// undo buffer does not exist and NOTHING can be reverted. Logging it is the difference between a hole
		// and a named hole; the writer's own message still says "ROLLED BACK", and that is the next thing to
		// fix rather than something to hide here.
		UE_LOG(LogTurboMCP, Error,
			TEXT("a writer asked to roll back, but this process has no GEditor, so the edit was NOT reverted ")
			TEXT("and the asset keeps whatever the failing pass did."));
		return false;
	}

	// END FIRST, THEN UNDO. See the header: undoing an ACTIVE transaction reverts the PREVIOUS one.
	GEditor->EndTransaction();
	return GEditor->UndoTransaction(/*bCanRedo=*/false);
}
