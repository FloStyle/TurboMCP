// Copyright FloStyle. All Rights Reserved.

#include "Tools/TurboMCPToolsets.h"

#include "Containers/UnrealString.h"

#include "TurboMCPGraphFixBackward.h"

/**
 * `P-GRAPH-FIX-BACKWARD` and `E-GRAPH-FIX-BACKWARD`.
 *
 * ONE CODE PATH, TWO OUTCOMES (`D-02`). `FTurboMCPGraphFixBackward::Plan` reads the graph, computes the
 * whole pass against VALUES, measures the wiring on the planned positions and decides whether the result
 * is better - so EVERY REFUSAL AND THE ROLL-BACK DECISION are already made when the Preview prints. The
 * Edit calls the same `Plan` and then `Apply`, which means it refuses exactly what the Preview refused
 * and writes exactly what the Preview described.
 *
 * `Force` LIVES ON `Plan` RATHER THAN ON `Apply`, and that is deliberate: the Preview must be able to say
 * "this would be worse and would be rolled back, but Force is set so it will be applied". A flag on
 * `Apply` would let the Preview describe a rollback the Edit then does not perform, which is the `D-02`
 * disagreement this project has been caught by twice.
 */
FString UTurboMCPPreviewToolset::PreviewTurboMCPGraphFixBackward(
	const FString& Asset, const FString& GraphName, bool Force)
{
	FTurboMCPGraphFixBackward::FPlan Plan;
	FString Error;
	if (!FTurboMCPGraphFixBackward::Plan(Asset, GraphName, Force, Plan, Error))
	{
		return FString::Printf(TEXT("error: %s"), *Error);
	}

	return FTurboMCPGraphFixBackward::RenderPreview(Plan);
}

FString UTurboMCPEditToolset::EditTurboMCPGraphFixBackward(
	const FString& Asset, const FString& GraphName, bool Force)
{
	FTurboMCPGraphFixBackward::FPlan Plan;
	FString Error;
	if (!FTurboMCPGraphFixBackward::Plan(Asset, GraphName, Force, Plan, Error))
	{
		return FString::Printf(TEXT("error: %s"), *Error);
	}

	return FTurboMCPGraphFixBackward::Apply(Plan);
}
