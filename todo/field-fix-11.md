# GL-15 execution merge reader classification

ID: FIELD-FIX-11
Status: done
Priority: P0
Readiness: ready
Owner: Codex
Design: docs/design/mutations.md
Kind: implementation
Review: source-reviewed
Validation: unbuilt_untested

## Problem
Urgent field fixes require implementation completion to be distinguished from runtime acceptance.

## Scope

GL-15: multiple valid exec predecessors are listed as execution sources instead of malformed value links. Validate direction/category/reciprocity; data-input rules unchanged. Partial evidence explicitly retains the unsupported multi-source round-trip limitation.

## Acceptance
Implementation task closure means source changes committed only. Build and runtime acceptance remain open in the deferred verification ledger. The owner explicitly deferred all tests; this record does not authorize running them now.

## Completion evidence
Source commit 1887125ba4e1cc319281b930193ab35c5bf41530. Implementation closed; unbuilt and untested.

## Dependencies

FIELD-04 reviewed source scope.

## Verification

Deferred by owner. See [deferred verification ledger](../docs/audit/deferred-field-verification.md); implementation completion is not runtime acceptance.

## Stop conditions

Do not run builds or tests until owner resumes verification. Do not claim runtime acceptance.

## Deliverable

Committed implementation evidence. Deferred checks remain in the verification ledger, not a separate product task.


### Reroute reader endpoint guard

34c545e checks the first reroute link for null, input direction, matching category and reciprocal membership before dereferencing/following it. A broken endpoint leaves the reroute in the reader path instead of crashing or silently following an invalid edge. Implemented; unbuilt_untested. Deferred evidence: malformed/null reroute links do not crash; valid exec reroutes still traverse. Separate from X-39 continuation diagnosis, which remains open. No tests/builds run.


### Reader branch depth and truncation

49421e1 bounds recursive execution-branch rendering at depth 64 and emits an explicit incomplete-dump error. The 2000-node linear limit now warns only if traversal actually attempts another node, avoiding a false error at exact completion. Empty branch continuations do not trigger depth warnings. Implemented; unbuilt_untested. Deferred evidence: deeply nested branches terminate with a clear warning, exactly 2000 linear nodes complete, longer chains report truncation. No tests/builds run.


### Reader error write-back gate

e07223a makes GraphCheck refuse standalone reader error comment lines before asset resolution, with the source line number. Append and other writers using this gate cannot silently discard an explicit malformed/truncated-reader diagnostic and treat its dump as complete. Implemented, unbuilt_untested. Scope is reader error markers only, not all forms of incomplete or unsupported DSL. Deferred evidence: valid comments/string literals remain accepted; indented reader error comments refuse before mutation. No tests/builds run.


### Whole graph text completeness

59004c8 scans graph text for balanced parentheses and closed double-quoted values before asset loading. Escapes, comments and the top-level read envelope are handled; unexpected closing delimiters include line/column. Reader error markers inside quoted content are not treated as diagnostics. Implemented, unbuilt_untested. Deferred evidence: truncated forms/strings refuse without writes, balanced reader output and quoted delimiters remain usable. This is structural completeness, not full semantic round-trip support. No tests/builds run.


### Function graph latent-node validation

235b961 replaces the graph-class-name heuristic with Blueprint FunctionGraphs membership. Stock Unreal creates ordinary function graphs as UEdGraph, so the prior check could bypass the existing IsLatentForMacros refusal. Implemented; unbuilt_untested. Deferred: ordinary function rejects latent nodes, event graph remains permitted, non-latent function nodes remain accepted. This is a source-level preflight correction, not proof of crash resolution or full graph round-trip correctness. No tests or builds run.
