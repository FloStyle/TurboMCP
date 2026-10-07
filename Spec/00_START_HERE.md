# Start here

Spec version: 1.0 (2026-10-07)

This folder is the full specification for a new Unreal Engine editor plugin.
You are the build side. Everything you need to know about WHAT to build is in
these files. HOW to build it is your decision.

## 1. White room

This specification is self-contained. It is the only description of the
plugin, and it was written from the engine's own source and public
documentation.

1. Use only these sources:
   - the files in this `Spec/` folder
   - the Unreal Engine source code and public engine documentation
   - public documentation for C++, Slate, the MCP protocol and similar
   - answers you get through `QUESTIONS.md` (see section 3)
2. Work from the specification and the engine. Every capability here is
   described by what it must do, with its inputs, its outputs, its
   refusals and a test that proves it.
3. Start in a new, empty workspace. Nothing is carried over from anywhere
   else.
4. Choose every name yourself: plugin name, module, classes, files, tools,
   parameters. The spec gives each capability an ID (for example `R-BP-LOAD`).
   These IDs are for this spec only. Do not use them as code names.
5. If something in the spec looks like it was copied from existing code,
   stop and ask through `QUESTIONS.md`.

## 2. Reading order

| File | What it gives you |
|---|---|
| `01_BRIEF.md` | What the plugin is for and who uses it |
| `02_PRINCIPLES.md` | Rules that apply to every tool |
| `03_TOOLS.md` | Every capability, with inputs, outputs, refusals and a done test |
| `04_FORMATS.md` | The shared text formats: graph text, node IDs, asset paths |
| `05_DECISIONS.md` | Decisions already made, with the reason for each |
| `06_ACCEPTANCE.md` | Test assets to create and the tests to pass |
| `07_ENGINE_NOTES.md` | Unreal Engine facts and traps in the engine's own MCP tools |
| `08_ROADMAP.md` | The order to build in |

Read `02_PRINCIPLES.md` and `05_DECISIONS.md` before you write any code.
They exist because the obvious approach failed before.

## 3. Questions

Write a question in `../QUESTIONS.md` with a number, the date and the spec
file and section it is about. Do not guess on a point the spec leaves open
if the answer changes how a tool behaves. Answers come back as a new spec
version, .

## 4. Project rules from the owner

- Reusable across projects. No hard coded asset names, paths or numbers.
  Every constant has a name and a comment that says why it has that value.
- Each comment gives the reason for the code, and the failure it stops.
- Keep files small: each file does a single job. The MCP layer is thin and calls plain
  C++ that does the work, so the panel and the MCP use the same code.
- Docs, tool descriptions and panel labels use simple English: short
  sentences, common words, active voice.
- Keep the docs current. When a tool changes, its description, the panel
  entry, the README and the test record change in the same piece of work.
  Nothing stale.
- Fixing a broken tool comes before building a new one.

## 5. Definition of done (every capability)

- It builds, and the tool shows in the MCP tool list after an editor restart.
- Its tool description matches its current behaviour, and starts with its kind
  (Read, Look, Preview or Edit).
- It has a panel entry in the label format of `03_TOOLS.md` section 1.4.
- Its tests in `06_ACCEPTANCE.md` are run on the test assets, and the
  result is recorded with the numbers in the plugin's own test record.
- No stale text: search the docs for any old name or old number and correct each one.
