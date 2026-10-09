// Copyright FloStyle. All Rights Reserved.

#include "TurboMCPBlueprintJson.h"

#include "Blueprint/BlueprintSupport.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "EdGraph/EdGraph.h"
#include "Engine/Blueprint.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UObject/Class.h"
#include "UObject/Object.h"

#include "TurboMCPBlueprintRead.h"

namespace
{
	/** How many names a JSON list carries before it caps, matching the text output. */
	constexpr int32 MaxManifestNames = 40;
}

FString FTurboMCPBlueprintJson::WriteError(const FString& InMessage)
{
	// A refusal is still a document. `ok:false` is the field a caller tests, so it never
	// has to parse the message to find out whether the call worked.
	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetBoolField(TEXT("ok"), false);
	Root->SetStringField(TEXT("error"), InMessage);

	FString Out;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
	FJsonSerializer::Serialize(Root, Writer);
	return Out;
}

FString FTurboMCPBlueprintJson::Write(const UBlueprint* InBlueprint, int32 InDetailLevel,
	FString& OutError)
{
	OutError.Empty();

	if (InBlueprint == nullptr)
	{
		OutError = TEXT("no Blueprint was given");
		return FString();
	}

	// Only `manifest` is written. Refusing the others by name is deliberate: a partial
	// document would be read as a complete one.
	if (InDetailLevel != 0)
	{
		OutError = FString::Printf(
			TEXT("format 'json' is implemented for detail 'manifest' only, and '%s' was ")
			TEXT("asked for. The text format carries every level. This refuses rather ")
			TEXT("than returning a manifest-shaped document that a caller would read as ")
			TEXT("a full answer."),
			InDetailLevel == 1 ? TEXT("signatures")
				: InDetailLevel == 2 ? TEXT("bodies")
				: InDetailLevel == 3 ? TEXT("full") : TEXT("that level"));
		return FString();
	}

	// The same accessors the text writer uses, so the two carry the same facts.
	TArray<UEdGraph*> Graphs;
	FTurboMCPBlueprintRead::CollectGraphs(InBlueprint, Graphs);

	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetBoolField(TEXT("ok"), true);
	Root->SetStringField(TEXT("detail"), TEXT("manifest"));
	Root->SetStringField(TEXT("asset"), InBlueprint->GetPathName());
	Root->SetStringField(TEXT("class"), InBlueprint->GetClass()->GetName());

	const UClass* Parent = InBlueprint->ParentClass;
	Root->SetStringField(TEXT("parent"),
		Parent != nullptr ? Parent->GetPathName() : FString());

	// `interfaces` is an ARRAY and empty when there are none, where the text says
	// "none". A JSON array is the honest shape: a caller tests its length, and the
	// absence of interfaces is a length of zero rather than a word it has to compare.
	TArray<TSharedPtr<FJsonValue>> Interfaces;
	for (const FBPInterfaceDescription& Interface : InBlueprint->ImplementedInterfaces)
	{
		Interfaces.Add(MakeShared<FJsonValueString>(
			Interface.Interface != nullptr
				? Interface.Interface->GetPathName() : FString()));
	}
	Root->SetArrayField(TEXT("interfaces"), Interfaces);

	// The counts, as numbers rather than as a sentence, because a caller should never
	// have to parse prose.
	TSharedRef<FJsonObject> Counts = MakeShared<FJsonObject>();
	Counts->SetNumberField(TEXT("graphs"), Graphs.Num());
	Counts->SetNumberField(TEXT("variables"), InBlueprint->NewVariables.Num());
	Counts->SetNumberField(TEXT("components"),
		InBlueprint->SimpleConstructionScript != nullptr
			? InBlueprint->SimpleConstructionScript->GetAllNodes().Num() : 0);
	Root->SetObjectField(TEXT("counts"), Counts);

	// Every graph, with its name. A graph body is added at `bodies` and above, where it
	// is carried as the S-expression text rather than reshaped; see the header.
	TArray<TSharedPtr<FJsonValue>> GraphsJson;
	for (int32 Index = 0; Index < Graphs.Num(); ++Index)
	{
		TSharedRef<FJsonObject> Graph = MakeShared<FJsonObject>();
		Graph->SetStringField(TEXT("name"), Graphs[Index]->GetName());
		GraphsJson.Add(MakeShared<FJsonValueObject>(Graph));
	}
	Root->SetArrayField(TEXT("graphs"), GraphsJson);

	// A cap is stated with the number left, in both renderings, as `04_FORMATS.md`
	// section 1 requires. The text prints "(N more)"; the JSON carries the number so a
	// caller can page without reading a sentence.
	TArray<TSharedPtr<FJsonValue>> VariablesJson;
	for (int32 Index = 0; Index < InBlueprint->NewVariables.Num()
			&& Index < MaxManifestNames; ++Index)
	{
		VariablesJson.Add(MakeShared<FJsonValueString>(
			InBlueprint->NewVariables[Index].VarName.ToString()));
	}
	Root->SetArrayField(TEXT("variables"), VariablesJson);
	if (InBlueprint->NewVariables.Num() > MaxManifestNames)
	{
		Root->SetNumberField(TEXT("variables_omitted"),
			InBlueprint->NewVariables.Num() - MaxManifestNames);
	}

	TArray<TSharedPtr<FJsonValue>> ComponentsJson;
	if (InBlueprint->SimpleConstructionScript != nullptr)
	{
		const TArray<USCS_Node*>& Nodes = InBlueprint->SimpleConstructionScript->GetAllNodes();
		for (int32 Index = 0; Index < Nodes.Num() && Index < MaxManifestNames; ++Index)
		{
			const USCS_Node* Node = Nodes[Index];
			ComponentsJson.Add(MakeShared<FJsonValueString>(
				Node != nullptr ? Node->GetVariableName().ToString() : FString()));
		}
		if (Nodes.Num() > MaxManifestNames)
		{
			Root->SetNumberField(TEXT("components_omitted"),
				Nodes.Num() - MaxManifestNames);
		}
	}
	Root->SetArrayField(TEXT("components"), ComponentsJson);

	FString Out;
	// INDENTED, and this is the default rather than an option. The audience is an agent
	// reading the document into a context window, where a compact one-line document costs
	// the same tokens and is far harder to read when part of it is quoted back.
	const TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> Writer =
		TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&Out);
	FJsonSerializer::Serialize(Root, Writer);
	return Out;
}
