"""Add the match-count overload to the shared graph resolver — WITHOUT changing any caller.

The confirmed gap: `TurboMCPNodeId::FindGraphByName` returns the FIRST matching graph and never counts the matches
(`TurboMCPNodeId.cpp:349-357`). A node id is `...:GraphName:<hash>` — the graph NAME ONLY — so an ambiguous name
lets a tool resolve one graph while the reader shows another, and the report names the same `name:hash` for either.
That is the ARC06 identity concern.

This step adds the OVERLOAD ONLY, so the six existing callers are untouched and nothing behaves differently yet:

    UEdGraph* FindGraphByName(const UBlueprint*, const FString&);                    // unchanged behaviour
    UEdGraph* FindGraphByName(const UBlueprint*, const FString&, int32& OutMatchCount);  // NEW: reports the count

Turning "count > 1" into a REFUSAL in the mutating tools, and a REPORT in the reader, is the next step — and it
needs a fixture with two graphs sharing a name, which does not exist yet.
"""

import pathlib

HEADER = pathlib.Path(r"C:\Users\Example\Desktop\TurboMCP\Source\TurboMCP\Public\TurboMCPNodeId.h")
SOURCE = pathlib.Path(r"C:\Users\Example\Desktop\TurboMCP\Source\TurboMCP\TurboMCPNodeId.cpp")

# --- header -------------------------------------------------------------------------------------
header = HEADER.read_text(encoding="utf-8")
DECL = "UEdGraph* FindGraphByName(const UBlueprint* InBlueprint, const FString& InGraphName);"
NEW_DECL = """UEdGraph* FindGraphByName(const UBlueprint* InBlueprint, const FString& InGraphName);

	/**
	 * The same resolution, and it also reports how many graphs matched.
	 *
	 * *** WHY THIS EXISTS: A NAME IS NOT AN IDENTITY. *** `FindGraphByName` returns the FIRST matching graph and
	 * never counts the rest, and a node id is `...:GraphName:<hash>` - the graph NAME ONLY. So when a blueprint
	 * holds two graphs of the same name in different arrays, a tool can resolve one while the reader shows the
	 * other and the report names the same `name:hash` for either. That is the ARC06 family: identity that cannot
	 * be verified across calls.
	 *
	 * `OutMatchCount > 1` means the name is AMBIGUOUS. A MUTATING caller must REFUSE and name the candidates
	 * rather than guess; a READER should report the count and stay available. The returned graph is still the
	 * first match, so this overload changes nothing by itself - it only makes the ambiguity observable.
	 */
	UEdGraph* FindGraphByName(const UBlueprint* InBlueprint, const FString& InGraphName, int32& OutMatchCount);"""
if DECL in header and "OutMatchCount" not in header:
    header = header.replace(DECL, NEW_DECL, 1)
    HEADER.write_text(header, encoding="utf-8")
    print("header: overload declared")
else:
    print("header: SKIP (already present or anchor missing)")

# --- source -------------------------------------------------------------------------------------
source = SOURCE.read_text(encoding="utf-8")
SIG = "	UEdGraph* FindGraphByName(const UBlueprint* InBlueprint, const FString& InGraphName)\n	{"
if SIG in source and "OutMatchCount" not in source:
    # Rename the existing body into the counting version, then add a forwarder.
    counting = ("	UEdGraph* FindGraphByName(const UBlueprint* InBlueprint, const FString& InGraphName, "
                "int32& OutMatchCount)\n	{\n		OutMatchCount = 0;\n")
    source = source.replace(SIG, counting, 1)
    # Count instead of returning at the first match: keep the FIRST as the result, count the rest.
    OLD_LOOP = """		for (UEdGraph* Graph : Candidates)
		{
			if (Graph->GetName().Equals(InGraphName, ESearchCase::IgnoreCase))
			{
				return Graph;
			}
		}

		return nullptr;
	}"""
    NEW_LOOP = """		UEdGraph* First = nullptr;
		for (UEdGraph* Graph : Candidates)
		{
			if (Graph->GetName().Equals(InGraphName, ESearchCase::IgnoreCase))
			{
				++OutMatchCount;
				if (First == nullptr)
				{
					First = Graph;
				}
			}
		}

		// The FIRST match is still the answer, so no existing caller changes behaviour. The COUNT is the new
		// information: `OutMatchCount > 1` means the name is ambiguous and a mutating caller must refuse.
		return First;
	}

	// The original entry point, unchanged for every existing caller.
	UEdGraph* FindGraphByName(const UBlueprint* InBlueprint, const FString& InGraphName)
	{
		int32 MatchCount = 0;
		return FindGraphByName(InBlueprint, InGraphName, MatchCount);
	}"""
    if OLD_LOOP in source:
        source = source.replace(OLD_LOOP, NEW_LOOP, 1)
        SOURCE.write_text(source, encoding="utf-8")
        print("source: counting version + forwarder installed")
    else:
        print("source: MISS the match loop - inspect by hand")
else:
    print("source: SKIP (already present or anchor missing)")
