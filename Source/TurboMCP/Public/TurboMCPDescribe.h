// Copyright FloStyle. All Rights Reserved.

#pragma once

#include "Containers/UnrealString.h"
#include "CoreTypes.h"

class UFunction;
struct FBPVariableDescription;

/**
 * The small descriptions `R-BP-LOAD` prints in more than one place.
 *
 * WHY THIS IS ITS OWN FILE
 * ------------------------
 * These began as free functions in an anonymous namespace inside
 * `TurboMCPBlueprintRead.cpp`, which was fine while the text writer was the only reader.
 * The JSON writer is a second rendering of the same facts, and `04_FORMATS.md` warns
 * that most bugs in this project came from two tools disagreeing about one format.
 *
 * A variable's flags and a function's parameters are exactly the facts that would drift
 * if each writer spelled them its own way: both could be technically right and still
 * disagree, and a caller comparing the two renderings would find a difference that means
 * nothing. So there is one definition and two callers.
 */
namespace TurboMCPDescribe
{
	/** A variable's flags, as short words an agent can act on. */
	FString VariableFlags(const FBPVariableDescription& InVariable);

	/** A function's parameters, with direction marked. */
	FString FunctionParameters(const UFunction* InFunction);
}
