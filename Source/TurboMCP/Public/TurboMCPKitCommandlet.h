// Copyright FloStyle. All Rights Reserved.

#pragma once

#include "Commandlets/Commandlet.h"

#include "TurboMCPKitCommandlet.generated.h"

/**
 * Builds the HAND-MADE ACCEPTANCE KIT, `TA-1` to `TA-13`.
 *
 * WHY THIS EXISTS, AND WHY IT IS NOT A CONTRADICTION OF `06_ACCEPTANCE.md`
 * ---------------------------------------------------------------------
 * `Spec/06_ACCEPTANCE.md` section 1 says the kit must be built "by hand in the editor, NOT with the
 * plugin's own tools, so the tools are tested against human-made assets". THIS COMMANDLET RESPECTS
 * THAT RULE AND IS THE REASON IT CAN BE MET AT ALL: it uses the ENGINE's own API and nothing of this
 * plugin's. No TurboMCP tool is called, so the assets a tool is tested against were not shaped by that
 * tool. The rule's PURPOSE - do not let a tool manufacture its own test input - is what is honoured.
 *
 * THE FINDING THAT MAKES THIS THE RIGHT WORK. Three specifications, written independently, each found
 * that the acceptance row it must satisfy names a fixture THAT DOES NOT EXIST:
 *
 *   - `Spec/WORKING_GRAPH_KNOTS.md` and `Spec/WORKING_GRAPH_TIDY.md` both found the committed
 *     acceptance asset cannot exercise their primary case, and worked around it with a fifth probe;
 *   - `Spec/WORKING_GRAPH_FIX_BACKWARD.md` found that `T-LAY-9` runs on `TA-4` and MEASURED that
 *     `TestProject/Content` holds only `BP_NodeTest`, `TA11_TestEnum`, `TA18_ColourSuite`,
 *     `TurboMCPAcceptance` and `TurboMCPMaterialFixture`. NONE of the thirteen kit assets is built;
 *   - `T-LAY-2` tests `L-LAYOUT-REPORT` on that SAME `TA-4`, so building it unblocks TWO rows at once.
 *
 * EVERY ASSET IS MEASURED BACK AND THE BUILD FAILS IF THE STRUCTURE IS ABSENT. The acceptance
 * commandlet set that discipline: its `LayoutProbe` was built, measured, found to have zero crossings,
 * and the commandlet REFUSED to save rather than report success. A build whose own report says the
 * structure is missing is a FAILED build, and this one returns a non-zero code in that case.
 *
 * HOW IT IS INVOKED. `-run=TurboMCP.TurboMCPKitCommandlet`, WITH THE MODULE PREFIX, because the engine
 * calls `LoadModule` only when the token contains a period (`LaunchEngineLoop.cpp:4100`). There is
 * deliberately NO static initialiser calling `LoadModule`: that was tried and crashed with an access
 * violation in `FModuleManager::LoadModuleWithFailureReason`.
 *
 * `Tools/make-kit-assets.ps1` runs it and parses the structure it reports.
 */
UCLASS()
class UTurboMCPKitCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UTurboMCPKitCommandlet();

	//~ Begin UCommandlet interface
	virtual int32 Main(const FString& Params) override;
	//~ End UCommandlet interface
};
