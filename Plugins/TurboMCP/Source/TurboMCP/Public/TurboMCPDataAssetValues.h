// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0

#pragma once
#include "CoreMinimal.h"
#include "UObject/StrongObjectPtr.h"

class FProperty;
class FMapProperty;

namespace TurboMCPDataAssetValues
{
/** Initialized property storage. This is a synchronous planning value, not a live asset.
 * Referenced objects are retained by the caller across GC-capable operations.
 */
class FValue
{
public:
    explicit FValue(const FProperty* InProperty, const void* Source = nullptr);
    ~FValue();
    FValue(const FValue&) = delete;
    FValue& operator=(const FValue&) = delete;
    void* Get() const { return Data; }
private:
    const FProperty* Property;
    void* Data;
};

/** Parse only scalar/value-struct text. Object references use ResolveReference instead.
 * Refusal may alter temporary storage, but never touches live property memory.
 */
bool ParseValue(const FProperty* Property, const FString& Text, void* Temporary, FString& Error);

/** Resolve an exact object path without loading or constructing anything. Keys are external
 * references; values marked instanced must already belong to Owner. The retained reference
 * keeps the resolved object alive until the caller finishes planning/application.
 */
bool ResolveReference(const FProperty* Property, const FString& Path, UObject* Owner,
    bool bMapKey, void* Temporary, TStrongObjectPtr<UObject>& Retained, FString& Error);

/** Copy then modify one entry in temporary map storage. No notifications or dirty changes.
 * Key and Value must have been validated with the map's exact property descriptors.
 */
void PlanMapEntry(const FMapProperty* Property, const void* LiveMap, const void* Key,
    const void* Value, bool bRemove, void* TemporaryMap);
}
