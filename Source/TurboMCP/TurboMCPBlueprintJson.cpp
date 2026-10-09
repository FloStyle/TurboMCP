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
#include "TurboMCPGraphText.h"

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

FString FTurboMCPBlueprintJson::Write(const UBlueprint* InBlueprint,
	const FRequest& InRequest, FString& OutError)
{
	OutError.Empty();

	const int32 InDetailLevel = InRequest.DetailLevel;
	const int32 InBudgetChars = InRequest.BudgetChars;

	if (InBlueprint == nullptr)
	{
		OutError = TEXT("no Blueprint was given");
		return FString();
	}

	// ALL FOUR LEVELS are written. The guard stays because the level arrives as an int, and
	// an out-of-range value must be refused rather than silently treated as one of the four.
	if (InDetailLevel < 0 || InDetailLevel > 3)
	{
		OutError = FString::Printf(
			TEXT("'%d' is not a detail level. Accepted: 0 manifest, 1 signatures, ")
			TEXT("2 bodies, 3 full."), InDetailLevel);
		return FString();
	}

	// The same accessors the text writer uses, so the two carry the same facts.
	TArray<UEdGraph*> AllGraphs;
	FTurboMCPBlueprintRead::CollectGraphs(InBlueprint, AllGraphs);

	// THE FILTER IS APPLIED THE WAY THE TEXT WRITER APPLIES IT, so both cover the same
	// graphs. A JSON that quietly included graphs the text omitted would look like the
	// filter did not work.
	TArray<UEdGraph*> Graphs;
	for (UEdGraph* Graph : AllGraphs)
	{
		if (FTurboMCPBlueprintRead::GraphMatchesFilter(
				Graph->GetName(), InRequest.GraphFilter))
		{
			Graphs.Add(Graph);
		}
	}

	// A filter that matched nothing is a REFUSAL, not an empty document. The text writer
	// refuses for the same reason: an empty answer looks like "this Blueprint has nothing"
	// when the truth is "your filter is wrong".
	if (InRequest.GraphFilter.Num() > 0 && Graphs.Num() == 0)
	{
		TArray<FString> Names;
		for (const UEdGraph* Graph : AllGraphs)
		{
			Names.Add(Graph->GetName());
		}
		OutError = FString::Printf(
			TEXT("no graph matches '%s'. The graphs are: %s"),
			*FString::Join(InRequest.GraphFilter, TEXT(", ")),
			Names.Num() > 0 ? *FString::Join(Names, TEXT(", ")) : TEXT("(none)"));
		return FString();
	}

	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetBoolField(TEXT("ok"), true);
	Root->SetStringField(TEXT("detail"),
		InDetailLevel == 0 ? TEXT("manifest")
			: InDetailLevel == 1 ? TEXT("signatures")
			: InDetailLevel == 2 ? TEXT("bodies")
			: TEXT("full"));
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

	// Every graph, with its name. At `bodies` and above each also carries its body, as the
	// S-EXPRESSION TEXT rather than reshaped into objects.
	//
	// This is where the "bodies stay strings" decision pays off. `04_FORMATS.md` section 2
	// defines one graph text format, and turning it into JSON objects here would create a
	// second node model for the same nodes: two shapes that could disagree, which is the
	// drift the format rules exist to prevent. A caller parses the graph text with the one
	// parser it already has.
	//
	// `full` IS PASSED STRAIGHT THROUGH to the graph writer, which already knows what it
	// adds: knot nodes named on a link instead of being followed through them, each node's
	// C++ class, and pin indexes. Reimplementing that here would be the second node model
	// the paragraph above refuses, so the level is forwarded and the extra detail arrives
	// inside the body text.
	const bool bWantsBodies = InDetailLevel >= 2;
	const FTurboMCPGraphText::EDetail GraphDetail = InDetailLevel >= 3
		? FTurboMCPGraphText::EDetail::Full
		: FTurboMCPGraphText::EDetail::Bodies;

	// The same writer `R-GRAPH-LOAD` uses, so a body cannot differ between the two tools.
	const FTurboMCPGraphText GraphWriter;
	TArray<FString> BodyNotes;

	TArray<TSharedPtr<FJsonValue>> GraphsJson;
	for (int32 Index = 0; Index < Graphs.Num(); ++Index)
	{
		TSharedRef<FJsonObject> Graph = MakeShared<FJsonObject>();
		Graph->SetStringField(TEXT("name"), Graphs[Index]->GetName());

		if (bWantsBodies)
		{
			Graph->SetStringField(TEXT("body"),
				GraphWriter.Render(Graphs[Index], GraphDetail,
					InBlueprint->GetPathName(), BodyNotes));
		}

		GraphsJson.Add(MakeShared<FJsonValueObject>(Graph));
	}
	Root->SetArrayField(TEXT("graphs"), GraphsJson);

	// Anything the graph writer could not print is carried as a list rather than dropped.
	// The text writer inlines these as `; error:` comments; a JSON caller gets them as
	// values it can test, and their PRESENCE is the signal that a body is incomplete.
	if (BodyNotes.Num() > 0)
	{
		TArray<TSharedPtr<FJsonValue>> NotesJson;
		for (const FString& Note : BodyNotes)
		{
			NotesJson.Add(MakeShared<FJsonValueString>(Note));
		}
		Root->SetArrayField(TEXT("body_notes"), NotesJson);
	}

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
