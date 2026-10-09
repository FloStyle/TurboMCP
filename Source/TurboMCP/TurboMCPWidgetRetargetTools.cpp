// Copyright FloStyle. All Rights Reserved.

#include "Tools/TurboMCPToolsets.h"

#include "Containers/UnrealString.h"

#include "TurboMCPWidgetRetarget.h"

/**
 * `P-WIDGET-RETARGET` and `E-WIDGET-RETARGET`.
 *
 * ONE CODE PATH, TWO OUTCOMES. `FTurboMCPWidgetRetarget::Plan` finds the bindings and decides
 * which ones the widgets' CURRENT classes can still satisfy; the Preview prints that and the Edit
 * applies exactly it. The decision is the engine's own — a binding's delegate is looked for on the
 * widget's current class, which is what `FDelegateEditorBinding::IsBindingValid` does — so a
 * binding this tool calls broken is one the engine's compiler would reject.
 */

FString UTurboMCPPreviewToolset::PreviewTurboMCPWidgetRetarget(const FString& Asset)
{
	FTurboMCPWidgetRetarget::FPlan Plan;
	FString Error;
	if (!FTurboMCPWidgetRetarget::Plan(Asset, Plan, Error))
	{
		return FString::Printf(TEXT("error: %s"), *Error);
	}

	return FTurboMCPWidgetRetarget::RenderPreview(Plan);
}

FString UTurboMCPEditToolset::EditTurboMCPWidgetRetarget(const FString& Asset)
{
	FTurboMCPWidgetRetarget::FPlan Plan;
	FString Error;
	if (!FTurboMCPWidgetRetarget::Plan(Asset, Plan, Error))
	{
		return FString::Printf(TEXT("error: %s"), *Error);
	}

	return FTurboMCPWidgetRetarget::Apply(Plan);
}
