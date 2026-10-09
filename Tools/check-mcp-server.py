"""Probe the engine's MCP server and report whether TurboMCP registered its toolsets.

This is the verification step for the objective: it proves the plugin loaded and
its tools are visible to an agent, rather than assuming it from a successful
build.

Why Python and not curl: this machine's Windows Schannel stack fails inside the
DSH sandbox (SEC_E_NO_CREDENTIALS), so every Windows-native HTTPS client fails.
Python carries its own TLS stack. Plain HTTP to localhost would work either way,
but Python is what is known to work here, and it can do real JSON-RPC.

Usage:
    python scripts/check-mcp-server.py
    python scripts/check-mcp-server.py --url http://localhost:8000/mcp
"""

import argparse
import json
import sys
import urllib.error
import urllib.request

# The engine advertises this as its latest supported version
# (ModelContextProtocol.h: ProtocolVersion).
PROTOCOL_VERSION = "2025-11-25"
CLIENT_NAME = "turbomcp-build-check"
CLIENT_VERSION = "1.0"

# list_toolsets is an MCP-level meta-tool. When bEnableToolSearch is on, which is
# the default, the client sees only list_toolsets, describe_toolset and
# call_tool, and every other toolset tool is reached through call_tool.
LIST_TOOLSETS = "list_toolsets"


class RpcError(Exception):
    """A JSON-RPC level error reported by the server."""


def rpc(url, method, params=None, request_id=1, session_id=None):
    """Send one JSON-RPC request and return its result.

    Returns the parsed ``result`` object, or raises RpcError with the server's
    own message so a failure is never reported as a mystery.
    """
    payload = {"jsonrpc": "2.0", "id": request_id, "method": method}
    if params is not None:
        payload["params"] = params

    headers = {
        "Content-Type": "application/json",
        # The MCP streamable HTTP transport expects the client to accept both.
        "Accept": "application/json, text/event-stream",
    }
    if session_id:
        headers["Mcp-Session-Id"] = session_id

    request = urllib.request.Request(
        url, data=json.dumps(payload).encode("utf-8"), headers=headers, method="POST"
    )

    with urllib.request.urlopen(request, timeout=30) as response:
        raw = response.read().decode("utf-8", "replace")
        new_session = response.headers.get("Mcp-Session-Id")

    if not raw.strip():
        raise RpcError("server returned an empty body")

    # The transport may answer as Server-Sent Events, in which case the JSON is
    # on the lines that start with "data:".
    body = raw
    if raw.lstrip().startswith("event:") or "data:" in raw[:200]:
        chunks = [
            line[len("data:"):].strip()
            for line in raw.splitlines()
            if line.startswith("data:")
        ]
        if not chunks:
            raise RpcError("SSE response with no data lines: %r" % raw[:200])
        body = chunks[-1]

    message = json.loads(body)

    if "error" in message:
        err = message["error"]
        raise RpcError("server error %s: %s" % (err.get("code"), err.get("message")))

    return message.get("result"), new_session


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--url", default="http://localhost:8000/mcp", help="MCP endpoint"
    )
    args = parser.parse_args()

    print("probing %s" % args.url)

    # 1. Handshake. A failure here means the server is not running at all, which
    #    is a different problem from the plugin failing to register.
    try:
        result, session = rpc(
            args.url,
            "initialize",
            {
                "protocolVersion": PROTOCOL_VERSION,
                "capabilities": {},
                "clientInfo": {"name": CLIENT_NAME, "version": CLIENT_VERSION},
            },
        )
    except urllib.error.URLError as exc:
        print("FAIL  cannot reach the server: %s" % exc)
        print()
        print("The server is probably not running. Check:")
        print("  - Project Settings > Model Context Protocol > Auto Start Server")
        print("  - Config/DefaultEditorPerProjectUserSettings.ini has bAutoStartServer=True")
        print("  - no other editor already holds port 8000")
        return 2
    except RpcError as exc:
        print("FAIL  initialize rejected: %s" % exc)
        return 2

    server_info = (result or {}).get("serverInfo", {})
    print(
        "  server: %s %s   protocol: %s"
        % (
            server_info.get("name", "?"),
            server_info.get("version", "?"),
            (result or {}).get("protocolVersion", "?"),
        )
    )
    if session:
        print("  session: %s" % session)

    # 2. tools/list. With bEnableToolSearch on this returns only the search
    #    entry points, which is expected and is what an agent sees first.
    try:
        listed, _ = rpc(args.url, "tools/list", {}, request_id=2, session_id=session)
    except RpcError as exc:
        print("FAIL  tools/list: %s" % exc)
        return 2

    tools = (listed or {}).get("tools", [])
    names = sorted(t.get("name", "?") for t in tools)
    print("  tools/list: %d tool(s)" % len(names))
    for name in names:
        print("      %s" % name)

    # 3. The real test: list the registered toolsets and look for TurboMCP.
    #
    #    list_toolsets is an MCP-level meta-tool, so it is called with tools/call
    #    taking the bare name, NOT through call_tool. Going through call_tool
    #    needs a toolset name for the meta-tool itself, and guessing it fails.
    #
    #    Note also that call_tool's own `arguments` is a JSON OBJECT, not a
    #    string, which this file got wrong before it was corrected against the
    #    live server.
    print()
    print("TurboMCP toolsets:")

    if LIST_TOOLSETS not in names:
        # Tool search off: every tool is registered natively, so TurboMCP's own
        # tools appear directly in tools/list.
        print("  tool search is OFF, so toolsets are not wrapped behind call_tool.")
        turbo = [n for n in names if "TurboMCP" in n]
        if turbo:
            print("  PASS  found %d TurboMCP tool(s) in tools/list:" % len(turbo))
            for name in turbo:
                print("      %s" % name)
            return 0
        print("  FAIL  no TurboMCP tool is listed at all.")
        return 1

    try:
        result, _ = rpc(
            args.url,
            "tools/call",
            {"name": LIST_TOOLSETS, "arguments": {}},
            request_id=3,
            session_id=session,
        )
    except RpcError as exc:
        print("  could not list toolsets: %s" % exc)
        return 2

    # The catalog arrives as content blocks whose text lists toolsets as
    # "- Name: description", and a description can run to many wrapped lines. Only
    # the names are wanted here, so each entry is cut at its first line and the
    # continuation lines are dropped: the point of this check is whether the four
    # toolsets are registered, not what they say about themselves.
    if isinstance(result, dict):
        blocks = result.get("content", [])
        catalog = "\n".join(b.get("text", "") for b in blocks if isinstance(b, dict))
    else:
        catalog = str(result)

    entries = [l.strip() for l in catalog.splitlines() if l.strip().startswith("- ")]
    for entry in entries:
        # Keep the toolset name; drop the description after the first colon.
        name = entry[2:].split(":", 1)[0].strip()
        print("      %s" % name)

    print()
    turbo = [e for e in entries if "TurboMCP" in e]
    if len(turbo) >= 4:
        print("PASS  all %d TurboMCP toolsets are registered, of %d toolsets served."
              % (len(turbo), len(entries)))
        return 0
    if turbo:
        print("FAIL  only %d of the four TurboMCP toolsets are registered:" % len(turbo))
        for entry in turbo:
            print("      %s" % entry[2:].split(":", 1)[0].strip())
        print("      Check the editor Output Log for:")
        print("        LogTurboMCP: Warning: Toolset '...' did not register.")
        return 1

    print("FAIL  the server answered but lists no TurboMCP toolset, of %d served." % len(entries))
    print("      The plugin is not registered. Check the editor Output Log for:")
    print("        LogTurboMCP: Warning: Toolset '...' did not register.")
    return 1


if __name__ == "__main__":
    sys.exit(main())
