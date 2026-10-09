// Copyright FloStyle. All Rights Reserved.

#include "Tools/TurboMCPToolsets.h"

#include "Containers/UnrealString.h"

#include "TurboMCPWidgetsAsVars.h"

/**
 * `E-WIDGETS-AS-VARS`.
 *
 * EDIT ONLY, on purpose. `03_TOOLS.md` says no Preview twin is needed because it is a flag change
 * and the Edit reports old and new state. The reporting is what replaces the preview, so the
 * result names the previous value of the flag beside the new one rather than only the new one.
 */
FString UTurboMCPEditToolset::EditTurboMCPWidgetsAsVars(
	const FString& Asset, const FString& Widgets, bool bIsVariable)
{
	FTurboMCPWidgetsAsVars::FPlan Plan;
	FString Error;
	if (!FTurboMCPWidgetsAsVars::Plan(Asset, Widgets, bIsVariable, Plan, Error))
	{
		return FString::Printf(TEXT("error: %s"), *Error);
	}

	return FTurboMCPWidgetsAsVars::Apply(Plan);
}
