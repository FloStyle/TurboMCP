"""Focused observation only: native asset opening versus dirty state; never saves."""
import json
import time
from tmcp import Session, READ_TOOLSET

session = Session()
session.connect()
def observe(label):
    value = session.call(READ_TOOLSET, 'ReadTurboMCPDirtyList', {'Limit': '1000'})
    print(json.dumps({'stage': label, 'dirty': value}), flush=True)
    if value.startswith('error:') or '!! isError' in value:
        raise RuntimeError(value)
    return value

# Preserve server discovery in the supervisor log, then verify the native schema.
print(json.dumps(session.rpc('tools/call', {'name':'list_toolsets','arguments':{}})), flush=True)
schema = session.rpc('tools/call', {'name':'describe_toolset','arguments':{'toolset_name':'EditorToolset.EditorAppToolset'}})
print(json.dumps(schema), flush=True)
if 'OpenEditorForAsset' not in json.dumps(schema):
    raise RuntimeError('Native open schema unavailable; no asset opened')
for asset in ['/Game/Tests/BP_NodeTest.BP_NodeTest', '/Game/Tests/WBP_NodeTest.WBP_NodeTest']:
    before = observe('before ' + asset)
    result = session.call('EditorToolset.EditorAppToolset','OpenEditorForAsset',{'AssetPath':asset})
    print(json.dumps({'asset':asset,'open_result':result}), flush=True)
    if '!! isError' in result or result.startswith('error:'):
        raise RuntimeError(result)
    after = observe('immediate ' + asset)
    time.sleep(3)
    delayed = observe('settled ' + asset)
    print(json.dumps({'asset':asset,'dirty_list_changed':before != after or before != delayed}),flush=True)
print('RESULT: PASS (observation completed; not a claim that the reported bug is fixed)',flush=True)
