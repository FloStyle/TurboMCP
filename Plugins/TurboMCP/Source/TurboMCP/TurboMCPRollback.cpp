// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "TurboMCPRollback.h"

#include "Editor/UnrealEd/Public/Editor.h"
#include "Logging.h"
#include "Editor/TransBuffer.h"

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

    UTransBuffer* Buffer = Cast<UTransBuffer>(GEditor->Trans);
    if (!Buffer || Buffer->ActiveCount != 1 || Buffer->UndoBuffer.IsEmpty())
    {
        UE_LOG(LogTurboMCP, Error, TEXT("Rollback refused: no single owned transaction; previous or nested work was not undone."));
        return false;
    }
    const FGuid ActiveId = Buffer->UndoBuffer.Last()->GetId();
    GEditor->EndTransaction();
    const int32 Index = Buffer->GetQueueLength() - Buffer->GetUndoCount() - 1;
    const FTransaction* Current = Index >= 0 ? Buffer->GetTransaction(Index) : nullptr;
    if (!Current || Current->GetId() != ActiveId)
    {
        UE_LOG(LogTurboMCP, Error, TEXT("Rollback refused: active transaction did not survive finalization; previous work was not undone."));
        return false;
    }
    return GEditor->UndoTransaction(/*bCanRedo=*/false);
}
