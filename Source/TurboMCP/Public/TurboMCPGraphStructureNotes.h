// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

// DOCUMENTATION ONLY - THIS FILE DECLARES NOTHING AND DEFINES NOTHING. It is not compiled into a tool and
// contains no code; it records the measurement that gates `E-GRAPH-COLLAPSE` and the reuse path found for
// `E-GRAPH-COPY`, because the workstream that took them ran out of context before it could build either.
//
// NO DECLARATION WAS ADDED TO `TurboMCPToolsets.h`. An earlier workstream on this same row landed nothing and
// a second stopped with an unrestored break test, so leaving the shared header untouched was the priority.
//
// ---------------------------------------------------------------------------------------------
// 1. THE MEASUREMENT THAT GATES `E-GRAPH-COLLAPSE` (row 6.2, T-STRUCT-10): *** REACHABLE - YES ***
// ---------------------------------------------------------------------------------------------
//
// `E-GRAPH-COLLAPSE` is NOT blocked. `FBlueprintEditor::CollapseNodesIntoGraph` is `UE_API` and every piece
// it is assembled from outside the editor is `static`:
//
//   FBlueprintEditorUtils::CreateNewGraph          BlueprintEditorUtils.h:338   static UNREALED_API
//   FBlueprintEditorUtils::AddFunctionGraph        BlueprintEditorUtils.h:399   static
//   FBlueprintEditorUtils::AddMacroGraph           BlueprintEditorUtils.h:430   static UNREALED_API
//   FBlueprintEditorUtils::FindUniqueKismetName    BlueprintEditorUtils.h:1561  static UNREALED_API
//   FEdGraphSchemaAction_K2NewNode::SpawnNode<>              EdGraphSchema_K2_Actions.h:106,125  static
//   FEdGraphSchemaAction_K2NewNode::SpawnNodeFromTemplate<>  EdGraphSchema_K2_Actions.h:95       static
//   FBlueprintEditor::CollapseNodesIntoGraph        BlueprintEditor.h:1125       UE_API
//
// AND THIS PROJECT ALREADY PROVES THE HALF THAT WAS IN DOUBT: a COMMANDLET builds function graphs with an
// entry and a result node, at `TurboMCPAcceptanceAssetCommandlet.cpp:522-541`, using exactly
// `CreateNewGraph` -> `AddFunctionGraph<UFunction>` -> `GetNodesOfClass<UK2Node_FunctionEntry>` plus a
// `UK2Node_FunctionResult`. So `UK2Node_FunctionEntry` and `UK2Node_FunctionResult` ARE creatable outside an
// open asset editor, measured by existing working code rather than by reading.
//
// THE ONLY EDITOR MEMBER `CollapseNodesIntoGraph` ACTUALLY NEEDS IS `GetBlueprintObj()`, AND IT IS USED IN
// TWO PLACES, BOTH NARROW, over its 290-line body (BlueprintEditor.cpp:8825-9115):
//
//   L8856 and L9066, plus ExtractEventTemplateForFunction at L8854 - ALL THREE INSIDE THE
//   `InGatewayNode->GetClass() == UK2Node_CallFunction` AND `UK2Node_CustomEvent` BRANCHES.
//
// Every other call in the body is a `UEdGraphNode`/`UEdGraphPin` member (`Modify`, `MakeLinkTo`, `FindPin`,
// `BreakAllPinLinks`, `Rename`, `SnapToGrid`, `IsExecPin`), a `UEdGraph` member, a `UEdGraphSchema_K2`
// member, or the FREE FUNCTION `SortPinsByConnectionPosition` (`BlueprintEditor.cpp:8767`, declared
// `static`). THIS ENUMERATION IS THE MEASUREMENT: a collapse whose selection contains NO CustomEvent and
// whose gateway is NOT a `UK2Node_CallFunction` never reaches an editor member.
//
// *** WHAT REMAINS UNKNOWN, STATED SO IT IS NOT MISTAKEN FOR SETTLED: whether the FBlueprintEditor INSTANCE
// ITSELF can be constructed outside an asset editor, because these are non-virtual member functions and
// `this` must still be a valid object even when no member is dereferenced. The macro path
// (BlueprintEditor.cpp:9203-9260) shows the minimal gateway - `SpawnNode<UK2Node_MacroInstance>` capturing
// the destination graph and calling `SetMacroGraph`, then `AddMacroGraph` for the sink nodes - and every part
// of THAT is static too. THE PRAGMATIC PATH THAT AVOIDS THE QUESTION ENTIRELY IS TO REPRODUCE THE SEQUENCE
// rather than call the member: the body's operations on `UEdGraph`/`UEdGraphNode`/pins are all public, and
// the entry and result nodes are proven creatable here. THAT IS THE RECOMMENDATION. ***
//
// ---------------------------------------------------------------------------------------------
// 2. THE REUSE PATH FOR `E-GRAPH-COPY` (row 6.3, T-STRUCT-11)
// ---------------------------------------------------------------------------------------------
//
// COPY takes node IDs in a source graph and produces nodes in a target graph, so unlike APPEND it does NOT
// have a type id to resolve: it has a CLASS and a set of PROPERTY VALUES. `FTurboMCPNodeType::SpawnNodeOfType`
// (TurboMCPNodeType.h:182) RESOLVES A TYPE ID through the action database, so it does NOT fit - measured, and
// worth recording because it is the obvious thing to reach for.
//
// *** THE MECHANISM THAT DOES FIT IS `FEdGraphSchemaAction_K2NewNode::SpawnNodeFromTemplate<NodeType>(
// ParentGraph, InTemplateNode, Location, bSelectNewNode)` (`EdGraphSchema_K2_Actions.h:95`), WHICH IS STATIC
// AND CARRIES THE TEMPLATE'S OWN PROPERTY VALUES. The source node IS the template, so a `UK2Node_CallFunction`
// copied this way keeps the function it calls and a `UK2Node_VariableGet` keeps the variable it reads,
// WITHOUT ANY PER-CLASS COPY CODE. The engine itself uses it for exactly this, at
// `EdGraphSchema_K2.cpp:2916`. THE COPY IS THEN: duplicate each selected node this way, remap the links that
// stay inside the selection, and REPORT BY NAME every link that crosses the selection and has nothing to
// point at in the target - which `03_TOOLS.md` requires be REPORTED rather than silently dropped. ***
//
// THE FIXTURE CAVEAT, MEASURED BY THE PREVIOUS WORKSTREAM AND WORTH REPEATING HERE BECAUSE IT CHANGES WHAT
// T-STRUCT-11 PROVES: `TA-2` IS NOT A SECOND ASSET. `TurboMCPKitCommandlet.cpp:939-940` sets
// `Ta2.Id = "TA-2"` and `Ta2.Name = "TA01_HandLaid"` - THE SAME BLUEPRINT - and its "event graph" is
// `Kit->UbergraphPages[0]` (`:918`). SO THE ROW, AS WRITTEN, TESTS COPYING BETWEEN TWO GRAPHS OF ONE
// BLUEPRINT AND NOT "between graphs and Blueprints". `BP_TowerArray` and `TA07_Card` exist on disk for a true
// cross-asset case. **A SUITE MUST STATE WHICH OF THE TWO IT PROVED.**
