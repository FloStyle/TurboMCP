// Copyright FloStyle. All Rights Reserved.

#include "Tools/TurboMCPToolsets.h"

#include "Containers/UnrealString.h"

#include "TurboMCPVarsAdd.h"

/**
 * `P-VARS-ADD` and `E-VARS-ADD`.
 *
 * ONE CODE PATH, TWO OUTCOMES. `FTurboMCPVarsAdd::Plan` parses and validates the list, the
 * Preview prints it, and the Edit applies exactly that list. Parsing is the part that must not
 * differ between the two: a preview that accepted a list the edit then refused would be worse
 * than no preview at all.
 */

FString UTurboMCPPreviewToolset::PreviewTurboMCPVarsAdd(
	const FString& Asset, const FString& Variables)
{
	FTurboMCPVarsAdd::FPlan Plan;
	FString Error;
	if (!FTurboMCPVarsAdd::Plan(Asset, Variables, Plan, Error))
	{
		return FString::Printf(TEXT("error: %s"), *Error);
	}

	return FTurboMCPVarsAdd::RenderPreview(Plan);
}

FString UTurboMCPEditToolset::EditTurboMCPVarsAdd(
	const FString& Asset, const FString& Variables)
{
	FTurboMCPVarsAdd::FPlan Plan;
	FString Error;
	if (!FTurboMCPVarsAdd::Plan(Asset, Variables, Plan, Error))
	{
		return FString::Printf(TEXT("error: %s"), *Error);
	}

	return FTurboMCPVarsAdd::Apply(Plan);
}
