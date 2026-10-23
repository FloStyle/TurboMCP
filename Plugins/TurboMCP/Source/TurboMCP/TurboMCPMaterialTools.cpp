// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "Tools/TurboMCPToolsets.h"

#include "TurboMCPToolError.h"

#include "Containers/UnrealString.h"

#include "TurboMCPMaterial.h"

// Material reader entry points share planning and rendering code.
// Load and Trace use the same tree renderer with different output selection.
// AsError avoids duplicating an error prefix supplied by the asset resolver.
namespace
{
	// Shared read path; positive budgets currently report overruns, not hard caps.

	FString ReadMaterial(const FString& InAsset, FTurboMCPMaterial::ERead InKind,
		const FString& InOutputProperty, bool bInIncludeInstanceOverrides, int32 InTokenBudget)
	{
		FTurboMCPMaterial::FPlan Plan;
		FString Error;
		if (!FTurboMCPMaterial::Plan(InAsset, InKind, InOutputProperty,
				bInIncludeInstanceOverrides, Plan, Error))
		{
			return FTurboMCPToolError::AsError(Error);
		}

		switch (InKind)
		{
		case FTurboMCPMaterial::ERead::Labels:
			// Label reports have no budget parameter.
			return FTurboMCPMaterial::RenderLabels(Plan);

		case FTurboMCPMaterial::ERead::Trace:
			return FTurboMCPMaterial::RenderTree(Plan, /*bTraceOnly=*/true, InTokenBudget);

		case FTurboMCPMaterial::ERead::Blocks:
			return FTurboMCPMaterial::RenderBlocks(Plan, InTokenBudget);

		case FTurboMCPMaterial::ERead::Load:
		default:
			return FTurboMCPMaterial::RenderTree(Plan, /*bTraceOnly=*/false, InTokenBudget);
		}
	}
}

FString UTurboMCPReadToolset::ReadTurboMCPMaterialLabels(const FString& Asset)
{
	// Enumerate named reroutes without selecting a material output.
	return ReadMaterial(Asset, FTurboMCPMaterial::ERead::Labels, FString(),
		/*bInIncludeInstanceOverrides=*/false, /*InTokenBudget=*/0);
}

FString UTurboMCPReadToolset::ReadTurboMCPMaterialLoad(
	const FString& Asset, bool bIncludeInstanceOverrides, int32 TokenBudget)
{
	return ReadMaterial(Asset, FTurboMCPMaterial::ERead::Load, FString(),
		bIncludeInstanceOverrides, TokenBudget);
}

FString UTurboMCPReadToolset::ReadTurboMCPMaterialTrace(
	const FString& Asset, const FString& OutputProperty, int32 TokenBudget)
{
	// The output property is required, and `Plan` refuses an empty or unconnected one by listing the
	// connected ones rather than by printing an empty tree.
	return ReadMaterial(Asset, FTurboMCPMaterial::ERead::Trace, OutputProperty,
		/*bInIncludeInstanceOverrides=*/false, TokenBudget);
}

FString UTurboMCPReadToolset::ReadTurboMCPMaterialBlocks(const FString& Asset, int32 TokenBudget)
{
	return ReadMaterial(Asset, FTurboMCPMaterial::ERead::Blocks, FString(),
		/*bInIncludeInstanceOverrides=*/false, TokenBudget);
}

