# Protocol, discovery and results

[Documentation](../README.md)

## Boundary

Unreal owns the MCP transport and ToolsetRegistry. TurboMCP registers editor tools; it does not introduce a second MCP server. Reflected declarations are in [TurboMCPToolsets.h](../../Plugins/TurboMCP/Source/TurboMCP/Public/Tools/TurboMCPToolsets.h). Registration and lifecycle start in TurboMCPModule.cpp; schema/help/traps are maintained by TurboMCPToolSchema.cpp, TurboMCPAgentHelp.cpp and TurboMCPTraps.cpp.

## Required interface

Preserve the current FString public signatures while migrating implementation to the ARC02 result contract. Emit one outer result envelope and one final tmcp-result-v1 record; wrappers must not infer success from arbitrary prose or add contradictory refusal prefixes. Transport success means a response arrived, not that an edit succeeded. Input parsing must distinguish omitted, invalid and explicit zero/false values; never replace an invalid value silently with a default.

Tool help must state parameter meaning, requiredness, accepted formats, side effects and a valid example. Reflected schemas are authoritative for the callable shape. Generated documentation must not advertise a capability merely because a name exists.

### Arguments for the Unreal `call_tool` bridge

Put the toolset name, tool name, and inner argument object inside `params.arguments`. Search fields such as `query` belong in the inner `arguments` object. If the server reports empty input parameters, check this nesting and confirm the field names with `describe_toolset`.

```json
{
  "jsonrpc": "2.0",
  "id": 1,
  "method": "tools/call",
  "params": {
    "name": "call_tool",
    "arguments": {
      "toolset_name": "TurboMCP.TurboMCPReadToolset",
      "tool_name": "ReadTurboMCPAssetSearch",
      "arguments": {
        "query": "type:Blueprint"
      }
    }
  }
}
```

Unreal passes only the inner argument object to the selected toolset. TurboMCP cannot recover fields placed outside it.

## Lifecycle and observability

Call recording must register/unregister without dangling wrappers and identify the wrapped toolset through supported engine APIs. Logs describe observed calls and results, not proof of persistence. Bound record size and rotation; do not log credentials. Trusted loopback clients are the deployment boundary.

The Python client in Tools/tmcp.py must not replay an ambiguous mutation after a disconnect. Preserve unknown-outcome status so a caller can inspect state before retrying. Retain native media blocks and structured-content references when the engine supplies them; never assume every response is one text block.

## Verification and forward compatibility

Use test-result-contract.py, test-client-retry.py and the native result-contract test for their specific layers. Runtime discovery/schema, recorder teardown, failed calls, media and null-reference cases require engine tests. Unreal 5.9 removes the recorder's GetToolsetClass dependency and changes media/null behavior; see [compatibility planning](../engine-compatibility.md). Implement an adapter only after verifying the target engine contract.
