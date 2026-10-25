# Member variable reader and spawner identity alignment

ID: FIELD-FIX-27
Status: done
Priority: P1
Readiness: done
Owner: Codex
Design: docs/design/mutations.md
Kind: implementation
Review: source-reviewed
Validation: unbuilt_untested

## Problem
X-43/X-47/X-53: reader and discovery getter IDs differ when menu titles prettify names, omit boolean prefixes, or use different categories.

## Scope
Standard member getter/setter spawners use real FProperty names and the reader's Blueprint category/default rule. Existing menu IDs retained as legacy aliases. Includes generated widget member properties exposed through standard variable spawners. Local-variable and custom spawners remain outside this slice; ambiguous IDs still refuse.

## Completion evidence
54b7ed9. Implemented, unbuilt_untested. No claim for native engine DSL IDs or unsupported custom spawners.

## Acceptance
Deferred: b-prefixed member, spaced member name, widget member, category and locale changes, legacy aliases and duplicate ambiguity. Confirm downstream text grammar for names containing whitespace separately.

## Verification
No tests/builds per owner.
