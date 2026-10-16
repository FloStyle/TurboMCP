// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "TurboMCPMaterialFixtureCommandlet.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Containers/UnrealString.h"
#include "HAL/PlatformFileManager.h"
#include "MaterialEditingLibrary.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionAdd.h"
#include "Materials/MaterialExpressionComment.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "Materials/MaterialExpressionFunctionOutput.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionNamedReroute.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionTextureSampleParameter2D.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialFunction.h"
#include "Engine/Texture.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "TurboMCPModule.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "UObject/UObjectGlobals.h"

#include "Logging.h"

#define LOCTEXT_NAMESPACE "TurboMCPMaterialFixture"

namespace
{
	/**
	 * THE FIXTURE IS THREE ASSETS IN ONE PACKAGE, and they are named rather than numbered so the
	 * reader sees what each is for.
	 */
	const FString PackagePath = TEXT("/Game/Tests/TurboMCPMaterialFixture");
	const FString MaterialName = TEXT("M_TurboMCPFixture");
	const FString InstanceName = TEXT("MI_TurboMCPFixture");
	const FString FunctionName = TEXT("MF_TurboMCPFixture");

	/** How many expressions were built, so the report says whether the structure arrived. */
	struct FCounts
	{
		int32 Expressions = 0;
		int32 Comments = 0;
		int32 Labels = 0;
		int32 LabelUsages = 0;
		int32 Connections = 0;
	};

	/**
	 * Adds one expression at a real position.
	 *
	 * THE POSITION IS NOT COSMETIC. `R-MAT-BLOCKS` decides box membership by comparing an
	 * expression's `MaterialExpressionEditorX/Y` against each box rectangle, so an expression placed
	 * at the wrong coordinate lands in the wrong box - or in two - and the nesting check would pass or
	 * fail for a reason that has nothing to do with the reader.
	 */
	template<typename TExpressionClass>
	TExpressionClass* Add(UMaterial* InMaterial, UMaterialFunction* InFunction, int32 InX, int32 InY)
	{
		// `CreateMaterialExpressionEx` IS THE ONE ENTRY POINT FOR BOTH OWNERS. Read from
		// `MaterialEditingLibrary.h`: it takes the material and the function, and the one that is not
		// null becomes the new expression's Outer. The two separate helpers the header also offers
		// (`CreateMaterialExpression` and `CreateMaterialExpressionInFunction`) exist, but using one
		// call means the function branch cannot drift from the material branch.
		UMaterialExpression* Created = UMaterialEditingLibrary::CreateMaterialExpressionEx(
			InMaterial, InFunction, TExpressionClass::StaticClass(), /*SelectedAsset=*/nullptr,
			InX, InY);
		return Cast<TExpressionClass>(Created);
	}

	/** Connects two expressions and counts the connection, so the report is evidence. */
	bool Wire(FCounts& InOutCounts, UMaterialExpression* From, const TCHAR* FromOutput,
		UMaterialExpression* To, const TCHAR* ToInput)
	{
		if (From == nullptr || To == nullptr)
		{
			// REPORTED RATHER THAN SWALLOWED. A null end means the expression was never created, and
			// a silently skipped wire would leave a reader printing "unset" for a link the fixture
			// intended - which is exactly the confusion this fixture exists to remove.
			UE_LOG(LogTurboMCP, Error,
				TEXT("wire %s.%s -> %s.%s SKIPPED: a null expression"),
				From != nullptr ? *From->GetName() : TEXT("(null)"), FromOutput,
				To != nullptr ? *To->GetName() : TEXT("(null)"), ToInput);
			return false;
		}
		const bool bOk = UMaterialEditingLibrary::ConnectMaterialExpressions(
			From, FromOutput, To, ToInput);
		if (bOk)
		{
			++InOutCounts.Connections;
		}
		else
		{
			// A FAILED WIRE IS LOUD. `ConnectMaterialExpressions` answers false when either the named
			// input does not exist on the target or the named output does not exist on the source, and
			// silently continuing would produce a fixture that looks built and is missing its links.
			UE_LOG(LogTurboMCP, Error,
				TEXT("wire %s.%s -> %s(%s).%s FAILED: the input or the output name was not found"),
				*From->GetName(), FromOutput, *To->GetName(), *To->GetClass()->GetName(), ToInput);
		}
		return bOk;
	}

	/**
	 * Builds the material: the four readers' code paths, each one deliberate.
	 *
	 * THE LAYOUT, and why each node is where it is. Coordinates are in the editor's own units and the
	 * boxes below are drawn around them:
	 *
	 *     (100,100) ................. OUTER BOX .................. (1100,800)
	 *          (120,120) .. INNER BOX .. (380,350)
	 *              150,150 ScalarParameter "LazyScalar"   <- reaches NO output
	 *              150,300 Constant3Vector "InnerConstant" <- the UNLINKED input's source
	 *              400,200 Multiply "InnerProduct"          <- in the INNER box
	 *              600,200 NamedRerouteDeclaration "VerticalMix"  <- read TWICE
	 *              800,450 NamedRerouteUsage "VerticalMix"  <- reader 1
	 *              800,600 NamedRerouteUsage "VerticalMix"  <- reader 2
	 *              600,700 NamedRerouteDeclaration "UnusedTint"   <- read ZERO times
	 *              900,700 Constant3Vector "UnusedTint"     <- feeds the unread declaration
	 *              1300,300 VectorParameter "OuterTint"     <- OUTSIDE both boxes
	 *              1500,300 Multiply "FinalMix"             <- OUTSIDE both boxes, feeds BaseColor
	 *
	 * `InnerProduct` is read by TWO consumers and has no label, which is the shared-value path.
	 * `FinalMix`'s B input is left UNLINKED so it keeps a constant, which is the reflection path.
	 */
	bool BuildMaterial(UMaterial* InMaterial, FCounts& OutCounts)
	{
		if (InMaterial == nullptr)
		{
			return false;
		}

		// --- THE UNREACHED PARAMETER -------------------------------------------------------------
		// Nothing reads it, nothing links from it, and it feeds no output. It exists so
		// `R-MAT-LOAD`'s unreached-parameter finding has a parameter to find. Its position is inside
		// the INNER box, which also makes it the inner box's proof of membership.
		UMaterialExpressionScalarParameter* LazyScalar =
			Add<UMaterialExpressionScalarParameter>(InMaterial, nullptr, 150, 150);
		if (LazyScalar != nullptr)
		{
			LazyScalar->ParameterName = TEXT("LazyScalar");
			LazyScalar->Group = TEXT("Fixture");
			LazyScalar->DefaultValue = 0.75f;
			++OutCounts.Expressions;
		}

		// --- THE INNER CONSTANT ------------------------------------------------------------------
		// This feeds `InnerProduct`'s B, and `InnerProduct` is read twice, so this value is shared
		// and must be DEFINED ONCE and referenced thereafter.
		UMaterialExpressionConstant3Vector* InnerConstant =
			Add<UMaterialExpressionConstant3Vector>(InMaterial, nullptr, 150, 300);
		if (InnerConstant != nullptr)
		{
			InnerConstant->Constant = FLinearColor(0.2f, 0.4f, 0.6f, 1.0f);
			++OutCounts.Expressions;
		}

		// --- THE SHARED VALUE --------------------------------------------------------------------
		// Read by TWO consumers below and by nothing else. It has no label, so the renderer must
		// define it as `$<key>` at its first use and print `$<key>` at the second.
		UMaterialExpressionMultiply* InnerProduct =
			Add<UMaterialExpressionMultiply>(InMaterial, nullptr, 400, 200);
		if (InnerProduct != nullptr)
		{
			++OutCounts.Expressions;
			Wire(OutCounts, InnerConstant, TEXT(""), InnerProduct, TEXT("A"));
		}

		// --- THE LABEL READ TWICE ----------------------------------------------------------------
		// `VerticalMix` is declared here and read by the two usages below, which is the label path.
		UMaterialExpressionNamedRerouteDeclaration* VerticalMix =
			Add<UMaterialExpressionNamedRerouteDeclaration>(InMaterial, nullptr, 600, 200);
		if (VerticalMix != nullptr)
		{
			VerticalMix->Name = TEXT("VerticalMix");
			++OutCounts.Expressions;
			++OutCounts.Labels;
			Wire(OutCounts, InnerProduct, TEXT(""), VerticalMix, TEXT(""));
		}

		UMaterialExpressionNamedRerouteUsage* Use1 = Add<UMaterialExpressionNamedRerouteUsage>(
			InMaterial, nullptr, 800, 450);
		if (Use1 != nullptr && VerticalMix != nullptr)
		{
			// BOTH THE POINTER AND THE GUID ARE SET. The reader prefers the pointer, because it is
			// what the editor maintains on a rename, and falls back to the GUID; a fixture that set
			// only the pointer would leave the fallback untested and would break on a reload that
			// has not run its fixup.
			Use1->Declaration = VerticalMix;
			Use1->DeclarationGuid = VerticalMix->VariableGuid;
			++OutCounts.Expressions;
			++OutCounts.LabelUsages;
		}

		UMaterialExpressionNamedRerouteUsage* Use2 = Add<UMaterialExpressionNamedRerouteUsage>(
			InMaterial, nullptr, 800, 600);
		if (Use2 != nullptr && VerticalMix != nullptr)
		{
			Use2->Declaration = VerticalMix;
			Use2->DeclarationGuid = VerticalMix->VariableGuid;
			++OutCounts.Expressions;
			++OutCounts.LabelUsages;
		}

		// --- THE LABEL READ ZERO TIMES -----------------------------------------------------------
		// THE UNREAD FINDING `R-MAT-LABELS` EXISTS TO PRODUCE. It is deliberately fed by a real
		// value, so it is a label somebody meant to use rather than an empty node.
		UMaterialExpressionConstant3Vector* UnusedSource =
			Add<UMaterialExpressionConstant3Vector>(InMaterial, nullptr, 900, 700);
		if (UnusedSource != nullptr)
		{
			UnusedSource->Constant = FLinearColor(1.0f, 0.2f, 0.2f, 1.0f);
			++OutCounts.Expressions;
		}

		UMaterialExpressionNamedRerouteDeclaration* UnusedTint =
			Add<UMaterialExpressionNamedRerouteDeclaration>(InMaterial, nullptr, 600, 700);
		if (UnusedTint != nullptr)
		{
			UnusedTint->Name = TEXT("UnusedTint");
			++OutCounts.Expressions;
			++OutCounts.Labels;
			Wire(OutCounts, UnusedSource, TEXT(""), UnusedTint, TEXT(""));
		}

		// --- TEXTURE PARAMETERS: ONE SRGB AND ONE LINEAR SAMPLER ----------------------------------
		// The reader must surface both the sampler type and the actual default texture handle. The
		// linear parameter deliberately uses DefaultNormal (sRGB disabled); the focused edit test
		// attempts to set DefaultTexture (sRGB enabled) by name and must refuse it.
		UTexture* SRGBDefault = LoadObject<UTexture>(nullptr,
			TEXT("/Engine/EngineResources/DefaultTexture.DefaultTexture"));
		UTexture* LinearDefault = LoadObject<UTexture>(nullptr,
			TEXT("/Engine/EngineMaterials/DefaultNormal.DefaultNormal"));
		UMaterialExpressionTextureSampleParameter2D* FixtureSRGBTexture =
			Add<UMaterialExpressionTextureSampleParameter2D>(InMaterial, nullptr, 1200, 450);
		if (FixtureSRGBTexture != nullptr)
		{
			FixtureSRGBTexture->ParameterName = TEXT("FixtureSRGBTexture");
			FixtureSRGBTexture->Group = TEXT("Fixture");
			FixtureSRGBTexture->Texture = SRGBDefault;
			FixtureSRGBTexture->SamplerType = SAMPLERTYPE_Color;
			++OutCounts.Expressions;
		}
		UMaterialExpressionTextureSampleParameter2D* FixtureLinearTexture =
			Add<UMaterialExpressionTextureSampleParameter2D>(InMaterial, nullptr, 1200, 600);
		if (FixtureLinearTexture != nullptr)
		{
			FixtureLinearTexture->ParameterName = TEXT("FixtureLinearTexture");
			FixtureLinearTexture->Group = TEXT("Fixture");
			FixtureLinearTexture->Texture = LinearDefault;
			FixtureLinearTexture->SamplerType = SAMPLERTYPE_LinearColor;
			++OutCounts.Expressions;
		}

		// --- THE FINAL MIX, OUTSIDE BOTH BOXES ---------------------------------------------------
		// Reading a labelled value from OUTSIDE the box is the "it arrives as a label" case, so a
		// box that reads `@VerticalMix` is clean rather than a cross-box wire.
		UMaterialExpressionVectorParameter* OuterTint =
			Add<UMaterialExpressionVectorParameter>(InMaterial, nullptr, 1300, 300);
		if (OuterTint != nullptr)
		{
			OuterTint->ParameterName = TEXT("OuterTint");
			OuterTint->Group = TEXT("Fixture");
			OuterTint->DefaultValue = FLinearColor(0.9f, 0.9f, 0.9f, 1.0f);
			++OutCounts.Expressions;
		}

		UMaterialExpressionMultiply* FinalMix =
			Add<UMaterialExpressionMultiply>(InMaterial, nullptr, 1500, 300);
		if (FinalMix != nullptr)
		{
			++OutCounts.Expressions;
			Wire(OutCounts, OuterTint, TEXT(""), FinalMix, TEXT("A"));
			// B CARRIES THE LABEL INTO THE OUTPUT, which is what makes `@VerticalMix` a REACHED
			// label. Without this the declaration and everything upstream of it reach no output, and
			// the unreached report - which exists to name a PARAMETER nothing uses - would instead be
			// dominated by the fixture's own orphaned structure. A finding that fires on everything is
			// worth no more than one that never fires.
			Wire(OutCounts, Use1, TEXT(""), FinalMix, TEXT("B"));
		}

		// --- THE UNLINKED INPUT THAT KEEPS A CONSTANT --------------------------------------------
		// `Add`'s `ConstB` is a real `float` the expression keeps for its B input when nothing is
		// linked. This is the POSITIVE case for the reflection path: with B left unlinked, the reader
		// must print the constant it finds by reflection.
		UMaterialExpressionAdd* ConstantHolder =
			Add<UMaterialExpressionAdd>(InMaterial, nullptr, 1500, 600);
		if (ConstantHolder != nullptr)
		{
			++OutCounts.Expressions;
			// A IS LINKED and B IS NOT, so exactly one input carries a reflected constant. Linking A
			// keeps this expression REACHED, so it does not pollute the unreached finding.
			Wire(OutCounts, FinalMix, TEXT(""), ConstantHolder, TEXT("A"));
			ConstantHolder->ConstB = 0.375f;

			// The Add reaches the BaseColor output through a second multiply so that BaseColor's tree
			// contains BOTH the shared value and the unlinked-constant case.
			UMaterialExpressionMultiply* BaseColorMix =
				Add<UMaterialExpressionMultiply>(InMaterial, nullptr, 1750, 300);
			if (BaseColorMix != nullptr)
			{
				++OutCounts.Expressions;
				Wire(OutCounts, ConstantHolder, TEXT(""), BaseColorMix, TEXT("A"));
				Wire(OutCounts, OuterTint, TEXT(""), BaseColorMix, TEXT("B"));
				UMaterialEditingLibrary::ConnectMaterialProperty(
					BaseColorMix, TEXT(""), MP_BaseColor);
				++OutCounts.Connections;
			}
		}

		// --- THE VALUE READ IN TWO PLACES, WHICH IS WHAT A LABEL IS FOR --------------------------
		//
		// `P/E-MAT-LABEL-PROMOTE` exists to name values that are "used far away or in several boxes",
		// and WITHOUT THIS THE FIXTURE HAS NO SUCH VALUE AT ALL. MEASURED while writing the capability:
		// every expression the fixture built was read by at most ONE distinct consumer, because the one
		// multiply that genuinely fans out is read by its two NamedRerouteUsage nodes - and a LABEL IS
		// NOT A REASON TO ADD A LABEL, so those are excluded by the promotion rule. A capability whose
		// positive case cannot occur on its own fixture cannot be tested, so the fixture gains the case
		// rather than the test gaining an exemption.
		//
		// THE CHANNEL IS DELIBERATELY SEPARATE. It reaches `MP_EmissiveColor` through its own Add, so it
		// touches no existing wire, moves no existing count, and leaves the label story - two
		// declarations, two usages, one of them unread - exactly as the other suites assert it.
		UMaterialExpressionConstant3Vector* SharedTint =
			Add<UMaterialExpressionConstant3Vector>(InMaterial, nullptr, 1200, 900);
		if (SharedTint != nullptr)
		{
			// The value is chosen to be recognisable in a report, so a reader can see which constant was
			// named rather than having to trust a key.
			SharedTint->Constant = FLinearColor(0.25f, 0.5f, 0.75f, 1.0f);
			++OutCounts.Expressions;

			// TWO DISTINCT CONSUMERS, which is the minimum the promotion rule recognises. ONE
			// EXPRESSION READING IT TWICE WOULD NOT DO: the rule counts distinct readers, so a single
			// Multiply with both A and B pointing here is ONE place the value is used.
			UMaterialExpressionMultiply* SharedUseA =
				Add<UMaterialExpressionMultiply>(InMaterial, nullptr, 1400, 900);
			UMaterialExpressionAdd* SharedUseB =
				Add<UMaterialExpressionAdd>(InMaterial, nullptr, 1400, 1050);
			if (SharedUseA != nullptr && SharedUseB != nullptr)
			{
				++OutCounts.Expressions;
				++OutCounts.Expressions;

				// A SHARED CONSTANT CARRIES A REAL NUMBER, not a unit one, so the multiply it feeds is
				// not the identity - a promotion that accidentally DROPPED this value would then show up
				// as a changed picture rather than as two expressions that happen to agree.
				Wire(OutCounts, SharedTint, TEXT(""), SharedUseA, TEXT("A"));
				Wire(OutCounts, SharedTint, TEXT(""), SharedUseB, TEXT("A"));

				// BOTH REACH THE OUTPUT SO THE SHARED VALUE IS REACHED BY THE UNREACHED WALK. A value
				// nothing reads would pollute the unreached finding, which exists to name a PARAMETER
				// the author forgot.
				UMaterialExpressionAdd* SharedOut =
					Add<UMaterialExpressionAdd>(InMaterial, nullptr, 1600, 950);
				if (SharedOut != nullptr)
				{
					++OutCounts.Expressions;
					Wire(OutCounts, SharedUseA, TEXT(""), SharedOut, TEXT("A"));
					Wire(OutCounts, SharedUseB, TEXT(""), SharedOut, TEXT("B"));
					UMaterialEditingLibrary::ConnectMaterialProperty(
						SharedOut, TEXT(""), MP_EmissiveColor);
					++OutCounts.Connections;
				}
			}
		}

		return true;
	}

	/**
	 * Builds the two comment boxes, one geometrically INSIDE the other.
	 *
	 * A COMMENT IS NOT CREATED LIKE AN EXPRESSION, AND USING THE EXPRESSION HELPER SILENTLY PUTS IT IN
	 * THE WRONG ARRAY. `CreateMaterialExpressionEx` WITH `UMaterialExpressionComment::StaticClass()`
	 * makes `R-MAT-BLOCKS` answer
	 * "blocks: 0 comment box(es), 14 expression(s)" against a material that HAD two boxes. The helper
	 * calls `AddExpression`, and a comment belongs in `EditorComments` - a different array, and the
	 * only one `FTurboMCPMaterial::Plan` scans:
	 *
	 *     Material->GetExpressionCollection().EditorComments
	 *
	 * The engine's own editor does not use the helper for a comment either. `FMaterialEditor::
	 * CreateNewMaterialExpressionComment` (`MaterialEditor.cpp:6060`) constructs the object and calls
	 * `Material->GetExpressionCollection().AddComment(NewComment)` (`:6081`). This follows that
	 * exactly. The failure was SILENT - no error, no warning, a plausible-looking count - which is why
	 * the fixture asserts the box count rather than trusting the build.
	 *
	 * THE NESTING IS THE POINT. `R-MAT-BLOCKS` gives every expression to the SMALLEST box that
	 * contains it, so `LazyScalar`, `InnerConstant` and `InnerProduct` must belong to the INNER box
	 * and not the outer one. A fixture where both boxes were the same size, or where the outer did not
	 * contain the inner, would make the nesting check pass without testing nesting.
	 */
	bool BuildBoxes(UMaterial* InMaterial, FCounts& OutCounts)
	{
		if (InMaterial == nullptr)
		{
			return false;
		}

		// A LAMBDA, because both boxes are made the same way and the second copy of a construction is
		// where the two drift apart.
		auto MakeBox = [InMaterial](const TCHAR* InText, int32 InX, int32 InY, int32 InSizeX,
			int32 InSizeY) -> UMaterialExpressionComment*
		{
			UMaterialExpressionComment* Comment = NewObject<UMaterialExpressionComment>(
				InMaterial, NAME_None, RF_Transactional);
			if (Comment == nullptr)
			{
				return nullptr;
			}
			Comment->MaterialExpressionEditorX = InX;
			Comment->MaterialExpressionEditorY = InY;
			Comment->SizeX = InSizeX;
			Comment->SizeY = InSizeY;
			Comment->Text = InText;
			InMaterial->GetExpressionCollection().AddComment(Comment);
			return Comment;
		};

		UMaterialExpressionComment* InnerComment = MakeBox(
			TEXT("INNER: the untouched interior"), 120, 120, 360, 230);
		// THE OUTER BOX IS LARGER IN BOTH DIMENSIONS AND CONTAINS THE INNER ONE, so the area
		// comparison the reader makes is meaningful: 1000x700 against 360x230.
		UMaterialExpressionComment* OuterComment = MakeBox(
			TEXT("OUTER: everything above the final mix"), 100, 100, 1000, 700);

		if (InnerComment != nullptr)
		{
			++OutCounts.Comments;
		}
		if (OuterComment != nullptr)
		{
			++OutCounts.Comments;
		}

		return InnerComment != nullptr && OuterComment != nullptr;
	}

	/**
	 * A material function with a label and a shared value, so `R-MAT-LOAD`'s FUNCTION branch runs.
	 *
	 * A function is its own graph, so the plan uses the function's OWN NAME as the graph part of every
	 * node ID rather than the word `Material`. Building it with expressions is what makes that branch
	 * reachable at all - a function with no expressions would take the branch and print nothing.
	 */
	bool BuildFunction(UMaterialFunction* InFunction, FCounts& OutCounts)
	{
		if (InFunction == nullptr)
		{
			return false;
		}

		UMaterialExpressionConstant3Vector* Base =
			Add<UMaterialExpressionConstant3Vector>(nullptr, InFunction, 0, 0);
		if (Base != nullptr)
		{
			Base->Constant = FLinearColor(0.1f, 0.9f, 0.1f, 1.0f);
			++OutCounts.Expressions;
		}

		UMaterialExpressionNamedRerouteDeclaration* FunctionLabel =
			Add<UMaterialExpressionNamedRerouteDeclaration>(nullptr, InFunction, 300, 0);
		if (FunctionLabel != nullptr)
		{
			FunctionLabel->Name = TEXT("FunctionTint");
			++OutCounts.Expressions;
			++OutCounts.Labels;
			Wire(OutCounts, Base, TEXT(""), FunctionLabel, TEXT(""));
		}

		UMaterialExpressionNamedRerouteUsage* FunctionUse =
			Add<UMaterialExpressionNamedRerouteUsage>(nullptr, InFunction, 600, 0);
		if (FunctionUse != nullptr && FunctionLabel != nullptr)
		{
			FunctionUse->Declaration = FunctionLabel;
			FunctionUse->DeclarationGuid = FunctionLabel->VariableGuid;
			++OutCounts.Expressions;
			++OutCounts.LabelUsages;
		}

		// A FUNCTION OUTPUT, SO THE LABEL IS ACTUALLY REACHED.
		//
		// MEASURED: without one, `R-MAT-LOAD` on this function answered "connected outputs: 0 of 0"
		// and then listed ALL THREE expressions as reaching no output - the constant, the declaration
		// AND the usage. The function branch was still exercised, but every finding it produced was an
		// artefact of the fixture having no output rather than anything about the function. A fixture
		// whose findings are all its own fault teaches nothing.
		//
		// A material function's output is `UMaterialExpressionFunctionOutput`, whose `A` input is the
		// value returned and whose `OutputName` is what a caller of the function sees.
		UMaterialExpressionFunctionOutput* FunctionOutput =
			Add<UMaterialExpressionFunctionOutput>(nullptr, InFunction, 900, 0);
		if (FunctionOutput != nullptr)
		{
			FunctionOutput->OutputName = TEXT("Result");
			++OutCounts.Expressions;

			// THE INPUT NAME IS LEFT EMPTY, AND THAT IS THE ONLY THING THAT WORKS.
			//
			// MEASURED, with the failure made loud rather than swallowed:
			//
			//     wire MaterialExpressionNamedRerouteUsage_0. ->
			//          MaterialExpressionFunctionOutput_0(MaterialExpressionFunctionOutput).A FAILED
			//
			// `UMaterialExpressionFunctionOutput` OVERRIDES `GetInputName` to `return NAME_None;`
			// unconditionally - read from `MaterialExpressionFunctionOutput.h`, where the whole
			// override is those two lines. `ConnectMaterialExpressions` resolves an input by NAME, so
			// NO name can ever match on this class, including the field name `A` that the header
			// declares.
			//
			// The documented way out is the empty name: `GetExpressionInputByName` opens with
			// "Return first input if no name specified" and answers `Expression->GetInput(0)`, which
			// for this node IS the `A` input. So the empty string is not a shortcut here, it is the
			// only addressable route to that pin.
			Wire(OutCounts, FunctionUse, TEXT(""), FunctionOutput, TEXT(""));
		}

		return Base != nullptr && FunctionOutput != nullptr;
	}
}

UTurboMCPMaterialFixtureCommandlet::UTurboMCPMaterialFixtureCommandlet()
{
	IsClient = false;
	IsServer = false;
	IsEditor = true;
	LogToConsole = true;
}

int32 UTurboMCPMaterialFixtureCommandlet::Main(const FString& Params)
{
	UE_LOG(LogTurboMCP, Display, TEXT("TurboMCP: building the material fixture"));

	// THE OLD OBJECTS ARE TAKEN OUT OF THE PACKAGE, AND THE PACKAGE IS MADE AFTERWARDS.
	//
	// MEASURED ON THE ACCEPTANCE COMMANDLET: building into a package that still holds an object of the
	// same name makes `SavePackage` answer "could not save" while every graph builds. `MarkAsGarbage`
	// does NOT clear it, because the object stays in the package's object list until a collection a
	// commandlet never reaches. Renaming to the transient package is what frees the name.
	{
		UPackage* Existing = FindPackage(nullptr, *PackagePath);
		if (Existing != nullptr)
		{
			Existing->SetDirtyFlag(false);
			for (const TCHAR* Name : { *MaterialName, *InstanceName, *FunctionName })
			{
				if (UObject* Old = FindObject<UObject>(Existing, Name))
				{
					Old->Rename(nullptr, GetTransientPackage(),
						REN_DontCreateRedirectors | REN_NonTransactional);
				}
			}
		}
	}

	UPackage* Package = CreatePackage(*PackagePath);
	if (Package == nullptr)
	{
		UE_LOG(LogTurboMCP, Error, TEXT("could not create the package %s"), *PackagePath);
		return 1;
	}
	Package->SetDirtyFlag(true);

	// THE MATERIAL. `NewObject` rather than a factory: `UMaterialFactoryNew` exists but a factory's
	// job is to drive an asset-creation UI flow, and this needs an object in a named package with a
	// known name. `RF_Public | RF_Standalone` is what makes it an asset rather than an inner object.
	UMaterial* Material = NewObject<UMaterial>(
		Package, *MaterialName, RF_Public | RF_Standalone | RF_Transactional);
	if (Material == nullptr)
	{
		UE_LOG(LogTurboMCP, Error, TEXT("could not create the material"));
		return 1;
	}

	FCounts Counts;
	const bool bMaterial = BuildMaterial(Material, Counts);
	const bool bBoxes = BuildBoxes(Material, Counts);

	// A USAGE FLAG, BECAUSE A MATERIAL WITH NONE CANNOT BE INSTANCED AND THE ENGINE SAYS SO.
	//
	// `SetMaterialUsage` is `MaterialEditingLibrary.h:188` and takes the usage plus an out-flag for
	// whether a recompile is needed. Only the usage is set; the recompile is NOT run, because nothing
	// here reads the compiled shader map and a shader compile in a headless commandlet costs minutes
	// for a result this fixture never consults.
	{
		bool bNeedsRecompile = false;
		UMaterialEditingLibrary::SetMaterialUsage(Material, MATUSAGE_StaticLighting, bNeedsRecompile);
		UE_LOG(LogTurboMCP, Display,
			TEXT("TurboMCP: usage MATUSAGE_StaticLighting set (recompile wanted=%s, not run because ")
			TEXT("nothing here reads the compiled result)"),
			bNeedsRecompile ? TEXT("yes") : TEXT("no"));
	}

	UE_LOG(LogTurboMCP, Display,
		TEXT("TurboMCP: material=%s boxes=%s expressions=%d comments=%d labels=%d usages=%d ")
		TEXT("connections=%d"),
		bMaterial ? TEXT("ok") : TEXT("FAILED"),
		bBoxes ? TEXT("ok") : TEXT("FAILED"),
		Counts.Expressions, Counts.Comments, Counts.Labels, Counts.LabelUsages,
		Counts.Connections);

	if (!bMaterial || !bBoxes)
	{
		UE_LOG(LogTurboMCP, Error, TEXT("the material could not be built"));
		return 2;
	}

	// A FIXTURE THAT BUILT NOTHING MUST NOT BE SAVED. The counts are the only evidence this commandlet
	// has, and a save that succeeded on an empty material would look identical to a good one.
	//
	// THE FUNCTION'S COUNTS ARE SEPARATE AND ARE CHECKED AFTER IT IS BUILT. A guard here that demanded
	// 3 labels and 3 usages from the MATERIAL would go red on a fixture that is RIGHT: the third label
	// lives in the **material function**, which is a different asset and is counted into its own
	// `FCounts`. A guard that demands the wrong total looks exactly like a wrong fixture, which is why
	// the split is stated here rather than left to the reader.
	if (Counts.Expressions == 0 || Counts.Comments != 2 || Counts.Labels != 2 || Counts.LabelUsages != 2)
	{
		UE_LOG(LogTurboMCP, Error,
			TEXT("the MATERIAL is not the structure it claims: expected 2 labels (VerticalMix read ")
			TEXT("twice, UnusedTint read zero times), 2 label usages and 2 comment boxes, got ")
			TEXT("%d label(s), %d usage(s) and %d box(es)"),
			Counts.Labels, Counts.LabelUsages, Counts.Comments);
		return 2;
	}

	// --- THE MATERIAL INSTANCE ---------------------------------------------------------------
	UMaterialInstanceConstant* Instance = NewObject<UMaterialInstanceConstant>(
		Package, *InstanceName, RF_Public | RF_Standalone | RF_Transactional);
	if (Instance == nullptr)
	{
		UE_LOG(LogTurboMCP, Error, TEXT("could not create the material instance"));
		return 3;
	}
	UMaterialEditingLibrary::SetMaterialInstanceParent(Instance, Material);

	// THE OVERRIDE IS VERIFIED BY READING THE INSTANCE BACK, BECAUSE THE SETTER ALWAYS ANSWERS false.
	//
	// MEASURED, and this is a real engine defect rather than a fixture problem. Read from
	// `MaterialEditingLibrary.cpp`, `SetMaterialInstanceScalarParameterValue` is:
	//
	//     bool bResult = false;
	//     if (Instance)
	//     {
	//         Instance->SetScalarParameterValueEditorOnly(...);
	//         UMaterialEditingLibrary::UpdateMaterialInstance(Instance);
	//     }
	//     return bResult;          // <- NEVER ASSIGNED
	//
	// The call DOES the work and then reports failure. Trusting its return value would have made this
	// commandlet either abort on a working fixture or, had the guard been written the other way,
	// accept a fixture whose override never landed. So the override is READ BACK off the instance's
	// own `ScalarParameterValues` array, which is the same array `R-MAT-LOAD` prints when
	// `IncludeInstanceOverrides` is set - the reader's own source of truth.
	UMaterialEditingLibrary::SetMaterialInstanceScalarParameterValue(
		Instance, TEXT("LazyScalar"), 0.5f);
	UMaterialEditingLibrary::UpdateMaterialInstance(Instance);

	bool bOverride = false;
	for (const FScalarParameterValue& Value : Instance->ScalarParameterValues)
	{
		if (Value.ParameterInfo.Name == FName(TEXT("LazyScalar")))
		{
			bOverride = true;
			break;
		}
	}

	UE_LOG(LogTurboMCP, Display,
		TEXT("TurboMCP: instance override on LazyScalar=%s (%d scalar override(s) on the instance)"),
		bOverride ? TEXT("ok") : TEXT("FAILED"), Instance->ScalarParameterValues.Num());

	if (!bOverride)
	{
		UE_LOG(LogTurboMCP, Error,
			TEXT("the instance has no LazyScalar override, so the override table R-MAT-LOAD prints ")
			TEXT("would be empty and the fixture would not exercise it"));
		return 3;
	}

	// --- THE MATERIAL FUNCTION ---------------------------------------------------------------
	UMaterialFunction* Function = NewObject<UMaterialFunction>(
		Package, *FunctionName, RF_Public | RF_Standalone | RF_Transactional);
	if (Function == nullptr)
	{
		UE_LOG(LogTurboMCP, Error, TEXT("could not create the material function"));
		return 4;
	}
	FCounts FunctionCounts;
	const bool bFunction = BuildFunction(Function, FunctionCounts);
	if (bFunction)
	{
		UMaterialEditingLibrary::UpdateMaterialFunction(Function, nullptr);
	}
	UE_LOG(LogTurboMCP, Display,
		TEXT("TurboMCP: function=%s expressions=%d labels=%d usages=%d"),
		bFunction ? TEXT("ok") : TEXT("FAILED"),
		FunctionCounts.Expressions, FunctionCounts.Labels, FunctionCounts.LabelUsages);

	// THE FUNCTION'S OWN GUARD, checked where the function's counts exist rather than folded into the
	// material's. A function that built nothing would still take the plan's function branch and print
	// an empty graph, which looks like a function with no expressions rather than a broken fixture.
	if (!bFunction || FunctionCounts.Expressions == 0 || FunctionCounts.Labels != 1)
	{
		UE_LOG(LogTurboMCP, Error,
			TEXT("the FUNCTION is not the structure it claims: expected a label read once, got ")
			TEXT("%d expression(s) and %d label(s)"),
			FunctionCounts.Expressions, FunctionCounts.Labels);
		return 4;
	}

	Package->MarkPackageDirty();

	// THE ABSOLUTE PATH IS BUILT BY HAND, BECAUSE THE ENGINE'S CONVERSION GOES RELATIVE HERE.
	//
	// MEASURED ON THE ACCEPTANCE COMMANDLET: `FPackageName::LongPackageNameToFilename` answered a
	// path RELATIVE TO THE ENGINE DIRECTORY and the save failed. `/Game` is remapped to the PROJECT
	// content directory, so the absolute path is the package path without its `/Game` prefix, under
	// `FPaths::ProjectContentDir()`.
	const FString ContentDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir());
	const FString FileName = ContentDir
		+ PackagePath.RightChop(FCString::Strlen(TEXT("/Game")))
		+ FPackageName::GetAssetPackageExtension();

	UE_LOG(LogTurboMCP, Display, TEXT("TurboMCP: saving to %s"), *FileName);

	// THE OLD FILE IS DELETED BEFORE THE SAVE, WHICH IS WHAT MAKES THIS RE-RUNNABLE.
	{
		IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
		if (PlatformFile.FileExists(*FileName) && !PlatformFile.DeleteFile(*FileName))
		{
			UE_LOG(LogTurboMCP, Error,
				TEXT("the existing asset at %s could not be deleted, so the save would collide"),
				*FileName);
			return 5;
		}
	}

	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	SaveArgs.SaveFlags = SAVE_NoError;
	if (!UPackage::SavePackage(Package, nullptr, *FileName, SaveArgs))
	{
		UE_LOG(LogTurboMCP, Error, TEXT("could not save %s"), *FileName);
		return 5;
	}

	FAssetRegistryModule::AssetCreated(Material);
	FAssetRegistryModule::AssetCreated(Instance);
	FAssetRegistryModule::AssetCreated(Function);

	UE_LOG(LogTurboMCP, Display, TEXT("TurboMCP: wrote %s"), *FileName);

	// THE COUNTS ARE THE EVIDENCE A CALLER HAS, so they are printed rather than left to the reader to
	// discover. Each number corresponds to one code path the fixture exists to make reachable.
	UE_LOG(LogTurboMCP, Display, TEXT("  material: %d expression(s), %d comment box(es)"),
		Material->GetExpressionCollection().Expressions.Num(),
		Material->GetExpressionCollection().EditorComments.Num());
	UE_LOG(LogTurboMCP, Display, TEXT("  labels: VerticalMix read twice, UnusedTint read zero times"));
	UE_LOG(LogTurboMCP, Display, TEXT("  boxes: OUTER 1000x700 at 100,100 containing INNER 360x230 at 120,120"));
	UE_LOG(LogTurboMCP, Display, TEXT("  instance: parent=%s override LazyScalar=0.5"),
		Instance->Parent != nullptr ? *Instance->Parent->GetName() : TEXT("(none)"));
	UE_LOG(LogTurboMCP, Display, TEXT("  function: %d expression(s)"),
		Function->GetExpressions().Num());

	return 0;
}

#undef LOCTEXT_NAMESPACE
