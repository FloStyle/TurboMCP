// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "Tools/TurboMCPToolsets.h"

#include "Containers/UnrealString.h"

#include "TurboMCPWidgetTreeSync.h"

#include "TurboMCPAssetResolver.h"
#include "Editor.h"
#include "WidgetBlueprint.h"
#include "WidgetBlueprintOperationUtils.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Widget.h"
#include "Kismet2/Kismet2NameValidators.h"

// Read selected widget-tree data through the shared inspector; no writer is invoked here.
FString UTurboMCPReadToolset::ReadTurboMCPWidgetTree(const FString& Asset)
{
	const FString Text = FTurboMCPWidgetTreeSync::Read(Asset);

	// `Read` already prefixes its own refusals, so this only ensures exactly one prefix survives. The guard
	// is on the RESULT rather than on a flag, because a flag can disagree with the string it describes.
	return Text.StartsWith(TEXT("error:")) || Text.StartsWith(TEXT("read:"))
		? Text
		: FString::Printf(TEXT("read: %s"), *Text);
}


// Use the editor's rename operation so graph and designer references follow the name.
// This does not rewrite arbitrary strings or promise a project-wide reference migration.

FString UTurboMCPEditToolset::EditTurboMCPWidgetRename(
    const FString& Asset, const FString& Widget, const FString& NewName)
{
    if (!IsInGameThread() || !GEditor || !GEditor->CanTransact() ||
        GEditor->IsPlaySessionInProgress() || GEditor->IsTransactionActive())
        return TEXT("error: Widget rename requires an idle editor outside PIE and transactions. Nothing changed.");
    // Reject rather than silently sanitize a requested identifier.
    const FString Name = NewName.TrimStartAndEnd();
    if (Name.IsEmpty() || Name.Len() > 100 ||
        !(FChar::IsAlpha(Name[0]) || Name[0] == TCHAR('_')))
        return TEXT("error: NewName must be an identifier of 1 to 100 letters, digits or underscores, starting with a letter or underscore. Nothing changed.");
    for (TCHAR Character : Name)
        if (!(FChar::IsAlnum(Character) || Character == TCHAR('_')))
            return TEXT("error: NewName must contain only letters, digits or underscores. Display labels with spaces are not supported. Nothing changed.");
    FTurboMCPAssetResolver::FResolved Resolved;
    const FString Error = FTurboMCPAssetResolver::Resolve(Asset, Resolved);
    if (!Error.IsEmpty()) return Error;
    UWidgetBlueprint* Blueprint = LoadObject<UWidgetBlueprint>(nullptr, *Resolved.ObjectPath);
    if (!Blueprint || !Blueprint->WidgetTree)
        return TEXT("error: Asset must be a Widget Blueprint with a widget tree. Nothing changed.");
    UWidget* Target = Blueprint->WidgetTree->FindWidget(FName(*Widget.TrimStartAndEnd()));
    if (!Target)
        return TEXT("error: Widget was not found in the named asset. Use ReadTurboMCPWidgetTree for its exact designer object name. Nothing changed.");
    const FString OldName = Target->GetName();
    if (Target->GetFName() == FName(*Name))
        return FString::Printf(TEXT("unchanged: Widget is already named %s (names are case-insensitive)."), *OldName);
    FKismetNameValidator Validator(Blueprint, Target->GetFName());
    if (Blueprint->WidgetTree->FindWidget(FName(*Name)) || Validator.IsValid(Name) != EValidatorResult::Ok)
        return TEXT("error: NewName conflicts with an existing widget, Blueprint member or reserved name. Choose an unused identifier. Nothing changed.");
    // The stock operation owns its undo transaction and structural refresh. It also
    // updates designer bindings, animation bindings, navigation and graph references.
    const bool bRenamed = FWidgetBlueprintOperationUtils::RenameWidget(Blueprint, Target, Name);
    if (!bRenamed || Target->GetFName() != FName(*Name))
        return FString::Printf(TEXT("error: Unreal did not confirm the requested rename from %s to %s. Current widget name: %s. Inspect the tree before retrying; no save was requested."), *OldName, *Name, *Target->GetName());
    return FString::Printf(TEXT("edited: Renamed widget %s to %s in %s using Unreal's editor rename operation. Graph and designer references handled by Unreal were refreshed. Dynamic string lookups and external assets are not verified. Structural refresh may affect loaded dependent Blueprints. Compile and inspect references before saving; no full compile or save was requested."), *OldName, *Target->GetName(), *Resolved.ObjectPath);
}
