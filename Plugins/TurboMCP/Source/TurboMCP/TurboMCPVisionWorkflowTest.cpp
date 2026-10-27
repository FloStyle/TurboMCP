// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
#include "Tools/TurboMCPToolsets.h"
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "BlueprintEditorLibrary.h"
#include "BlueprintActionDatabase.h"
#include "StructUtils/UserDefinedStruct.h"
#include "Kismet2/StructureEditorUtils.h"
#include "K2Node_MakeStruct.h"
#include "K2Node_BreakStruct.h"
#include "Internationalization/Regex.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Components/SizeBox.h"
#include "Components/Button.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphUtilities.h"
#include "EdGraphNode_Comment.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "GameFramework/Actor.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_VariableGet.h"
#include "K2Node_ComponentBoundEvent.h"
#include "Blueprint/WidgetNavigation.h"
#include "K2Node_ExecutionSequence.h"
#include "K2Node_IfThenElse.h"
#include "K2Node_AddDelegate.h"
#include "K2Node_CallFunction.h"
#include "K2Node_MacroInstance.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "WidgetBlueprint.h"
#include "WidgetBlueprintOperationUtils.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Misc/ScopeExit.h"
#include "HAL/FileManager.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"
#include "ImageUtils.h"
#include "ImageCore.h"
#include "TurboMCPNodeId.h"
#include "TurboMCPGraphScratch.h"
#include "TurboMCPSettings.h"
#include "TurboMCPModule.h"
#include "ToolsetRegistry/UToolsetRegistry.h"

// Fixture authoring is setup, not evidence that graph editing tools created it.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTurboMCPVisionWorkflowTest,"TurboMCP.Workflow.Vision",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTurboMCPVisionWorkflowTest::RunTest(const FString& Parameters)
{
 // FT-112: preferences are personal; disabled activation must not touch active registries.
 {
  UTurboMCPSettings* Preferences = UTurboMCPSettings::GetMutable();
  if (!TestNotNull(TEXT("Personal preferences available"), Preferences)) return false;
  const bool PreviousEnabled = Preferences->bEnableTurboMCP;
  ON_SCOPE_EXIT { Preferences->bEnableTurboMCP = PreviousEnabled; };
  TestEqual(TEXT("Preferences use Editor container"), Preferences->GetContainerName(), FName(TEXT("Editor")));
  TestEqual(TEXT("Preferences use per-user project config"), Preferences->GetClass()->ClassConfigName, FName(TEXT("EditorPerProjectUserSettings")));
  TestFalse(TEXT("Preferences do not save shared defaults"), Preferences->GetClass()->HasAnyClassFlags(CLASS_DefaultConfig));
  Preferences->bEnableTurboMCP = false; // In-memory only; never save test preferences.
  const bool WasRegistered = UToolsetRegistry::IsToolsetClassRegistered(UTurboMCPReadToolset::StaticClass());
  FTurboMCPModule DisabledInstance;
  DisabledInstance.StartupModule();
  TestFalse(TEXT("Disabled startup remains inactive"), DisabledInstance.IsEnabledForSession());
  DisabledInstance.RegisterToolsets();
  DisabledInstance.ShutdownModule();
  TestEqual(TEXT("Disabled lifecycle preserves existing registry"), UToolsetRegistry::IsToolsetClassRegistered(UTurboMCPReadToolset::StaticClass()), WasRegistered);
  const FString PendingStatus = UTurboMCPReadToolset::ReadTurboMCPStatus();
  TestTrue(TEXT("Status distinguishes pending disable from current activation"), PendingStatus.Contains(TEXT("activation this session: enabled")) && PendingStatus.Contains(TEXT("per-user preference: disabled")) && PendingStatus.Contains(TEXT("restart required: yes")));
 }
 const FString Suffix=FGuid::NewGuid().ToString(EGuidFormats::Digits);
 UPackage* Package=CreatePackage(*(TEXT("/Game/Tests/Vision_")+Suffix));
 FString Transcript;
 ON_SCOPE_EXIT {
  const FString Folder=FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("TurboMCP/Workflow"),Suffix);
  IFileManager::Get().MakeDirectory(*Folder,true);
  FFileHelper::SaveStringToFile(Transcript,*FPaths::Combine(Folder,TEXT("vision-review.txt")),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
  Package->SetDirtyFlag(false); // Only our never-saved disposable fixture package.
 };
 auto Record=[&](const FString& Name,const FString& Value){Transcript+=TEXT("\n=== ")+Name+TEXT(" ===\n")+Value+TEXT("\n");return Value;};
 auto ImageCheck=[&](const FString& Value,int32 W=0,int32 H=0){
  TestFalse(TEXT("Capture did not refuse"),Value.StartsWith(TEXT("error:")));
  FString Prefix,Path; if(!TestTrue(TEXT("Image path returned"),Value.Split(TEXT("image: "),&Prefix,&Path)))return;
  int32 End; if(Path.FindChar('\n',End))Path=Path.Left(End);Path=Path.TrimStartAndEnd();
  TArray<uint8> Bytes;FImage Image;
  if(!TestTrue(TEXT("Image decodes"),FFileHelper::LoadFileToArray(Bytes,*Path)&&FImageUtils::DecompressImage(Bytes.GetData(),Bytes.Num(),Image)))return;
  if(W){TestEqual(TEXT("Requested width"),Image.SizeX,W);TestEqual(TEXT("Requested height"),Image.SizeY,H);}
  TestTrue(TEXT("Nonempty PNG"),Bytes.Num()>1000);
  Record(TEXT("Image dimensions"),FString::Printf(TEXT("%dx%d; %lld bytes"),Image.SizeX,Image.SizeY,int64(Bytes.Num())));
 };
 UBlueprint* BP=FKismetEditorUtilities::CreateBlueprint(AActor::StaticClass(),Package,TEXT("BP_VisionStress"),BPTYPE_Normal,UBlueprint::StaticClass(),UBlueprintGeneratedClass::StaticClass());
 if(!TestNotNull(TEXT("Blueprint"),BP))return false;

 // FT-118: read identities must create the same concrete user-defined struct nodes.
 UUserDefinedStruct* ReaderStruct = FStructureEditorUtils::CreateUserDefinedStruct(Package, TEXT("ReaderStruct"), RF_Transient);
 if (!TestNotNull(TEXT("Disposable reader struct"), ReaderStruct)) return false;
 FBlueprintActionDatabase::Get().RefreshAssetActions(ReaderStruct);
 UEdGraph* StructGraph = UBlueprintEditorLibrary::AddFunctionGraph(BP, TEXT("StructIdentityProbe"));
 auto* MakeStruct = NewObject<UK2Node_MakeStruct>(StructGraph);
 auto* BreakStruct = NewObject<UK2Node_BreakStruct>(StructGraph);
 MakeStruct->StructType = ReaderStruct; BreakStruct->StructType = ReaderStruct;
 for (UK2Node_StructOperation* Node : {static_cast<UK2Node_StructOperation*>(MakeStruct), static_cast<UK2Node_StructOperation*>(BreakStruct)})
 {
  Node->CreateNewGuid(); StructGraph->AddNode(Node, false, false); Node->AllocateDefaultPins();
 }
 // Connect the pure nodes to a return value so the bodies reader visits them.
 TArray<UK2Node_FunctionEntry*> StructEntries; StructGraph->GetNodesOfClass(StructEntries);
 auto* StructResult = FBlueprintEditorUtils::FindOrCreateFunctionResultNode(StructEntries[0]);
 UEdGraphPin* MadeValue = nullptr; UEdGraphPin* BreakInput = nullptr; UEdGraphPin* BrokenValue = nullptr;
 for (auto* Pin : MakeStruct->Pins) if (Pin->Direction == EGPD_Output) MadeValue = Pin;
 for (auto* Pin : BreakStruct->Pins)
 {
  if (Pin->Direction == EGPD_Input) BreakInput = Pin;
  else if (!Pin->bHidden && !BrokenValue) BrokenValue = Pin;
 }
 if (!TestNotNull(TEXT("Struct output"), MadeValue) || !TestNotNull(TEXT("Struct input"), BreakInput)
     || !TestNotNull(TEXT("Struct member output"), BrokenValue)) return false;
 auto* ReturnedValue = StructResult->CreateUserDefinedPin(TEXT("Value"), BrokenValue->PinType, EGPD_Input, false);
 const auto* StructSchema = GetDefault<UEdGraphSchema_K2>();
 TestTrue(TEXT("Make feeds break"), StructSchema->TryCreateConnection(MadeValue, BreakInput));
 TestTrue(TEXT("Break feeds return"), StructSchema->TryCreateConnection(BrokenValue, ReturnedValue));
 TestTrue(TEXT("Function reaches return"), StructSchema->TryCreateConnection(StructEntries[0]->FindPinChecked(UEdGraphSchema_K2::PN_Then), StructResult->FindPinChecked(UEdGraphSchema_K2::PN_Execute)));
 const FString MakeId = TurboMCPNodeId::GetNodeTypeId(MakeStruct);
 const FString BreakId = TurboMCPNodeId::GetNodeTypeId(BreakStruct);
 TestEqual(TEXT("Make identity includes exact struct path"), MakeId, TEXT("Struct|Make|") + ReaderStruct->GetPathName());
 TestEqual(TEXT("Break identity includes exact struct path"), BreakId, TEXT("Struct|Break|") + ReaderStruct->GetPathName());
 const FString StructRead = Record(TEXT("Struct reader identities"), UTurboMCPReadToolset::ReadTurboMCPGraphLoad(BP->GetPathName(), StructGraph->GetName(), TEXT("read"), TEXT("bodies"), 0, 24000));
 TestTrue(TEXT("Reader exposes both writable struct identities"), StructRead.Contains(MakeId) && StructRead.Contains(BreakId));
 const FString StructAppend = Record(TEXT("Append reader struct identities"), UTurboMCPEditToolset::EditTurboMCPGraphAppend(BP->GetPathName(), StructGraph->GetName(), TEXT("(\"") + MakeId + TEXT("\")\n(\"") + BreakId + TEXT("\")")));
 TestTrue(TEXT("Append accepts reader struct identities"), StructAppend.StartsWith(TEXT("changed:")));
 TArray<UK2Node_MakeStruct*> Makes; StructGraph->GetNodesOfClass(Makes);
 TArray<UK2Node_BreakStruct*> Breaks; StructGraph->GetNodesOfClass(Breaks);
 TestEqual(TEXT("One matching make node appended"), Makes.Num(), 2);
 TestEqual(TEXT("One matching break node appended"), Breaks.Num(), 2);
 for (auto* Node : Makes) TestTrue(TEXT("Make retains exact struct"), Node->StructType == ReaderStruct);
 for (auto* Node : Breaks) TestTrue(TEXT("Break retains exact struct"), Node->StructType == ReaderStruct);

 // GC-55/56/62: labelled getter -> Branch, explicit links, then a converted replacement.
 FEdGraphPinType SyntaxBool; SyntaxBool.PinCategory = UEdGraphSchema_K2::PC_Boolean;
 TestTrue(TEXT("Declare syntax fixture variable"), FBlueprintEditorUtils::AddMemberVariable(BP, TEXT("SyntaxValue"), SyntaxBool));
 UEdGraph* SyntaxGraph = UBlueprintEditorLibrary::AddFunctionGraph(BP, TEXT("WriterSyntaxProbe"));
 TArray<UK2Node_FunctionEntry*> SyntaxEntries; SyntaxGraph->GetNodesOfClass(SyntaxEntries);
 const FString SyntaxEntryId = TurboMCPNodeId::MakeForNode(SyntaxEntries[0]);
 const FString GetterType = TEXT("Variable|") + BP->GetPathName() + TEXT("|Get|SyntaxValue");
 const FString BranchType = TurboMCPNodeId::GetNodeTypeId(GetDefault<UK2Node_IfThenElse>());
 const FString LabelText = FString::Printf(TEXT(R"DSL(("%s") ; label Flag
(%s
 execute (-> %s:then)
 Condition (-> Flag:SyntaxValue)
))DSL"), *GetterType, *BranchType, *SyntaxEntryId);
 const FString LabelEdit = Record(TEXT("Labelled getter append"), UTurboMCPEditToolset::EditTurboMCPGraphAppend(BP->GetPathName(), SyntaxGraph->GetName(), LabelText));
 TestTrue(TEXT("Getter label append accepted"), LabelEdit.StartsWith(TEXT("changed:")));
 TArray<UK2Node_IfThenElse*> SyntaxBranches; SyntaxGraph->GetNodesOfClass(SyntaxBranches);
 TArray<UK2Node_VariableGet*> SyntaxGetters; SyntaxGraph->GetNodesOfClass(SyntaxGetters);
 if (!TestEqual(TEXT("One Branch created"), SyntaxBranches.Num(), 1) || !TestEqual(TEXT("One getter created"), SyntaxGetters.Num(), 1)) return false;
 TestTrue(TEXT("Label resolves to actual getter output"), SyntaxBranches[0]->GetConditionPin()->LinkedTo.Contains(SyntaxGetters[0]->GetValuePin()));
 const int32 SyntaxCount = SyntaxGraph->Nodes.Num();
 const FString SectionText = FString::Printf(TEXT(R"DSL((%s
 :then
))DSL"), *BranchType);
 const FString SectionRefusal = Record(TEXT("Reader branch section refusal"), UTurboMCPEditToolset::EditTurboMCPGraphAppend(BP->GetPathName(), SyntaxGraph->GetName(), SectionText));
 TestTrue(TEXT("Reader sections explain explicit conversion"), SectionRefusal.Contains(TEXT("reader-only branch section")) && SectionRefusal.Contains(TEXT("Choice:then")));
 TestEqual(TEXT("Reader section refusal preserves nodes"), SyntaxGraph->Nodes.Num(), SyntaxCount);
 TestTrue(TEXT("Reader section refusal preserves existing condition link"), SyntaxBranches[0]->GetConditionPin()->LinkedTo.Contains(SyntaxGetters[0]->GetValuePin()));
 const FGuid RetainedBranch = SyntaxBranches[0]->NodeGuid;
 const FString ReplacementText = FString::Printf(TEXT(R"DSL((%s
 ; %s
 execute (-> %s:then)
 Condition true
))DSL"), *BranchType, *TurboMCPNodeId::MakeForNode(SyntaxBranches[0]), *SyntaxEntryId);
 const FString ReplacementPreview = Record(TEXT("Converted replacement preview"), UTurboMCPPreviewToolset::PreviewTurboMCPGraphReplace(BP->GetPathName(), SyntaxGraph->GetName(), ReplacementText));
 TestEqual(TEXT("Replacement preview preserves nodes"), SyntaxGraph->Nodes.Num(), SyntaxCount);
 const FString ReplacementEdit = Record(TEXT("Converted replacement edit"), UTurboMCPEditToolset::EditTurboMCPGraphReplace(BP->GetPathName(), SyntaxGraph->GetName(), ReplacementText));
 TestTrue(TEXT("Converted replacement accepted"), ReplacementEdit.StartsWith(TEXT("changed:")));
 SyntaxGetters.Reset(); SyntaxGraph->GetNodesOfClass(SyntaxGetters);
 TestEqual(TEXT("Replacement removes only omitted getter"), SyntaxGetters.Num(), 0);
 TestEqual(TEXT("Replacement preserves Branch identity"), SyntaxBranches[0]->NodeGuid, RetainedBranch);
 TestEqual(TEXT("Replacement sets condition"), SyntaxBranches[0]->GetConditionPin()->DefaultValue, FString(TEXT("true")));
 TestTrue(TEXT("Replacement preserves execution wire"), SyntaxBranches[0]->GetExecPin()->LinkedTo.Contains(SyntaxEntries[0]->FindPinChecked(UEdGraphSchema_K2::PN_Then)));

 // Merge prerequisites are exercised on disposable assets, never field assets.
 UEdGraph* MergeFunction=UBlueprintEditorLibrary::AddFunctionGraph(BP,TEXT("MergeMemberProbe"));
 const FString MergeAsset=BP->GetPathName();
 const bool DirtyBeforePreview=Package->IsDirty();
 const FString LocalPreview=Record(TEXT("Local declaration preview"),UTurboMCPPreviewToolset::PreviewTurboMCPLocalVariableAdd(MergeAsset,TEXT("MergeMemberProbe"),TEXT("MergeCount"),TEXT("int"),TEXT("7")));
 TestTrue(TEXT("Local preview accepted"),LocalPreview.StartsWith(TEXT("preview:")));
 TArray<UK2Node_FunctionEntry*> MergeEntries;MergeFunction->GetNodesOfClass(MergeEntries);
 TestEqual(TEXT("Preview leaves locals unchanged"),MergeEntries[0]->LocalVariables.Num(),0);
 TestEqual(TEXT("Preview preserves dirty state"),Package->IsDirty(),DirtyBeforePreview);
 TestTrue(TEXT("Local edit accepted"),Record(TEXT("Local declaration edit"),UTurboMCPEditToolset::EditTurboMCPLocalVariableAdd(MergeAsset,TEXT("MergeMemberProbe"),TEXT("MergeCount"),TEXT("int"),TEXT("7"))).StartsWith(TEXT("edited:")));
 MergeEntries.Reset();MergeFunction->GetNodesOfClass(MergeEntries);
 TestEqual(TEXT("One local declared"),MergeEntries[0]->LocalVariables.Num(),1);
 if(MergeEntries[0]->LocalVariables.Num()==1){
  const auto& Local=MergeEntries[0]->LocalVariables[0];
  TestEqual(TEXT("Local default preserved"),Local.DefaultValue,FString(TEXT("7")));
  TestEqual(TEXT("Local type preserved"),Local.VarType.PinCategory,UEdGraphSchema_K2::PC_Int);
  TestTrue(TEXT("Local has identity"),Local.VarGuid.IsValid());
  const FGuid LocalGuid=Local.VarGuid;
  TestTrue(TEXT("Repeated local is a no-op"),UTurboMCPEditToolset::EditTurboMCPLocalVariableAdd(MergeAsset,TEXT("MergeMemberProbe"),TEXT("MergeCount"),TEXT("int"),TEXT("7")).StartsWith(TEXT("unchanged:")));
  TestEqual(TEXT("Repeat preserves identity"),MergeEntries[0]->LocalVariables[0].VarGuid,LocalGuid);
 }
 TestTrue(TEXT("Conflicting local refuses"),UTurboMCPEditToolset::EditTurboMCPLocalVariableAdd(MergeAsset,TEXT("MergeMemberProbe"),TEXT("MergeCount"),TEXT("bool"),TEXT("")).StartsWith(TEXT("error:")));
 TestTrue(TEXT("Missing function refuses"),UTurboMCPEditToolset::EditTurboMCPLocalVariableAdd(MergeAsset,TEXT("MissingFunction"),TEXT("MissingLocal"),TEXT("int"),TEXT("")).StartsWith(TEXT("error:")));
 UBlueprint* MergeInterface=FKismetEditorUtilities::CreateBlueprint(UInterface::StaticClass(),Package,TEXT("BPI_MergeProbe"),BPTYPE_Interface,UBlueprint::StaticClass(),UBlueprintGeneratedClass::StaticClass());
 if(!TestNotNull(TEXT("Merge interface fixture"),MergeInterface))return false;
 UEdGraph* InterfaceGraph=UBlueprintEditorLibrary::AddFunctionGraph(MergeInterface,TEXT("ReadMergeState"));
 TArray<UK2Node_FunctionEntry*> InterfaceEntries;InterfaceGraph->GetNodesOfClass(InterfaceEntries);
 auto* InterfaceResult=FBlueprintEditorUtils::FindOrCreateFunctionResultNode(InterfaceEntries[0]);
 FEdGraphPinType MergeResultType;MergeResultType.PinCategory=UEdGraphSchema_K2::PC_Int;
 InterfaceResult->CreateUserDefinedPin(TEXT("State"),MergeResultType,EGPD_Input,false);
 FKismetEditorUtilities::CompileBlueprint(MergeInterface);
 const int32 InterfacesBefore=BP->ImplementedInterfaces.Num();
 TestTrue(TEXT("Interface preview accepted"),Record(TEXT("Interface preview"),UTurboMCPPreviewToolset::PreviewTurboMCPInterfaceAdd(MergeAsset,MergeInterface->GetPathName())).StartsWith(TEXT("preview:")));
 TestEqual(TEXT("Preview leaves interfaces unchanged"),BP->ImplementedInterfaces.Num(),InterfacesBefore);
 TestTrue(TEXT("Interface edit accepted"),Record(TEXT("Interface edit"),UTurboMCPEditToolset::EditTurboMCPInterfaceAdd(MergeAsset,MergeInterface->GetPathName())).StartsWith(TEXT("edited:")));
 TestEqual(TEXT("Interface added once"),BP->ImplementedInterfaces.Num(),InterfacesBefore+1);
 if(BP->ImplementedInterfaces.Num()==InterfacesBefore+1)
  TestEqual(TEXT("Required interface graph created"),BP->ImplementedInterfaces.Last().Graphs.Num(),1);
 TestTrue(TEXT("Repeated interface is a no-op"),UTurboMCPEditToolset::EditTurboMCPInterfaceAdd(MergeAsset,MergeInterface->GetPathName()).StartsWith(TEXT("unchanged:")));
 TestTrue(TEXT("Non-interface refuses"),UTurboMCPEditToolset::EditTurboMCPInterfaceAdd(MergeAsset,MergeAsset).StartsWith(TEXT("error:")));
 const UEdGraphSchema_K2* Schema=GetDefault<UEdGraphSchema_K2>();
 auto Wire=[&](UEdGraphPin* A,UEdGraphPin* B){TestTrue(TEXT("Fixture connection"),A&&B&&Schema->TryCreateConnection(A,B));};
 auto Init=[&](UEdGraphNode* N,UEdGraph* G,int32 X,int32 Y){N->CreateNewGuid();G->AddNode(N,false,false);N->AllocateDefaultPins();N->NodePosX=X;N->NodePosY=Y;};
 auto Call=[&](UEdGraph* G,UClass* Owner,const FName Name,int32 X,int32 Y){auto* N=NewObject<UK2Node_CallFunction>(G);N->SetFromFunction(Owner->FindFunctionByName(Name));Init(N,G,X,Y);return N;};
 TArray<UEdGraph*> Graphs;
 TArray<UEdGraphNode*> Targets;
 for(int32 Group=0;Group<3;++Group){
  UEdGraph* G=FBlueprintEditorUtils::CreateNewGraph(BP,FName(*FString::Printf(TEXT("VisionEvents_%d"),Group)),UEdGraph::StaticClass(),UEdGraphSchema_K2::StaticClass());
  FBlueprintEditorUtils::AddUbergraphPage(BP,G);Graphs.Add(G);
  for(int32 Flow=0;Flow<4;++Flow){
   const int32 Y=Flow*1800;
   auto* Event=NewObject<UK2Node_CustomEvent>(G);Event->CustomFunctionName=FName(*FString::Printf(TEXT("Request_%d_%d"),Group,Flow));Init(Event,G,0,Y);
   auto* Seq=NewObject<UK2Node_ExecutionSequence>(G);Init(Seq,G,350,Y);Seq->AddInputPin();Seq->AddInputPin();Wire(Event->FindPin(UEdGraphSchema_K2::PN_Then),Seq->FindPin(UEdGraphSchema_K2::PN_Execute));
   auto* Bind=NewObject<UK2Node_AddDelegate>(G);Bind->SetFromProperty(FindFProperty<FMulticastDelegateProperty>(AActor::StaticClass(),TEXT("OnDestroyed")),true,AActor::StaticClass());Init(Bind,G,750,Y);
   auto* Handler=NewObject<UK2Node_CustomEvent>(G);Handler->CustomFunctionName=FName(*FString::Printf(TEXT("Destroyed_%d_%d"),Group,Flow));Init(Handler,G,350,Y-240);
   FEdGraphPinType ActorType;ActorType.PinCategory=UEdGraphSchema_K2::PC_Object;ActorType.PinSubCategoryObject=AActor::StaticClass();Handler->CreateUserDefinedPin(TEXT("DestroyedActor"),ActorType,EGPD_Output,false);
   Wire(Seq->GetThenPinGivenIndex(0),Bind->FindPin(UEdGraphSchema_K2::PN_Execute));Wire(Handler->GetDelegatePin(),Bind->FindPin(TEXT("Delegate")));
   for(int32 Lane=1;Lane<4;++Lane){
    const int32 Row=Y+Lane*300;
    auto* Branch=NewObject<UK2Node_IfThenElse>(G);Init(Branch,G,750,Row);
    auto* Compare=Call(G,UKismetMathLibrary::StaticClass(),TEXT("Greater_DoubleDouble"),400,Row+110);
    Compare->FindPinChecked(TEXT("A"))->DefaultValue=FString::FromInt(Flow+Lane);Compare->FindPinChecked(TEXT("B"))->DefaultValue=TEXT("2");
    Wire(Compare->GetReturnValuePin(),Branch->GetConditionPin());Wire(Seq->GetThenPinGivenIndex(Lane),Branch->GetExecPin());
    for(int32 Choice=0;Choice<2;++Choice){
     auto* Print=Call(G,UKismetSystemLibrary::StaticClass(),TEXT("PrintString"),1150+Choice*400,Row+Choice*120);
     Print->FindPinChecked(TEXT("InString"))->DefaultValue=FString::Printf(TEXT("Flow %d/%d lane %d result %d"),Group,Flow,Lane,Choice);
     Print->FindPinChecked(TEXT("bPrintToScreen"))->DefaultValue=TEXT("false");
     Wire(Choice?Branch->GetElsePin():Branch->GetThenPin(),Print->GetExecPin());
    }
   }
   auto* Box=NewObject<UEdGraphNode_Comment>(G);Init(Box,G,-80,Y-320);Box->NodeComment=FString::Printf(TEXT("Vision flow %d/%d - dispatcher and three decisions"),Group,Flow);Box->NodeWidth=2050;Box->NodeHeight=1550;
   if(Flow==0)Targets.Add(Seq);
  }
 }
 UEdGraph* Function=UBlueprintEditorLibrary::AddFunctionGraph(BP,TEXT("CalculateDenseScore"));Graphs.Add(Function);
 TArray<UK2Node_FunctionEntry*> Entries;Function->GetNodesOfClass(Entries);auto* Entry=Entries[0];
 auto* Result=FBlueprintEditorUtils::FindOrCreateFunctionResultNode(Entry);
 FEdGraphPinType Number;Number.PinCategory=UEdGraphSchema_K2::PC_Real;Number.PinSubCategory=UEdGraphSchema_K2::PC_Double;
 UEdGraphPin* Value=Entry->CreateUserDefinedPin(TEXT("Seed"),Number,EGPD_Output,false);
 auto* Output=Result->CreateUserDefinedPin(TEXT("Score"),Number,EGPD_Input,false);
 Entry->NodePosX=0;Entry->NodePosY=0;Result->NodePosX=3700;Result->NodePosY=0;Wire(Entry->FindPin(UEdGraphSchema_K2::PN_Then),Result->GetExecPin());
 for(int32 I=0;I<48;++I){auto* N=Call(Function,UKismetMathLibrary::StaticClass(),I%2?TEXT("Multiply_DoubleDouble"):TEXT("Add_DoubleDouble"),350+(I%12)*280,250+(I/12)*260);N->FindPinChecked(TEXT("B"))->DefaultValue=I%2?TEXT("1.01"):TEXT("2");Wire(Value,N->FindPinChecked(TEXT("A")));Value=N->GetReturnValuePin();if(I==24)Targets.Add(N);}
 Wire(Value,Output);
 Record(TEXT("Compile fixture"),UTurboMCPEditToolset::EditTurboMCPCompile(BP->GetPathName()));TestTrue(TEXT("Dense Blueprint compiles"),BP->Status!=BS_Error);

 // Reproduce S4: a graph calls a custom event that it also declares.
 auto* InternalEventCall = Call(Graphs[0], BP->SkeletonGeneratedClass, TEXT("Request_0_0"), 2200, 0);
 InternalEventCall->FunctionReference.SetSelfMember(TEXT("Request_0_0"));
 auto* FunctionEventCall=Call(Function,BP->SkeletonGeneratedClass,TEXT("Request_0_0"),1900,0);
 FunctionEventCall->FunctionReference.SetSelfMember(TEXT("Request_0_0"));
 Entry->FindPin(UEdGraphSchema_K2::PN_Then)->BreakAllPinLinks();
 Wire(Entry->FindPin(UEdGraphSchema_K2::PN_Then),FunctionEventCall->GetExecPin());
 Wire(FunctionEventCall->GetThenPin(),Result->GetExecPin());
 Record(TEXT("Compile internal event call"), UTurboMCPEditToolset::EditTurboMCPCompile(MergeAsset));
 TestTrue(TEXT("Source with internal custom-event call compiles"), BP->Status != BS_Error);
 // A second Blueprint receives declarations and then complete graph bodies.
 UBlueprint* MergeTarget=FKismetEditorUtilities::CreateBlueprint(AActor::StaticClass(),Package,TEXT("BP_MergeTarget"),BPTYPE_Normal,UBlueprint::StaticClass(),UBlueprintGeneratedClass::StaticClass());
 if(!TestNotNull(TEXT("Merge target"),MergeTarget))return false;
 auto MergeSnapshot=[&](UEdGraph* G){TSet<UObject*> Nodes;for(UEdGraphNode* N:G->Nodes)Nodes.Add(N);FString Text;FEdGraphUtilities::ExportNodesToText(Nodes,Text);return Text;};
 const FString SourceSnapshot=MergeSnapshot(Function);
 // S8 X-103: a refused preview must not leave a valid errored Blueprint for PIE.
 UBlueprint* RetiredScratch = nullptr;
 UClass* SourceGeneratedClass = BP->GeneratedClass;
 {
  TurboMCPGraphScratch::FCopy Scratch;
  if (!TestTrue(TEXT("Scratch fixture initialized"), Scratch.Init(BP, Function))) return false;
  RetiredScratch = Scratch.Owner.Get();
  RetiredScratch->Status = BS_Error;
  TestTrue(TEXT("Scratch exists while its operation runs"), IsValid(RetiredScratch));
 }
 TestFalse(TEXT("Failed scratch is retired before garbage collection"), IsValid(RetiredScratch));
 TestTrue(TEXT("Source generated class remains valid"), IsValid(SourceGeneratedClass));
 TestTrue(TEXT("Source class identity preserved"), BP->GeneratedClass == SourceGeneratedClass);

 TestTrue(TEXT("Typed dispatcher preview"),UTurboMCPPreviewToolset::PreviewTurboMCPDispatcherAdd(MergeAsset,TEXT("MergeReady"),TEXT("Ready:bool,Owner:object</Script/Engine.Actor>")).StartsWith(TEXT("preview:")));
 TestTrue(TEXT("Typed dispatcher edit"),Record(TEXT("Create dispatcher"),UTurboMCPEditToolset::EditTurboMCPDispatcherAdd(MergeAsset,TEXT("MergeReady"),TEXT("Ready:bool,Owner:object</Script/Engine.Actor>"))).StartsWith(TEXT("edited:")));
 const auto* DispatcherProperty=FindFProperty<FMulticastDelegateProperty>(BP->SkeletonGeneratedClass,TEXT("MergeReady"));
 TestNotNull(TEXT("Dispatcher property created"),DispatcherProperty);
 if(DispatcherProperty && DispatcherProperty->SignatureFunction){TestNotNull(TEXT("Dispatcher bool parameter preserved"),FindFProperty<FBoolProperty>(DispatcherProperty->SignatureFunction,TEXT("Ready")));const auto* OwnerParameter=FindFProperty<FObjectPropertyBase>(DispatcherProperty->SignatureFunction,TEXT("Owner"));TestNotNull(TEXT("Dispatcher object parameter preserved"),OwnerParameter);if(OwnerParameter)TestTrue(TEXT("Dispatcher object class preserved"),OwnerParameter->PropertyClass==AActor::StaticClass());}
 TestTrue(TEXT("Dispatcher duplicate refuses"),UTurboMCPEditToolset::EditTurboMCPDispatcherAdd(MergeAsset,TEXT("MergeReady"),TEXT("")).StartsWith(TEXT("error:")));
 TestTrue(TEXT("Dispatcher duplicate parameter refuses"),UTurboMCPPreviewToolset::PreviewTurboMCPDispatcherAdd(MergeAsset,TEXT("InvalidDispatcher"),TEXT("Same:bool,Same:int")).StartsWith(TEXT("error:")));
 UBlueprint* ExcludedTarget=FKismetEditorUtilities::CreateBlueprint(AActor::StaticClass(),Package,TEXT("BP_ExcludedMembers"),BPTYPE_Normal,UBlueprint::StaticClass(),UBlueprintGeneratedClass::StaticClass());
 const FString Exclusions=TEXT("MergeMemberProbe|Request_0_0|MergeReady");
 const int32 ExcludedPagesBefore=ExcludedTarget->UbergraphPages.Num();
 TestTrue(TEXT("Unknown member exclusion refuses"),UTurboMCPEditToolset::EditTurboMCPMergeMembers(MergeAsset,ExcludedTarget->GetPathName(),TEXT("NoSuchMember")).StartsWith(TEXT("error:")));
 TestEqual(TEXT("Unknown exclusion leaves target pages unchanged"),ExcludedTarget->UbergraphPages.Num(),ExcludedPagesBefore);
 const FString ExcludedPreview=Record(TEXT("Excluded member preview"),UTurboMCPPreviewToolset::PreviewTurboMCPMergeMembers(MergeAsset,ExcludedTarget->GetPathName(),Exclusions));
 TestTrue(TEXT("Preview identifies excluded event"),ExcludedPreview.Contains(TEXT("excluded: Request_0_0")));
 TestTrue(TEXT("Preview identifies event destination"),ExcludedPreview.Contains(TEXT("new declaration event graph:")));
 TestEqual(TEXT("Exclusion preview does not create pages"),ExcludedTarget->UbergraphPages.Num(),ExcludedPagesBefore);
 TestTrue(TEXT("Selected declarations created"),UTurboMCPEditToolset::EditTurboMCPMergeMembers(MergeAsset,ExcludedTarget->GetPathName(),Exclusions).StartsWith(TEXT("edited:")));
 TestNull(TEXT("Excluded function absent"),ExcludedTarget->SkeletonGeneratedClass->FindFunctionByName(TEXT("MergeMemberProbe")));
 TestNull(TEXT("Excluded event absent"),ExcludedTarget->SkeletonGeneratedClass->FindFunctionByName(TEXT("Request_0_0")));
 TestNull(TEXT("Excluded dispatcher absent"),FindFProperty<FMulticastDelegateProperty>(ExcludedTarget->SkeletonGeneratedClass,TEXT("MergeReady")));
 TestNotNull(TEXT("Included custom event declared"),ExcludedTarget->SkeletonGeneratedClass->FindFunctionByName(TEXT("Destroyed_0_0")));
 auto FindEvent=[](UBlueprint* Asset,FName Name)->UK2Node_CustomEvent*{for(UEdGraph* Page:Asset->UbergraphPages)if(Page)for(UEdGraphNode* Node:Page->Nodes)if(auto* Event=Cast<UK2Node_CustomEvent>(Node))if(Event->CustomFunctionName==Name)return Event;return nullptr;};
 auto* DeclaredEvent=FindEvent(ExcludedTarget,TEXT("Destroyed_0_0"));
 if(TestNotNull(TEXT("Event declaration node exists"),DeclaredEvent))
 {
  TestNotNull(TEXT("Event parameter preserved"),DeclaredEvent->FindPin(TEXT("DestroyedActor")));
  for(UEdGraphPin* Pin:DeclaredEvent->Pins)TestTrue(TEXT("Declaration has no body links"),Pin->LinkedTo.IsEmpty());
  DeclaredEvent->bCallInEditor=!DeclaredEvent->bCallInEditor;
  TestTrue(TEXT("Event settings conflict refuses"),UTurboMCPPreviewToolset::PreviewTurboMCPMergeMembers(MergeAsset,ExcludedTarget->GetPathName(),Exclusions).StartsWith(TEXT("error:")));
  DeclaredEvent->bCallInEditor=!DeclaredEvent->bCallInEditor;
 }
 TestTrue(TEXT("Repeated selected preparation unchanged"),UTurboMCPEditToolset::EditTurboMCPMergeMembers(MergeAsset,ExcludedTarget->GetPathName(),Exclusions).StartsWith(TEXT("unchanged:")));
 const int32 TargetGraphsBefore=MergeTarget->FunctionGraphs.Num();
 TestTrue(TEXT("Member preview"),Record(TEXT("Prepare members preview"),UTurboMCPPreviewToolset::PreviewTurboMCPMergeMembers(MergeAsset,MergeTarget->GetPathName())).StartsWith(TEXT("preview:")));
 TestEqual(TEXT("Member preview leaves target unchanged"),MergeTarget->FunctionGraphs.Num(),TargetGraphsBefore);
 TestTrue(TEXT("Member creation"),Record(TEXT("Prepare members edit"),UTurboMCPEditToolset::EditTurboMCPMergeMembers(MergeAsset,MergeTarget->GetPathName())).StartsWith(TEXT("edited:")));
 TestTrue(TEXT("Repeated member preparation is unchanged"),Record(TEXT("Repeat member preparation"),UTurboMCPEditToolset::EditTurboMCPMergeMembers(MergeAsset,MergeTarget->GetPathName())).StartsWith(TEXT("unchanged:")));
 TestTrue(TEXT("Same-asset merge refuses"),UTurboMCPPreviewToolset::PreviewTurboMCPMergeMembers(MergeAsset,MergeAsset).StartsWith(TEXT("error:")));
 TestTrue(TEXT("Missing graph refuses"),UTurboMCPPreviewToolset::PreviewTurboMCPGraphTransfer(MergeAsset,TEXT("MissingGraph"),MergeTarget->GetPathName(),TEXT("")).StartsWith(TEXT("error:")));
 TestTrue(TEXT("Graph transfer preview"),Record(TEXT("Transfer preview"),UTurboMCPPreviewToolset::PreviewTurboMCPGraphTransfer(MergeAsset,Function->GetName(),MergeTarget->GetPathName(),TEXT(""))).StartsWith(TEXT("preview:")));
 const FString TransferPlan = UTurboMCPPreviewToolset::PreviewTurboMCPGraphTransfer(MergeAsset, Function->GetName(), MergeTarget->GetPathName(), TEXT(""));
 TestTrue(TEXT("Transfer preview lists nodes"), TransferPlan.Contains(TEXT("\nnode:")));
 TestTrue(TEXT("Transfer preview lists links"), TransferPlan.Contains(TEXT("\nlink:")));
 const FString TransferResult = Record(TEXT("Transfer edit"),UTurboMCPEditToolset::EditTurboMCPGraphTransfer(MergeAsset,Function->GetName(),MergeTarget->GetPathName(),TEXT("")));
 TestTrue(TEXT("Graph transfer edit"), TransferResult.StartsWith(TEXT("edited:")));
 TestTrue(TEXT("Local graph transfer"),Record(TEXT("Local graph transfer"),UTurboMCPEditToolset::EditTurboMCPGraphTransfer(MergeAsset,TEXT("MergeMemberProbe"),MergeTarget->GetPathName(),TEXT(""))).StartsWith(TEXT("edited:")));
 auto* ProtectedEvent=FindEvent(MergeTarget,TEXT("Request_0_0"));
 if(TestNotNull(TEXT("Prepared event is available for transfer"),ProtectedEvent))
 {
  ProtectedEvent->NodeComment=TEXT("Authored declaration: preserve me");
  TestTrue(TEXT("Authored event declaration refuses transfer"),UTurboMCPPreviewToolset::PreviewTurboMCPGraphTransfer(MergeAsset,Graphs[0]->GetName(),MergeTarget->GetPathName(),TEXT("CopiedEvents")).StartsWith(TEXT("error:")));
  ProtectedEvent->NodeComment.Reset();
 }
 TestTrue(TEXT("Event graph transfer"),Record(TEXT("Event graph transfer"),UTurboMCPEditToolset::EditTurboMCPGraphTransfer(MergeAsset,Graphs[0]->GetName(),MergeTarget->GetPathName(),TEXT("CopiedEvents"))).StartsWith(TEXT("edited:")));
 TestTrue(TEXT("Occupied function refuses replacement"),UTurboMCPEditToolset::EditTurboMCPGraphTransfer(MergeAsset,Function->GetName(),MergeTarget->GetPathName(),TEXT("" )).StartsWith(TEXT("error:")));
 TestEqual(TEXT("Source graph was not modified"),MergeSnapshot(Function),SourceSnapshot);
 UEdGraph* Transferred=TurboMCPNodeId::FindGraphByName(MergeTarget,Function->GetName());
 TestNotNull(TEXT("Transferred function exists"),Transferred);
 if(Transferred != nullptr){
  TestEqual(TEXT("Graph node count preserved"),Transferred->Nodes.Num(),Function->Nodes.Num());
  for (UEdGraphNode* Node : Transferred->Nodes)
   TestTrue(TEXT("Edit result provides every fresh target ID"), TransferResult.Contains(TEXT(" -> ")+TurboMCPNodeId::MakeForNode(Node,true)));
  TestFalse(TEXT("Edit result has no unresolved identity mappings"), TransferResult.Contains(TEXT("unresolved after structural refresh")));

  auto Links=[](UEdGraph* G){int32 Count=0;for(UEdGraphNode* N:G->Nodes)for(auto* P:N->Pins)if(P)Count+=P->LinkedTo.Num();return Count;};
  TestEqual(TEXT("Graph links preserved"),Links(Transferred),Links(Function));
 }
 UEdGraph* LocalGraph=TurboMCPNodeId::FindGraphByName(MergeTarget,TEXT("MergeMemberProbe"));
 TestNotNull(TEXT("Transferred local function exists"),LocalGraph);
 if(LocalGraph != nullptr){TArray<UK2Node_FunctionEntry*> LE;LocalGraph->GetNodesOfClass(LE);TestEqual(TEXT("Local copied"),LE[0]->LocalVariables.Num(),1);if(LE[0]->LocalVariables.Num())TestEqual(TEXT("Copied local default"),LE[0]->LocalVariables[0].DefaultValue,FString(TEXT("7")));}
 Record(TEXT("Compile merged target"),UTurboMCPEditToolset::EditTurboMCPCompile(MergeTarget->GetPathName()));TestTrue(TEXT("Merged target compiles"),MergeTarget->Status!=BS_Error);
 // S4 GC-52: a real inherited function override must retain its parent identity.
 UBlueprint* OverrideSource = FKismetEditorUtilities::CreateBlueprint(BP->GeneratedClass, Package, TEXT("BP_TransferOverrideSource"), BPTYPE_Normal, UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass());
 UBlueprint* OverrideTarget = FKismetEditorUtilities::CreateBlueprint(BP->GeneratedClass, Package, TEXT("BP_TransferOverrideTarget"), BPTYPE_Normal, UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass());
 if (!TestNotNull(TEXT("Override source created"), OverrideSource) || !TestNotNull(TEXT("Override target created"), OverrideTarget)) return false;
 UEdGraph* OverrideGraph = UBlueprintEditorLibrary::AddFunctionOverride(OverrideSource, Function->GetFName());
 if (!TestNotNull(TEXT("Source inherited override created"), OverrideGraph)) return false;
 FKismetEditorUtilities::CompileBlueprint(OverrideSource);
 TestTrue(TEXT("Source override compiles"), OverrideSource->Status != BS_Error);
 UEdGraph* TargetScaffold = UBlueprintEditorLibrary::AddFunctionOverride(OverrideTarget, Function->GetFName());
 if (!TestNotNull(TEXT("Target inherited scaffold created"), TargetScaffold)) return false;
 FKismetEditorUtilities::CompileBlueprint(OverrideTarget);
 for (UEdGraphNode* N : TargetScaffold->Nodes)
 {
  FString Details = N->GetClass()->GetName() + TEXT(" comment=") + N->NodeComment;
  for (UEdGraphPin* P : N->Pins) if (P) Details += FString::Printf(TEXT("\n%s dir=%d links=%d default=%s auto=%s object=%s"), *P->PinName.ToString(), int32(P->Direction), P->LinkedTo.Num(), *P->DefaultValue, *P->AutogeneratedDefaultValue, *GetPathNameSafe(P->DefaultObject));
  Record(TEXT("Scaffold shape"), Details);
 }
 const FString ScaffoldPreview = Record(TEXT("Parent scaffold preview"), UTurboMCPPreviewToolset::PreviewTurboMCPGraphTransfer(OverrideSource->GetPathName(), Function->GetName(), OverrideTarget->GetPathName(), TEXT("")));
 TestTrue(TEXT("Fresh parent scaffold accepted explicitly"), ScaffoldPreview.StartsWith(TEXT("preview:")) && ScaffoldPreview.Contains(TEXT("target scaffold:")));
 UEdGraphNode* ScaffoldNode = TargetScaffold->Nodes.Last();
 ScaffoldNode->NodeComment = TEXT("authored note");
 TestTrue(TEXT("Authored scaffold is protected"), UTurboMCPPreviewToolset::PreviewTurboMCPGraphTransfer(OverrideSource->GetPathName(), Function->GetName(), OverrideTarget->GetPathName(), TEXT("" )).StartsWith(TEXT("error:")));
 ScaffoldNode->NodeComment.Empty();
 ScaffoldNode->SetEnabledState(ENodeEnabledState::Disabled);
 TestTrue(TEXT("Disabled scaffold node is protected"), UTurboMCPPreviewToolset::PreviewTurboMCPGraphTransfer(OverrideSource->GetPathName(), Function->GetName(), OverrideTarget->GetPathName(), TEXT("" )).StartsWith(TEXT("error:")));
 ScaffoldNode->SetEnabledState(ENodeEnabledState::Enabled);

 TestTrue(TEXT("Inherited override transfer"), Record(TEXT("Inherited override transfer"), UTurboMCPEditToolset::EditTurboMCPGraphTransfer(OverrideSource->GetPathName(), Function->GetName(), OverrideTarget->GetPathName(), TEXT(""))).StartsWith(TEXT("edited:")));
 FKismetEditorUtilities::CompileBlueprint(OverrideTarget);
 TestTrue(TEXT("Transferred inherited override compiles"), OverrideTarget->Status != BS_Error);
 UEdGraph* CopiedOverride = TurboMCPNodeId::FindGraphByName(OverrideTarget, Function->GetName());
 TestNotNull(TEXT("Transferred override graph exists"), CopiedOverride);
 if (CopiedOverride != nullptr) {
  TArray<UK2Node_FunctionEntry*> OverrideEntries; CopiedOverride->GetNodesOfClass(OverrideEntries);
  TestEqual(TEXT("Override entry count"), OverrideEntries.Num(), 1);
  if (OverrideEntries.Num() == 1) TestNotNull(TEXT("Override retains external parent reference"), OverrideEntries[0]->FunctionReference.GetMemberParentClass());
 }
 // S5 CB-06: refit follows moved nodes without retaining an empty old rectangle.
 auto* RefitBox = NewObject<UEdGraphNode_Comment>(Function);
 Init(RefitBox, Function, -9000, -9000); RefitBox->NodeWidth = 200; RefitBox->NodeHeight = 200;
 const FString RefitId = TurboMCPNodeId::MakeForNode(RefitBox, true);
 const FString RefitNodes = TurboMCPNodeId::MakeForNode(Entry, true);
 const int32 EntryX = Entry->NodePosX, EntryY = Entry->NodePosY;
 const FString RefitPreview = UTurboMCPPreviewToolset::PreviewTurboMCPComment(MergeAsset, Function->GetName(), TEXT("refit"), RefitNodes, RefitId);
 TestTrue(TEXT("Refit preview explains selection-only enclosure"), RefitPreview.Contains(TEXT("explicit refit:")));
 TestEqual(TEXT("Refit preview preserves old box position"), RefitBox->NodePosX, -9000);
 Record(TEXT("Refit moved selection"), UTurboMCPEditToolset::EditTurboMCPComment(MergeAsset, Function->GetName(), TEXT("refit"), RefitNodes, RefitId));
 TestTrue(TEXT("Refit releases old empty rectangle"), RefitBox->NodePosX > -1000 && RefitBox->NodePosY > -1000 && RefitBox->NodeWidth < 2000);
 TestEqual(TEXT("Refit preserves node X"), Entry->NodePosX, EntryX);
 TestEqual(TEXT("Refit preserves node Y"), Entry->NodePosY, EntryY);
 TestTrue(TEXT("Refit requires explicit nodes"), UTurboMCPPreviewToolset::PreviewTurboMCPComment(MergeAsset, Function->GetName(), TEXT("refit"), TEXT(""), RefitId).StartsWith(TEXT("error:")));
 const FString Removal=Record(TEXT("Interface removal preview"),UTurboMCPPreviewToolset::PreviewTurboMCPInterfaceRemove(MergeAsset,MergeInterface->GetPathName()));
 TestTrue(TEXT("Removal preview accepted"),Removal.StartsWith(TEXT("preview:")));
 TestTrue(TEXT("Removal without confirmation refuses"),UTurboMCPEditToolset::EditTurboMCPInterfaceRemove(MergeAsset,MergeInterface->GetPathName(),TEXT("")).StartsWith(TEXT("error:")));
 TestTrue(TEXT("Confirmed interface removal"),Record(TEXT("Remove interface"),UTurboMCPEditToolset::EditTurboMCPInterfaceRemove(MergeAsset,MergeInterface->GetPathName(),BP->GetPathName()+TEXT("|")+MergeInterface->GeneratedClass->GetPathName())).StartsWith(TEXT("edited:")));
 TestNotNull(TEXT("Interface function body retained"),TurboMCPNodeId::FindGraphByName(BP,TEXT("ReadMergeState")));
 int32 Total=0;for(auto* G:Graphs)Total+=G->Nodes.Num();Record(TEXT("Fixture manifest"),FString::Printf(TEXT("asset: %s\n12 request events + 12 delegate handlers; three event graphs; 48 arithmetic nodes; total nodes: %d"),*BP->GetPathName(),Total));
 for(int32 I=0;I<Graphs.Num();++I){
  auto* G=Graphs[I];const int32 Count=G->Nodes.Num();
  auto Snapshot=[&](){TSet<UObject*> Nodes;for(UEdGraphNode* N:G->Nodes)Nodes.Add(N);FString Text;FEdGraphUtilities::ExportNodesToText(Nodes,Text);return Text;};
  const FString Before=Snapshot();
  const FString Read=Record(TEXT("Graph context"),UTurboMCPReadToolset::ReadTurboMCPGraphLoad(BP->GetPathName(),G->GetName(),TEXT("read"),TEXT("bodies"),0,24000));
  TestFalse(TEXT("Graph context available"),Read.StartsWith(TEXT("error:")));
  auto Shot=[&](const FString& Mode,const FString& Target,const FString& Size,int32 W=0,int32 H=0){const FString Out=Record(G->GetName()+TEXT(" / ")+Mode+TEXT(" / ")+Size,UTurboMCPLookToolset::LookTurboMCPCapture(BP->GetPathName(),G->GetName(),Mode,Target,Size));ImageCheck(Out,W,H);if(Mode==TEXT("graph"))
  {
   FRegexMatcher Bounds(FRegexPattern(TEXT("frame bounds: x (-?[0-9.]+) to (-?[0-9.]+), y (-?[0-9.]+) to (-?[0-9.]+)")),Out);
   const bool HasBounds=Bounds.FindNext();TestTrue(TEXT("Overview reports bounds"),HasBounds);
   if(HasBounds)
   {
    const double Left=FCString::Atod(*Bounds.GetCaptureGroup(1)),Right=FCString::Atod(*Bounds.GetCaptureGroup(2));
    const double Top=FCString::Atod(*Bounds.GetCaptureGroup(3)),Bottom=FCString::Atod(*Bounds.GetCaptureGroup(4));
    for(UEdGraphNode* N:G->Nodes) if(const auto* Comment=Cast<UEdGraphNode_Comment>(N))
     TestTrue(TEXT("Overview includes every comment rectangle"),Left<=Comment->NodePosX && Top<=Comment->NodePosY && Right>=Comment->NodePosX+Comment->NodeWidth && Bottom>=Comment->NodePosY+Comment->NodeHeight);
   }
  }
  TestTrue(TEXT("Capture explains readability uncertainty"),Out.Contains(TEXT("readability:")));TestTrue(TEXT("Capture offers exact detail workflow"),Out.Contains(TEXT("ReadTurboMCPNodeFind")) && Out.Contains(TEXT("hop count, not pixels")));};
  Shot(TEXT("graph"),TEXT(""),TEXT("1600x900"),1600,900);
  Shot(TEXT("node"),TurboMCPNodeId::MakeForNode(Targets[I]),TEXT("1200x800"),1200,800);
  Shot(TEXT("neighbours"),TurboMCPNodeId::MakeForNode(Targets[I]),TEXT("1"));
  if(I==0){Shot(TEXT("region"),TEXT("-100,-350,2100,1600"),TEXT("2048x1536"),2048,1536);Shot(TEXT("box"),TEXT("0"),TEXT("1600x900"),1600,900);Shot(TEXT("graph"),TEXT(""),TEXT(""),1600,900);
   TestTrue(TEXT("Dimensions rejected in hop mode"),UTurboMCPLookToolset::LookTurboMCPCapture(BP->GetPathName(),G->GetName(),TEXT("neighbours"),TurboMCPNodeId::MakeForNode(Targets[I]),TEXT("1200x800")).Contains(TEXT("hop count")));
   TestTrue(TEXT("Hop count rejected in dimension mode"),UTurboMCPLookToolset::LookTurboMCPCapture(BP->GetPathName(),G->GetName(),TEXT("node"),TurboMCPNodeId::MakeForNode(Targets[I]),TEXT("1")).Contains(TEXT("not a size")));}
  TestEqual(TEXT("Node count preserved"),G->Nodes.Num(),Count);
  TestEqual(TEXT("Graph text preserved"),Snapshot(),Before);
 }
 const FString Refusal=Record(TEXT("Viewport requires explicit target"),UTurboMCPLookToolset::LookTurboMCPCapture(TEXT(""),TEXT(""),TEXT("viewport"),TEXT(""),TEXT("")));TestTrue(TEXT("Missing PIE target refuses"),Refusal.StartsWith(TEXT("error:")));
 ImageCheck(Record(TEXT("Editor screen"),UTurboMCPLookToolset::LookTurboMCPCapture(TEXT(""),TEXT(""),TEXT("screen"),TEXT(""),TEXT(""))));
 UWidgetBlueprint* WB=FWidgetBlueprintOperationUtils::CreateWidgetBlueprint(Package,TEXT("WBP_Vision"),BPTYPE_Normal,UUserWidget::StaticClass(),UCanvasPanel::StaticClass(),TEXT("Vision"),false);
 auto* Subtree=WB->WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(),TEXT("PurchasePanel"));
 WB->WidgetVariableNameToGuidMap.Add(Subtree->GetFName(),FGuid::NewGuid());
 auto* SubtreeSlot=CastChecked<UCanvasPanel>(WB->WidgetTree->RootWidget)->AddChildToCanvas(Subtree);SubtreeSlot->SetPosition(FVector2D(20,30));SubtreeSlot->SetSize(FVector2D(620,420));
 for(int32 I=0;I<9;++I){auto* Extra=WB->WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),FName(*FString::Printf(TEXT("Extra%d"),I)));Extra->SetText(FText::FromString(FString::Printf(TEXT("Item %d"),I)));WB->WidgetVariableNameToGuidMap.Add(Extra->GetFName(),FGuid::NewGuid());auto* ExtraSlot=Subtree->AddChildToCanvas(Extra);ExtraSlot->SetPosition(FVector2D(I*10,120+I*25));}
 auto* Label=WB->WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),TEXT("StatusLabel"));Label->SetText(FText::FromString(TEXT("Vision fixture: READY 0123456789")));WB->WidgetVariableNameToGuidMap.Add(Label->GetFName(),FGuid::NewGuid());
 auto* Slot=Subtree->AddChildToCanvas(Label);Slot->SetPosition(FVector2D(40,40));Slot->SetSize(FVector2D(560,80));

 UWidgetBlueprint* WidgetTarget=FWidgetBlueprintOperationUtils::CreateWidgetBlueprint(Package,TEXT("WBP_MergeTarget"),BPTYPE_Normal,UUserWidget::StaticClass(),UCanvasPanel::StaticClass(),TEXT("Target"),false);
 if(!TestNotNull(TEXT("Widget target"),WidgetTarget))return false;
 const FString ParentName=WidgetTarget->WidgetTree->RootWidget->GetName();
 const FText OriginalText=Label->GetText();
 TestTrue(TEXT("Subtree preview"),Record(TEXT("Subtree preview"),UTurboMCPPreviewToolset::PreviewTurboMCPWidgetSubtreeCopy(WB->GetPathName(),Subtree->GetName(),WidgetTarget->GetPathName(),ParentName)).StartsWith(TEXT("preview:")));
 TestNull(TEXT("Subtree preview does not add widgets"),WidgetTarget->WidgetTree->FindWidget(Label->GetFName()));
 TestTrue(TEXT("Subtree edit"),Record(TEXT("Subtree edit"),UTurboMCPEditToolset::EditTurboMCPWidgetSubtreeCopy(WB->GetPathName(),Subtree->GetName(),WidgetTarget->GetPathName(),ParentName)).StartsWith(TEXT("edited:")));
 auto* CopiedPanel=Cast<UCanvasPanel>(WidgetTarget->WidgetTree->FindWidget(Subtree->GetFName()));
 if(CopiedPanel){TestEqual(TEXT("All ten child widgets copied"),CopiedPanel->GetChildrenCount(),10);auto* RootSlot=Cast<UCanvasPanelSlot>(CopiedPanel->Slot);if(RootSlot){TestEqual(TEXT("Subtree root position preserved"),RootSlot->GetPosition(),SubtreeSlot->GetPosition());TestEqual(TEXT("Subtree root size preserved"),RootSlot->GetSize(),SubtreeSlot->GetSize());}}
 TestNotNull(TEXT("Subtree panel exists"),CopiedPanel);
 auto* CopiedLabel=Cast<UTextBlock>(WidgetTarget->WidgetTree->FindWidget(Label->GetFName()));
 if(TestNotNull(TEXT("Copied label"),CopiedLabel)){
  TestTrue(TEXT("Copied root belongs to target tree"),CopiedLabel->GetOuter()==WidgetTarget->WidgetTree);
  TestTrue(TEXT("Localized text identity retained"),CopiedLabel->GetText().IdenticalTo(OriginalText));
  auto* CopiedSlot=Cast<UCanvasPanelSlot>(CopiedLabel->Slot);if(TestNotNull(TEXT("Copied root slot"),CopiedSlot)){TestEqual(TEXT("Slot position preserved"),CopiedSlot->GetPosition(),Slot->GetPosition());TestEqual(TEXT("Slot size preserved"),CopiedSlot->GetSize(),Slot->GetSize());}
 }
 TestTrue(TEXT("Name collision refuses"),UTurboMCPEditToolset::EditTurboMCPWidgetSubtreeCopy(WB->GetPathName(),Subtree->GetName(),WidgetTarget->GetPathName(),ParentName).StartsWith(TEXT("error:")));
 // FT-109: repeat copy with collisions, preserving the first copy and source.
 Label->Navigation = NewObject<UWidgetNavigation>(Label);
 Label->Navigation->Next.Rule = EUINavigationRule::Explicit;
 Label->Navigation->Next.WidgetToFocus = TEXT("Extra0");
 const FString RenamePreview = Record(TEXT("Subtree auto-rename preview"), UTurboMCPPreviewToolset::PreviewTurboMCPWidgetSubtreeCopy(WB->GetPathName(), Subtree->GetName(), WidgetTarget->GetPathName(), ParentName, true));
 TestTrue(TEXT("Preview lists root and descendant mappings"), RenamePreview.Contains(TEXT("PurchasePanel -> PurchasePanel_Copy")) && RenamePreview.Contains(TEXT("StatusLabel -> StatusLabel_Copy")));
 TestNull(TEXT("Auto-rename preview leaves tree unchanged"), WidgetTarget->WidgetTree->FindWidget(TEXT("PurchasePanel_Copy")));
 const FString RenamedCopy = Record(TEXT("Subtree auto-rename edit"), UTurboMCPEditToolset::EditTurboMCPWidgetSubtreeCopy(WB->GetPathName(), Subtree->GetName(), WidgetTarget->GetPathName(), ParentName, true));
 TestTrue(TEXT("Auto-rename copy accepted"), RenamedCopy.StartsWith(TEXT("edited:")));
 auto* RenamedPanel = Cast<UCanvasPanel>(WidgetTarget->WidgetTree->FindWidget(TEXT("PurchasePanel_Copy")));
 auto* RenamedLabel = Cast<UTextBlock>(WidgetTarget->WidgetTree->FindWidget(TEXT("StatusLabel_Copy")));
 if (!TestNotNull(TEXT("Renamed panel exists"), RenamedPanel) || !TestNotNull(TEXT("Renamed label exists"), RenamedLabel)) return false;
 TestEqual(TEXT("Renamed subtree retains all children"), RenamedPanel->GetChildrenCount(), 10);
 TestTrue(TEXT("Renamed descendant retains parent"), RenamedLabel->GetParent() == RenamedPanel);
 TestTrue(TEXT("First copy is unchanged"), WidgetTarget->WidgetTree->FindWidget(TEXT("PurchasePanel")) == CopiedPanel && WidgetTarget->WidgetTree->FindWidget(TEXT("StatusLabel")) == CopiedLabel);
 TestTrue(TEXT("Renamed text identity retained"), RenamedLabel->GetText().IdenticalTo(OriginalText));
 if (TestNotNull(TEXT("Copied navigation exists"), RenamedLabel->Navigation.Get()))
  TestEqual(TEXT("Copied navigation uses renamed sibling"), RenamedLabel->Navigation->Next.WidgetToFocus, FName(TEXT("Extra0_Copy")));
 TestEqual(TEXT("Source navigation unchanged"), Label->Navigation->Next.WidgetToFocus, FName(TEXT("Extra0")));
 TestTrue(TEXT("Source name unchanged"), WB->WidgetTree->FindWidget(TEXT("StatusLabel")) == Label);
 const FString NextNames = UTurboMCPPreviewToolset::PreviewTurboMCPWidgetSubtreeCopy(WB->GetPathName(), Subtree->GetName(), WidgetTarget->GetPathName(), ParentName, true);
 TestTrue(TEXT("Next copy skips reserved suffix"), NextNames.Contains(TEXT("PurchasePanel -> PurchasePanel_Copy2")));
 Label->Navigation->Next.WidgetToFocus = WB->WidgetTree->RootWidget->GetFName();
 TestTrue(TEXT("Outside navigation refuses"), UTurboMCPPreviewToolset::PreviewTurboMCPWidgetSubtreeCopy(WB->GetPathName(), Subtree->GetName(), WidgetTarget->GetPathName(), ParentName, true).Contains(TEXT("outside widget")));
 Label->Navigation->Next.WidgetToFocus = TEXT("Extra0");
 TestTrue(TEXT("Source text unchanged"),Label->GetText().IdenticalTo(OriginalText));
 Record(TEXT("Compile copied widget"),UTurboMCPEditToolset::EditTurboMCPCompile(WidgetTarget->GetPathName()));TestTrue(TEXT("Copied widget compiles"),WidgetTarget->Status!=BS_Error);
 if(CopiedLabel){
  const FString OldWidgetName=CopiedLabel->GetName();
  UPanelSlot* OriginalSlot=CopiedLabel->Slot;
  FWidgetBlueprintOperationUtils::ToggleWidgetAsVariable(WidgetTarget,CopiedLabel,true,true);
  FKismetEditorUtilities::CompileBlueprint(WidgetTarget);
  UEdGraph* RenameGraph=UBlueprintEditorLibrary::AddFunctionGraph(WidgetTarget,TEXT("ReadRenamedWidget"));
  UK2Node_VariableGet* RenameGetter=NewObject<UK2Node_VariableGet>(RenameGraph);
  RenameGetter->VariableReference.SetSelfMember(FName(*OldWidgetName));
  RenameGraph->AddNode(RenameGetter,false,false);RenameGetter->CreateNewGuid();RenameGetter->AllocateDefaultPins();
  const FGuid GetterGuid=RenameGetter->NodeGuid;

  TestTrue(TEXT("Widget rename collision refuses"),UTurboMCPEditToolset::EditTurboMCPWidgetRename(WidgetTarget->GetPathName(),OldWidgetName,ParentName).StartsWith(TEXT("error:")));
  TestTrue(TEXT("Widget rename invalid identifier refuses"),UTurboMCPEditToolset::EditTurboMCPWidgetRename(WidgetTarget->GetPathName(),OldWidgetName,TEXT("Bad Name")).StartsWith(TEXT("error:")));
  TestEqual(TEXT("Refused rename preserves name"),CopiedLabel->GetName(),OldWidgetName);
  TestTrue(TEXT("Widget rename succeeds"),Record(TEXT("Widget rename"),UTurboMCPEditToolset::EditTurboMCPWidgetRename(WidgetTarget->GetPathName(),OldWidgetName,TEXT("RenamedPurchaseLabel"))).StartsWith(TEXT("edited:")));
  TestNull(TEXT("Old widget name removed"),WidgetTarget->WidgetTree->FindWidget(FName(*OldWidgetName)));
  TestTrue(TEXT("New name resolves same widget"),WidgetTarget->WidgetTree->FindWidget(TEXT("RenamedPurchaseLabel"))==CopiedLabel);
  TestTrue(TEXT("Rename preserves slot identity"),CopiedLabel->Slot==OriginalSlot);
  TestTrue(TEXT("Rename preserves localized text"),CopiedLabel->GetText().IdenticalTo(OriginalText));
  TestEqual(TEXT("Graph getter follows renamed widget"),RenameGetter->VariableReference.GetMemberName(),FName(TEXT("RenamedPurchaseLabel")));
  TestEqual(TEXT("Graph getter identity preserved"),RenameGetter->NodeGuid,GetterGuid);
  TestNotNull(TEXT("Renamed getter value pin exists"),RenameGetter->FindPin(TEXT("RenamedPurchaseLabel")));

  TestTrue(TEXT("Repeated rename is no-op"),UTurboMCPEditToolset::EditTurboMCPWidgetRename(WidgetTarget->GetPathName(),TEXT("RenamedPurchaseLabel"),TEXT("RenamedPurchaseLabel")).StartsWith(TEXT("unchanged:")));
  Record(TEXT("Compile renamed widget"),UTurboMCPEditToolset::EditTurboMCPCompile(WidgetTarget->GetPathName()));
  TestTrue(TEXT("Renamed widget compiles"),WidgetTarget->Status!=BS_Error);
  TestTrue(TEXT("Referenced widget removal refuses"),Record(TEXT("Referenced removal"),UTurboMCPEditToolset::EditTurboMCPWidgetRemove(WidgetTarget->GetPathName(),TEXT("RenamedPurchaseLabel"))).StartsWith(TEXT("unchanged:")));
  TestTrue(TEXT("Referenced subtree removal refuses"),UTurboMCPEditToolset::EditTurboMCPWidgetRemove(WidgetTarget->GetPathName(),CopiedPanel->GetName()).StartsWith(TEXT("unchanged:")));
  TestTrue(TEXT("Refused removal preserves widget"),WidgetTarget->WidgetTree->FindWidget(TEXT("RenamedPurchaseLabel"))==CopiedLabel);
  TestTrue(TEXT("Refused removal preserves getter"),RenameGraph->Nodes.Contains(RenameGetter));
  UK2Node_ComponentBoundEvent* RemovalEvent=NewObject<UK2Node_ComponentBoundEvent>(RenameGraph);
  RemovalEvent->ComponentPropertyName=TEXT("Extra8");RenameGraph->AddNode(RemovalEvent,false,false);RemovalEvent->CreateNewGuid();
  TestTrue(TEXT("Bound event blocks removal"),UTurboMCPEditToolset::EditTurboMCPWidgetRemove(WidgetTarget->GetPathName(),TEXT("Extra8")).StartsWith(TEXT("unchanged:")));
  RenameGraph->RemoveNode(RemovalEvent);
  UWidget* NavigationSource=WidgetTarget->WidgetTree->FindWidget(TEXT("Extra7"));
  if(NavigationSource){
   NavigationSource->Navigation=NewObject<UWidgetNavigation>(NavigationSource);
   NavigationSource->Navigation->Right.Rule=EUINavigationRule::Explicit;
   NavigationSource->Navigation->Right.WidgetToFocus=TEXT("Extra8");
   TestTrue(TEXT("Explicit navigation blocks removal"),UTurboMCPEditToolset::EditTurboMCPWidgetRemove(WidgetTarget->GetPathName(),TEXT("Extra8")).StartsWith(TEXT("unchanged:")));
   NavigationSource->Navigation=nullptr;
  }
  const FString RemovePreview=Record(TEXT("Widget removal preview"),UTurboMCPPreviewToolset::PreviewTurboMCPWidgetRemove(WidgetTarget->GetPathName(),TEXT("Extra8")));
  TestTrue(TEXT("Unreferenced removal previews"),RemovePreview.StartsWith(TEXT("preview:")));
  TestTrue(TEXT("Removal preview includes selected root"),RemovePreview.Contains(TEXT("widgets (1): Extra8")));
  TestNotNull(TEXT("Removal preview preserves widget"),WidgetTarget->WidgetTree->FindWidget(TEXT("Extra8")));
  TestTrue(TEXT("Unreferenced removal succeeds"),Record(TEXT("Widget removal edit"),UTurboMCPEditToolset::EditTurboMCPWidgetRemove(WidgetTarget->GetPathName(),TEXT("Extra8"))).StartsWith(TEXT("edited:")));
  TestNull(TEXT("Removed widget absent"),WidgetTarget->WidgetTree->FindWidget(TEXT("Extra8")));
  TestTrue(TEXT("Sibling preserved"),WidgetTarget->WidgetTree->FindWidget(TEXT("RenamedPurchaseLabel"))==CopiedLabel);
  TestTrue(TEXT("Missing removal refuses"),UTurboMCPEditToolset::EditTurboMCPWidgetRemove(WidgetTarget->GetPathName(),TEXT("Extra8")).StartsWith(TEXT("error:")));
  UCanvasPanel* DisposablePanel=WidgetTarget->WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(),TEXT("DisposablePanel"));
  CopiedPanel->AddChildToCanvas(DisposablePanel);
  UTextBlock* DisposableChild=WidgetTarget->WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),TEXT("DisposableChild"));
  DisposablePanel->AddChildToCanvas(DisposableChild);
  const FString SubtreeRemoval=Record(TEXT("Subtree removal preview"),UTurboMCPPreviewToolset::PreviewTurboMCPWidgetRemove(WidgetTarget->GetPathName(),TEXT("DisposablePanel")));
  TestTrue(TEXT("Subtree preview lists root and child"),SubtreeRemoval.Contains(TEXT("widgets (2): DisposableChild, DisposablePanel")));
  TestTrue(TEXT("Subtree removal succeeds"),UTurboMCPEditToolset::EditTurboMCPWidgetRemove(WidgetTarget->GetPathName(),TEXT("DisposablePanel")).StartsWith(TEXT("edited:")));
  TestNull(TEXT("Removed subtree root absent"),WidgetTarget->WidgetTree->FindWidget(TEXT("DisposablePanel")));
  TestNull(TEXT("Removed subtree child absent"),WidgetTarget->WidgetTree->FindWidget(TEXT("DisposableChild")));
  TestTrue(TEXT("Subtree removal preserves sibling"),WidgetTarget->WidgetTree->FindWidget(TEXT("RenamedPurchaseLabel"))==CopiedLabel);
  Record(TEXT("Compile after widget removal"),UTurboMCPEditToolset::EditTurboMCPCompile(WidgetTarget->GetPathName()));
  TestTrue(TEXT("Widget compiles after removal"),WidgetTarget->Status!=BS_Error);

 }
 Record(TEXT("Compile widget"),UTurboMCPEditToolset::EditTurboMCPCompile(WB->GetPathName()));
 const int32 ChildrenBeforeAdd=CopiedPanel->GetChildrenCount();
 const FString AddPreview=Record(TEXT("Widget add preview"),UTurboMCPPreviewToolset::PreviewTurboMCPWidgetAdd(WidgetTarget->GetPathName(),CopiedPanel->GetName(),WB->GetPathName(),TEXT("AddedCard")));
 TestTrue(TEXT("User widget add previews"),AddPreview.StartsWith(TEXT("preview:")));
 TestEqual(TEXT("Widget add preview preserves count"),CopiedPanel->GetChildrenCount(),ChildrenBeforeAdd);
 TestNull(TEXT("Widget add preview creates no object"),WidgetTarget->WidgetTree->FindWidget(TEXT("AddedCard")));
 TestTrue(TEXT("Missing parent refuses"),UTurboMCPEditToolset::EditTurboMCPWidgetAdd(WidgetTarget->GetPathName(),TEXT("MissingParent"),WB->GetPathName(),TEXT("AddedCard")).StartsWith(TEXT("error:")));
 TestTrue(TEXT("Self nesting refuses"),UTurboMCPEditToolset::EditTurboMCPWidgetAdd(WidgetTarget->GetPathName(),CopiedPanel->GetName(),WidgetTarget->GetPathName(),TEXT("RecursiveCard")).StartsWith(TEXT("error:")));
 TestTrue(TEXT("User widget add succeeds"),Record(TEXT("Widget add edit"),UTurboMCPEditToolset::EditTurboMCPWidgetAdd(WidgetTarget->GetPathName(),CopiedPanel->GetName(),WB->GetPathName(),TEXT("AddedCard"))).StartsWith(TEXT("edited:")));
 UWidget* AddedCard=WidgetTarget->WidgetTree->FindWidget(TEXT("AddedCard"));
 TestNotNull(TEXT("Added widget found"),AddedCard);
 if(AddedCard){TestTrue(TEXT("Added widget class correct"),AddedCard->GetClass()==WB->GeneratedClass);TestTrue(TEXT("Added widget parent correct"),AddedCard->GetParent()==CopiedPanel);TestNotNull(TEXT("Added widget slot exists"),AddedCard->Slot.Get());}
 TestEqual(TEXT("Exactly one child added"),CopiedPanel->GetChildrenCount(),ChildrenBeforeAdd+1);
 TestTrue(TEXT("Duplicate widget add refuses"),UTurboMCPEditToolset::EditTurboMCPWidgetAdd(WidgetTarget->GetPathName(),CopiedPanel->GetName(),WB->GetPathName(),TEXT("AddedCard")).StartsWith(TEXT("error:")));
 Record(TEXT("Compile added widget"),UTurboMCPEditToolset::EditTurboMCPCompile(WidgetTarget->GetPathName()));TestTrue(TEXT("Target with added widget compiles"),WidgetTarget->Status!=BS_Error);
 TestTrue(TEXT("Indirect widget cycle refuses"),UTurboMCPEditToolset::EditTurboMCPWidgetAdd(WB->GetPathName(),Subtree->GetName(),WidgetTarget->GetPathName(),TEXT("RecursiveMenu")).StartsWith(TEXT("error:")));
 TestTrue(TEXT("Non-panel parent refuses"),UTurboMCPEditToolset::EditTurboMCPWidgetAdd(WidgetTarget->GetPathName(),TEXT("RenamedPurchaseLabel"),TEXT("/Script/UMG.TextBlock"),TEXT("InvalidChild")).StartsWith(TEXT("error:")));
 TestTrue(TEXT("Native widget add succeeds"),Record(TEXT("Native widget add"),UTurboMCPEditToolset::EditTurboMCPWidgetAdd(WidgetTarget->GetPathName(),CopiedPanel->GetName(),TEXT("/Script/UMG.TextBlock"),TEXT("AddedNativeText"))).StartsWith(TEXT("edited:")));
 TestNotNull(TEXT("Native child has requested class"),Cast<UTextBlock>(WidgetTarget->WidgetTree->FindWidget(TEXT("AddedNativeText"))));
 Record(TEXT("Compile native widget addition"),UTurboMCPEditToolset::EditTurboMCPCompile(WidgetTarget->GetPathName()));TestTrue(TEXT("Native addition compiles"),WidgetTarget->Status!=BS_Error);

 const FString PropertyList=Record(TEXT("Widget property list"),UTurboMCPReadToolset::ReadTurboMCPWidgetProperty(WidgetTarget->GetPathName(),TEXT("AddedNativeText"),TEXT("widget"),TEXT("")));
 TestTrue(TEXT("Property reader exposes visibility"),PropertyList.Contains(TEXT("Visibility")));
 UWidget* PropertyWidget=WidgetTarget->WidgetTree->FindWidget(TEXT("AddedNativeText"));
 const ESlateVisibility OldVisibility=PropertyWidget->GetVisibility();
 TestTrue(TEXT("Property preview accepted"),Record(TEXT("Property preview"),UTurboMCPPreviewToolset::PreviewTurboMCPWidgetProperty(WidgetTarget->GetPathName(),TEXT("AddedNativeText"),TEXT("widget"),TEXT("Visibility"),TEXT("Hidden"))).StartsWith(TEXT("preview:")));
 TestEqual(TEXT("Property preview preserves value"),PropertyWidget->GetVisibility(),OldVisibility);
 TestTrue(TEXT("Visibility write accepted"),Record(TEXT("Property edit"),UTurboMCPEditToolset::EditTurboMCPWidgetProperty(WidgetTarget->GetPathName(),TEXT("AddedNativeText"),TEXT("widget"),TEXT("Visibility"),TEXT("Hidden"))).StartsWith(TEXT("edited:")));
 TestEqual(TEXT("Visibility actually changed"),PropertyWidget->GetVisibility(),ESlateVisibility::Hidden);
 TestTrue(TEXT("Property readback matches"),UTurboMCPReadToolset::ReadTurboMCPWidgetProperty(WidgetTarget->GetPathName(),TEXT("AddedNativeText"),TEXT("widget"),TEXT("Visibility")).Contains(TEXT("Hidden")));
 TestTrue(TEXT("Invalid enum refuses"),UTurboMCPEditToolset::EditTurboMCPWidgetProperty(WidgetTarget->GetPathName(),TEXT("AddedNativeText"),TEXT("widget"),TEXT("Visibility"),TEXT("NotAVisibility")).StartsWith(TEXT("error:")));
 TestEqual(TEXT("Invalid value preserves property"),PropertyWidget->GetVisibility(),ESlateVisibility::Hidden);
 TestTrue(TEXT("Slot layout accepted"),Record(TEXT("Slot layout edit"),UTurboMCPEditToolset::EditTurboMCPWidgetProperty(WidgetTarget->GetPathName(),TEXT("AddedNativeText"),TEXT("slot"),TEXT("LayoutData"),TEXT("(Offsets=(Left=12,Top=20,Right=320,Bottom=80))"))).StartsWith(TEXT("edited:")));
 UCanvasPanelSlot* PropertySlot=Cast<UCanvasPanelSlot>(PropertyWidget->Slot);
 if(TestNotNull(TEXT("Property canvas slot"),PropertySlot)){TestEqual(TEXT("Slot position readback"),PropertySlot->GetPosition(),FVector2D(12,20));TestEqual(TEXT("Slot size readback"),PropertySlot->GetSize(),FVector2D(320,80));}
 TestTrue(TEXT("Repeated property is no-op"),UTurboMCPEditToolset::EditTurboMCPWidgetProperty(WidgetTarget->GetPathName(),TEXT("AddedNativeText"),TEXT("widget"),TEXT("Visibility"),TEXT("Hidden")).StartsWith(TEXT("unchanged:")));
 TestTrue(TEXT("Trailing value text refuses"),UTurboMCPEditToolset::EditTurboMCPWidgetProperty(WidgetTarget->GetPathName(),TEXT("AddedNativeText"),TEXT("widget"),TEXT("Visibility"),TEXT("Hidden garbage")).StartsWith(TEXT("error:")));
 TestTrue(TEXT("Object ownership property refuses"),UTurboMCPEditToolset::EditTurboMCPWidgetProperty(WidgetTarget->GetPathName(),TEXT("AddedNativeText"),TEXT("widget"),TEXT("Slot"),TEXT("None")).StartsWith(TEXT("error:")));
 TestTrue(TEXT("Text padding edit accepted"),Record(TEXT("Padding edit"),UTurboMCPEditToolset::EditTurboMCPWidgetProperty(WidgetTarget->GetPathName(),TEXT("AddedNativeText"),TEXT("widget"),TEXT("Margin"),TEXT("(Left=4,Top=8,Right=12,Bottom=16)"))).StartsWith(TEXT("edited:")));
 FStructProperty* MarginProperty=FindFProperty<FStructProperty>(PropertyWidget->GetClass(),TEXT("Margin"));
 if(TestNotNull(TEXT("Margin property exists"),MarginProperty))TestTrue(TEXT("Text padding readback"),*MarginProperty->ContainerPtrToValuePtr<FMargin>(PropertyWidget)==FMargin(4,8,12,16));
 TestTrue(TEXT("SizeBox fixture added"),UTurboMCPEditToolset::EditTurboMCPWidgetAdd(WidgetTarget->GetPathName(),CopiedPanel->GetName(),TEXT("/Script/UMG.SizeBox"),TEXT("PropertySizeBox")).StartsWith(TEXT("edited:")));
 TestTrue(TEXT("Width edit accepted"),Record(TEXT("Width override"),UTurboMCPEditToolset::EditTurboMCPWidgetProperty(WidgetTarget->GetPathName(),TEXT("PropertySizeBox"),TEXT("widget"),TEXT("WidthOverride"),TEXT("240"))).StartsWith(TEXT("edited:")));
 TestTrue(TEXT("Width flag accepted"),UTurboMCPEditToolset::EditTurboMCPWidgetProperty(WidgetTarget->GetPathName(),TEXT("PropertySizeBox"),TEXT("widget"),TEXT("bOverride_WidthOverride"),TEXT("True")).StartsWith(TEXT("edited:")));
 USizeBox* PropertySizeBox=Cast<USizeBox>(WidgetTarget->WidgetTree->FindWidget(TEXT("PropertySizeBox")));
 if(TestNotNull(TEXT("SizeBox exists"),PropertySizeBox))TestEqual(TEXT("Width readback"),PropertySizeBox->GetWidthOverride(),240.f);
 Record(TEXT("Compile property edits"),UTurboMCPEditToolset::EditTurboMCPCompile(WidgetTarget->GetPathName()));TestTrue(TEXT("Property edits compile"),WidgetTarget->Status!=BS_Error);
 for(const FString Size:{FString(TEXT("640x240")),FString(TEXT("1920x720"))})ImageCheck(Record(TEXT("Widget preview ")+Size,UTurboMCPLookToolset::LookTurboMCPWidgetPreview(WB->GetPathName(),TEXT(""),TEXT(""),Size,TEXT("allow"))));
 UMaterial* Material=NewObject<UMaterial>(Package,TEXT("M_Vision"),RF_Public|RF_Standalone);
 auto* Color=NewObject<UMaterialExpressionConstant3Vector>(Material);Color->Constant=FLinearColor(0.1f,0.6f,0.2f);Material->GetExpressionCollection().AddExpression(Color);Material->GetEditorOnlyData()->BaseColor.Expression=Color;Color->MaterialExpressionEditorX=-900; Color->MaterialExpressionEditorY=400; Material->EditorX=300; Material->EditorY=0; Material->PostEditChange();
 const FString MaterialFit=Record(TEXT("Material editor fit"),UTurboMCPLookToolset::LookTurboMCPCapture(Material->GetPathName(),TEXT(""),TEXT("asset-editor"),TEXT("fit"),TEXT("")));
 ImageCheck(MaterialFit);
 Record(TEXT("Material fixture path"),Material->GetPathName());
 TestTrue(TEXT("Material fit reports its scope"),MaterialFit.Contains(TEXT("material framing: fit all measured graph nodes")));
 TestTrue(TEXT("Material preview does not claim shader readiness"),MaterialFit.Contains(TEXT("material shaders:")) && MaterialFit.Contains(TEXT("does not prove the displayed frame")));
 TestEqual(TEXT("Material expression position preserved"),Color->MaterialExpressionEditorX,-900);
 TestEqual(TEXT("Material output position preserved"),Material->EditorX,300);
 TestTrue(TEXT("Material connection preserved"),Material->GetEditorOnlyData()->BaseColor.Expression==Color);
 TestTrue(TEXT("Unsupported fit refuses"),UTurboMCPLookToolset::LookTurboMCPCapture(WB->GetPathName(),TEXT(""),TEXT("asset-editor"),TEXT("fit"),TEXT("" )).StartsWith(TEXT("error:")));
 TestTrue(TEXT("Invalid framing refuses"),UTurboMCPLookToolset::LookTurboMCPCapture(Material->GetPathName(),TEXT(""),TEXT("asset-editor"),TEXT("unknown"),TEXT("" )).StartsWith(TEXT("error:")));
 ImageCheck(Record(TEXT("Widget designer editor"),UTurboMCPLookToolset::LookTurboMCPCapture(WB->GetPathName(),TEXT(""),TEXT("asset-editor"),TEXT(""),TEXT(""))));
 return !HasAnyErrors();
}
#endif
