// Copyright FloStyle. All Rights Reserved.

#include "Tools/TurboMCPToolsets.h"

#include "Blueprint/UserWidget.h"
#include "BaseWidgetBlueprint.h"
#include "Components/Widget.h"
#include "Containers/UnrealString.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Engine/TextureRenderTarget2D.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Object.h"
#include "UObject/UnrealType.h"
#include "UObject/UObjectGlobals.h"
#include "Widgets/SWidget.h"
#include "Widgets/SNullWidget.h"
#include "Slate/WidgetRenderer.h"

#include "Logging.h"
#include "TurboMCPAssetResolver.h"
#include "TurboMCPLogCapture.h"

/**
 * L-WIDGET-PREVIEW: render a widget with sample values.
 *
 * `03_TOOLS.md` asks for a temporary instance made off screen, the values set, an
 * optional function called, the result rendered to a PNG, and the instance
 * destroyed, with **all script errors and warnings** reported, and no play session.
 *
 * This is a Look tool, so it changes nothing that outlives the call (`P3`). The
 * instance it makes is transient, is not saved, and is destroyed before the tool
 * returns. The one thing it does write is the PNG, which is the point of the tool.
 *
 * THE INSTANCE IS CREATED THROUGH THE BLUEPRINT'S GENERATED CLASS, so `Construct`
 * runs and the widget is a real instance rather than a designer tree walk. That is
 * what makes setting a variable and calling a function meaningful: a value set on a
 * designer tree would not run any of the widget's own logic.
 */
namespace
{
	/** The default render size when the caller does not give one. */
	constexpr int32 DefaultWidth = 1280;
	constexpr int32 DefaultHeight = 720;

	/** The largest edge that is rendered, because a huge size would exhaust memory. */
	constexpr int32 MaxEdge = 4096;

	/** How many script messages are reported per severity. */
	constexpr int32 MaxMessages = 12;
}

FString UTurboMCPLookToolset::LookTurboMCPWidgetPreview(
	const FString& Widget,
	const FString& SampleValuesJson,
	const FString& FunctionName,
	const FString& Size)
{
	if (Widget.IsEmpty())
	{
		return TEXT("error: no widget given. Pass a Widget Blueprint in any path ")
			TEXT("form, for example WBP_NodeTest.");
	}

	// Every input is validated before anything is created, so a typo costs nothing
	// and leaves no half-built instance behind.
	int32 Width = DefaultWidth;
	int32 Height = DefaultHeight;
	if (!Size.TrimStartAndEnd().IsEmpty())
	{
		FString Left;
		FString Right;
		if (!Size.Split(TEXT("x"), &Left, &Right)
			&& !Size.Split(TEXT(","), &Left, &Right))
		{
			return FString::Printf(
				TEXT("error: '%s' is not a size. Pass WxH, for example 1280x720."), *Size);
		}

		Left.TrimStartAndEndInline();
		Right.TrimStartAndEndInline();
		if (!Left.IsNumeric() || !Right.IsNumeric())
		{
			return FString::Printf(
				TEXT("error: '%s' is not a size. Both parts must be whole numbers, ")
				TEXT("for example 1280x720."), *Size);
		}

		Width = FCString::Atoi(*Left);
		Height = FCString::Atoi(*Right);

		if (Width <= 0 || Height <= 0)
		{
			return FString::Printf(
				TEXT("error: a size of %dx%d cannot be rendered. Both parts must be ")
				TEXT("positive."), Width, Height);
		}
		if (Width > MaxEdge || Height > MaxEdge)
		{
			return FString::Printf(
				TEXT("error: %dx%d is larger than the %d limit. Pass a smaller size."),
				Width, Height, MaxEdge);
		}
	}

	// The sample values, parsed before the instance exists.
	TSharedPtr<FJsonObject> SampleValues;
	if (!SampleValuesJson.TrimStartAndEnd().IsEmpty())
	{
		const TSharedRef<TJsonReader<>> Reader =
			TJsonReaderFactory<>::Create(SampleValuesJson);
		if (!FJsonSerializer::Deserialize(Reader, SampleValues) || !SampleValues.IsValid())
		{
			return TEXT("error: the sample values are not a JSON object. Pass them as ")
				TEXT("{\"VariableName\": value, ...}, or leave the field empty. A JSON ")
				TEXT("array or a bare value is refused because a variable has to be ")
				TEXT("named.");
		}
	}

	FTurboMCPAssetResolver::FResolved Resolved;
	const FString ResolveError = FTurboMCPAssetResolver::Resolve(Widget, Resolved);
	if (!ResolveError.IsEmpty())
	{
		return ResolveError;
	}

	UObject* Loaded = LoadObject<UObject>(nullptr, *Resolved.ObjectPath);
	UBaseWidgetBlueprint* WidgetBlueprint = Cast<UBaseWidgetBlueprint>(Loaded);
	if (WidgetBlueprint == nullptr)
	{
		return FString::Printf(
			TEXT("error: %s is a %s, and only a Widget Blueprint can be previewed."),
			*Resolved.ObjectPath,
			Loaded != nullptr ? *Loaded->GetClass()->GetName() : TEXT("missing object"));
	}

	// The generated class is what carries Construct and the blueprint's own logic, so
	// a preview needs it. A widget that has never been compiled has none, and saying
	// so is better than rendering an instance that would not run its own code.
	UClass* WidgetClass = WidgetBlueprint->GeneratedClass;
	if (WidgetClass == nullptr || !WidgetClass->IsChildOf(UUserWidget::StaticClass()))
	{
		return FString::Printf(
			TEXT("error: %s has no generated class, so it cannot be instantiated. ")
			TEXT("Compile the Widget Blueprint first."), *Resolved.ObjectPath);
	}

	// The instance is transient and named so it is recognisable in the log if it ever
	// leaks. Anything captured during the whole call is reported.
	FTurboMCPLogCapture Capture;

	UUserWidget* Instance = NewObject<UUserWidget>(
		GetTransientPackage(), WidgetClass, NAME_None, RF_Transient);
	if (Instance == nullptr)
	{
		return FString::Printf(
			TEXT("error: could not instantiate %s."), *Resolved.ObjectPath);
	}

	FString Out = FString::Printf(TEXT("look: preview of %s"), *Resolved.ObjectPath);
	Out += FString::Printf(TEXT("\nsize: %dx%d"), Width, Height);

	int32 ValuesSet = 0;
	int32 ValuesRefused = 0;
	FString ValueErrors;

	// Sample values, set by name through reflection, so a wrong name is reported
	// rather than silently ignored.
	if (SampleValues.IsValid() && SampleValues->Values.Num() > 0)
	{
		for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : SampleValues->Values)
		{
			const FName PropertyName(*Pair.Key);
			FProperty* Property = WidgetClass->FindPropertyByName(PropertyName);

			if (Property == nullptr)
			{
				// Naming what does exist costs one line and saves a second call.
				TArray<FString> Candidates;
				for (TFieldIterator<FProperty> It(WidgetClass); It && Candidates.Num() < 8; ++It)
				{
					if (It->HasAnyPropertyFlags(CPF_Edit | CPF_BlueprintVisible))
					{
						Candidates.Add(It->GetName());
					}
				}

				ValueErrors += FString::Printf(
					TEXT("\n  '%s' is not a property of this widget. Some it does have: %s"),
					*Pair.Key,
					Candidates.Num() > 0 ? *FString::Join(Candidates, TEXT(", "))
						: TEXT("(none that are editable)"));
				++ValuesRefused;
				continue;
			}

			void* ValuePtr = Property->ContainerPtrToValuePtr<void>(Instance);
			const FString ValueText = Pair.Value.IsValid()
				? Pair.Value->AsString() : FString();

			// ImportText is what the details panel uses, so the accepted spellings are
			// the engine's own and not a format invented here.
			const TCHAR* Imported = Property->ImportText_Direct(
				*ValueText, ValuePtr, Instance, PPF_None);

			if (Imported == nullptr)
			{
				ValueErrors += FString::Printf(
					TEXT("\n  could not set '%s' from '%s' (%s)."),
					*Pair.Key, *ValueText, *Property->GetCPPType());
				++ValuesRefused;
			}
			else
			{
				++ValuesSet;
			}
		}
	}

	Out += FString::Printf(TEXT("\nsample values: %d set, %d refused"),
		ValuesSet, ValuesRefused);

	// The optional function, called on the instance by name.
	FString FunctionReport = TEXT("none asked for");
	if (!FunctionName.TrimStartAndEnd().IsEmpty())
	{
		UFunction* Function = WidgetClass->FindFunctionByName(FName(*FunctionName));
		if (Function == nullptr)
		{
			FunctionReport = FString::Printf(
				TEXT("'%s' is not a function on this widget, so nothing was called"),
				*FunctionName);
		}
		else if (Function->NumParms != 0)
		{
			// Calling a function with parameters would need values this tool has no
			// way to supply, so it is refused rather than called with garbage.
			FunctionReport = FString::Printf(
				TEXT("'%s' takes %d parameter(s), and this tool can only call a ")
				TEXT("function that takes none"),
				*FunctionName, Function->NumParms);
		}
		else
		{
			Instance->ProcessEvent(Function, nullptr);
			FunctionReport = FString::Printf(TEXT("called '%s'"), *FunctionName);
		}
	}
	Out += FString::Printf(TEXT("\nfunction: %s"), *FunctionReport);

	// Rendering. The widget is taken as Slate, drawn into a render target, and read
	// back, which needs no play session and no viewport.
	FString ImagePath;
	FString RenderError;

	TSharedRef<SWidget> SlateWidget = Instance->TakeWidget();

	UTextureRenderTarget2D* RenderTarget = FWidgetRenderer::CreateTargetFor(
		FVector2D(Width, Height), TF_Bilinear, /*bUseGammaCorrection=*/false);

	if (RenderTarget == nullptr)
	{
		RenderError = TEXT("could not create a render target");
	}
	else
	{
		FWidgetRenderer* Renderer = new FWidgetRenderer(/*bUseGammaCorrection=*/false);
		Renderer->DrawWidget(RenderTarget, SlateWidget, FVector2D(Width, Height),
			/*InScale=*/1.0f, /*bDeferRenderTargetUpdate=*/false);

		// The pixels, read back from the GPU.
		TArray<FColor> Pixels;
		FReadSurfaceDataFlags ReadFlags(RCM_UNorm, CubeFace_MAX);
		ReadFlags.SetLinearToGamma(false);

		if (!RenderTarget->GameThread_GetRenderTargetResource()->ReadPixels(
				Pixels, ReadFlags) || Pixels.Num() != Width * Height)
		{
			RenderError = FString::Printf(
				TEXT("the render target read back %d pixel(s) for a %dx%d image"),
				Pixels.Num(), Width, Height);
		}
		else
		{
			// A widget's background is transparent by default, and a transparent PNG
			// looks empty in most viewers, so the alpha is forced opaque and the
			// result says so.
			for (FColor& Pixel : Pixels)
			{
				Pixel.A = 255;
			}

			// PNGCompressImageArray takes the FColor array directly, which is exactly
			// what was read back. FImage was tried first and its constructor is not
			// exported from the module, so it links as an unresolved symbol.
			TArray64<uint8> PngData;
			FImageUtils::PNGCompressImageArray(Width, Height, Pixels, PngData);

			if (PngData.Num() == 0)
			{
				RenderError = TEXT("could not compress the image to PNG");
			}
			else
			{
				// A fixed folder under the project's Saved folder, per the spec.
				const FString Folder = FPaths::Combine(
					FPaths::ProjectSavedDir(), TEXT("TurboMCP"), TEXT("Captures"));
				IFileManager::Get().MakeDirectory(*Folder, /*Tree=*/true);

				const FString FileName = FString::Printf(TEXT("%s_%dx%d.png"),
					*WidgetBlueprint->GetName(), Width, Height);
				ImagePath = FPaths::Combine(Folder, FileName);

				if (!FFileHelper::SaveArrayToFile(PngData, *ImagePath))
				{
					RenderError = FString::Printf(TEXT("could not write %s"), *ImagePath);
					ImagePath.Empty();
				}
			}
		}

		// The renderer is heap allocated because it is meant to be reused; this call
		// makes one and is done, so it is released here.
		delete Renderer;
	}

	if (!RenderError.IsEmpty())
	{
		// A failed render is an error naming why, and the instance is still destroyed
		// below, so nothing is left behind.
		Out += FString::Printf(TEXT("\nerror: %s"), *RenderError);
	}
	else
	{
		Out += FString::Printf(TEXT("\nimage: %s"), *ImagePath);
		Out += TEXT("\n; the image is written to the project's Saved/TurboMCP/Captures ");
		Out += TEXT("folder. Its alpha channel is forced opaque, because a widget's ");
		Out += TEXT("background is transparent by default and a transparent PNG looks ");
		Out += TEXT("empty in most viewers.");
	}

	// The instance is destroyed before returning, which is what the spec asks and
	// what keeps this a Look tool.
	Instance->RemoveFromRoot();
	Instance->MarkAsGarbage();

	// Everything the log captured during the call, which is what the spec means by
	// reporting all script errors and warnings.
	const int32 Warnings = Capture.WarningCount();
	const int32 Errors = Capture.ErrorCount();

	if (Warnings == 0 && Errors == 0 && ValueErrors.IsEmpty())
	{
		Out += TEXT("\nscript messages: none. No error and no warning was raised ");
		Out += TEXT("while setting the values or rendering.");
	}
	else
	{
		Out += FString::Printf(TEXT("\nscript messages: %d warning(s), %d error(s)"),
			Warnings, Errors);
		if (!ValueErrors.IsEmpty())
		{
			Out += TEXT("\nvalue problems:");
			Out += ValueErrors;
		}
		if (Warnings > 0)
		{
			// The warnings themselves, capped, because a widget that logs on Construct
			// is exactly what a caller needs to see here.
			Out += TEXT("\nwarnings raised:");
			Out += Capture.SummariseSince(/*InMark=*/0, MaxMessages);
		}
	}

	return Out;
}
