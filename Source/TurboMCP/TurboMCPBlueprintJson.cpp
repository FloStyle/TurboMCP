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
#include "TurboMCPDescribe.h"

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
	int32 InBudgetChars, FString& OutError)
{
	OutError.Empty();

	if (InBlueprint == nullptr)
	{
		OutError = TEXT("no Blueprint was given");
		return FString();
	}

	// Only `manifest` is written. Refusing the others by name is deliberate: a partial
	// document would be read as a complete one.
	// `manifest` and `signatures` are written. `bodies` and `full` are refused BY NAME:
	// a partial document would be read as a complete one.
	if (InDetailLevel != 0 && InDetailLevel != 1)
	{
		OutError = FString::Printf(
			TEXT("format 'json' is implemented for detail 'manifest' and 'signatures' ")
			TEXT("only, and '%s' was asked for. The text format carries every level. This ")
			TEXT("refuses rather than returning a partial document that a caller would ")
			TEXT("read as a full answer."),
			InDetailLevel == 2 ? TEXT("bodies")
				: InDetailLevel == 3 ? TEXT("full") : TEXT("that level"));
		return FString();
	}

	// The same accessors the text writer uses, so the two carry the same facts.
	TArray<UEdGraph*> Graphs;
	FTurboMCPBlueprintRead::CollectGraphs(InBlueprint, Graphs);

	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetBoolField(TEXT("ok"), true);
	Root->SetStringField(TEXT("detail"),
		InDetailLevel == 0 ? TEXT("manifest") : TEXT("signatures"));
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

	// ---------------------------------------------------------------------------------
	// `signatures` AND ABOVE.
	//
	// The manifest is a list of names; here each name gets its facts. The type, the flag
	// words and the parameter lists come from the SAME code the text writer calls, so the
	// two renderings cannot spell one fact two ways.
	// ---------------------------------------------------------------------------------
	if (InDetailLevel >= 1)
	{
		// Variables in full. The names are already in `variables` above; this array
		// carries the facts, and both are present so a caller can read the short list
		// without walking objects.
		TArray<TSharedPtr<FJsonValue>> VariableDetails;
		for (const FBPVariableDescription& Variable : InBlueprint->NewVariables)
		{
			TSharedRef<FJsonObject> One = MakeShared<FJsonObject>();
			One->SetStringField(TEXT("name"), Variable.VarName.ToString());
			One->SetStringField(TEXT("type"),
				FTurboMCPBlueprintRead::DescribeVariableType(Variable.VarType));

			// An empty default is a FACT and not a missing value: it distinguishes "at
			// the type's zero" from "has a value", which is what a caller deciding
			// whether to set it needs. The text prints "(type default)" and this prints
			// an empty string, which is the honest shape for a field that is genuinely
			// empty; a null would be read as "not collected".
			One->SetStringField(TEXT("default"), Variable.DefaultValue);

			// The category, with `Default` named when the designer left it unset, so the
			// two renderings agree on that case too.
			One->SetStringField(TEXT("category"), Variable.Category.IsEmpty()
				? TEXT("Default") : Variable.Category.ToString());

			One->SetStringField(TEXT("flags"),
				TurboMCPDescribe::VariableFlags(Variable));

			if (!Variable.RepNotifyFunc.IsNone())
			{
				One->SetStringField(TEXT("rep_notify"),
					Variable.RepNotifyFunc.ToString());
			}

			VariableDetails.Add(MakeShared<FJsonValueObject>(One));
		}
		Root->SetArrayField(TEXT("variable_details"), VariableDetails);

		// Events and functions, from the shared collection so the signatures match the
		// text exactly.
		TArray<FString> Events;
		TArray<FString> Functions;
		FTurboMCPBlueprintRead::CollectEventsAndFunctions(
			InBlueprint, Events, Functions);

		auto ToArray = [](const TArray<FString>& In)
		{
			TArray<TSharedPtr<FJsonValue>> Out;
			for (const FString& One : In)
			{
				Out.Add(MakeShared<FJsonValueString>(One));
			}
			return Out;
		};
		Root->SetArrayField(TEXT("events"), ToArray(Events));
		Root->SetArrayField(TEXT("functions"), ToArray(Functions));

		// Components with their class, because a component's class decides which nodes
		// can be attached to it.
		TArray<TSharedPtr<FJsonValue>> ComponentDetails;
		if (InBlueprint->SimpleConstructionScript != nullptr)
		{
			for (const USCS_Node* Node : InBlueprint->SimpleConstructionScript->GetAllNodes())
			{
				if (Node == nullptr)
				{
					continue;
				}
				TSharedRef<FJsonObject> One = MakeShared<FJsonObject>();
				One->SetStringField(TEXT("name"), Node->GetVariableName().ToString());
				One->SetStringField(TEXT("class"),
					Node->ComponentClass != nullptr
						? Node->ComponentClass->GetName() : FString());
				ComponentDetails.Add(MakeShared<FJsonValueObject>(One));
			}
		}
		Root->SetArrayField(TEXT("component_details"), ComponentDetails);
	}

	FString Out;
	// INDENTED, and this is the default rather than an option. The audience is an agent
	// reading the document into a context window, where a compact one-line document costs
	// the same tokens and is far harder to read when part of it is quoted back.
	const TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> Writer =
		TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&Out);
	FJsonSerializer::Serialize(Root, Writer);

	// THE BUDGET, ENFORCED AS A REFUSAL RATHER THAN A CUT.
	//
	// The document is complete at this point, so it can be measured honestly. Cutting it
	// here would produce something that does not parse, and a caller holding an unparseable
	// document cannot even read the explanation of what went wrong. Naming the exact call
	// that fits is what `03_TOOLS.md` asks a reduction to end with, and it works the same
	// way here.
	if (InBudgetChars > 0 && Out.Len() > InBudgetChars)
	{
		OutError = FString::Printf(
			TEXT("the JSON document for detail '%s' is %d characters and the budget is %d, ")
			TEXT("so it was refused rather than cut. A cut JSON document does not parse, ")
			TEXT("which is worse than no document. Ask for a lower detail level, or use the ")
			TEXT("text format, whose writer steps a graph's detail down to fit."),
			InDetailLevel == 0 ? TEXT("manifest") : TEXT("signatures"),
			Out.Len(), InBudgetChars);
		return FString();
	}

	return Out;
}
