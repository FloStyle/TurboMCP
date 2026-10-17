"""Call a TurboMCP tool through the running MCP server and print its answer.

The fast verify loop: build, restart the editor, then run this.

Usage:
    python Tools/tmcp.py                                   # start, describe, index
    python Tools/tmcp.py --tool ReadTurboMCPStatus
    python Tools/tmcp.py --tool ReadTurboMCPToolHelp --args '{"ToolsetName":"TurboMCP.TurboMCPReadToolset","ToolName":"ReadTurboMCPToolIndex"}'
    python Tools/tmcp.py --raw --tool ReadTurboMCPToolIndex

Exits non-zero when the server is unreachable, the tool errors, or the answer
looks like an error line, so it can be used as a check rather than read by eye.

Exit codes: 0 completed, 1 refusal/failure, 2 unknown outcome or protocol failure.

*** A MUTATION IS NEVER REPLAYED AFTER A LOST RESPONSE. ***

Release finding R02 was confirmed against this file's own control flow and with a mocked-I/O probe
(`Release/Audit/audit_retry_probe.py`, `DEFECT_CONFIRMED`): `rpc` caught every exception, cleared the session
id, reconnected, and repeated the original request - including a request that had already changed an asset
and whose response was merely lost. One lost response produced TWO effects. Repeating an edit is not a
recovery; it is a second edit, and for a tool like `EditTurboMCPWidgetPreview` it can be a second execution of
project code.

So the retry is now split by what the request can do:

  * A READ/Look request may reconnect and be repeated. A repeat of a pure read cannot double an effect.
  * A MUTATION request may reconnect, but the original request is NEVER repeated. The caller receives
    `UnknownOutcomeError` and must find the outcome by independent means: read the asset back, list the dirty
    packages, or inspect the operation id the result line carries. The CLI exits 2, because "the outcome is
    not known" is neither success nor failure.

The mutation list is deliberately STATIC rather than derived from the server. Deriving it would require a
call to the server, and the classification has to be correct precisely when the server is not answering.
`--mutates` overrides it for a tool whose name this file does not know; a name it does not recognise is
treated as a mutation, because guessing "read" about an unknown tool is the wrong way to be wrong.
"""

import argparse
import json
import re
import sys
import urllib.error
import urllib.request

URL = "http://localhost:8000/mcp"
PROTOCOL_VERSION = "2025-11-25"

# The toolset that owns the agent help tools. TurboMCP's toolsets are reached
# through call_tool, so every call needs the real toolset name.
READ_TOOLSET = "TurboMCP.TurboMCPReadToolset"
LOOK_TOOLSET = "TurboMCP.TurboMCPLookToolset"
PREVIEW_TOOLSET = "TurboMCP.TurboMCPPreviewToolset"
EDIT_TOOLSET = "TurboMCP.TurboMCPEditToolset"

#: Exit code for "the outcome could not be established". Distinct from 1, which means a refusal or a
#: failure that WAS established.
EXIT_UNKNOWN = 2

#: Tool-name prefixes that can change something. A `Preview` tool is listed as a mutation on purpose: it is
#: documented as non-mutating, but release finding R05 records that four toolset classes do not enforce
#: read-only behaviour, so the safe assumption about a preview whose implementation has not been read is that
#: repeating it is not obviously free.
MUTATION_PREFIXES = (
    "Edit",
    "Preview",
)

#: Look through these verbs before treating an unknown name as a read.
READ_VERBS = ("Read", "Look", "List", "Get", "Find", "Search", "Describe")

#: ARC02's exit codes, imported from the contract module so there is ONE definition of each. Kept as module
#: attributes too, because callers and tests read them from the client.
EXIT_OK = 0
EXIT_FAILED = 1
EXIT_UNKNOWN = 2


def _load_result_module():
    """The result contract, imported by path so the client needs no package layout.

    A FALLBACK IS DELIBERATELY ABSENT. If `tmcp_result.py` cannot be imported then no answer can be classified,
    and a client that silently degrades to text would certify an outcome it cannot read - which is the defect
    the contract exists to remove.
    """
    import importlib.util
    import os

    path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "tmcp_result.py")
    spec = importlib.util.spec_from_file_location("tmcp_result", path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


tmcp_result = _load_result_module()


class StructuredAnswer:
    """One tool answer, decoded: every text block in order, the body, and the TurboMCP result if any.

    `blocks` is kept because ARC02 requires the structured method to extract ALL MCP text blocks IN ORDER - the
    engine splits a long answer across blocks, and a client that reads only the first one sees a partial answer.
    """

    def __init__(self, blocks, result, is_error=False, tool="", toolset=""):
        self.blocks = list(blocks)
        self.result = result
        self.is_error = bool(is_error)
        self.tool = tool
        self.toolset = toolset

    @property
    def is_turbomcp(self):
        """Whether this answer carries a TurboMCP result line.

        A native engine tool never does, and that is what keeps the two adapters separate (C07). This is a
        statement about what the answer CONTAINS, not a guess about which toolset name was used.
        """
        return self.result is not None

    @property
    def body(self):
        """The answer without the metadata line - the compatibility view.

        *** IT IS COMPUTED FROM `unwrapped_text()`, NOT FROM `text()`, AND THAT WAS A REAL DEFECT. *** MEASURED:
        with `text()`, an FString-wrapped answer kept its `{"returnValue": ...}` wrapper in the body, because the
        wrapper and the metadata line are the same single JSON line - so there was no line to strip and the whole
        wrapper came back as the "body". Every wrapped migrated tool would have shown its answer as raw JSON to
        the compatibility method while the structured method decoded it correctly, which is the kind of split
        personality this contract exists to remove.
        """
        return tmcp_result.body_of(self.unwrapped_text())

    def text(self):
        """Every block joined, which is what a reader sees."""
        return "".join(self.blocks)

    def unwrapped_text(self):
        """The joined text with the engine's return-value wrapper removed, when present.

        *** THE WRAPPER ARRIVES IN TWO SHAPES AND BOTH MUST BE HANDLED. *** The engine supplies an FString tool's
        answer as JSON around the string, and it does so either as an object (`{"returnValue": "..."}`) or as a
        JSON-ENCODED STRING OF THAT OBJECT (`"{\\"returnValue\\": \\"...\\"}"`). MEASURED while writing this:
        only the first shape was unwrapped, so the second reached the result parser as a quoted blob and every
        migrated FString tool would have looked like a protocol failure. Both shapes decode to the same answer, so
        this tries the object form and then the string form, and stops as soon as the text stops changing.
        """
        text = self.text()
        for _ in range(3):
            try:
                parsed = json.loads(text)
            except (ValueError, TypeError):
                return text
            if isinstance(parsed, dict) and "returnValue" in parsed:
                text = parsed["returnValue"] if isinstance(parsed["returnValue"], str) else json.dumps(
                    parsed["returnValue"])
                continue
            if isinstance(parsed, str) and parsed.strip() != text.strip():
                # A JSON string that decodes to DIFFERENT text is a wrapper layer. A string that decodes to
                # itself would loop, and the guard above stops that.
                text = parsed
                continue
            return text
        return text

    def require_result(self):
        """The result, or a protocol failure - for a tool that is KNOWN to be migrated.

        The distinction this preserves is the whole of C04 versus C07: an answer with no line from a migrated
        tool is a protocol failure (exit 2), while the same absence from a native tool is normal.
        """
        if self.result is None:
            raise tmcp_result.ResultProtocolError(
                "the TurboMCP tool '%s' answered without a %s line, so its outcome cannot be established"
                % (self.tool, tmcp_result.PREFIX))
        return self.result

    def suggest_exit_code(self):
        """ARC02's exit code: 0 completed, 1 refusal/failure/partial, 2 unknown or protocol failure."""
        if self.result is None:
            return EXIT_UNKNOWN
        return self.result.suggest_exit_code()


class UnknownOutcomeError(RuntimeError):
    """A mutation's request may or may not have been applied. Do not repeat it.

    Attributes:
        tool: the tool that was called.
        transport: the transport failure that made the outcome ambiguous.
    """

    def __init__(self, tool, transport):
        super().__init__(
            "the request for '%s' failed after it may have been applied (%s). The outcome is UNKNOWN and the "
            "request was NOT repeated. Establish the outcome independently: read the asset or the dirty "
            "package list, or inspect the operationId in the result line if one was recorded. Do not simply "
            "call the tool again." % (tool, transport)
        )
        self.tool = tool
        self.transport = transport


def is_mutation(tool, override=None):
    """Whether repeating this tool's request could apply a second effect.

    `override` is the CLI's `--mutates` flag: True forces mutation, False forces read, None uses the name.
    An unrecognised name is a MUTATION, because the cost of wrongly replaying a write is higher than the cost
    of wrongly refusing to replay a read.
    """
    if override is not None:
        return bool(override)
    if tool in ("call_tool", "initialize", "tools/list", "tools/call"):
        # The MCP envelope itself carries no edit; the tool NAMED INSIDE its arguments does.
        return False
    if tool.startswith(MUTATION_PREFIXES):
        return True
    if tool.startswith(READ_VERBS):
        return False
    return True


def tier_of_return(value):
    """Reserved: whether one JSON-RPC error is worth ONE reconnect attempt.

    A JSON-RPC error whose code says the request was malformed or the method is wrong will not be fixed by
    reconnecting; only a transport-level failure - a dead or restarted editor - is. This is not used by the
    retry above, which reconnects on any exception and then decides what to do by MUTATION, not by error code.
    It is kept because the distinction is the next thing to tighten if a malformed request ever starts a
    reconnect it does not deserve, and because deleting it would lose the reasoning.
    """
    if isinstance(value, dict):
        code = value.get("code")
        if isinstance(code, int) and -32700 <= code <= -32600:
            return False
    return True


class Session:
    def __init__(self):
        self.id = None
        # TRUE WHILE A RE-CONNECT IS IN PROGRESS, SO A FAILURE INSIDE IT PROPAGATES INSTEAD OF RE-ENTERING THE
        # RETRY. `connect()` calls `rpc()` and `rpc()` calls `connect()` on failure, so without this flag an
        # unreachable server recurses - each level waiting out a full request timeout - which is what made
        # `check-probe-graphs` run past 600 seconds.
        self._connecting = False

    def rpc(self, method, params=None, request_id=1, mutates=None):
        """One JSON-RPC call, with ONE reconnect - and NO replay when the request can mutate.

        The reconnect exists because a fixture rebuild stops the editor and starts a new one with a new
        `Mcp-Session-Id`, while a suite still holds the old one; measured, that is the cause of a class of
        failures that cost six rounds. The reconnect is exactly one, deliberately: a loop here would turn a
        genuinely dead editor into a run that appears to hang.

        `mutates` decides whether the original request may be repeated after the reconnect. See the module
        docstring: a repeated mutation is a second mutation, not a recovery. `None` means "decide from the
        tool name inside `params`".
        """
        tool = None
        if isinstance(params, dict):
            arguments = params.get("arguments")
            if isinstance(arguments, dict):
                tool = arguments.get("tool_name")
            if not tool:
                tool = params.get("name")
        mutation = is_mutation(tool or method, mutates)

        try:
            return self._rpc_once(method, params, request_id)
        except Exception as transport:
            if self._connecting:
                raise
            # *** A MUTATION IS NOT REPEATED, AND THIS IS THE WHOLE OF FINDING R02. ***
            #
            # The reconnect still happens, because leaving the session id pointing at an editor that no
            # longer exists would break every later call. What does NOT happen is the second `_rpc_once`.
            if mutation:
                self.id = None
                self._connecting = True
                try:
                    self.connect()
                except Exception:
                    # The reconnect failing is not a new outcome; the request's outcome was already unknown.
                    pass
                finally:
                    self._connecting = False
                raise UnknownOutcomeError(tool or method, "%s: %s" % (type(transport).__name__, transport))

            # A read may be repeated: a repeated read cannot double an effect.
            self.id = None
            self._connecting = True
            try:
                self.connect()
            finally:
                self._connecting = False
            return self._rpc_once(method, params, request_id)

    def _rpc_once(self, method, params=None, request_id=1):
        payload = {"jsonrpc": "2.0", "id": request_id, "method": method}
        if params is not None:
            payload["params"] = params

        headers = {
            "Content-Type": "application/json",
            "Accept": "application/json, text/event-stream",
        }
        if self.id:
            headers["Mcp-Session-Id"] = self.id

        req = urllib.request.Request(
            URL, data=json.dumps(payload).encode(), headers=headers, method="POST"
        )
        with urllib.request.urlopen(req, timeout=120) as resp:
            raw = resp.read().decode("utf-8", "replace")
            self.id = resp.headers.get("Mcp-Session-Id") or self.id

        body = raw
        if raw.lstrip().startswith("event:") or "\ndata:" in raw:
            chunks = [l[5:].strip() for l in raw.splitlines() if l.startswith("data:")]
            if chunks:
                body = chunks[-1]

        msg = json.loads(body)
        if "error" in msg:
            raise RuntimeError(json.dumps(msg["error"]))
        return msg.get("result")

    def call(self, toolset, tool, args=None, mutates=None):
        """Call one TurboMCP tool and return its text - the COMPATIBILITY method.

        `arguments` is sent as a JSON object, not as a string. That was verified against the running server:
        sending a string makes the engine answer "input params Json is empty", because it feeds the value
        straight into the function's parameter struct.

        This method returns the BODY, which for a migrated tool is the answer without its metadata line. ARC02
        requires the human-readable body to survive, and it requires the metadata to be read by the structured
        method rather than inferred from this text.

        Raises:
            UnknownOutcomeError: when a mutating tool's response was lost. The request was not repeated.
        """
        blocks, _ = fetch_blocks(self, toolset, tool, args, mutates)
        answer = StructuredAnswer(blocks, None, tool=tool, toolset=toolset)
        text = answer.unwrapped_text()
        if answer.is_error:
            return "!! isError: " + text
        answer.result = tmcp_result.parse_answer(text)
        return answer.body

    def connect(self):
        r = self.rpc(
            "initialize",
            {
                "protocolVersion": PROTOCOL_VERSION,
                "capabilities": {},
                "clientInfo": {"name": "tmcp", "version": "1"},
            },
        )
        return r


def toolset_for(tool):
    """The toolset that owns a tool, from its name prefix."""
    if tool.startswith("Look"):
        return LOOK_TOOLSET
    if tool.startswith("Preview"):
        return PREVIEW_TOOLSET
    if tool.startswith("Edit"):
        return EDIT_TOOLSET
    return READ_TOOLSET


def fetch_blocks(session, toolset, tool, args=None, mutates=None):
    """The raw text blocks of one answer, in order, plus the transport's error flag.

    Split out from `Session.call` so BOTH the text method and the structured method read the same blocks. ARC02
    requires the structured method to extract every block in order; having two code paths extract them
    differently is how one of them ends up seeing a partial answer.
    """
    result = session.rpc(
        "tools/call",
        {
            "name": "call_tool",
            "arguments": {
                "toolset_name": toolset,
                "tool_name": tool,
                "arguments": args or {},
            },
        },
        2,
        mutates=mutates,
    )
    if not isinstance(result, dict) or "content" not in result:
        # Not an answer at all. The caller gets a single synthetic block so the failure is visible rather than
        # becoming an empty string that a classifier might read as "nothing happened".
        return ["(no content) %s" % json.dumps(result)[:400]], bool(result.get("isError")) if isinstance(result, dict) else False

    blocks = []
    for block in result.get("content") or []:
        if isinstance(block, dict):
            # *** THE BLOCK'S `text` IS TAKEN AS ITS VALUE, NOT AS `str()` OF ITS DICT. ***
            # MEASURED: `str(block.get("text", ""))` was written as `str(block)` by mistake in the first
            # version, which turned every block into the Python repr of the whole content object -
            # `"{'type': 'text', 'text': 'changed: ...'}"` - so a wrapped FString could never be unwrapped and
            # every later parse saw a repr instead of an answer.
            blocks.append(str(block.get("text", "") or ""))
        elif isinstance(block, str):
            blocks.append(block)
    return blocks, bool(result.get("isError"))


def call_structured(session, toolset, tool, args=None, mutates=None, requires_result=None):
    """Call one tool through the structured result method (ARC02).

    `requires_result` answers the question the client cannot answer alone: does this toolset serve a MIGRATED
    tool? A native engine tool legitimately carries no result line (C07), so the default is decided by the
    toolset name - a `TurboMCP.*` toolset is migrated, and anything else is native - and the caller can override
    it when it knows better.
    """
    if requires_result is None:
        requires_result = str(toolset).startswith("TurboMCP.")
    answer = StructuredAnswer(*fetch_blocks(session, toolset, tool, args, mutates),
                              tool=tool, toolset=toolset)
    text = answer.unwrapped_text()
    answer.result = tmcp_result.parse_answer(text)
    if requires_result and answer.result is None:
        raise tmcp_result.ResultProtocolError(
            "the TurboMCP tool '%s' answered without a %s line, so its outcome cannot be established. Its "
            "answer was: %s" % (tool, tmcp_result.PREFIX, text[:200].replace("\n", " | ")))
    return answer


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--tool", help="tool name, for example ReadTurboMCPStatus")
    parser.add_argument("--args", default="{}", help="JSON object of arguments")
    parser.add_argument("--toolset", help="override the toolset")
    parser.add_argument("--raw", action="store_true", help="do not decode returnValue")
    parser.add_argument(
        "--no-error-check",
        action="store_true",
        help="do not treat an 'error:' answer as a failure",
    )
    parser.add_argument(
        "--mutates",
        choices=("yes", "no"),
        help="force the mutation classification for a tool whose name this file does not know. "
             "Default: decide from the name, and treat an unrecognised name as a mutation.",
    )
    args = parser.parse_args()
    mutates = {"yes": True, "no": False, None: None}[args.mutates]

    session = Session()
    try:
        session.connect()
    except urllib.error.URLError as exc:
        print("FAIL cannot reach %s: %s" % (URL, exc))
        print("Is the editor running with the MCP server up?")
        return EXIT_UNKNOWN

    calls = (
        [(args.tool, args.toolset, json.loads(args.args))]
        if args.tool
        else [
            ("ReadTurboMCPStatus", None, {}),
            ("ReadTurboMCPToolIndex", None, {}),
        ]
    )

    worst = EXIT_OK
    for tool, toolset, tool_args in calls:
        toolset = toolset or toolset_for(tool)
        print("=" * 74)
        print("CALL %s / %s  args=%s" % (toolset, tool, json.dumps(tool_args)))
        print("-" * 74)
        try:
            answer = call_structured(session, toolset, tool, tool_args, mutates=mutates,
                                     requires_result=False)
        except UnknownOutcomeError as exc:
            # *** NOT A FAILURE AND NOT A SUCCESS: AN UNKNOWN OUTCOME. ***
            # Printing "error:" here would be wrong: the edit may well have applied. The caller is told
            # exactly what to do instead, and the exit code says the outcome is not established.
            print("outcome_unknown: %s" % exc)
            worst = max(worst, EXIT_UNKNOWN)
            continue
        except tmcp_result.ResultProtocolError as exc:
            # A migrated tool answered with metadata no client can read. That is a PROTOCOL failure, which
            # ARC02 separates from a refusal: exit 2, never 0. Saying "error" here would collapse it into the
            # exit code a caller gets for a tool that refused on purpose, and the two need different responses.
            print("protocol_failure: %s" % exc)
            worst = max(worst, EXIT_UNKNOWN)
            continue
        except urllib.error.HTTPError as exc:
            print("HTTP %s: %s" % (exc.code, exc.read().decode()[:400]))
            worst = max(worst, EXIT_FAILED)
            continue
        except RuntimeError as exc:
            print("server error: %s" % exc)
            worst = max(worst, EXIT_FAILED)
            continue

        text = answer.body if not args.raw else answer.unwrapped_text()
        print(text)
        if answer.result is not None:
            # *** THE METADATA DECIDES, AND THE PROSE DOES NOT. *** C03's whole point: a body full of alarming
            # words with a `changed` line exits 0, and a reassuring body with a `refused` line exits 1.
            worst = max(worst, answer.result.suggest_exit_code())
            if answer.result.outcome not in ("read", "planned", "unchanged", "changed"):
                print("; outcome %s (code %s) -> exit %d"
                      % (answer.result.outcome, answer.result.code, answer.result.suggest_exit_code()))
        elif not args.no_error_check and re.search(r"(^|\n)\s*(error|!!)", text or ""):
            # A NATIVE tool carries no metadata, so its text is all the evidence there is. This is the
            # documented limitation ARC02 records rather than a gap to be closed by pretending the text is
            # metadata: engine-native tools keep their separate adapter.
            worst = max(worst, EXIT_FAILED)
        print()

    return worst


if __name__ == "__main__":
    sys.exit(main())
