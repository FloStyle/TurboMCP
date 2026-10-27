// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
#include "Tools/TurboMCPToolsets.h"
#include "TurboMCPAssetResolver.h"
#include "TurboMCPTypeParser.h"
#include "TurboMCPNodeId.h"
#include "Editor.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphUtilities.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "K2Node_CallFunction.h"
#include "K2Node_CallParentFunction.h"
#include "K2Node_Variable.h"
#include "K2Node_BaseMCDelegate.h"
#include "K2Node_MacroInstance.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_Event.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "WidgetBlueprint.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/WidgetNavigation.h"
#include "Kismet2/Kismet2NameValidators.h"
#include "Components/PanelWidget.h"
#include "Components/PanelSlot.h"
#include "UObject/UnrealType.h"
#include "UObject/StrongObjectPtr.h"

namespace TurboMCPMergeTransfer
{
static FString LoadBP(const FString& Asset,UBlueprint*& BP)
{
    if(!IsInGameThread() || !GEditor || GEditor->IsPlaySessionInProgress() || GEditor->IsTransactionActive())
        return TEXT("error: Requires an idle editor outside PIE and transactions. Nothing changed.");
    FTurboMCPAssetResolver::FResolved R;FString Error=FTurboMCPAssetResolver::Resolve(Asset,R);
    if(!Error.IsEmpty())return Error;
    BP=LoadObject<UBlueprint>(nullptr,*R.ObjectPath);
    if(!BP)return TEXT("error: Asset is not a Blueprint. Nothing changed.");
    if(BP->BlueprintType==BPTYPE_Interface || BP->BlueprintType==BPTYPE_MacroLibrary)
        return TEXT("error: Use an executable Blueprint or Widget Blueprint, not an interface or macro-library asset. Nothing changed.");
    return FString();
}
static FString Pair(const FString& Source,const FString& Target,UBlueprint*& S,UBlueprint*& T)
{
    FString E=LoadBP(Source,S);if(!E.IsEmpty())return E;
    E=LoadBP(Target,T);if(!E.IsEmpty())return E;
    if(S==T)return TEXT("error: Source and target must be different Blueprints. Nothing changed.");
    if(!S->ParentClass || !T->ParentClass || !T->ParentClass->IsChildOf(S->ParentClass))
        return TEXT("error: Target parent must derive from the source parent so inherited references remain valid. Nothing changed.");
    return FString();
}
static UEdGraph* GraphByName(UBlueprint* BP,const FString& Name)
{
    TArray<UEdGraph*> Graphs;BP->GetAllGraphs(Graphs);UEdGraph* Found=nullptr;
    for(UEdGraph* G:Graphs)if(G && G->GetName().Equals(Name,ESearchCase::IgnoreCase)){if(Found && Found!=G)return nullptr;Found=G;}
    return Found;
}
static const FBPVariableDescription* Variable(UBlueprint* BP,FName Name)
{
    return BP->NewVariables.FindByPredicate([&](const FBPVariableDescription& V){return V.VarName==Name;});
}
static bool HasVariable(UBlueprint* BP,FName Name)
{
    return Variable(BP,Name) || (BP->SkeletonGeneratedClass && FindFProperty<FProperty>(BP->SkeletonGeneratedClass,Name));
}
static bool HasFunction(UBlueprint* BP,FName Name)
{
    return GraphByName(BP,Name.ToString()) || (BP->SkeletonGeneratedClass && BP->SkeletonGeneratedClass->FindFunctionByName(Name));
}
static FString End(UBlueprint* BP, bool HasSource = true)
{
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
    return FString(TEXT("\nStructural refresh requested; compile success is not established. Nothing saved. Undo is not guaranteed across structural compilation. Read back and compile the target before saving.")) + (HasSource ? TEXT(" Source unchanged.") : TEXT(""));
}
static bool ValidName(const FString& N)
{
    if(N.IsEmpty() || N.Len()>=NAME_SIZE || N.Equals(TEXT("None"),ESearchCase::IgnoreCase) || (!FChar::IsAlpha(N[0]) && N[0]!='_'))return false;
    for(TCHAR C:N)if(!FChar::IsAlnum(C) && C!='_')return false;
    return true;
}
static FString Signature(const FString& Text,TArray<TPair<FName,FEdGraphPinType>>& Pins)
{
    TArray<FString> Parts;int32 Depth=0,Start=0;
    for(int32 I=0;I<Text.Len();++I){if(Text[I]=='<')++Depth;else if(Text[I]=='>')--Depth;else if(Text[I]==',' && Depth==0){Parts.Add(Text.Mid(Start,I-Start));Start=I+1;}if(Depth<0)return TEXT("Unbalanced type brackets.");}
    if(Depth)return TEXT("Unbalanced type brackets.");
    if(!Text.TrimStartAndEnd().IsEmpty())Parts.Add(Text.Mid(Start));
    TSet<FName> Names;
    for(FString Part:Parts){FString N,T;if(!Part.Split(TEXT(":"),&N,&T))return TEXT("Use Name:Type pairs separated by commas.");N=N.TrimStartAndEnd();T=T.TrimStartAndEnd();
        if(!ValidName(N)||Names.Contains(FName(*N)))return TEXT("Parameter names must be distinct valid identifiers.");
        FEdGraphPinType PT;FString E;if(!FTurboMCPTypeParser::Resolve(T,PT,E))return E;
        Names.Add(FName(*N));Pins.Emplace(FName(*N),PT);
    }return FString();
}
static FString Dispatcher(const FString& Asset,const FString& Name,const FString& Inputs,bool Apply)
{
    UBlueprint* BP=nullptr;FString E=LoadBP(Asset,BP);if(!E.IsEmpty())return E;
    if(!ValidName(Name))return TEXT("error: Dispatcher name must be a valid identifier. Nothing changed.");
    TArray<TPair<FName,FEdGraphPinType>> Pins;E=Signature(Inputs,Pins);if(!E.IsEmpty())return TEXT("error: ")+E+TEXT(" Nothing changed.");
    if(HasVariable(BP,FName(*Name)) || HasFunction(BP,FName(*Name)))return TEXT("error: A member already uses this name. Nothing changed.");
    FString Out=FString::Printf(TEXT("asset: %s\ndispatcher: %s\ninputs: %s"),*BP->GetPathName(),*Name,*Inputs);
    if(!Apply)return TEXT("preview: Create event dispatcher. No changes made.\n")+Out;
    FEdGraphPinType DT;DT.PinCategory=UEdGraphSchema_K2::PC_MCDelegate;
    if(!FBlueprintEditorUtils::AddMemberVariable(BP,FName(*Name),DT))return TEXT("error: Unreal refused the dispatcher variable. No save requested.");
    UEdGraph* G=FBlueprintEditorUtils::CreateNewGraph(BP,FName(*Name),UEdGraph::StaticClass(),UEdGraphSchema_K2::StaticClass());
    const auto* Schema=GetDefault<UEdGraphSchema_K2>();G->bEditable=false;
    Schema->CreateDefaultNodesForGraph(*G);Schema->CreateFunctionGraphTerminators(*G,(UClass*)nullptr);
    Schema->AddExtraFunctionFlags(G,FUNC_BlueprintCallable|FUNC_BlueprintEvent|FUNC_Public);Schema->MarkFunctionEntryAsEditable(G,true);
    TArray<UK2Node_FunctionEntry*> Entries;G->GetNodesOfClass(Entries);
    if(Entries.Num()!=1)return TEXT("error: Unreal did not create one dispatcher entry. The variable may already exist; inspect target before retrying. Nothing saved.");
    for(const auto& P:Pins)Entries[0]->CreateUserDefinedPin(P.Key,P.Value,EGPD_Output,false);
    BP->DelegateSignatureGraphs.Add(G);
    return TEXT("edited: Event dispatcher created.\n")+Out+End(BP, false);
}

// Dispatcher pin types contain an asset-specific signature object. Compare
// storage shape here and compare the callable signature separately below.
static bool SameMemberType(FEdGraphPinType Left, FEdGraphPinType Right)
{
    if (Left.PinCategory == UEdGraphSchema_K2::PC_MCDelegate && Right.PinCategory == Left.PinCategory)
    {
        Left.PinSubCategoryObject = nullptr;
        Right.PinSubCategoryObject = nullptr;
        Left.PinSubCategoryMemberReference = FSimpleMemberReference();
        Right.PinSubCategoryMemberReference = FSimpleMemberReference();
    }
    return Left == Right;
}
static UFunction* DispatcherSignature(UBlueprint* BP, FName Name)
{
    const auto* Property = BP->SkeletonGeneratedClass
        ? FindFProperty<FMulticastDelegateProperty>(BP->SkeletonGeneratedClass, Name) : nullptr;
    return Property ? Property->SignatureFunction : nullptr;
}

// Event declarations are distinct from ordinary functions with the same signature.
static UK2Node_CustomEvent* CustomEvent(UBlueprint* BP, FName Name)
{
    UK2Node_CustomEvent* Found = nullptr;
    for (UEdGraph* Page : BP->UbergraphPages) if (Page)
        for (UEdGraphNode* Node : Page->Nodes)
            if (auto* Event = Cast<UK2Node_CustomEvent>(Node)) if (Event->CustomFunctionName == Name)
            { if (Found) return nullptr; Found = Event; }
    return Found;
}
static bool SameEvent(UK2Node_CustomEvent* A, UK2Node_CustomEvent* B)
{
    if (!A || !B || A->FunctionFlags != B->FunctionFlags || A->bCallInEditor != B->bCallInEditor
        || A->bIsDeprecated != B->bIsDeprecated || A->DeprecationMessage != B->DeprecationMessage
        || A->UserDefinedPins.Num() != B->UserDefinedPins.Num()) return false;
    for (int32 Index=0; Index<A->UserDefinedPins.Num(); ++Index)
    {
        const auto& Left=A->UserDefinedPins[Index]; const auto& Right=B->UserDefinedPins[Index];
        if (!Left.IsValid() || !Right.IsValid() || Left->PinName!=Right->PinName
            || !(Left->PinType==Right->PinType) || Left->DesiredPinDirection!=Right->DesiredPinDirection
            || Left->PinDefaultValue!=Right->PinDefaultValue) return false;
    }
    return true;
}
static bool EmptyEventDeclaration(UK2Node_CustomEvent* Event)
{
    if (!Event || !Event->NodeComment.IsEmpty() || Event->GetDesiredEnabledState()!=ENodeEnabledState::Enabled) return false;
    for (UEdGraphPin* Pin : Event->Pins)
        if (!Pin || !Pin->LinkedTo.IsEmpty() || Pin->bOrphanedPin || Pin->ParentPin || !Pin->SubPins.IsEmpty()) return false;
    return true;
}

static FString Members(const FString& Source,const FString& Target,const FString& Exclude,bool Apply)
{
    UBlueprint *S=nullptr,*T=nullptr;FString E=Pair(Source,Target,S,T);if(!E.IsEmpty())return E;
    TArray<const FBPVariableDescription*> Vars;TArray<UEdGraph*> Functions,Delegates;FString Summary;
    TSet<FName> Known, Skipped; TArray<UK2Node_CustomEvent*> Events;
    for (const auto& V:S->NewVariables) Known.Add(V.VarName);
    for (UEdGraph* G:S->FunctionGraphs) if(G) Known.Add(G->GetFName());
    for (UEdGraph* G:S->DelegateSignatureGraphs) if(G) Known.Add(G->GetFName());
    for (UEdGraph* G:S->UbergraphPages) if(G) for(UEdGraphNode* N:G->Nodes)
        if(auto* C=Cast<UK2Node_CustomEvent>(N)) Known.Add(C->CustomFunctionName);
    for (const FString& Name:FTurboMCPAssetResolver::SplitList(Exclude))
    {
        if(!Known.Contains(FName(*Name))) return TEXT("error: Unknown excluded source member: ")+Name+TEXT(". Use exact member names. Nothing changed.");
        Skipped.Add(FName(*Name)); Summary+=TEXT("\nexcluded: ")+Name;
    }
    for (UEdGraph* G:S->UbergraphPages) if(G) for(UEdGraphNode* N:G->Nodes)
        if(auto* C=Cast<UK2Node_CustomEvent>(N))
        {
            if(Skipped.Contains(C->CustomFunctionName)) continue;
            if(CustomEvent(S,C->CustomFunctionName)!=C) return TEXT("error: Ambiguous source custom event. Nothing changed.");
            if(HasFunction(T,C->CustomFunctionName))
            {
                if(!SameEvent(C,CustomEvent(T,C->CustomFunctionName))) return TEXT("error: Target custom-event signature or settings conflict: ")+C->CustomFunctionName.ToString()+TEXT(". Nothing changed.");
                continue;
            }
            if(HasVariable(T,C->CustomFunctionName)) return TEXT("error: Target variable conflicts with custom event: ")+C->CustomFunctionName.ToString();
            if(!S->SkeletonGeneratedClass || !S->SkeletonGeneratedClass->FindFunctionByName(C->CustomFunctionName)) return TEXT("error: Compile the source before preparing custom events. Nothing changed.");
            Events.Add(C); Summary+=TEXT("\ncustom-event declaration: ")+C->CustomFunctionName.ToString();
        }
    FString EventPageName=TEXT("TurboMCP_MemberDeclarations");
    for(int32 Suffix=2; GraphByName(T,EventPageName); ++Suffix) EventPageName=FString::Printf(TEXT("TurboMCP_MemberDeclarations_%d"),Suffix);
    if(!Events.IsEmpty()) Summary+=TEXT("\nnew declaration event graph: ")+EventPageName;

    for(const auto& V:S->NewVariables)
    {
        if(Skipped.Contains(V.VarName)) continue;
        if(const auto* Existing=Variable(T,V.VarName)){if(!SameMemberType(Existing->VarType,V.VarType))return TEXT("error: Target variable type conflict: ")+V.VarName.ToString();continue;}
        if(HasVariable(T,V.VarName)){
            FEdGraphPinType ExistingType;const FProperty* P=T->SkeletonGeneratedClass?FindFProperty<FProperty>(T->SkeletonGeneratedClass,V.VarName):nullptr;
            if(!P || !GetDefault<UEdGraphSchema_K2>()->ConvertPropertyToPinType(P,ExistingType) || !SameMemberType(ExistingType,V.VarType))return TEXT("error: Inherited/widget member type conflict: ")+V.VarName.ToString();
            continue;
        }
        if(HasFunction(T,V.VarName))return TEXT("error: Target function conflicts with source variable: ")+V.VarName.ToString();
        Vars.Add(&V);Summary+=TEXT("\nvariable: ")+V.VarName.ToString();
    }
    // A same-named target is usable only with the same callable signature.
    // Never treat a conflicting function or dispatcher as an already merged member.
    for(UEdGraph* G:S->DelegateSignatureGraphs)if(G)
    {
        if(Skipped.Contains(G->GetFName())) continue;
        UEdGraph* Existing=GraphByName(T,G->GetName());
        if(Existing)
        {
            UFunction* SF=DispatcherSignature(S,G->GetFName());
            UFunction* TF=DispatcherSignature(T,G->GetFName());
            if(!T->DelegateSignatureGraphs.Contains(Existing) || !SF || !TF || !SF->IsSignatureCompatibleWith(TF))
                return TEXT("error: Target dispatcher signature conflicts or is not compiled: ")+G->GetName()+TEXT(". Nothing changed.");
        }
        else {Delegates.Add(G);Summary+=TEXT("\ndispatcher signature: ")+G->GetName();}
    }
    for(UEdGraph* G:S->FunctionGraphs)if(G)
    {
        if(Skipped.Contains(G->GetFName())) continue;
        UFunction* SF=S->SkeletonGeneratedClass?S->SkeletonGeneratedClass->FindFunctionByName(G->GetFName()):nullptr;
        if(!SF)return TEXT("error: Compile the source before preparing function signatures. Nothing changed.");
        if(HasFunction(T,G->GetFName()))
        {
            UFunction* TF=T->SkeletonGeneratedClass?T->SkeletonGeneratedClass->FindFunctionByName(G->GetFName()):nullptr;
            if(!TF || !SF->IsSignatureCompatibleWith(TF))return TEXT("error: Target function signature conflicts or is not compiled: ")+G->GetName()+TEXT(". Nothing changed.");
            continue;
        }
        if(HasVariable(T,G->GetFName()))return TEXT("error: Target variable conflicts with source function: ")+G->GetName();
        Functions.Add(G);Summary+=TEXT("\nfunction declaration: ")+G->GetName();
    }
    if(Vars.IsEmpty() && Functions.IsEmpty() && Delegates.IsEmpty() && Events.IsEmpty())return TEXT("unchanged: No missing selected variables, functions, dispatchers or custom events. Widget trees, interfaces and graph bodies require their dedicated tools.")+Summary;
    if(!Apply)return TEXT("preview: Create missing declarations only; existing members are retained. No changes made.")+Summary+TEXT("\nThen copy widget subtrees and graph bodies. Interfaces are added separately. This is not a complete asset merge.");
    TArray<FBPVariableDescription> Copies;for(const auto* V:Vars){
        auto& Copy=Copies.Add_GetRef(*V);
        if(V->VarType.PinCategory!=UEdGraphSchema_K2::PC_MCDelegate && S->GeneratedClass){
            const FProperty* P=FindFProperty<FProperty>(S->GeneratedClass,V->VarName);UObject* CDO=S->GeneratedClass->GetDefaultObject();
            if(P && CDO){Copy.DefaultValue.Reset();P->ExportTextItem_Direct(Copy.DefaultValue,P->ContainerPtrToValuePtr<void>(CDO),nullptr,CDO,PPF_None);}
        }
    } // Source descriptors survive target structural refreshes.
    for(const auto& V:Copies)
    {
        if(!FBlueprintEditorUtils::AddMemberVariable(T,V.VarName,V.VarType,V.DefaultValue))return TEXT("error: Member preparation stopped after possible earlier changes; inspect target. No save requested. Failed variable: ")+V.VarName.ToString();
        if(auto* Added=T->NewVariables.FindByPredicate([&](const FBPVariableDescription& X){return X.VarName==V.VarName;}))
        {const FGuid NewGuid=Added->VarGuid;*Added=V;Added->VarGuid=NewGuid;}
    }
    for(UEdGraph* G:Delegates)
    {
        UEdGraph* Copy=FEdGraphUtilities::CloneGraph(G,T);if(!Copy)return TEXT("error: Dispatcher signature copy failed after possible earlier member changes. Inspect target before retrying. Nothing saved.");Copy->Rename(*G->GetName(),T,REN_DontCreateRedirectors|REN_NonTransactional);Copy->GraphGuid=FGuid::NewGuid();T->DelegateSignatureGraphs.Add(Copy);
    }
    for(UEdGraph* G:Functions)
    {
        UEdGraph* Copy=FBlueprintEditorUtils::CreateNewGraph(T,G->GetFName(),UEdGraph::StaticClass(),UEdGraphSchema_K2::StaticClass());
        FBlueprintEditorUtils::AddFunctionGraph(T,Copy,true,S->SkeletonGeneratedClass->FindFunctionByName(G->GetFName()));
    }
    if(!Events.IsEmpty())
    {
        UEdGraph* Page=FBlueprintEditorUtils::CreateNewGraph(T,FName(*EventPageName),UEdGraph::StaticClass(),UEdGraphSchema_K2::StaticClass());
        T->UbergraphPages.Add(Page);
        int32 Y=0;
        for(UK2Node_CustomEvent* SourceEvent:Events)
        {
            auto* Added=UK2Node_CustomEvent::CreateFromFunction(FVector2D(0,Y),Page,SourceEvent->CustomFunctionName.ToString(),S->SkeletonGeneratedClass->FindFunctionByName(SourceEvent->CustomFunctionName),false);
            if(!Added) return TEXT("error: Custom-event creation failed after possible earlier member changes. Inspect target. Nothing saved.");
            // Reflection can normalize pin qualifiers and omit editor defaults.
            // Own fresh descriptors so the target never shares mutable source pin data.
            Added->UserDefinedPins.Reset();
            for(const auto& Pin:SourceEvent->UserDefinedPins)
                Added->UserDefinedPins.Add(MakeShared<FUserPinInfo>(*Pin));
            Added->ReconstructNode();
            Added->FunctionFlags=SourceEvent->FunctionFlags; Added->bCallInEditor=SourceEvent->bCallInEditor;
            Added->bIsDeprecated=SourceEvent->bIsDeprecated; Added->DeprecationMessage=SourceEvent->DeprecationMessage;
            Y+=160+48*Added->UserDefinedPins.Num();
        }
    }
    return TEXT("edited: Missing member declarations created; graph bodies were not copied.")+Summary+End(T);
}

// Recognize only the minimal inherited override scaffold. Position changes are
// harmless, but local state, comments, extra nodes, defaults and alternate wiring
// are treated as authored logic and must not be overwritten.
static bool IsParentScaffold(UBlueprint* BP, UEdGraph* Graph)
{
    UFunction* Parent = BP->ParentClass ? BP->ParentClass->FindFunctionByName(Graph->GetFName()) : nullptr;
    if (!Parent) return false;
    UK2Node_FunctionEntry* Entry = nullptr;
    UK2Node_FunctionResult* Result = nullptr;
    UK2Node_CallParentFunction* Call = nullptr;
    for (UEdGraphNode* N : Graph->Nodes)
    {
        if (!N || !N->NodeComment.IsEmpty() || N->GetDesiredEnabledState() != ENodeEnabledState::Enabled) return false;
        if (auto* E = Cast<UK2Node_FunctionEntry>(N)) { if (Entry || !E->LocalVariables.IsEmpty()) return false; Entry = E; }
        else if (auto* R = Cast<UK2Node_FunctionResult>(N)) { if (Result) return false; Result = R; }
        else if (auto* C = Cast<UK2Node_CallParentFunction>(N)) { if (Call) return false; Call = C; }
        else return false;
    }
    if (!Entry || !Call) return false;
    const UFunction* Called = Call->GetTargetFunction();
    const UClass* ExpectedOwner = Parent->GetOwnerClass();
    const UClass* ActualOwner = Called ? Called->GetOwnerClass() : nullptr;
    // Reflection may resolve the same parent through its skeleton or generated class.
    if (!Called || Called->GetFName() != Parent->GetFName() || !ActualOwner
        || !(ActualOwner == ExpectedOwner || (ActualOwner->ClassGeneratedBy
            && ActualOwner->ClassGeneratedBy == ExpectedOwner->ClassGeneratedBy))) return false;
    for (UEdGraphNode* N : Graph->Nodes)
        for (UEdGraphPin* P : N->Pins)
        {
            if (!P || P->bOrphanedPin || P->ParentPin || !P->SubPins.IsEmpty() || P->LinkedTo.Num() > 1) return false;
            if (P->LinkedTo.IsEmpty())
            {
                if (P->DefaultObject || !P->DefaultTextValue.IsEmpty() || P->DefaultValue != P->AutogeneratedDefaultValue) return false;
                if (P->Direction == EGPD_Input && P->PinName != UEdGraphSchema_K2::PN_Self) return false;
                if (P->Direction == EGPD_Output && !(N == Call && !Result && P->PinType.PinCategory == TEXT("exec"))) return false;
                continue;
            }
            UEdGraphPin* Q = P->LinkedTo[0];
            if (!Q || !Q->LinkedTo.Contains(P)) return false;
            UEdGraphPin* From = P->Direction == EGPD_Output ? P : Q;
            UEdGraphPin* To = From == P ? Q : P;
            if (From->Direction != EGPD_Output || To->Direction != EGPD_Input) return false;
            if (!((From->GetOwningNode() == Entry && To->GetOwningNode() == Call)
                || (From->GetOwningNode() == Call && To->GetOwningNode() == Result))) return false;
            const bool Exec = From->PinType.PinCategory == TEXT("exec") && To->PinType.PinCategory == TEXT("exec");
            if (!Exec && (From->PinName != To->PinName || !(From->PinType == To->PinType))) return false;
        }
    return true;
}
static FString Graph(const FString& Source,const FString& SourceGraph,const FString& Target,const FString& TargetGraph,bool Apply)
{
    UBlueprint *S=nullptr,*T=nullptr;FString E=Pair(Source,Target,S,T);if(!E.IsEmpty())return E;
    UEdGraph* G=GraphByName(S,SourceGraph);if(!G)return TEXT("error: Source graph is missing or ambiguous. Nothing changed.");
    const bool Function=S->FunctionGraphs.Contains(G),Event=S->UbergraphPages.Contains(G),Macro=S->MacroGraphs.Contains(G);
    bool InterfaceFunction=false;for(const auto& I:S->ImplementedInterfaces)InterfaceFunction|=I.Graphs.Contains(G);
    if(!Function && !Event && !Macro && !InterfaceFunction)return TEXT("error: Select an owned function, interface function, macro or event graph. Nothing changed.");
    const FString Name=TargetGraph.IsEmpty()?G->GetName():TargetGraph;
    if(!ValidName(Name))return TEXT("error: TargetGraph must be a valid graph identifier. Nothing changed.");
    UEdGraph* Old=GraphByName(T,Name);
    const bool AppendEvent = Event && Old && T->UbergraphPages.Contains(Old);
    if (Event && Old && !AppendEvent) return TEXT("error: An event graph can only append into an owned event page. Nothing changed.");
    if (AppendEvent && !G->SubGraphs.IsEmpty()) return TEXT("error: Append does not support nested source graphs yet. Transfer to a new page instead. Nothing changed.");
    const bool ParentScaffold = Old && IsParentScaffold(T, Old);
    if(Old && !AppendEvent)
    {
        // Only declarations produced by member/interface preparation may be filled.
        // Never overwrite executable logic, even when graph names match.
        if(Event || Macro)return TEXT("error: Target graph already exists. Choose a new name; existing graphs are not overwritten.");
        for(UEdGraphNode* N:Old->Nodes)if(!ParentScaffold && !N->IsA<UK2Node_FunctionEntry>() && !N->IsA<UK2Node_FunctionResult>())return TEXT("error: Target function contains authored logic. Only an empty declaration or untouched parent-call scaffold can be filled. Nothing changed.");
        TArray<UK2Node_FunctionEntry*> OE;Old->GetNodesOfClass(OE);
        if(OE.Num()!=1 || !OE[0]->LocalVariables.IsEmpty())return TEXT("error: Target declaration has local state or an invalid entry. Nothing changed.");
        UFunction* SF=S->SkeletonGeneratedClass?S->SkeletonGeneratedClass->FindFunctionByName(G->GetFName()):nullptr;
        UFunction* TF=T->SkeletonGeneratedClass?T->SkeletonGeneratedClass->FindFunctionByName(Old->GetFName()):nullptr;
        if(!SF || !TF || !SF->IsSignatureCompatibleWith(TF))return TEXT("error: Source and target function signatures differ or are not compiled. Nothing changed.");
    }
    else if(HasVariable(T,FName(*Name)))return TEXT("error: Target name conflicts with a variable. Nothing changed.");
    // Move the incoming group as a unit. Unmeasured nodes use conservative
    // dimensions; this is separation, not a rendered-layout certification.
    int32 AppendOffsetY = 0;
    if (AppendEvent && !Old->Nodes.IsEmpty() && !G->Nodes.IsEmpty())
    {
        int64 Bottom = MIN_int32, Top = MAX_int32, IncomingBottom = MIN_int32;
        for (UEdGraphNode* Node : Old->Nodes) if (Node)
            Bottom = FMath::Max(Bottom, int64(Node->NodePosY) + FMath::Max(Node->NodeHeight, 512));
        for (UEdGraphNode* Node : G->Nodes) if (Node)
        {
            Top = FMath::Min(Top, int64(Node->NodePosY));
            IncomingBottom = FMath::Max(IncomingBottom, int64(Node->NodePosY) + FMath::Max(Node->NodeHeight, 512));
        }
        const int64 Offset = Bottom + 192 - Top;
        if (Offset < MIN_int32 || Offset > MAX_int32 || IncomingBottom + Offset > MAX_int32)
            return TEXT("error: Appended graph positions would exceed supported coordinates. Nothing changed.");
        AppendOffsetY = int32(Offset);
    }
    TArray<UK2Node_FunctionEntry*> Entries;G->GetNodesOfClass(Entries);
    // Calls to custom events carried by this graph are internal dependencies.
    // Other graphs must still be transferred after their event definitions.
    TSet<FName> IncomingEvents;
    for (UEdGraphNode* Node : G->Nodes)
        if (const auto* EventNode = Cast<UK2Node_CustomEvent>(Node))
            IncomingEvents.Add(EventNode->CustomFunctionName);
    UFunction* ParentFunction = T->ParentClass ? T->ParentClass->FindFunctionByName(FName(*Name)) : nullptr;
    if (Function && ParentFunction && Name != G->GetName())
        return TEXT("error: An inherited override cannot be renamed during transfer. Keep its original name. Nothing changed.");
    // Native/override event entry points are unique within a Blueprint. Custom
    // events use the more precise empty-declaration compatibility path below.
    if (AppendEvent)
        for (UEdGraphNode* Node : G->Nodes)
            if (auto* Incoming = Cast<UK2Node_Event>(Node); Incoming && !Incoming->IsA<UK2Node_CustomEvent>())
                for (UEdGraph* Page : T->UbergraphPages) if (Page)
                    for (UEdGraphNode* Existing : Page->Nodes)
                        if (auto* Other = Cast<UK2Node_Event>(Existing); Other && Other->EventReference.GetMemberName() == Incoming->EventReference.GetMemberName())
                            return TEXT("error: A matching event entry point already exists in the target. Nothing changed; choose distinct chains instead.");
    FString Missing; TArray<UK2Node_CustomEvent*> ReplaceEvents;
    for(UEdGraphNode* N:G->Nodes)
    {
        if(auto* V=Cast<UK2Node_Variable>(N))
        {
            if(!V->VariableReference.IsLocalScope() && V->VariableReference.IsSelfContext() && !HasVariable(T,V->GetVarName()))Missing+=TEXT("\nvariable/widget: ")+V->GetVarName().ToString();
        }
        if(auto* C=Cast<UK2Node_CallFunction>(N))
        {
            if(C->FunctionReference.IsSelfContext() && C->FunctionReference.GetMemberName()!=G->GetFName() && !IncomingEvents.Contains(C->FunctionReference.GetMemberName()) && !HasFunction(T,C->FunctionReference.GetMemberName()))Missing+=TEXT("\nfunction: ")+C->FunctionReference.GetMemberName().ToString();
            if(!C->FunctionReference.IsSelfContext() && !C->GetTargetFunction())Missing+=TEXT("\nunresolved external function: ")+C->FunctionReference.GetMemberName().ToString();
        }
        if(auto* D=Cast<UK2Node_BaseMCDelegate>(N))if(D->DelegateReference.IsSelfContext() && !HasVariable(T,D->DelegateReference.GetMemberName()))Missing+=TEXT("\ndispatcher: ")+D->DelegateReference.GetMemberName().ToString();
        if(auto* M=Cast<UK2Node_MacroInstance>(N))if(UEdGraph* MG=M->GetMacroGraph())
        {if(MG->GetOuter()==S && !T->MacroGraphs.Contains(GraphByName(T,MG->GetName())))Missing+=TEXT("\nmacro: ")+MG->GetName();}
        if(auto* C=Cast<UK2Node_CustomEvent>(N))if(HasFunction(T,C->CustomFunctionName))
        {
            auto* Existing=CustomEvent(T,C->CustomFunctionName);
            if(SameEvent(C,Existing) && EmptyEventDeclaration(Existing)) ReplaceEvents.Add(Existing);
            else Missing+=TEXT("\nconflicting or authored event: ")+C->CustomFunctionName.ToString();
        }
    }
    if(!Missing.IsEmpty())return TEXT("error: Prepare these dependencies or resolve these conflicts before graph transfer. Nothing changed.")+Missing+TEXT("\nUse MergeMembers, InterfaceAdd and WidgetSubtreeCopy as appropriate.");
    FString Summary=FString::Printf(TEXT("\nsource: %s:%s\ntarget: %s:%s\nnodes: %d; existing empty declaration: %s"),*S->GetPathName(),*G->GetName(),*T->GetPathName(),*Name,G->Nodes.Num(),Old?TEXT("yes"):TEXT("no"));
    for(auto* Existing:ReplaceEvents) Summary+=TEXT("\nreplace empty event declaration: ")+Existing->CustomFunctionName.ToString();
    if (ParentScaffold) Summary += TEXT("\ntarget scaffold: replace the untouched parent-call scaffold with the source graph body");
    // Use source GUIDs for the plan; clones receive new GUIDs on application.
    for (UEdGraphNode* Node : G->Nodes)
    {
        Summary += FString::Printf(TEXT("\nnode: %s | %s"), *Node->NodeGuid.ToString(), *Node->GetNodeTitle(ENodeTitleType::ListView).ToString());
        for (UEdGraphPin* Pin : Node->Pins)
            if (Pin && Pin->Direction == EGPD_Output)
                for (UEdGraphPin* Linked : Pin->LinkedTo)
                    if (Linked && Linked->GetOwningNode())
                        Summary += FString::Printf(TEXT("\nlink: %s.%s -> %s.%s"), *Node->NodeGuid.ToString(), *Pin->PinName.ToString(), *Linked->GetOwningNode()->NodeGuid.ToString(), *Linked->PinName.ToString());
    }
    for (const UK2Node_FunctionEntry* Entry : Entries)
        for (const FBPVariableDescription& Local : Entry->LocalVariables)
            Summary += TEXT("\nlocal: ") + Local.VarName.ToString();
    if (Function && ParentFunction) Summary += TEXT("\nfunction kind: inherited override");
    if (AppendEvent) Summary += FString::Printf(TEXT("\nmode: append into existing event page; existing nodes and links preserved\nincoming vertical offset: %d; 512-unit fallback height for unmeasured nodes; no automatic execution links or tidy"), AppendOffsetY);
    if(!Apply)return TEXT("preview: Copy the complete graph, including connections, positions, comments and locals. No changes made. Listed IDs belong to the source; use the edit result for target IDs.")+Summary;
    T->Modify();
    UEdGraph* Copy=FEdGraphUtilities::CloneGraph(G,T);
    if(!Copy)return TEXT("error: Unreal could not clone the graph. No save requested.");
    if(Old && !AppendEvent)Old->Rename(nullptr,GetTransientPackage(),REN_DontCreateRedirectors|REN_NonTransactional);
    if (!AppendEvent) Copy->Rename(*Name,T,REN_DontCreateRedirectors|REN_NonTransactional);
    Copy->GraphGuid=FGuid::NewGuid();
    TArray<TPair<FString, FGuid>> IdentityMap;
    for(UEdGraphNode* N:Copy->Nodes)
    {
        FString SourceId;
        for (UEdGraphNode* SourceNode : G->Nodes)
            if (SourceNode && SourceNode->NodeGuid == N->NodeGuid)
            { SourceId = TurboMCPNodeId::MakeForNode(SourceNode, true); break; }
        N->CreateNewGuid();
        IdentityMap.Emplace(SourceId, N->NodeGuid);
        if(auto* Term=Cast<UK2Node_FunctionTerminator>(N)){FMemberReference R;if (Function && ParentFunction) R.SetExternalMember(FName(*Name), ParentFunction->GetOwnerClass()); else R.SetMemberName(FName(*Name));Term->FunctionReference=R;}
        if(auto* V=Cast<UK2Node_Variable>(N))
        {
            if(V->VariableReference.IsLocalScope())V->VariableReference.SetLocalMember(V->GetVarName(),Name,V->VariableReference.GetMemberGuid());
            else if(V->VariableReference.IsSelfContext())V->VariableReference.SetSelfMember(V->GetVarName());
        }
        if(auto* C=Cast<UK2Node_CallFunction>(N))if(C->FunctionReference.IsSelfContext())C->FunctionReference.SetSelfMember(C->FunctionReference.GetMemberName()==G->GetFName()?FName(*Name):C->FunctionReference.GetMemberName());
        if(auto* D=Cast<UK2Node_BaseMCDelegate>(N))if(D->DelegateReference.IsSelfContext())D->DelegateReference.SetSelfMember(D->DelegateReference.GetMemberName());
        if(auto* M=Cast<UK2Node_MacroInstance>(N))if(UEdGraph* MG=M->GetMacroGraph())if(MG->GetOuter()==S)M->SetMacroGraph(GraphByName(T,MG->GetName()));
        for(UEdGraphPin* P:N->Pins)if(P && P->PinName==UEdGraphSchema_K2::PN_Self && P->PinType.PinSubCategoryObject.Get()==S->GeneratedClass)P->PinType.PinSubCategoryObject=T->GeneratedClass;
    }
    bool Replaced=false;
    if (AppendEvent)
    {
        Old->Modify();
        // Clone first, then reparent nodes without breaking their internal links.
        const auto IncomingNodes = Copy->Nodes;
        for (UEdGraphNode* Node : IncomingNodes) if (Node)
        {
            Node->Rename(nullptr, Old, REN_DontCreateRedirectors|REN_NonTransactional);
            Node->NodePosY += AppendOffsetY;
            Old->Nodes.Add(Node);
        }
        Copy->Nodes.Reset();
        Replaced = true;
    }
    if(Old && !AppendEvent)
    {
        for(auto& Existing:T->FunctionGraphs)if(Existing==Old){Existing=Copy;Replaced=true;}
        for(auto& I:T->ImplementedInterfaces)for(auto& Existing:I.Graphs)if(Existing==Old){Existing=Copy;Replaced=true;}
    }
    if(!Replaced){if(Event)T->UbergraphPages.Add(Copy);else if(Macro)T->MacroGraphs.Add(Copy);else T->FunctionGraphs.Add(Copy);}
    for(auto* Existing:ReplaceEvents) FBlueprintEditorUtils::RemoveNode(T,Existing,true);
    const FString Completion = End(T);
    FString Result = FString::Printf(TEXT("edited: Complete graph copied.\nsource: %s:%s\ntarget: %s:%s\nsource-to-target node IDs:"), *S->GetPathName(), *G->GetName(), *T->GetPathName(), *Name);
    UEdGraph* FinalGraph = GraphByName(T, Name);
    for (const auto& Pair : IdentityMap)
    {
        UEdGraphNode* TargetNode = nullptr;
        if (FinalGraph)
            for (UEdGraphNode* Node : FinalGraph->Nodes)
                if (Node && Node->NodeGuid == Pair.Value) { TargetNode = Node; break; }
        Result += TEXT("\n") + Pair.Key + TEXT(" -> ") + (TargetNode ? TurboMCPNodeId::MakeForNode(TargetNode, true) : TEXT("unresolved after structural refresh; read the target graph"));
    }
    if (AppendEvent) Result += TEXT("\nmode: appended; existing page identity and unrelated nodes retained. No automatic execution links or tidy.");
    return Result + Completion;
}

// Duplication may assign fresh localization identities. Restore text values from
// the source without copying widget references or instanced ownership pointers.
static void PreserveText(const FProperty* Property, const void* Source, void* Target)
{
    if (CastField<FTextProperty>(Property)) { Property->CopyCompleteValue(Target, Source); return; }
    if (const auto* Struct = CastField<FStructProperty>(Property))
        for (TFieldIterator<FProperty> Field(Struct->Struct); Field; ++Field)
            for (int32 Index = 0; Index < Field->ArrayDim; ++Index)
                PreserveText(*Field, Field->ContainerPtrToValuePtr<void>(Source, Index), Field->ContainerPtrToValuePtr<void>(Target, Index));
    else if (const auto* Array = CastField<FArrayProperty>(Property))
    {
        FScriptArrayHelper From(Array, Source), To(Array, Target);
        if (From.Num() == To.Num()) for (int32 Index = 0; Index < From.Num(); ++Index)
            PreserveText(Array->Inner, From.GetRawPtr(Index), To.GetRawPtr(Index));
    }
}
static void PreserveWidgetText(const UObject* Source, UObject* Target)
{
    for (TFieldIterator<FProperty> Property(Source->GetClass()); Property; ++Property)
        for (int32 Index = 0; Index < Property->ArrayDim; ++Index)
            PreserveText(*Property, Property->ContainerPtrToValuePtr<void>(Source, Index), Property->ContainerPtrToValuePtr<void>(Target, Index));
}

static FString Widgets(const FString& Source,const FString& RootName,const FString& Target,const FString& ParentName,bool AutoRename,bool Apply)
{
    UBlueprint *S=nullptr,*T=nullptr;FString E=Pair(Source,Target,S,T);if(!E.IsEmpty())return E;
    auto* SW=Cast<UWidgetBlueprint>(S);auto* TW=Cast<UWidgetBlueprint>(T);
    if(!SW || !TW || !SW->WidgetTree || !TW->WidgetTree)return TEXT("error: Source and target must be Widget Blueprints with widget trees.");
    UWidget* Root=SW->WidgetTree->FindWidget(FName(*RootName));if(!Root)return TEXT("error: Source widget name was not found. Nothing changed.");
    UPanelWidget* Parent=ParentName.IsEmpty()?nullptr:Cast<UPanelWidget>(TW->WidgetTree->FindWidget(FName(*ParentName)));
    if((ParentName.IsEmpty() && TW->WidgetTree->RootWidget) || (!ParentName.IsEmpty() && !Parent))return TEXT("error: Supply a target panel name, or leave Parent empty only for an empty target tree. Nothing changed.");
    if(Parent && !Parent->CanAddMoreChildren())return TEXT("error: Target panel has no free child slot. Nothing changed.");
    if(Root->Slot && Parent && Root->Slot->Parent && Root->Slot->Parent->GetClass()!=Parent->GetClass())return TEXT("error: Target panel class differs from the source parent; slot properties cannot be preserved. Choose a matching panel. Nothing changed.");
    // ForWidgetAndChildren visits descendants only; include the selected root.
    TArray<UWidget*> Nodes{Root};UWidgetTree::ForWidgetAndChildren(Root,[&](UWidget* W){Nodes.Add(W);});
    TSet<FName> Names; for (UWidget* W : Nodes) Names.Add(W->GetFName());
    TSet<FName> Reserved = Names;
    TMap<FName, FName> Renames;
    FString NameReport;
    FKismetNameValidator NameValidator(TW);
    auto TargetNameAvailable = [&](FName Name)
    {
        return !TW->WidgetTree->FindWidget(Name)
            && !StaticFindObjectFast(UObject::StaticClass(), TW->WidgetTree, Name)
            && NameValidator.IsValid(Name.ToString()) == EValidatorResult::Ok;
    };
    for (UWidget* W : Nodes)
    {
        if (TargetNameAvailable(W->GetFName())) continue;
        if (!AutoRename) return TEXT("error: Target widget name collision: ") + W->GetName()
            + TEXT(". Preview with AutoRename=true to inspect replacement names. Nothing changed.");
        FName Candidate;
        for (int32 Index = 1; Index <= 10000; ++Index)
        {
            Candidate = FName(*(W->GetName().Left(80) + (Index == 1 ? TEXT("_Copy") : FString::Printf(TEXT("_Copy%d"), Index))));
            if (!Reserved.Contains(Candidate) && TargetNameAvailable(Candidate)
                && !StaticFindObjectFast(UObject::StaticClass(), SW->WidgetTree, Candidate)) break;
            Candidate = NAME_None;
        }
        if (Candidate.IsNone()) return TEXT("error: Could not reserve a collision-free widget name. Nothing changed.");
        Reserved.Add(Candidate); Renames.Add(W->GetFName(), Candidate);
        NameReport += FString::Printf(TEXT("\nrename: %s -> %s"), *W->GetName(), *Candidate.ToString());
    }
    // Explicit navigation stores names separately from UObject references.
    // Refuse outside targets instead of binding a same-named widget in the destination.
    for (UWidget* W : Nodes) if (W->Navigation)
        for (const auto* Nav : {&W->Navigation->Up, &W->Navigation->Down, &W->Navigation->Left,
            &W->Navigation->Right, &W->Navigation->Next, &W->Navigation->Previous})
            if (Nav->Rule == EUINavigationRule::Explicit && !Nav->WidgetToFocus.IsNone() && !Names.Contains(Nav->WidgetToFocus))
                return TEXT("error: Subtree has explicit navigation to an outside widget. Copy their common parent or update navigation first. Nothing changed.");

    for(const auto& B:SW->Bindings)if(Names.Contains(FName(*B.ObjectName)))return TEXT("error: Subtree has designer bindings. Migrate those explicitly before copying; they are not silently dropped. Nothing changed.");
    FString Summary=FString::Printf(TEXT("\nsource: %s/%s\ntarget: %s/%s\nwidgets: %d; properties, internal slots and localized text preserved; names preserved except listed renames"),*S->GetPathName(),*RootName,*T->GetPathName(),Parent?*ParentName:TEXT("root"),Nodes.Num());
    Summary += NameReport;
    if (!Renames.IsEmpty()) Summary += TEXT("\nExplicit internal navigation names are remapped. Graphs, animations and custom string-based references are not migrated; use the listed names when reconnecting graph references. GraphTransfer does not automatically remap renamed widgets. Preview does not reserve names; edit replans.");
    if(!Apply)return TEXT("preview: Copy widget subtree. No changes made. Graphs, animations and designer bindings are not copied.")+Summary;
    // Duplicate the tree together so sibling references and instanced slot objects
    // are remapped by Unreal before moving just the selected subtree.
    TStrongObjectPtr<UWidgetTree> Staging(DuplicateObject<UWidgetTree>(SW->WidgetTree,GetTransientPackage()));
    UWidget* Copy=Staging->FindWidget(FName(*RootName));if(!Copy)return TEXT("error: Duplicate tree did not contain the source root. Nothing changed.");
    TArray<UWidget*> Copies{Copy};UWidgetTree::ForWidgetAndChildren(Copy,[&](UWidget* W){Copies.Add(W);});
    // External widget references would point to a detached staging tree. Refuse
    // rather than claiming a complete copy with dangling sibling dependencies.
    for(UWidget* W:Copies)for(TFieldIterator<FObjectProperty> P(W->GetClass());P;++P)
    {UObject* V=P->GetObjectPropertyValue_InContainer(W);if(auto* Other=Cast<UWidget>(V))if(Other->GetOuter()==Staging.Get() && !Copies.Contains(Other))return TEXT("error: Subtree references a widget outside the selection. Copy their common parent instead. Nothing changed.");}
    for(UWidget* W:Copies)if(UWidget* Original=SW->WidgetTree->FindWidget(W->GetFName()))PreserveWidgetText(Original,W);
    // Rename only staged objects. No source or target object changes if staging fails.
    for (UWidget* W : Copies)
        if (const FName* NewName = Renames.Find(W->GetFName()))
            if (!W->Rename(*NewName->ToString(), Staging.Get(), REN_DontCreateRedirectors | REN_NonTransactional))
                return TEXT("error: Could not rename a staged widget. Nothing changed.");
    for (UWidget* W : Copies) if (W->Navigation)
        for (auto* Nav : {&W->Navigation->Up, &W->Navigation->Down, &W->Navigation->Left,
            &W->Navigation->Right, &W->Navigation->Next, &W->Navigation->Previous})
            if (Nav->Rule == EUINavigationRule::Explicit)
                if (const FName* NewName = Renames.Find(Nav->WidgetToFocus))
                { Nav->WidgetToFocus = *NewName; Nav->Widget.Reset(); }
    T->Modify();TW->WidgetTree->Modify();
    if(UPanelWidget* Previous=Copy->GetParent())Previous->RemoveChild(Copy);
    for(UWidget* W:Copies){if(!UWidgetTree::TryMoveWidgetToNewTree(W,TW->WidgetTree))return TEXT("error: Unreal could not move a copied widget into the target tree. Inspect target before retrying; no save requested.");TW->WidgetVariableNameToGuidMap.Add(W->GetFName(),FGuid::NewGuid());}
    if(Parent)
    {
        Parent->Modify();UPanelSlot* Slot=Parent->AddChild(Copy);
        if(!Slot)return TEXT("error: Target panel refused the copied root after staging. Inspect target before retrying; no save requested.");
        if(Root->Slot && Slot->GetClass()==Root->Slot->GetClass())
            for(TFieldIterator<FProperty> P(Slot->GetClass());P;++P)
                if(P->HasAnyPropertyFlags(CPF_Edit) && !P->HasAnyPropertyFlags(CPF_Transient))P->CopyCompleteValue_InContainer(Slot,Root->Slot);
    }
    else {Copy->Slot=nullptr;TW->WidgetTree->RootWidget=Copy;}
    return TEXT("edited: Widget subtree copied. Graphs and animations were not copied.")+Summary+End(T);
}
}
FString UTurboMCPPreviewToolset::PreviewTurboMCPDispatcherAdd(const FString& Asset,const FString& Name,const FString& Inputs){return TurboMCPMergeTransfer::Dispatcher(Asset,Name,Inputs,false);}
FString UTurboMCPEditToolset::EditTurboMCPDispatcherAdd(const FString& Asset,const FString& Name,const FString& Inputs){return TurboMCPMergeTransfer::Dispatcher(Asset,Name,Inputs,true);}
FString UTurboMCPPreviewToolset::PreviewTurboMCPMergeMembers(const FString& SourceAsset,const FString& TargetAsset,const FString& Exclude){return TurboMCPMergeTransfer::Members(SourceAsset,TargetAsset,Exclude,false);}
FString UTurboMCPEditToolset::EditTurboMCPMergeMembers(const FString& SourceAsset,const FString& TargetAsset,const FString& Exclude){return TurboMCPMergeTransfer::Members(SourceAsset,TargetAsset,Exclude,true);}
FString UTurboMCPPreviewToolset::PreviewTurboMCPGraphTransfer(const FString& SourceAsset,const FString& SourceGraph,const FString& TargetAsset,const FString& TargetGraph){return TurboMCPMergeTransfer::Graph(SourceAsset,SourceGraph,TargetAsset,TargetGraph,false);}
FString UTurboMCPEditToolset::EditTurboMCPGraphTransfer(const FString& SourceAsset,const FString& SourceGraph,const FString& TargetAsset,const FString& TargetGraph){return TurboMCPMergeTransfer::Graph(SourceAsset,SourceGraph,TargetAsset,TargetGraph,true);}
FString UTurboMCPPreviewToolset::PreviewTurboMCPWidgetSubtreeCopy(const FString& SourceAsset,const FString& SourceWidget,const FString& TargetAsset,const FString& Parent,bool AutoRename){return TurboMCPMergeTransfer::Widgets(SourceAsset,SourceWidget,TargetAsset,Parent,AutoRename,false);}
FString UTurboMCPEditToolset::EditTurboMCPWidgetSubtreeCopy(const FString& SourceAsset,const FString& SourceWidget,const FString& TargetAsset,const FString& Parent,bool AutoRename){return TurboMCPMergeTransfer::Widgets(SourceAsset,SourceWidget,TargetAsset,Parent,AutoRename,true);}
