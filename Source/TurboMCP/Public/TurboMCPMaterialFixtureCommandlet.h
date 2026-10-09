// Copyright FloStyle. All Rights Reserved.

#pragma once

#include "Commandlets/Commandlet.h"
#include "CoreMinimal.h"

#include "TurboMCPMaterialFixtureCommandlet.generated.h"

/**
 * Builds the material fixture that the four material readers need and no writer could produce.
 *
 * WHY THIS EXISTS
 * ---------------
 * `R-MAT-LABELS`, `R-MAT-LOAD`, `R-MAT-TRACE` and `R-MAT-BLOCKS` COMPILE and answer, but the test
 * project contains NO MATERIAL, so the only thing the suites could prove about them is that they
 * REFUSE a non-material asset. `Tools/test-batch-tools.py` says so in its own docstring rather than
 * pretending the refusal is a test of the reading. This commandlet closes that gap.
 *
 * WHY A COMMANDLET AND NOT ONE OF THE PLUGIN'S OWN TOOLS
 * -----------------------------------------------------
 * `06_ACCEPTANCE.md` says the structure an acceptance test measures should not be built by the tool
 * under test, because an asset a tool wrote proves nothing about that tool. A commandlet is not one
 * of the plugin's tools - it is a build step, like the `.cpp` file next to it. Building the fixture
 * from C++ rather than committing an opaque binary also means the structure is LEGIBLE IN THE
 * REPOSITORY: a reader can see exactly which expressions were made and how they were wired, and
 * re-running it reproduces the asset.
 *
 * WHAT IT BUILDS
 * --------------
 * `/Game/Tests/TurboMCPMaterialFixture`, a material plus a function and an instance, each item
 * existing to make ONE code path reachable:
 *
 * | Item | The code path it makes reachable |
 * |---|---|
 * | a named reroute read in TWO places | the label-replaces-node-ID rule |
 * | a named reroute read in ZERO places | the UNREAD finding `R-MAT-LABELS` exists to produce |
 * | one value read by two consumers with no label | the shared value defined ONE time as `$key` |
 * | a Multiply with an UNLINKED input | the reflected constant `2.5` requires by name |
 * | a ScalarParameter reaching no output | the unreached-parameter flag |
 * | two comment boxes, one inside the other | `R-MAT-BLOCKS` nesting, innermost wins |
 * | a wire crossing a box boundary with no label | the cross-box-wire finding |
 * | a Material Instance with a scalar override | refusal-by-parent and the override table |
 * | a Material Function | the function branch of the plan |
 *
 * EVERY EXPRESSION IS PLACED AT A REAL COORDINATE, AND THE COORDINATES ARE THE FIXTURE. Box
 * membership in `R-MAT-BLOCKS` is GEOMETRIC - the reader compares each expression's editor position
 * against each box rectangle and gives the expression to the SMALLEST box containing it. A fixture
 * whose nodes all sat at (0, 0) would put every node in every box and the nesting would be
 * untestable while looking populated.
 */
UCLASS()
class UTurboMCPMaterialFixtureCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UTurboMCPMaterialFixtureCommandlet();

	virtual int32 Main(const FString& Params) override;
};
