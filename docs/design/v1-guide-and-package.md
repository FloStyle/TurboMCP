# V1 interactive guide and development package

## Reviewed scope and priority

The owner requests a useful, colourful technical presentation within the existing
GitHub Pages documentation, plus a reviewable plugin package. The integration
owner approves these independent bounded assignments. Stable 1.0.0 acceptance
and engine compatibility claims remain governed by the existing release gates.
Readiness: ready. No engine-source edit or safety-guard removal is authorized.

## v1-interactive-guide

Use the current site and source documentation as the public baseline. Buildless
HTML, CSS and JavaScript are sufficient. Continue the Noto/dark chrome identity
with more deliberate cyan, amber and violet accents, readable contrast and real
visual evidence. Preserve the searchable 196-tool catalogue and copyable examples.
The guide must be linked from the homepage and offer two views over one content
source: readable technical chapters and keyboard-operable presentation mode.
Use stable hash links so refreshing a chapter works on GitHub Pages.

Cover motivation, stock Unreal MCP integration, Read/View/Preview/Edit effects,
discovery and context steering, complex Blueprint investigation, captures, graph
changes and tidy, Data Assets/CDOs/DataTables, batching, UI/material inspection,
input/PIE limitations, safeguards, reporting and release/installation workflow.
Include a TurboUX chapter explaining structured HTML/JSON UX prototyping, the
canvas/edit/play loop, workspaces, reusable elements/styles/input contexts, MCP
parity, portable projects and the SVG/Figma bridge. Clearly distinguish shipped
behavior, experimental support and planned work; use current source contracts.

Prefer short technical English, concrete before/request/result examples, labelled
workflow diagrams and screenshots over repeated claims. The guide is the content:
do not require speaker notes or a companion document. Search, chapter navigation,
expandable details, copy examples, print styling and reduced-motion behavior must
work. Retain content in ordinary HTML when JavaScript is unavailable where practical.

The former internal slide deck is narrative source only. Do not copy private
project identifiers, CCXI captures/assets, local paths or embedded credentials to
the public tree. Replace internal examples with the already-cleared synthetic
captures and clearly labelled generic diagrams. No made-up benchmarks, customer
claims or certification. No remote analytics, authentication, backend, framework
migration or new publication workflow. Source screenshots already in site/assets
retain provenance; document any changed usage in the existing provenance file.

Worker scope: exactly the four existing and four new files in worker-scopes.json.
No shared tracker or tool-catalogue generation changes. Check HTML/JS references,
source claims and public boundaries; use the embedded browser at desktop/mobile
widths for navigation, search, presentation keys, focus and image layout. Record
visual evidence and limits. Return a clean commit for integrator review; no push.

## v1-supervised-package

Add a closed package profile to Tools/execute-tests.py, implemented by
Tools/package-plugin.ps1, under the existing ResourceLocks, preflight, owned child
job, bounded timeout, command logging and supervisor-context admission. Preserve
all current profiles and refuse incompatible suite/fixture combinations. Do not
manufacture ownership environment variables or run raw UAT outside the supervisor.

Stage plugin inputs from exact tracked bytes at a clean committed source revision,
with unique fresh input and output paths under ignored release/artifacts. Refuse
existing output destinations, links or paths outside the owned staging boundary.
Unreal BuildPlugin may clear its output and modify staging inputs; never pass the
live plugin folder or user data as either target. Use the pinned stock engine and
Win64 only. Do not change engine.lock.json or claim Launcher compatibility.

Preserve the tracked plugin LICENSE and NOTICES.json even if stock packaging
filters omit them. Produce a ZIP and sidecar manifest recording source commit,
plugin tree, actual engine version/revision, artifact and DLL hashes, platform,
packaging command and observed descriptor changes. Verify the archive contents,
license/notices identity and output hashes before reporting success. Label it a
development preview; Installed=true and stock descriptor version rewriting are
not independent proof of compatibility. Keep generated outputs ignored.

Add only focused offline supervisor tests in Tools/test-execution-runtime.py for
admission/refusal and planned command/output boundaries, with no real engine in
those tests. Check PowerShell syntax and relevant existing regression coverage.
Worker returns source review, commands/results and remaining native limitations.
The integrator schedules the serialized package run after review. A successful
BuildPlugin package still requires separate clean-consumer BP-only/C++ testing
before claiming stable installation support.
