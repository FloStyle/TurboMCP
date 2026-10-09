# Questions

Questions for the owner about anything the specification leaves open, where a
guess would change how a tool behaves.

Format: a number, the date, the spec file and section, the question, and the
answer when it arrives. Answers come back as a new spec version, ***REMOVED***
. (`Spec/00_START_HERE.md` section 3).

---

## Q-1 2026-10-07 Spec/01_BRIEF.md target

The brief targets Unreal Engine 5.8.1. The owner chose branch `release`, which is
5.8.3, for the engine build. Every engine fact in `Spec/07_ENGINE_NOTES.md` was
checked against 5.8.1.

Question: should `Spec/07_ENGINE_NOTES.md` be re-verified against 5.8.3, and
should the brief's target line be updated to 5.8.3?

Answer: not yet answered.

---

## Q-2 2026-10-07 Spec/03_TOOLS.md section 1.3

Section 1.3 asks for one pair of action words used for every Preview/Edit pair,
and gives "Suggest X" and "Apply X" as an example.

Question: is "Suggest" and "Apply" the pair to use, or does the owner want
another pair?

Answer: not yet answered.

---

## Q-3 2026-10-09 Spec/03_TOOLS.md R-NODE-RESOLVE

`R-NODE-RESOLVE` must print "the node type ID (the one the engine's node creation
tool accepts)". That ID is `<menu category>|<node title>` with whitespace
removed, and **the title half follows the editor's language**. On this French
editor the engine's own `find_node_types` offers `Affichage|Événement|...` and
`Utilitaires|Casting|...`, so the localised form is genuinely what the engine's
writer accepts here.

This plugin now prints the un-localised category (`AddEvent` rather than
`Ajouterunévénement`) but cannot un-localise the title, because Epic builds a node
title at run time from separate localised fragments. It also prints the node's
C++ class (`K2Node_Event`) beside it as the one locale-independent name.

Question: is that acceptable, or does the specification want something stronger —
for example a type ID that is stable across languages even when the engine's own
writer would not accept it? A stable ID would have to come from somewhere other
than the title, and would then need translating before it could be handed to the
engine's writer.

Answer: not yet answered.

---

## Q-4 2026-10-09 Spec/03_TOOLS.md E-SAVE

`E-SAVE` is classified as an **Edit** tool, but it does not make a change: it
writes to disk changes that were already made. Its own spec even has it report
"had no changes" as a normal outcome.

This plugin follows the spec and puts it in `UTurboMCPEditToolset`, and it opens
its result with `unchanged:` when nothing was saved and `changed: saved N` when
something was, which satisfies `P4`.

Question: is `Edit` the intended class for a save, or should `E-SAVE` sit outside
the four kinds? The four kinds are described as Read, Look, Preview and Edit, and
"an Edit is one transaction" (`P4`) does not obviously describe a save.

Answer: not yet answered.

---

## Q-5 2026-10-09 Spec/03_TOOLS.md R-COMMENT-LIST, nesting and overlap

`R-COMMENT-LIST` must report a box's "parent box if nested", and a node can fall
inside two overlapping boxes. Both are computed from the rectangles, because the
engine stores neither.

**Neither path has been exercised against real content.** Every engine asset
reachable from this project reports `0 nested` and `0 node(s) inside more than one
box`. The arithmetic is checked against a model in `Tools/test-comments.py`, which
proves the geometry and does not prove the rendering. `TestRecord.md` section 2k
records this as unverified.

Question: is there a piece of content the owner knows of that has nested or
overlapping comment boxes, so the two paths can be verified against something real
rather than a model? Failing that, is a hand-made nested box acceptable as part of
the test kit, given `06_ACCEPTANCE.md` requires the kit's structure to be built by
hand anyway?

Answer: not yet answered.

---

## Q-6 2026-10-09 Spec/03_TOOLS.md R-DATATABLE, project-owned tables

`R-DATATABLE` is verified against an engine DataTable with a known struct, fields and
rows. It is **not** verified against a table whose row struct is a project asset,
because no such table could be built and four attempts failed on engine rules rather
than on anything in this plugin:

- a `UserDefinedEnum` cannot be a row struct, because it is not a `UScriptStruct`;
- `BlueprintEditorLibrary.add_member_variable` only accepts a **Blueprint**, so a
  `UserDefinedStruct` cannot be given fields that way;
- `DataTable::RowStruct` is **read-only** once the asset exists;
- a CSV import through `AssetImportTask` refuses with "A Data table row type must be
  specified in the import settings json file for automated import".

Question: should a project-owned table be built, and if so how? Two routes are known
and both cost more than the check is worth right now: an import-settings JSON beside
the CSV, or a small C++ commandlet inside the plugin that creates the struct with
`FStructureEditorUtils` and the table with `NewObject`.

The second may be worth preferring, because a commandlet in the plugin is also the way
to build the hand-made acceptance kit that `06_ACCEPTANCE.md` requires. If so, that
kit and this check share one piece of machinery rather than two.

Answer: not yet answered.
