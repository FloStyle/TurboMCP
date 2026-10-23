"""Inventory live TurboMCP schemas and help; does not invoke the inventoried tools."""
import json
from tmcp import Session, READ_TOOLSET, LOOK_TOOLSET, PREVIEW_TOOLSET, EDIT_TOOLSET

session = Session()
session.connect()
count = 0
for toolset in (READ_TOOLSET, LOOK_TOOLSET, PREVIEW_TOOLSET, EDIT_TOOLSET):
    response = session.rpc('tools/call', {'name':'describe_toolset', 'arguments':{'toolset_name':toolset}})
    if response.get('isError'):
        raise RuntimeError(response)
    texts = [block['text'] for block in response.get('content', []) if block.get('type') == 'text']
    schema = json.loads('\n'.join(texts))
    tools = schema['tools']
    if not tools:
        raise RuntimeError('No tools returned for ' + toolset)
    for tool in tools:
        name = tool['name'].rsplit('.', 1)[-1]
        help_text = session.call(READ_TOOLSET, 'ReadTurboMCPToolHelp', {'ToolsetName':toolset, 'ToolName':name})
        print(json.dumps({'toolset':toolset, 'schema':tool, 'help':help_text}, ensure_ascii=False), flush=True)
        if help_text.startswith('error:') or '!! isError' in help_text:
            raise RuntimeError('Help unavailable for ' + name)
        required_text = {
            'LookTurboMCPWidgetPreview': ('kind: Execute', 'Execution=allow', 'not sandboxed or reversible'),
            'PreviewTurboMCPAssetRevert': ('preview only; does not discard edits', 'confirmation'),
            'EditTurboMCPAssetRevert': ('discards ALL unsaved edits', 'current preview confirmation', 'may reset undo'),
        }.get(name, ())
        for expected in required_text:
            if expected not in help_text:
                raise RuntimeError(name + ' lost essential help: ' + expected)
        if len(help_text) > 2000:
            raise RuntimeError(name + ' exceeds compact help budget')
        documented = {'ReadTurboMCPToolHelp', 'ReadTurboMCPAssetSearch', 'ReadTurboMCPAssetDescribe', 'ReadTurboMCPTypeFind', 'ReadTurboMCPDirtyList', 'ReadTurboMCPGraphLoad'}
        documented.update(prefix + 'TurboMCP' + suffix for prefix in ('Preview', 'Edit') for suffix in ('GraphAppend', 'GraphPatch', 'GraphSwapCall', 'GraphConnect'))
        documented.update(prefix + 'TurboMCP' + suffix for prefix in ('Preview', 'Edit') for suffix in ('GraphTidy', 'GraphFixBackward', 'GraphSplitValue', 'GraphRemove', 'GraphSequence'))
        documented.add('EditTurboMCPGraphPositions')
        if name in documented:
            for parameter, definition in tool['inputSchema'].get('properties', {}).items():
                if not definition.get('description'):
                    raise RuntimeError(name + ' missing live parameter description: ' + parameter)
        count += 1
print('RESULT: PASS (inventoried %d tools; behavioral correctness not certified)' % count, flush=True)
