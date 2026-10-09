"""Call a TurboMCP tool through the running MCP server and print its answer.

The fast verify loop: build, restart the editor, then run this.

Usage:
    python Tools/tmcp.py                                   # start, describe, index
    python Tools/tmcp.py --tool ReadTurboMCPStatus
    python Tools/tmcp.py --tool ReadTurboMCPToolHelp --args '{"ToolsetName":"TurboMCP.TurboMCPReadToolset","ToolName":"ReadTurboMCPToolIndex"}'
    python Tools/tmcp.py --raw --tool ReadTurboMCPToolIndex

Exits non-zero when the server is unreachable, the tool errors, or the answer
looks like an error line, so it can be used as a check rather than read by eye.
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


class Session:
    def __init__(self):
        self.id = None

    def rpc(self, method, params=None, request_id=1):
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

    def call(self, toolset, tool, args=None):
        """Call one TurboMCP tool and return its text.

        `arguments` is sent as a JSON object, not as a string. That was verified
        against the running server: sending a string makes the engine answer
        "input params Json is empty", because it feeds the value straight into the
        function's parameter struct.
        """
        result = self.rpc(
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
        )
        if not isinstance(result, dict) or "content" not in result:
            return "(no content) %s" % json.dumps(result)[:400]

        text = result["content"][0].get("text", "")
        if result.get("isError"):
            return "!! isError: " + text

        # Engine tools that return FString arrive wrapped.
        try:
            parsed = json.loads(text)
            if isinstance(parsed, dict) and "returnValue" in parsed:
                return parsed["returnValue"]
        except (ValueError, TypeError):
            pass
        return text

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
    args = parser.parse_args()

    session = Session()
    try:
        session.connect()
    except urllib.error.URLError as exc:
        print("FAIL cannot reach %s: %s" % (URL, exc))
        print("Is the editor running with the MCP server up?")
        return 2

    calls = (
        [(args.tool, args.toolset, json.loads(args.args))]
        if args.tool
        else [
            ("ReadTurboMCPStatus", None, {}),
            ("ReadTurboMCPToolIndex", None, {}),
        ]
    )

    failed = False
    for tool, toolset, tool_args in calls:
        toolset = toolset or toolset_for(tool)
        print("=" * 74)
        print("CALL %s / %s  args=%s" % (toolset, tool, json.dumps(tool_args)))
        print("-" * 74)
        try:
            text = session.call(toolset, tool, tool_args)
        except urllib.error.HTTPError as exc:
            print("HTTP %s: %s" % (exc.code, exc.read().decode()[:400]))
            failed = True
            continue
        except RuntimeError as exc:
            print("server error: %s" % exc)
            failed = True
            continue

        print(text)
        if not args.no_error_check and re.search(r"(^|\n)\s*(error|!!)", text or ""):
            failed = True
        print()

    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
