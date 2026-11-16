# Public source and external-agent readiness

Approved bounded repair contract, 2026-11-16. Parent: R04. Assignment: nov16-public-agent-readiness.

## Problem

A fresh source export must preserve current approved content without private ancestry. External agents need an English, provider-neutral way to inspect, propose and verify work without local Codex assumptions or private machine state. A historical security audit does not establish current semantic privacy, ownership or release readiness.

## Scope

Inspect existing export controls and contributor workflow. Repair concrete source-export safety gaps only in Tools/export-public.py. Add Tools/test-public-source-safety.py for minimal synthetic positive/negative controls, using disposable repositories and no sensitive fixtures. Document an external-agent workflow in docs/external-agent-workflow.md and link it from docs/contributing.md and docs/public-preparation.md. Clarify clean source snapshot, independent Git root, hashes, exclusion of untracked local data, and private-to-public content transfer without merging private ancestry. Do not alter AGENTS.md or weaken its approval/claim/lease rules. External agents may perform unassigned read-only audits; code work requires an approved scope/base and actual durable claim through the same CLI when available, otherwise an integrator-owned patch proposal outside the checkout.

## Required behavior

- Never mark public_export_approved or readiness ready; never invent asset ownership or test results.
- No public visibility changes, remote creation, push, history rewriting or private history transfer.
- Export every approved tracked file unchanged; preserve existing fail-closed checks and source identity. Do not silently omit unsafe tracked content.
- Examine whether output trees/manifests and sensitive patterns fail safely, and repair evidenced defects. No gratuitous scanner framework rewrite; dedicated scanners and manual review remain separate gates.
- Explain build/source requirements and supervised focused verification for Claude/DeepSeek or other external clients without requiring those providers. Do not send private source to external services.
- Report remaining blockers with specific evidence needed, especially current binary/privacy review, ownership, release metadata reconciliation and final acceptance. Do not confuse a development preview with validated binaries.

## Verification and handoff

Run only relevant offline exporter/claim tests and repository structure checks. No Unreal build/editor runs needed for this slice. Inspect changed behavior, English docs and examples. Return full base/candidate SHAs, claim ID, exact changed files, command results, limitations and redacted evidence paths. Stop after submitting the bounded candidate; integrator reviews before next assignment. Shared trackers and catalog registration are integrator-owned.
