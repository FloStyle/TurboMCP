// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
#pragma once

#include "EdGraph/EdGraphPin.h"

/** Resolves the type syntax shared by struct fields, Blueprint variables and function pins. */
class FTurboMCPTypeParser
{
public:
	/** Resolves primitives, structs, assets, containers and typed object/class references. */
	static bool Resolve(const FString& InType, FEdGraphPinType& OutPinType, FString& OutWhy);
};
