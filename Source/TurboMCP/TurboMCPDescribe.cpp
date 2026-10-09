// Copyright FloStyle. All Rights Reserved.

#include "TurboMCPDescribe.h"

#include "UObject/Class.h"
#include "UObject/UnrealType.h"

namespace
{
	/** How many signature parameters are printed per function. */
	constexpr int32 MaxSignatureParams = 24;
}

FString TurboMCPDescribe::VariableFlags(const FBPVariableDescription& InVariable)
{
	TArray<FString> Flags;
	const uint64 F = InVariable.PropertyFlags;

	if (F & CPF_BlueprintVisible) { Flags.Add(TEXT("BlueprintReadWrite")); }
	if (F & CPF_BlueprintReadOnly) { Flags.Add(TEXT("BlueprintReadOnly")); }

	// `CPF_Edit` means "user-settable in the editor", and EVERY Blueprint variable carries
	// it, so it is not evidence of anything. Printing it as "InstanceEditable" made the
	// list say `InstanceEditable, NotInstanceEditable` at the same time, which is how the
	// first version of this was wrong. The per-instance question is answered by
	// `CPF_DisableEditOnInstance` alone.
	Flags.Add((F & CPF_DisableEditOnInstance)
		? TEXT("NotInstanceEditable") : TEXT("InstanceEditable"));

	if (F & CPF_Config)    { Flags.Add(TEXT("Config")); }
	if (F & CPF_Transient) { Flags.Add(TEXT("Transient")); }
	if (F & CPF_RepNotify) { Flags.Add(TEXT("RepNotify")); }
	if (F & CPF_Net)       { Flags.Add(TEXT("Replicated")); }

	return FString::Join(Flags, TEXT(", "));
}

FString TurboMCPDescribe::FunctionParameters(const UFunction* InFunction)
{
	if (InFunction == nullptr)
	{
		return FString();
	}

	TArray<FString> Ins;
	TArray<FString> Outs;

	int32 Shown = 0;
	for (TFieldIterator<FProperty> It(InFunction); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
	{
		if (Shown >= MaxSignatureParams)
		{
			break;
		}
		++Shown;

		const FProperty* Property = *It;
		// A return parm and an out parm are both outputs and are printed the same way,
		// because a caller writing a call node does not distinguish them.
		if (Property->HasAnyPropertyFlags(CPF_ReturnParm)
			|| Property->HasAnyPropertyFlags(CPF_OutParm))
		{
			Outs.Add(FString::Printf(TEXT("%s %s"),
				*Property->GetCPPType(), *Property->GetName()));
		}
		else
		{
			Ins.Add(FString::Printf(TEXT("%s %s"),
				*Property->GetCPPType(), *Property->GetName()));
		}
	}

	FString Out = FString::Printf(TEXT("(%s)"), *FString::Join(Ins, TEXT(", ")));
	if (Outs.Num() > 0)
	{
		Out += FString::Printf(TEXT(" -> (%s)"), *FString::Join(Outs, TEXT(", ")));
	}
	return Out;
}
