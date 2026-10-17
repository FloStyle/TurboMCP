// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once

#include "CoreMinimal.h"

/**
 * THE REVERT, WHICH `FScopedTransaction::Cancel()` IS NOT.
 *
 * *** `Cancel()` DOES NOT UNDO ANYTHING. *** It reaches `UTransBuffer::Cancel` (`EditorTransaction.cpp:1411`),
 * which broadcasts `TransactionCanceled`, calls `GUndo->EndOperation()`, sets `GUndo = nullptr`, and POPS the
 * transaction off the undo buffer. IT NEVER CALLS `Apply()` AND IT TOUCHES NO OBJECT. The header says it in one
 * line (`Transactor.h:538`): "Cancels the current transaction, NO LONGER CAPTURE ACTIONS TO BE PLACED IN THE
 * UNDO BUFFER." So a writer that cancels and reports "ROLLED BACK" has reverted nothing, and the asset keeps
 * whatever the failing pass did.
 *
 * WHAT ACTUALLY REVERTS IS `UEditorEngine::UndoTransaction` (`EditorEngine.h:1068`), which runs
 * `UTransBuffer::Undo` and applies the transaction's inverse - AND THE ORDER IS THE WHOLE POINT:
 *
 *     `UTransBuffer::Undo` reads the UNDO BUFFER (`EditorTransaction.cpp:1638`), and the ACTIVE transaction is
 *     not on it yet. `EndTransaction` is what pushes it. So UNDOING FIRST REVERTS THE PREVIOUS TRANSACTION -
 *     SOMEBODY ELSE'S WORK - and `Cancel()` cannot be used to fix that up afterwards, because it POPS the
 *     record rather than applying it.
 *
 * `bCanRedo == false` then REMOVES the record, so a failed edit cannot be redone either.
 *
 * THIS IS ONE FUNCTION BECAUSE THE TWO CALLS ARE WORTHLESS APART AND THE ORDER BETWEEN THEM IS THE WHOLE
 * CORRECTNESS ARGUMENT. `TurboMCPMaterialBlocks.cpp` proves it with a read-back that names the expression set
 * the plan read; the writers that adopt it next have their suites and nothing finer yet.
 */
struct FTurboMCPRollback
{
	/**
	 * Ends the active transaction and undoes it, so the asset is what it was before that transaction.
	 *
	 * SAFE WHILE A `FScopedTransaction` IS STILL IN SCOPE. `FScopedTransaction::IsOutstanding()` is
	 * `Index >= 0` (`ScopedTransaction.cpp:54-57`) and `EndTransaction` does not change it, so the scope's
	 * destructor calls `EndTransaction()` again - which is a NO-OP, because `UTransBuffer::End` is guarded by
	 * `if (ActiveCount >= 1)` (`EditorTransaction.cpp:1308`).
	 *
	 * RETURNS WHETHER THE REVERT HAPPENED, AND A REVERT THAT CANNOT HAPPEN IS SAID OUT LOUD. In a commandlet
	 * there is no `GEditor`, so there is no undo buffer and nothing can be reverted; that is logged as an error
	 * rather than swallowed, because a caller whose message says "ROLLED BACK" would otherwise be claiming
	 * something no one performed.
	 */
	static bool EndAndUndoActiveTransaction();
};
