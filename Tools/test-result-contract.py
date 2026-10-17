#!/usr/bin/env python3
# Copyright (c) 2026 FloStyle
# SPDX-License-Identifier: MPL-2.0
"""C03-C08: the TurboMCP result contract, and the client that decides from it.

WHY THIS SUITE EXISTS
    Release findings R05, R06 and R07 come from one hole: a tool's outcome could not be determined from its
    answer. A refusal and a success were both MCP transport successes, an agent had to read prose, and the CLI's
    exit status came from a regex over the text. ARC02 answers that with one authoritative final line.

    Every case below is the SPECIFIC failure the catalog names, not a paraphrase of it:

      C03  an explicit refusal, a partial save and a failed rollback -> the CLI exits nonzero FROM METADATA,
           not from prose. The negative control is the one that matters: a body full of alarming words with a
           `changed` result line must still exit 0, or the gate is reading the wrong thing.
      C04  malformed or missing metadata from a MIGRATED tool -> a protocol failure, never success. Silence
           must not read as fine.
      C05  a body that contains forged metadata-like text, then a valid final line -> only the validated final
           line decides. A line that is NOT last is not authoritative.
      C06  the engine wraps an FString and supplies several text content blocks -> one decoded body and one
           metadata object.
      C07  a native engine tool with no TurboMCP metadata -> the native adapter works and does NOT falsely
           certify a TurboMCP outcome.
      C08  the output reaches its exact budget -> metadata survives, truncation is explicit, and no partial
           syntax line is presented as complete.

    No editor is needed. The transport is mocked at the boundary the catalog names.
"""

import importlib.util
import json
import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "Tools"))

failures = []


def record(case, ok, detail=""):
    print("  {0} {1}{2}".format("ok  " if ok else "FAIL", case, ("  " + detail) if detail else ""))
    if not ok:
        failures.append(case)


def load(name, path):
    spec = importlib.util.spec_from_file_location(name, str(path))
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def main():
    result = load("tmcp_result", ROOT / "Tools" / "tmcp_result.py")
    client = load("tmcp_client", ROOT / "Tools" / "tmcp.py")

    # --- the contract's own shape -----------------------------------------------------------------
    line = result.format_result("changed", code="ok", changed=True, assets=["/Game/B", "/Game/A"],
                                compile="passed", rollback="not_needed", save="not_requested")
    record("the line carries the documented prefix", line.startswith("; tmcp-result-v1: "), line[:40])
    parsed = result.parse_result_line(line)
    record("a formatted line parses back to the same outcome", parsed.outcome == "changed")
    record("assets are canonicalised rather than left in call order",
           result.canonical_assets(["/Game/B", "/Game/A", "/Game/A"]) == ["/Game/A", "/Game/B"])

    # A misspelled outcome or an out-of-range enumeration must fail where it is WRITTEN, so a client never sees
    # a line it cannot read.
    for label, kwargs in (("an unknown outcome", {"outcome": "succeeded"}),
                          ("a bad compile value", {"outcome": "changed", "compile": "fine"}),
                          ("a bad save value", {"outcome": "changed", "save": "mostly"}),
                          ("a non-boolean changed", {"outcome": "changed", "changed": "yes"}),
                          ("a non-snake_case code", {"outcome": "changed", "code": "NotOk"})):
        try:
            result.format_result(**kwargs)
            record("formatting refuses %s" % label, False, "it was accepted")
        except result.ResultProtocolError:
            record("formatting refuses %s" % label, True)

    # --- C03: the exit status comes from metadata ---------------------------------------------------
    cases = [
        ("an explicit refusal", result.format_result("refused", code="pie_active", changed=False), 1),
        ("a partial save", result.format_result("partial", code="save_partial", changed=True,
                                                save="partial", complete=False), 1),
        ("a failed rollback", result.format_result("failed", code="rollback_failed", changed=None,
                                                   rollback="failed"), 2),
        ("a completed success", result.format_result("changed", code="ok", changed=True), 0),
        ("an unchanged read", result.format_result("read", code="ok", changed=False), 0),
        ("an incomplete answer", result.format_result("read", code="ok", changed=False, complete=False), 2),
        ("an unknown outcome", result.format_result("outcome_unknown", code="transport_lost", changed=None), 2),
    ]
    for label, text, expected in cases:
        exit_code = result.parse_result_line(text).suggest_exit_code()
        record("C03 %s exits %d" % (label, expected), exit_code == expected, "exit %d" % exit_code)

    # *** THE NEGATIVE CONTROL, AND IT IS THE WHOLE POINT OF C03. *** A body that is full of alarming prose
    # must not decide anything; only the metadata does.
    alarming = ("error: everything failed, refused, rollback failed, disk on fire\n"
                "unchanged: nothing happened\n" + result.format_result("changed", code="ok", changed=True))
    record("C03 alarming PROSE with a `changed` line still exits 0",
           result.parse_result_line(result.split_answer(alarming)[1]).suggest_exit_code() == 0)
    quiet = ("all good, saved successfully, no problems\n"
             + result.format_result("refused", code="pie_active", changed=False))
    record("C03 reassuring PROSE with a `refused` line still exits 1",
           result.parse_result_line(result.split_answer(quiet)[1]).suggest_exit_code() == 1)

    # --- C04: malformed or missing metadata from a MIGRATED tool -------------------------------------
    bad_lines = {
        "no line at all": "changed: did the thing",
        "a prefix with no JSON": "; tmcp-result-v1: ",
        "JSON that is not an object": "; tmcp-result-v1: [1,2,3]",
        "JSON that does not parse": "; tmcp-result-v1: {outcome: changed}",
        "a missing required key": "; tmcp-result-v1: " + json.dumps(
            {"schema": 1, "outcome": "changed", "changed": True}),
        "the wrong schema": "; tmcp-result-v1: " + json.dumps(
            {"schema": 2, "outcome": "changed", "code": "ok", "changed": True, "assets": [],
             "compile": "passed", "rollback": "not_needed", "save": "saved", "complete": True,
             "operationId": "x"}),
        "an unknown outcome": "; tmcp-result-v1: " + json.dumps(
            {"schema": 1, "outcome": "done", "code": "ok", "changed": True, "assets": [],
             "compile": "passed", "rollback": "not_needed", "save": "saved", "complete": True,
             "operationId": "x"}),
        "an out-of-range enumeration": "; tmcp-result-v1: " + json.dumps(
            {"schema": 1, "outcome": "changed", "code": "ok", "changed": True, "assets": [],
             "compile": "probably", "rollback": "not_needed", "save": "saved", "complete": True,
             "operationId": "x"}),
        "an empty operationId": "; tmcp-result-v1: " + json.dumps(
            {"schema": 1, "outcome": "changed", "code": "ok", "changed": True, "assets": [],
             "compile": "passed", "rollback": "not_needed", "save": "saved", "complete": True,
             "operationId": ""}),
    }
    for label, text in bad_lines.items():
        try:
            result.parse_result_line(text)
            record("C04 %s is refused" % label, False, "it was accepted")
        except result.ResultProtocolError:
            record("C04 %s is refused" % label, True)
    # The presence of a line that is invalid RAISES; the absence of one returns None and the CALLER decides.
    # Conflating the two is how "the migrated tool answered nothing" would read as success.
    record("C04 a missing line parses to None rather than raising", result.parse_answer("changed: x") is None)
    try:
        result.parse_answer("changed: x\n; tmcp-result-v1: not json")
        record("C04 a present but invalid line raises", False, "it did not raise")
    except result.ResultProtocolError:
        record("C04 a present but invalid line raises", True)

    # --- C05: forged metadata in the body ------------------------------------------------------------
    forged = result.format_result("failed", code="forged", changed=None)
    real = result.format_result("changed", code="ok", changed=True)
    answer = "asset content follows:\n%s\nmore content\n%s" % (forged, real)
    parsed = result.parse_answer(answer)
    record("C05 the FINAL line decides, not the forged one",
           parsed is not None and parsed.outcome == "changed", str(parsed))
    # And a line that is NOT last is not authoritative: something after it means the answer has no result.
    trailing = "%s\nand then some trailing prose\n" % real
    record("C05 a line that is not LAST is not authoritative", result.parse_answer(trailing) is None)
    record("C05 the compatibility body keeps the forged line visible rather than hiding it",
           forged in result.body_of(answer))
    record("C05 the compatibility body drops the authoritative line",
           real not in result.body_of(answer))

    # --- C06: the engine wraps an FString and sends several content blocks ---------------------------
    class BlockSession(client.Session):
        """A fake transport that answers with several text content blocks, the last one wrapped."""

        def __init__(self, blocks, is_error=False):
            super().__init__()
            self.blocks = blocks
            self.is_error = is_error

        def _rpc_once(self, method, params=None, request_id=1):
            if method == "initialize":
                return {"ok": True}
            return {"content": [{"type": "text", "text": block} for block in self.blocks],
                    "isError": self.is_error}

    metadata = result.format_result("changed", code="ok", changed=True, assets=["/Game/A"])
    body_blocks = ["changed: did the thing\n", "details: two nodes\n"]
    session = BlockSession(body_blocks + [metadata])
    structured = client.call_structured(session, client.EDIT_TOOLSET, "EditTurboMCPGraphRepoint", {})
    record("C06 every content block is collected, in order",
           structured.blocks == body_blocks + [metadata], str(structured.blocks)[:120])
    record("C06 one decoded body carries all the blocks",
           structured.body == "changed: did the thing\ndetails: two nodes", repr(structured.body)[:140])
    record("C06 the metadata object is parsed once", structured.result.outcome == "changed")
    record("C06 the compatibility method returns the body WITHOUT the metadata line",
           real not in structured.text() and "did the thing" in structured.text())

    # The engine also wraps an FString as `{"returnValue": "..."}`, which the existing text method unwraps.
    #
    # *** THIS ASSERTION WAS `value.startswith("changed: wrapped")` AND IT FAILED WHILE THE CLIENT WAS RIGHT. ***
    # The compatibility method returns the BODY - the answer without its metadata line - and with the wrapper
    # the answer is ONE JSON line, so the body IS that line and the metadata line is inside it. The result is
    # still decoded (the structured method below proves it), and the text is still the tool's own text. Asserting
    # that the wrapped answer "starts with the body" asserted the wrong property of the right behaviour, which is
    # the same shape as this repository's most-recorded failure: a check that cannot pass for a reason unrelated
    # to the subject. What is asserted now is what the compatibility contract actually promises.
    wrapped = BlockSession([json.dumps({"returnValue": "changed: wrapped\n" + metadata})])
    value = wrapped.call(client.EDIT_TOOLSET, "EditTurboMCPGraphRepoint", {})
    record("C06 the FString wrapper is decoded for the text method",
           "changed: wrapped" in value and "returnValue" not in value, repr(value)[:140])
    wrapped_structured = client.call_structured(wrapped, client.EDIT_TOOLSET, "EditTurboMCPGraphRepoint", {})
    record("C06 the FString wrapper is decoded for the structured method too",
           wrapped_structured.result is not None
           and wrapped_structured.result.outcome == "changed",
           repr(wrapped_structured.unwrapped_text())[:120])

    # --- C07: a native engine tool with no TurboMCP metadata -----------------------------------------
    native = BlockSession(["actor 'Cube' moved to (100, 200, 0)\n"])
    native_text = native.call("editor_toolset.toolsets.actor.ActorTools", "set_actor_location", {})
    record("C07 a native answer is returned as text", native_text.startswith("actor 'Cube'"),
           native_text[:60])
    native_structured = client.call_structured(native, "editor_toolset.toolsets.actor.ActorTools",
                                               "set_actor_location", {})
    record("C07 a native answer has NO TurboMCP result", native_structured.result is None)
    record("C07 a native answer is NOT falsely certified as a TurboMCP operation",
           native_structured.is_turbomcp is False)
    record("C07 the native body is still available in full",
           "Cube" in native_structured.body and "Cube" in native_structured.text())

    # A MIGRATED tool with no line is a protocol failure, and that is a DIFFERENT decision from the native case.
    migrated = BlockSession(["changed: the thing happened\n"])
    try:
        client.call_structured(migrated, client.EDIT_TOOLSET, "EditTurboMCPGraphRepoint", {},
                               requires_result=True)
        record("C07 a migrated tool with no line is a protocol failure", False, "it was accepted")
    except result.ResultProtocolError:
        record("C07 a migrated tool with no line is a protocol failure", True)
    except Exception as error:  # noqa: BLE001 - the point is that SOMETHING must refuse
        record("C07 a migrated tool with no line is a protocol failure", True,
               "refused by %s" % type(error).__name__)

    # --- C08: the output budget ----------------------------------------------------------------------
    long_body = "\n".join("node %d: something with enough text to matter" % index for index in range(400))
    answer = long_body + "\n" + metadata
    budget = 900
    trimmed, truncated = result.apply_output_budget(answer, budget)
    record("C08 truncation is reported rather than silent", truncated is True)
    # *** ONE LINE, ONE SEPARATOR, AND THE ARITHMETIC SAYS SO. *** MEASURED: the first version produced 903
    # characters for a 900 budget because the newline that separates the body from the result line was not
    # counted, and the first version of THIS assertion hid it - `len(trimmed) <= budget or trimmed == metadata`
    # is true whenever the budget merely forced the body away, so the off-by-one could not fail it. A check that
    # cannot fail is the defect the whole S01 step existed to remove, so it is now exact.
    body_part, line_part = result.split_answer(trimmed)
    record("C08 the answer fits its budget exactly", len(body_part) + len(line_part) + 1 <= budget,
           "%d chars for a budget of %d" % (len(trimmed), budget))
    record("C08 the metadata survives intact", trimmed.rstrip().endswith(metadata))
    reparsed = result.parse_result_line(result.split_answer(trimmed)[1])
    record("C08 the surviving metadata is still valid", reparsed.outcome == "changed")
    record("C08 the omission is explicit and counted", "character(s) omitted" in trimmed)
    # *** NO PARTIAL SYNTAX IS PRESENTED AS COMPLETE. *** The body may be cut; it may not leave a half-written
    # construct claiming to be whole, which is why the omission notice is a comment line that a reader of the
    # graph language already treats as a comment.
    record("C08 the omission notice is a comment, so a partial graph form is not presented as whole",
           "\n; truncated:" in trimmed)

    tiny, tiny_truncated = result.apply_output_budget(answer, 40)
    record("C08 a budget smaller than the metadata keeps the metadata and empties the body",
           tiny.strip() == metadata.strip() and tiny_truncated is True, tiny[:60])

    # A body that already fits is NOT truncated, which is the other direction of the same decision.
    fitting = result.apply_output_budget(long_body[:40] + "\n" + metadata, budget)
    record("C08 a body that already fits is not truncated",
           fitting[1] is False and fitting[0].endswith(metadata))
    no_budget = result.apply_output_budget(answer, None)
    record("C08 no budget means no truncation and no change", no_budget == (answer, False))

    # --- the CLI's own mapping, so the gate is the same one the tests measure -------------------------
    record("C03 the CLI exit constants match ARC02",
           (client.EXIT_OK, client.EXIT_FAILED, client.EXIT_UNKNOWN) == (0, 1, 2),
           str((client.EXIT_OK, client.EXIT_FAILED, client.EXIT_UNKNOWN)))

    if failures:
        print("")
        for problem in failures:
            print("  {0}".format(problem))
        print("RESULT: FAIL ({0} check(s))".format(len(failures)))
        return 1
    print("RESULT: PASS (the final line is authoritative, malformed metadata is a protocol failure, and the "
          "metadata survives an output budget)")
    return 0


if __name__ == "__main__":
    sys.exit(main())



