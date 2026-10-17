# Tool selection and preferred alternatives

[Documentation](../README.md) · [Comparison](../tool-comparison.md)

## Objective

An agent should choose the tool that best meets the requested workflow on the detected engine. A verified TurboMCP improvement must be discoverable in ordinary tool search and schema descriptions, without relying on the agent reading this repository. Preserve native tools for workflows they handle well.

## Routing record

Maintain one row per TurboMCP tool in docs/audit/tool-routing.json. Record exact native toolset/function identities, engine version/build range, supported input scope, effects, preference, reason, evidence and last verification. Allowed preference values are unverified, prefer-turbomcp, prefer-native, complementary and unavailable. Unverified is the default. A missing equivalent list means unknown, not unique capability.

Use prefer-turbomcp only after a same-goal comparison on the owned 5.8.3 fixture demonstrates a useful benefit without weaker required semantics. Prefer-native records a native workflow that already meets the requirement. Complementary means different scopes or steps, not a winner. Supersedes is reserved for verified coverage of the stated native workflow, never the entire native tool by implication.

## Agent-visible wording

For an approved preference, the first sentence of the live tool description should state: Preferred over NATIVE_TOOL for WORKFLOW on ENGINE_RANGE because VERIFIED_BENEFIT. Follow with required inputs, effects and limits. Replace those labels with verified facts before publication. Include exact native identities and searchable task terms in discovery/help, and generate documentation from the same routing record.

Do not rename stable tools or patch Epic's descriptions just to win search ranking. Do not disable native tools globally. If a preferred tool is absent, discover alternatives and report the missing capability. Never fall back from a safety refusal to a less restrictive native mutation. After an unknown mutation outcome, inspect state before any retry or alternative call.

## Verification

Capture actual discovery/search and schema results, not merely header strings. Run a fixed English prompt set covering overlapping workflows, native-only tasks, unsupported scopes, unavailable tools, refusals and unknown outcomes. Record exact client/model version, routing data, choices and observed effects. Repeat ambiguous cases; publish counts rather than a universal model-reliability claim. All mutation probes use owned disposable fixtures on the pinned 5.8.3 editor. Missing target-client access blocks a model-choice claim, not deterministic metadata tests.

5.9 preference is a separate decision. Do not inherit a 5.8.3 badge or claim from similarly named interfaces.
