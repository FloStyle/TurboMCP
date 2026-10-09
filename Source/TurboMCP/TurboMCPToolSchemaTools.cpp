// Copyright FloStyle. All Rights Reserved.

#include "Tools/TurboMCPToolsets.h"

#include "Containers/UnrealString.h"

#include "TurboMCPToolSchema.h"

/**
 * `R-TOOL-SCHEMA`, and the two node-type tools that share its matcher.
 *
 * READ-ONLY, so there is no `Apply` and no Edit twin. The plan/render split is kept anyway
 * because it is what makes the refusals testable: everything that can be refused is refused in
 * `Plan`, and a caller that gets a plan back is guaranteed a rendering.
 */

FString UTurboMCPReadToolset::ReadTurboMCPToolSchema(
	const FString& Toolset, const FString& Tool)
{
	FTurboMCPToolSchema::FPlan Plan;
	FString Error;
	if (!FTurboMCPToolSchema::Plan(Toolset, Tool, Plan, Error))
	{
		return FString::Printf(TEXT("error: %s"), *Error);
	}

	return FTurboMCPToolSchema::Render(Plan);
}
