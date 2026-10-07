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

# The engine's own tool-search entry point. TurboMCP tools are reached through
# call_tool when bEnableToolSearch is on, which is the default.
CALL_TOOL = "call_tool"
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

    # 3. The real test: ask the server to list TurboMCP's own toolsets.
    #    Tool search wraps every toolset behind call_tool, so this is how an
    #    agent discovers them.
    print()
    print("TurboMCP toolsets:")

    if LIST_TOOLSETS not in names:
        print("  tool search appears to be OFF (list_toolsets is not offered).")
        print("  TurboMCP toolsets would then appear directly in tools/list above.")
        turbo = [n for n in names if "TurboMCP" in n]
        if turbo:
            print("  PASS  found %d TurboMCP tool(s):" % len(turbo))
            for name in turbo:
                print("      %s" % name)
            return 0
        print("  FAIL  no TurboMCP tool is listed at all.")
        return 1

    try:
        result, _ = rpc(
            args.url,
            CALL_TOOL,
            {"toolset_name": "ToolsetRegistry", "tool_name": LIST_TOOLSETS,
             "arguments": "{}"},
            request_id=3,
            session_id=session,
        )
    except RpcError:
        # The tool-set listing toolset name differs between engine versions.
        # Fall back to reading it straight from tools/call on the MCP level.
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

    text = json.dumps(result, indent=2) if not isinstance(result, str) else result
    print(text)

    turbo_hits = text.count("TurboMCP")
    print()
    if turbo_hits:
        print("PASS  the server knows about TurboMCP (%d mention(s))." % turbo_hits)
        return 0

    print("FAIL  the server answered but never mentions TurboMCP.")
    print("      The plugin is not registered. Check the editor Output Log for:")
    print("        LogTurboMCP: Warning: Toolset '...' did not register.")
    return 1


if __name__ == "__main__":
    sys.exit(main())
