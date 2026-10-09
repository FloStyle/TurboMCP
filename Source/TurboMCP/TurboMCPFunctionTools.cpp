// Copyright FloStyle. All Rights Reserved.

#include "Tools/TurboMCPToolsets.h"

#include "Containers/UnrealString.h"

#include "TurboMCPFunction.h"

/**
 * `E-FUNCTION-ADD` and `E-FUNCTION-REMOVE`.
 *
 * NO PREVIEW TWIN, as `03_TOOLS.md` lists them. The name checks happen BEFORE anything is created,
 * so a refusal changes nothing, and the result reports what it did in enough detail to act on. That
 * is the same shape `E-WIDGETS-AS-VARS` uses.
 *
 * The two belong together because REMOVE is what frees a name an ADD took by mistake. Without it,
 * a mistyped function name could only be cleared in the editor by hand.
 */

FString UTurboMCPEditToolset::EditTurboMCPFunctionAdd(
	const FString& Asset, const FString& Name)
{
	return FTurboMCPFunction::Add(Asset, Name);
}

FString UTurboMCPEditToolset::EditTurboMCPFunctionRemove(
	const FString& Asset, const FString& Name)
{
	return FTurboMCPFunction::Remove(Asset, Name);
}
