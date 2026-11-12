# NOV09-12: report container reference writes

Readiness: ready. Integration owner: integrator.

## Source evidence
BlueprintRead.cpp RenderRelations records writers only for UK2Node_VariableSet. VariableGet records readers but does not inspect consumers. The November 9 BL-17 report describes array Add/Remove incorrectly producing no-writer output. Stock 5.8.4 KismetArrayLibrary declares mutable TargetArray reference parameters for these operations; Array_Length takes a const reference.

## Repair contract
- Preserve explicit Set reporting. Trace own member getter outputs through reroute nodes to input consumers with cycle protection. Never attribute another object's member or a local to an own member with the same name.
- Recognize resolved stock container mutators as mutation sites; this describes graph operations, not proof that execution changes a value. Distinguish other non-const reference parameters as possible writes. Const-reference consumers must not become writers.
- Use resolved function identity and parameter metadata, not display names. Do not traverse arbitrary value-producing nodes as if they preserve aliases. Explain this static analysis boundary in output/help.
- Do not report no writes when known or possible reference-write sites exist. Preserve selected-graph scope and conservative unused-variable reporting.
- Review shared text/JSON behavior and avoid claiming unsupported JSON parity.

## Scope and focused verification
Approve BlueprintRead.cpp and public tool help header, plus existing Blueprint reader output suite. Add a disposable synthetic fixture through the existing supervised test path if that suite supports it; otherwise amend scope before implementation. Verify Add/Remove, const Length, custom mutable-reference possible write, reroutes, and foreign/local same-name exclusion. One batched compile and focused scenario; no full regression.

## Integrator scope amendment
The existing Python reader suite cannot construct graph fixtures itself. Approve TurboMCPReferenceWritesTest.cpp as an additional exact new target; invoke that native test from the already scoped Python suite. Keep the existing durable claim and original scope files; use this amended base for submission verification. Engine source review also confirms map/set custom thunks mutate const-declared targets; classify exact resolved stock target operations before the generic const exclusion.
