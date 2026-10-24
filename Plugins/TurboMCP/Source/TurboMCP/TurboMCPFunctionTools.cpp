// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "Tools/TurboMCPToolsets.h"

#include "Containers/UnrealString.h"

#include "TurboMCPFunction.h"

// The implementation validates names and reports mutation and cleanup outcomes.

FString UTurboMCPEditToolset::EditTurboMCPFunctionAdd(
	const FString& Asset, const FString& Name, const FString& Inputs, const FString& Outputs)
{
	return FTurboMCPFunction::Add(Asset, Name, Inputs, Outputs);
}

FString UTurboMCPEditToolset::EditTurboMCPFunctionRemove(
	const FString& Asset, const FString& Name)
{
	return FTurboMCPFunction::Remove(Asset, Name);
}
