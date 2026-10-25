// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
#include "Tools/TurboMCPToolsets.h"
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "BlueprintEditorLibrary.h"
#include "Internationalization/Regex.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphUtilities.h"
#include "EdGraphNode_Comment.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "GameFramework/Actor.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_ExecutionSequence.h"
#include "K2Node_IfThenElse.h"
#include "K2Node_AddDelegate.h"
#include "K2Node_CallFunction.h"
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

// Fixture authoring is setup, not evidence that graph editing tools created it.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTurboMCPVisionWorkflowTest,"TurboMCP.Workflow.Vision",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTurboMCPVisionWorkflowTest::RunTest(const FString& Parameters)
{
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
 auto* Label=WB->WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),TEXT("StatusLabel"));Label->SetText(FText::FromString(TEXT("Vision fixture: READY 0123456789")));WB->WidgetVariableNameToGuidMap.Add(Label->GetFName(),FGuid::NewGuid());
 auto* Slot=CastChecked<UCanvasPanel>(WB->WidgetTree->RootWidget)->AddChildToCanvas(Label);Slot->SetPosition(FVector2D(40,40));Slot->SetSize(FVector2D(560,80));
 Record(TEXT("Compile widget"),UTurboMCPEditToolset::EditTurboMCPCompile(WB->GetPathName()));
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
