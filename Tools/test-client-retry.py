#!/usr/bin/env python3
# Copyright (c) 2026 FloStyle
# SPDX-License-Identifier: MPL-2.0
"""C01 and C02: the client never replays a mutation after an ambiguous failure.

WHY THIS SUITE EXISTS
    Release finding R02 was confirmed twice over: once by reading `Tools/tmcp.py`'s control flow, and once with
    a mocked-I/O probe (`Release/Audit/audit_retry_probe.py`) that produced TWO effects from ONE lost response.
    The old `rpc` caught every exception, cleared the session id, reconnected, and repeated the original
    request - including a request that had already changed an asset and whose response was merely lost.

    The repair denies replay by default. Only an explicitly safe request may be repeated. This
    suite drives the REAL `Session.rpc` and `Session.call` with a mocked transport, so it measures the
    production control flow rather than a description of it.

WHAT EACH CASE DISCRIMINATES
    C01 is the reported defect as a negative control: apply the mutation, drop the response, and assert that
    exactly ONE effect happened. A client that replayed would produce two, which is what the audit measured.
    C02 checks reconnect independently: an explicitly safe request can retry after response loss,
    while an ambiguous tool call reconnects without repeating the request.
"""

import importlib.util
import json
import os
import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]
CLIENT = ROOT / "Tools" / "tmcp.py"

failures = []


def record(case, ok, detail=""):
    print("  {0} {1}{2}".format("ok  " if ok else "FAIL", case, ("  " + detail) if detail else ""))
    if not ok:
        failures.append(case)


def load_client():
    spec = importlib.util.spec_from_file_location("tmcp_under_test", str(CLIENT))
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def main():
    if not CLIENT.is_file():
        print("RESULT: BLOCKED (missing %s)" % CLIENT)
        return 1
    tmcp = load_client()

    # --- C01: a mutation whose response was lost is applied exactly once --------------------------
    class DroppingMutationProbe(tmcp.Session):
        """A fake transport that applies the edit, then loses the response."""

        def __init__(self):
            super().__init__()
            self.effects = 0
            self.reconnects = 0
            self.requests = 0

        def connect(self):
            self.reconnects += 1
            return {"ok": True}

        def _rpc_once(self, method, params=None, request_id=1):
            self.requests += 1
            self.effects += 1  # the server applied it, and THEN the response was lost
            raise TimeoutError("simulated response loss AFTER a committed edit")

    probe = DroppingMutationProbe()
    raised = None
    try:
        probe.call(tmcp.EDIT_TOOLSET, "EditTurboMCPGraphRepoint",
                   {"asset": "A", "graphName": "G"})
    except Exception as exc:  # noqa: BLE001 - the type is asserted below
        raised = exc

    record("C01 exactly one effect follows one lost response", probe.effects == 1,
           "simulated effects: %d" % probe.effects)
    record("C01 the request was sent once", probe.requests == 1, "requests: %d" % probe.requests)
    record("C01 the caller is told the outcome is unknown", isinstance(raised, tmcp.UnknownOutcomeError),
           type(raised).__name__ if raised else "no exception")
    record("C01 the reconnect still happened", probe.reconnects == 1, "reconnects: %d" % probe.reconnects)
    record("C01 the message refuses to guess and says what to do instead",
           raised is not None and "UNKNOWN" in str(raised) and "NOT repeated" in str(raised),
           (str(raised)[:160] + "...") if raised else "")
    record("C01 the message names the tool",
           raised is not None and raised.tool == "EditTurboMCPGraphRepoint")

    # Replay requires an explicit assertion that this exact request is safe.
    # A Read prefix alone is insufficient: readers can execute editor callbacks.
    class DroppingReadProbe(tmcp.Session):
        def __init__(self):
            super().__init__()
            self.attempts = 0
            self.reconnects = 0

        def connect(self):
            self.reconnects += 1
            return {"ok": True}

        def _rpc_once(self, method, params=None, request_id=1):
            self.attempts += 1
            if self.attempts == 1:
                raise TimeoutError("simulated loss on a read")
            # The result shape `call()` reads: one text content block.
            return {"content": [{"text": "attempt %d" % self.attempts}]}

    read_probe = DroppingReadProbe()
    denied = DroppingReadProbe()
    try:
        denied.call(tmcp.READ_TOOLSET, "ReadTurboMCPStatus", {})
    except tmcp.UnknownOutcomeError:
        record("Read labels alone do not authorize replay", denied.attempts == 1 and denied.reconnects == 1)
    else:
        record("Read labels alone do not authorize replay", False)
    outcome = read_probe.call(tmcp.READ_TOOLSET, "ReadTurboMCPStatus", {}, mutates=False)
    record("C01 a read whose response was lost is still retried", read_probe.attempts == 2,
           "attempts: %d" % read_probe.attempts)
    record("C01 the retried read succeeds", outcome == "attempt 2", repr(outcome)[:80])

    # --- C01: the classification itself, including the deliberate bias ---------------------------
    record("C01 an Edit tool is a mutation", tmcp.is_mutation("EditTurboMCPGraphRepoint") is True)
    record("C01 a Preview tool is treated as a mutation too, because R05 says the class does not enforce "
           "read-only behaviour", tmcp.is_mutation("PreviewTurboMCPWidgetPreview") is True)
    record("C01 a Read label does not establish replay safety", tmcp.is_mutation("ReadTurboMCPStatus") is True)
    record("C01 an unknown name is treated as a mutation, which is the safe direction",
           tmcp.is_mutation("FrobnicateTurboMCPThing") is True)
    record("C01 an explicit override wins",
           tmcp.is_mutation("EditTurboMCPGraphRepoint", override=False) is False
           and tmcp.is_mutation("ReadTurboMCPStatus", override=True) is True)
    record("C01 unresolved tool envelopes deny replay; initialization may retry",
           tmcp.is_mutation("call_tool") is True and tmcp.is_mutation("initialize") is False)

    # --- C02: a reconnect alone must not repeat the previous request -----------------------------
    class ReconnectProbe(tmcp.Session):
        """Loses the FIRST response, then answers. Records the order of requests it was asked to send."""

        def __init__(self):
            super().__init__()
            self.sent = []
            self.mutations = 0
            self.reconnects = 0

        def connect(self):
            self.reconnects += 1
            return {"ok": True}

        def _rpc_once(self, method, params=None, request_id=1):
            name = None
            if isinstance(params, dict):
                arguments = params.get("arguments")
                if isinstance(arguments, dict):
                    name = arguments.get("tool_name")
            self.sent.append(name)
            if name == "EditTurboMCPGraphRepoint":
                self.mutations += 1
            if len(self.sent) == 1:
                raise TimeoutError("the first response was lost")
            return {"content": [{"text": "ok"}]}

    probe2 = ReconnectProbe()
    probe2.call(tmcp.READ_TOOLSET, "ReadTurboMCPStatus", {}, mutates=False)
    record("C02 a reconnect re-sends only the read that failed",
           probe2.sent == ["ReadTurboMCPStatus", "ReadTurboMCPStatus"], str(probe2.sent))

    probe3 = ReconnectProbe()
    raised3 = None
    try:
        probe3.call(tmcp.EDIT_TOOLSET, "EditTurboMCPGraphRepoint", {})
    except Exception as exc:  # noqa: BLE001
        raised3 = exc
    record("C02 a reconnect after a mutation does NOT repeat the mutation",
           probe3.sent == ["EditTurboMCPGraphRepoint"], str(probe3.sent))
    record("C02 the mutation is never counted twice", probe3.mutations == 1,
           "mutations sent: %d" % probe3.mutations)
    record("C02 the reconnect still happened for the mutation",
           probe3.reconnects == 1, "reconnects: %d" % probe3.reconnects)
    record("C02 the ambiguous mutation still reports an unknown outcome",
           isinstance(raised3, tmcp.UnknownOutcomeError),
           type(raised3).__name__ if raised3 else "no exception")

    # Loss before dispatch and loss after application are both uncertain to the client.
    class BeforeDispatchProbe(DroppingMutationProbe):
        def _rpc_once(self, method, params=None, request_id=1):
            self.requests += 1
            raise ConnectionError("secret-test-value must not be copied")
        def connect(self):
            self.reconnects += 1
            raise TimeoutError("private-reconnect-detail")
    before = BeforeDispatchProbe()
    try:
        before.call(tmcp.EDIT_TOOLSET, "EditTurboMCPGraphRepoint", {"secret": "private-argument"})
    except tmcp.UnknownOutcomeError as error:
        incident = error.diagnostic
        record("C03 before-dispatch loss is not replayed", before.requests == 1 and before.effects == 0)
        record("C03 failed reconnect retained without guessing outcome", incident["reconnect"] == "failed" and incident["outcome"] == "unknown")
        record("C03 UTC incident retained on session", incident["utc"].endswith("+00:00") and before.last_failure == incident)
        record("C03 diagnostic excludes private input", all(text not in str(error) for text in ("secret-test-value", "private-reconnect-detail", "private-argument")))
    else:
        record("C03 unknown outcome exception required", False)
    record("C03 after-effect evidence retains successful reconnect",
           raised.diagnostic["reconnect"] == "succeeded" and raised.diagnostic["replayed"] is False)

    # --- the CLI's exit code for an unknown outcome ----------------------------------------------
    record("C02 the CLI exit code for an unknown outcome is 2 and is distinct from failure",
           tmcp.EXIT_UNKNOWN == 2)

    if failures:
        print("")
        for problem in failures:
            print("  {0}".format(problem))
        print("RESULT: FAIL ({0} check(s))".format(len(failures)))
        return 1
    print("RESULT: PASS (one lost response means one effect, and explicitly safe reads may retry)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
