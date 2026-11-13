# NOV05-14 quoted pin reconciliation

Readiness: ready. Integration owner: integrator.

At 2e36b8f4, ParsePinLine already reads a double-quoted name without splitting spaces. GraphAppend stores that name intact and resolves against full probe pin names. ParseLinkValue also decodes quoted source pin names. The historical report lacks the exact call, so do not claim its cause is reproduced or add duplicate syntax.

Verify append planning/application with a synthetic non-default value on an input pin containing spaces and a quoted source pin link. Inspect output/read-back, no unrelated graph mutations. Document the accepted syntax in agent help. Correct only demonstrated parsing failures. Include escaped quotes and malformed quote refusal if the existing lexer requires repair; do not broaden graph creation scope. Additional Return creation remains NOV05-13.

Scope: TurboMCPGraphTextLex.cpp, TurboMCPGraphAppendAliasTest.cpp, TurboMCPAgentHelp.cpp. Reuse existing append alias suite, one focused execution after batching. No full regression. Preserve unknown original call as a coverage limit.
