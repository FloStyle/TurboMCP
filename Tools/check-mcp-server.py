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
import os
import subprocess
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


def owner_check(expected_project, args_url=""):
    """Return (ok, message) for whether the editor on this port is OURS.

    *** WHY THIS EXISTS: THIS SCRIPT CONNECTED TO WHATEVER ANSWERED ON PORT 8000. ***
    Four separate collisions in this project's record came from exactly that - two processes that both own an
    editor, where the harness's checks passed against SOMEONE ELSE'S project and the suite then failed with a
    message about the capability instead of about the owner. The failure was real and the diagnosis was wrong,
    which cost a round each time.

    *** THE DISCRIMINATOR IS THE PROCESS, NOT THE PROTOCOL. *** The MCP handshake reports a server name and a
    protocol version and nothing that identifies a project, so a session id cannot distinguish "our editor" from
    "another editor on the same port". What CAN is the editor's own command line: `Tools/start-editor.ps1:260`
    builds `$Args = @($Project)` with the ABSOLUTE path, so the process that owns this project has that path in
    its command line and a foreign editor does not.

    AND IT IS DELIBERATELY A SEPARATE REPORT RATHER THAN A SILENT GATE. A caller may have a legitimate reason to
    probe an editor that is not this project's; what must not happen is doing so WITHOUT KNOWING.
    """
    if not expected_project:
        return True, "not checked (no --project given)"
    wanted = os.path.normcase(os.path.abspath(expected_project))
    # `wmic` and `Get-CimInstance` both format CommandLine in ways that made the earlier probe print nothing at
    # all, so this reads the process list through PowerShell and matches on the project's BASE NAME, which is
    # what actually appears whether the path is quoted, forward-slashed or back-slashed.
    wanted_base = os.path.basename(wanted)
    script = (
        "Get-Process UnrealEditor,UnrealEditor-Cmd -ErrorAction SilentlyContinue | "
        "ForEach-Object { $_.Id.ToString() + ' ' + [string]$_.Path }"
    )
    try:
        outp = subprocess.run(
            ["powershell", "-NoProfile", "-Command", "$p = Get-CimInstance Win32_Process -Filter \"Name='UnrealEditor.exe' or Name='UnrealEditor-Cmd.exe'\"; "
             "foreach ($x in $p) { Write-Output ($x.ProcessId.ToString() + '|' + $x.CommandLine) }"],
            capture_output=True, text=True, timeout=30,
        ).stdout
    except Exception as exc:
        return True, "not checked (%s)" % exc

    lines = [ln for ln in outp.splitlines() if ln.strip()]
    if not lines:
        # *** NOTHING LISTENING IS NOT A FOREIGN EDITOR. *** The first version of this check treated "no editor
        # process" as "another project owns the port", which is FALSE for the innocent case - the editor is
        # simply stopped - and would have failed a caller for it. THAT IS THE SAME CLASS OF FALSE POSITIVE THAT
        # HAD TO BE REMOVED FROM THE SUITE-QUALITY AUDIT BEFORE IT COULD BECOME A GATE. So the port is tested
        # instead: the dangerous case is a port that ANSWERS while no editor of ours is running, because then
        # every check below measures somebody else's project.
        try:
            import socket
            from urllib.parse import urlparse
            parsed = urlparse(args_url)
            host = parsed.hostname or "127.0.0.1"
            port = parsed.port or 80
            with socket.create_connection((host, port), timeout=3):
                port_answers = True
        except Exception:
            port_answers = False
        if port_answers:
            return False, ("NOTHING OF OURS IS RUNNING but the port ANSWERS - the server belongs to ANOTHER "
                           "PROJECT or is a stale process, and every check below measures that one")
        return False, "NOTHING IS RUNNING: no editor process and nothing listening on the port"
    mine = [ln for ln in lines if wanted_base.lower() in ln.lower()]
    if mine:
        return True, "%d editor process(es), %d of them on %s" % (len(lines), len(mine), wanted_base)
    return False, ("%d editor process(es) RUNNING BUT NONE HAS %s IN ITS COMMAND LINE - the server on this port "
                    "belongs to ANOTHER PROJECT, and every check below would be measuring that one"
                    % (len(lines), wanted_base))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--url", default="http://localhost:8000/mcp", help="MCP endpoint"
    )
    parser.add_argument(
        "--project", default=os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                          "..", "TestProject", "TurboMCPTest.uproject"),
        help="the .uproject this probe is meant to be talking to, so a FOREIGN editor on the same "
             "port is named instead of silently measured",
    )
    args = parser.parse_args()

    print("probing %s" % args.url)

    owner_ok, owner_why = owner_check(args.project, args.url)
    print("  owner: %s" % owner_why)
    if not owner_ok:
        print()
        # *** THE RESULT LINE REPEATS THE REASON RATHER THAN A GENERIC SENTENCE ABOUT A PORT. *** The first
        # version printed "the editor on this port is not the one this probe was aimed at" WHATEVER went wrong,
        # INCLUDING when nothing was running at all - so a reader would go looking for a foreign editor that
        # did not exist. A FAILURE MESSAGE THAT NAMES THE WRONG CAUSE IS WORSE THAN A VAGUE ONE, because it is
        # confidently wrong: "an invisible refusal reads as a capability", pointed the other way.
        print("RESULT: FAIL (%s)" % owner_why)
        return 3

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
