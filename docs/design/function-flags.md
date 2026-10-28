# Function flags: OCT27-16

## Approved behavior

Add PreviewTurboMCPFunctionFlags and EditTurboMCPFunctionFlags with Asset, Function, Pure and Const parameters. Pure and Const each accept keep, true or false; explicit strings avoid accidental default-boolean changes. Report current and requested flags, affected callers, refusal reasons and the next verification step in plain English. Pair with existing FunctionAdd to create a helper and then set its flags. Do not duplicate function creation.

Operate on an exact Blueprint-owned function in an idle editor. Refuse inherited overrides, interface implementations and unsupported static/const combinations. Inspect the body for latent or otherwise incompatible operations before conversion. Do not describe const as proof that all external objects remain unchanged: Unreal const constrains the receiver, not every reachable object.

Inspect registered Blueprint dependencies and loaded callers before mutation; report unavailable dependencies and explicit inspection limits. Detect child overrides and delegate references rather than assuming ordinary call nodes cover all uses. Refuse impure-to-pure conversion when removing execution pins would discard connected flow. Refuse pure-to-impure conversion when connected consumers would acquire an unexecuted call. Eligible disconnected callers may be reconstructed. Preserve all existing data links, defaults and positions. A refusal must not alter function flags or graph links; dependency loading and its engine callbacks must be disclosed separately.

Use stock engine APIs inside the plugin. Update entry/function flags coherently, reconstruct affected nodes and independently verify the intended flags and preserved connections. Report partial mutation explicitly if postconditions fail; do not claim rollback without checking restoration. No automatic save. No-op requests do not dirty assets. Keep transactions and compilation effects explicit in help and results.

## Required evidence

Create a pure string helper through FunctionAdd plus the flags tool; compile and evaluate its result. Exercise eligible const changes and unsafe body/override/purity conversions. Include connected execution callers, connected pure consumers, disconnected callers, dependent Blueprint callers, unchanged refusal state, no-op dirty state, actual output and live parameter help. Inspect a rendered graph when reconstruction affects visual pins or layout. Keep failures in the review record.

The implementation must satisfy the complete task acceptance; source investigation is not implementation completion. Shared catalog, routing, coverage and evidence integration remain integrator-owned. DSH stays paused and Unreal stays stock.
