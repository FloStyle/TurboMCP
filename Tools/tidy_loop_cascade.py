"""Evidence-first builder for the neutral loop-cascade field scenario.

All editor calls use the public registry. A lost response is never replayed.
Discovery is a checkpoint, not successful acceptance of the layout scenario.
"""
import json
import hashlib
import math
import os
from pathlib import Path
import re
import runpy
import uuid
from execution_runtime import require_context
from tmcp import Session, toolset_for, READ_TOOLSET, PREVIEW_TOOLSET, EDIT_TOOLSET, LOOK_TOOLSET


def build_graph(call, evidence, persist):
    """Construct the report's blocks; every edge names a discovered pin explicitly."""
    asset = evidence['asset']
    graph = 'LoopCascade'
    initial = evidence['initial_graph']
    ids = {'N01': re.search(r'\(fn LoopCascade ; ([^\s;]+)', initial).group(1),
           'N80': re.search(r'\(Function\|Result ; ([^\s;]+)', initial).group(1)}
    # Remove only the factory's template edge; the recipe specifies both final exits.
    call('EditTurboMCPGraphRemove',asset=asset,graphName=graph,nodes='[]',links=json.dumps([{'sourceNodeId':ids['N01'],'sourcePin':'then','targetNodeId':ids['N80'],'targetPin':'execute'}]))
    # Connections are source-label:pin strings; numbers are literal defaults.
    rows = []
    def n(label, query, **pins):
        rows.append((label, query, pins))
    def block(name):
        evidence.setdefault('blocks', []).append({'name': name, 'labels': [x[0] for x in rows]})
        forms = []
        types = []
        for label, query, pins in rows:
            access = None
            if query.startswith(('Get ', 'Set ')) and query != 'Get (a copy)':
                access, query = query.split(' ', 1)
            choices = evidence['types'][query]
            if access:
                choices = [x for x in choices if ('|' + access + '|') in x]
            if len(choices) != 1:
                raise RuntimeError('Ambiguous fixture type: ' + query)
            kind = choices[0]
            types.append(kind)
            lines = ['(' + kind + ' ; label ' + label]
            for pin, value in pins.items():
                if isinstance(value, str):
                    source, source_pin = value.split(':', 1)
                    value = '(-> ' + ids.get(source, source) + ':' + json.dumps(source_pin) + ')'
                lines.append('  ' + json.dumps(pin) + ' ' + str(value))
            forms.append('\n'.join(lines) + '\n)')
        text = '\n'.join(forms)
        args = dict(asset=asset, graphName=graph, text=text, continuation='auto', placement='local', containingCommentId='')
        preview = call('PreviewTurboMCPGraphAppend', **args)
        if not preview.startswith('preview:'):
            raise RuntimeError('Append preview did not approve construction: ' + preview)
        result = call('EditTurboMCPGraphAppend', previewToken='', **args)
        created = re.findall(r'^created (.*?): type (.*)$', result, re.M)
        if len(created) != len(rows) or [x[1] for x in created] != types:
            raise RuntimeError('Created identities/types differ from submitted forms: ' + result)
        for row, created_row in zip(rows, created):
            ids[row[0]] = created_row[0]
        evidence['label_ids'] = ids.copy()
        persist()
        compiled = call('EditTurboMCPCompile', assets=asset)
        if not re.search(r'0 with errors', compiled, re.I):
            raise RuntimeError('Compile did not explicitly report zero errors: ' + compiled)
        rows.clear()

    n('N03','PathPoints'); n('N02','Length',TargetArray='N03:PathPoints')
    n('N05','MakeLiteralInt',Value=2); n('N04','GreaterEqual',A='N02:ReturnValue',B='N05:ReturnValue')
    n('N08','Scale'); n('N07','Set ResultScale',execute='N01:then',ResultScale='N08:Scale')
    n('N06','Branch',execute='N07:then',Condition='N04:ReturnValue'); block('S')
    n('N11','Get FlatPoints'); n('N10','Clear',execute='N06:then',TargetArray='N11:FlatPoints')
    n('N13','PathPoints'); n('N12','ForEachLoop',Exec='N10:then',Array='N13:PathPoints'); block('L1')
    n('N20','PathTransform'); n('N19','TransformLocation',T='N20:PathTransform',Location='N12:Array Element')
    n('N17','BreakVector',InVec='N19:ReturnValue'); n('N18','BreakVector',InVec='N19:ReturnValue')
    n('N16','MakeVector',X='N17:X',Y='N18:Y',Z=0); n('N15','Get FlatPoints')
    n('N14','Array_Add',execute='N12:LoopBody',TargetArray='N15:FlatPoints',NewItem='N16:ReturnValue'); block('L1B')
    n('N23','Set BestDistSq',execute='N12:Completed',BestDistSq=0)
    n('N22','Get WorkWeights'); n('N21','Clear',execute='N23:then',TargetArray='N22:WorkWeights'); block('R')
    n('N25','SamplePoints'); n('N24','ForEachLoop',Exec='N21:then',Array='N25:SamplePoints'); block('L2')
    n('N26','Set BestDistSq',execute='N24:LoopBody',BestDistSq=1000000000000.0)
    n('N29','MakeLiteralInt',Value=1); n('N28','Subtract',A='N02:ReturnValue',B='N29:ReturnValue')
    n('N27','ForLoop',execute='N26:then',FirstIndex=0,LastIndex='N28:ReturnValue'); block('L2B')
    n('N35','BreakVector',InVec='N24:Array Element'); n('N36','BreakVector',InVec='N24:Array Element')
    n('N34','MakeVector',X='N35:X',Y='N36:Y',Z=0); n('N38','Get FlatPoints')
    n('N37','Get (a copy)',**{'Array':'N38:FlatPoints','Dimension 1':'N27:Index'})
    n('N44','MakeLiteralInt',Value=1); n('N43','Add',A='N27:Index',B='N44:ReturnValue')
    n('N48','MakeLiteralInt',Value=1); n('N47','Add',A='N27:Index',B='N48:ReturnValue')
    n('N46','Equal',A='N47:ReturnValue',B='N02:ReturnValue'); n('N45','MakeLiteralInt',Value=0)
    n('N42','Select',**{'Index':'N46:ReturnValue','Option 0':'N43:ReturnValue','Option 1':'N45:ReturnValue'})
    n('N41','Get FlatPoints'); n('N39','Get (a copy)',**{'Array':'N41:FlatPoints','Dimension 1':'N42:ReturnValue'})
    n('N49','FindClosestPointOnSegment',Point='N34:ReturnValue',SegmentStart='N37:Output',SegmentEnd='N39:Output')
    n('N50','BreakVector',InVec='N34:ReturnValue'); n('N51','BreakVector',InVec='N49:ReturnValue')
    n('N52','Subtract',A='N50:X',B='N51:X'); n('N53','Multiply',A='N52:ReturnValue',B='N52:ReturnValue')
    n('N54','Subtract',A='N50:Y',B='N51:Y'); n('N55','Multiply',A='N54:ReturnValue',B='N54:ReturnValue')
    n('N40','Add',A='N53:ReturnValue',B='N55:ReturnValue'); n('N32','Get BestDistSq')
    n('N31','Less',A='N40:ReturnValue',B='N32:BestDistSq'); n('N30','Branch',execute='N27:LoopBody',Condition='N31:ReturnValue')
    n('N33','Set BestDistSq',execute='N30:then',BestDistSq='N40:ReturnValue'); block('L3B')
    n('N66','Get BestDistSq'); n('N65','Sqrt',A='N66:BestDistSq')
    n('N67','InnerDistance'); n('N69','InnerDistance'); n('N70','FalloffDistance')
    n('N68','Add',A='N69:InnerDistance',B='N70:FalloffDistance')
    n('N64','MapRangeClamped',Value='N65:ReturnValue',InRangeA='N67:InnerDistance',InRangeB='N68:ReturnValue',OutRangeA=0,OutRangeB=1)
    n('N59','Multiply',A='N64:ReturnValue',B='N64:ReturnValue'); n('N61','MakeLiteralDouble',Value=3)
    n('N63','MakeLiteralDouble',Value=2); n('N62','Multiply',A='N63:ReturnValue',B='N64:ReturnValue')
    n('N60','Subtract',A='N61:ReturnValue',B='N62:ReturnValue'); n('N58','Multiply',A='N59:ReturnValue',B='N60:ReturnValue')
    n('N57','Get WorkWeights'); n('N56','Array_Add',execute='N27:Completed',TargetArray='N57:WorkWeights',NewItem='N58:ReturnValue'); block('F')
    n('N85','FallbackWeight'); n('N78','Multiply',A='N07:Output_Get',B='N85:FallbackWeight')
    n('N74','MakeLiteralDouble',Value=0); n('N75','MakeLiteralDouble',Value=1)
    n('N73','FClamp',Value='N78:ReturnValue',Min='N74:ReturnValue',Max='N75:ReturnValue')
    n('N71','Set ResultScale',execute='N24:Completed',ResultScale='N73:ReturnValue'); n('N81','Get WorkWeights'); block('E')
    n('N87','Get WorkWeights'); n('N86','Clear',execute='N06:else',TargetArray='N87:WorkWeights')
    n('N91','FallbackWeight'); n('N90','Multiply',A='N91:FallbackWeight',B='N07:Output_Get')
    n('N89','Get WorkWeights'); n('N88','Array_Add',execute='N86:then',TargetArray='N89:WorkWeights',NewItem='N90:ReturnValue'); block('X')
    # Connect both exits explicitly; verify the merge rather than assume replacement behavior.
    for source, pin, target_pin in [('N71','then','execute'),('N88','then','execute'),('N81','WorkWeights','Weights')]:
        call('EditTurboMCPGraphConnect',asset=asset,graphName=graph,sourceNodeId=ids[source],sourcePin=pin,targetNodeId=ids['N80'],targetPin=target_pin)
    evidence['final_graph'] = call('ReadTurboMCPGraphLoad',asset=asset,graphName=graph,format='read',detail='full',offset=0,maxChars=24000,revision='',entryFilter='')
    call('EditTurboMCPCompile',assets=asset)
    recipe=json.loads((Path(__file__).resolve().parents[1]/'Tests/tidy-loop-cascade.json').read_text())
    if set(ids) != {x['label'] for x in recipe['nodes']}:
        raise RuntimeError('Constructed labels differ from the complete report recipe')
    evidence['stage']='graph_constructed_baseline_pending'
    persist()


def record_baseline(call, evidence, persist):
    """Record the default result, including refusal; never force a passing layout."""
    asset = evidence['asset']
    graph = 'LoopCascade'
    ids = evidence['label_ids']
    merged = call('ReadTurboMCPNodeResolve', nodeIds=ids['N80'])
    for label in ('N71', 'N88'):
        if ids[label].rsplit(':', 1)[-1] not in merged:
            raise RuntimeError('Return merge lost source ' + label + ': ' + merged)
    evidence['merge_readback'] = merged
    # Read all pages: the full graph exceeds the reader's per-call text budget.
    offset, revision, bodies = 0, '', []
    while True:
        page = json.loads(call('ReadTurboMCPGraphLoad', asset=asset, graphName=graph, format='json', detail='full', offset=offset, maxChars=24000, revision=revision, entryFilter=''))
        bodies.append(page['body'])
        revision = page['revision']
        offset = page['nextOffset']
        if offset == -1:
            break
    evidence['complete_graph_before_tidy'] = ''.join(bodies)
    geometry = {}
    offset = 0
    while True:
        page = call('LookTurboMCPGraphPositions', asset=asset, graphName=graph, nodeIds='', maxNodes=256, offset=offset)
        for match in re.finditer(r'^(\S+) x=(-?\d+) y=(-?\d+) width=(\d+) height=(\d+)$', page, re.M):
            identity, x, y, w, h = match.groups()
            geometry[identity] = dict(x=int(x), y=int(y), width=int(w), height=int(h))
        next_page = re.search(r'next_offset=(-?\d+)', page)
        if not next_page:
            raise RuntimeError('Position page has no explicit continuation')
        offset = int(next_page.group(1))
        if offset == -1:
            break
    if set(geometry) != set(ids.values()):
        raise RuntimeError('Measured graph identities differ from the 83-node recipe')
    state = 20261105
    def draw():
        nonlocal state
        state = (1103515245 * state + 12345) % (2 ** 31)
        return state
    planned = []
    for label, identity in sorted(ids.items()):
        planned.append(dict(node=identity, label=label, x=draw()%9000, y=draw()%3000, **{k:geometry[identity][k] for k in ('width','height')}))
    placed = []
    for item in sorted(planned, key=lambda x:(x['x'],x['label'])):
        while any(item['x'] < old['x']+old['width'] and item['x']+item['width'] > old['x'] and item['y'] < old['y']+old['height'] and item['y']+item['height'] > old['y'] for old in placed):
            item['y'] += item['height'] + 40
        placed.append(item)
    evidence['scrambled_positions'] = placed
    persist()
    call('EditTurboMCPGraphPositions',asset=asset,graphName=graph,positions=json.dumps([{k:r[k] for k in ('node','x','y')} for r in placed]))
    call('EditTurboMCPCompile',assets=asset)
    call('EditTurboMCPSave',assets=asset)
    evidence['baseline_before'] = call('LookTurboMCPLayoutDoneCheck',asset=asset,graphName=graph,alignmentTolerance=64.0)
    args = dict(asset=asset,graphName=graph,bForce=False,nodeIds=[],mainPath='',horizontalGap=120.0,verticalGap=100.0,lanePitch=280.0,wrapAfter='')
    evidence['baseline_preview'] = call('PreviewTurboMCPGraphTidy',**args)
    # Refusal is valid baseline evidence, not layout acceptance.
    evidence['baseline_edit'] = call('EditTurboMCPGraphTidy',**args)
    evidence['baseline_after'] = call('LookTurboMCPLayoutDoneCheck',asset=asset,graphName=graph,alignmentTolerance=64.0)
    evidence['baseline_capture'] = call('LookTurboMCPCapture',asset=asset,graphName=graph,mode='graph',target='',size='1600x900')
    evidence['stage'] = 'default_baseline_recorded_regressions_pending'
    persist()



def read_positions(call, asset, graph):
    result, offset = {}, 0
    while True:
        page = call('LookTurboMCPGraphPositions', asset=asset, graphName=graph,
                    nodeIds='', maxNodes=256, offset=offset)
        for match in re.finditer(r'^(\S+) x=(-?\d+) y=(-?\d+) width=(\d+) height=(\d+)$', page, re.M):
            identity, *values = match.groups()
            result[identity] = dict(zip(('x', 'y', 'width', 'height'), map(int, values)))
        continuation = re.search(r'next_offset=(-?\d+)', page)
        if not continuation:
            raise RuntimeError('Position response has no explicit continuation')
        offset = int(continuation.group(1))
        if offset == -1:
            return result

def record_scoped_regression(call, evidence, persist):
    """J5: record a scoped baseline and require every outside rectangle to stay fixed.

    This checks the scope boundary, not compact-layout acceptance. Restore the
    original baseline coordinates after recording the scoped result.
    """
    asset, graph = evidence['asset'], 'LoopCascade'
    labels = next(block['labels'] for block in evidence['blocks'] if block['name'] == 'L3B')
    selected = [evidence['label_ids'][label] for label in labels]

    def positions():
        return read_positions(call, asset, graph)

    row = dict(selected_labels=labels, selected_ids=selected, acceptance=False)
    evidence.setdefault('regressions', {})['J5'] = row
    row['before'] = positions()
    if set(row['before']) != set(evidence['label_ids'].values()):
        raise RuntimeError('J5 baseline node set differs from the recipe')
    persist()
    args = dict(asset=asset, graphName=graph, bForce=False, nodeIds=selected,
                mainPath='', horizontalGap=120.0, verticalGap=100.0,
                lanePitch=280.0, wrapAfter='')
    row['preview'] = call('PreviewTurboMCPGraphTidy', **args)
    row['edit'] = call('EditTurboMCPGraphTidy', **args)
    row['edit_changed'] = row['edit'].startswith('changed:')
    row['edit_unchanged'] = row['edit'].startswith('unchanged:')
    row['after'] = positions()
    persist()
    if set(row['after']) != set(row['before']):
        raise RuntimeError('J5 changed the graph node set')
    changed_outside = [identity for identity in row['before']
                       if identity not in selected and row['before'][identity] != row['after'][identity]]
    row['changed_outside'] = changed_outside
    persist()
    if changed_outside:
        raise RuntimeError('J5 moved or resized unselected nodes: ' + ', '.join(changed_outside))
    row['scope_boundary_passed'] = True
    row['capture'] = call('LookTurboMCPCapture', asset=asset, graphName=graph,
                          mode='graph', target='', size='1600x900')
    call('EditTurboMCPGraphPositions', asset=asset, graphName=graph,
         positions=json.dumps([dict(node=identity, x=rect['x'], y=rect['y'])
                               for identity, rect in row['before'].items()]))
    row['restored'] = positions()
    if row['restored'] != row['before']:
        raise RuntimeError('J5 failed to restore baseline geometry')
    row['baseline_restored'] = True
    row['remaining'] = 'Compact rectangle placement and connection readability require candidate comparison and visual review.'
    persist()



def record_scoped_loop_boundary(call, evidence, persist):
    """J5: a compact scoped edit must not worsen the fixed LoopBody handoff silently."""
    asset, graph = evidence['asset'], 'LoopCascade'
    labels = next(block['labels'] for block in evidence['blocks'] if block['name'] == 'L3B')
    selected = [evidence['label_ids'][label] for label in labels]
    args = dict(asset=asset, graphName=graph, bForce=False, nodeIds=selected,
                mainPath='', horizontalGap=120.0, verticalGap=100.0,
                lanePitch=280.0, wrapAfter='', layoutMode='blocks')
    row = dict(selected_ids=selected, before_positions=read_positions(call, asset, graph),
               before_graph=read_complete_graph(call, asset, graph))
    evidence.setdefault('regressions', {})['J5_boundary'] = row
    persist()
    row['preview'] = call('PreviewTurboMCPGraphTidy', **args)
    row['after_preview_positions'] = read_positions(call, asset, graph)
    row['after_preview_graph'] = read_complete_graph(call, asset, graph)
    if row['after_preview_positions'] != row['before_positions'] or row['after_preview_graph'] != row['before_graph']:
        raise RuntimeError('J5 boundary preview mutated the graph')
    if not row['preview'].startswith('WOULD APPLY:'):
        raise RuntimeError('J5 boundary placement was refused: ' + row['preview'])
    drift = re.search(r'Body row drift ([\d.]+) -> ([\d.]+)', row['preview'])
    if not drift or float(drift[2]) > float(drift[1]) + 1.0:
        raise RuntimeError('J5 boundary preview did not preserve or improve LoopBody separation')
    row['body_drift'] = dict(before=float(drift[1]), proposed=float(drift[2]))
    row['edit'] = call('EditTurboMCPGraphTidy', **args)
    row['after_edit_positions'] = read_positions(call, asset, graph)
    row['after_edit_graph'] = read_complete_graph(call, asset, graph)
    if not row['edit'].startswith('changed:'):
        raise RuntimeError('J5 valid scoped placement was not applied: ' + row['edit'])
    if row['after_edit_graph'] != row['before_graph']:
        raise RuntimeError('J5 scoped edit changed graph semantics')
    if set(row['after_edit_positions']) != set(row['before_positions']):
        raise RuntimeError('J5 scoped edit changed the node identity set')
    if any(row['after_edit_positions'][identity] != rectangle
           for identity, rectangle in row['before_positions'].items() if identity not in selected):
        raise RuntimeError('J5 scoped edit moved an unselected node')
    if row['after_edit_positions'] == row['before_positions']:
        raise RuntimeError('J5 reported a change without moving any selected node')
    row['capture'] = call('LookTurboMCPCapture', asset=asset, graphName=graph,
                          mode='graph', target='', size='1600x900')
    persist()
    row['control_preview'] = call('PreviewTurboMCPGraphTidy', **dict(args, nodeIds=[]))
    if 'scoped LoopBody handoff' in row['control_preview']:
        raise RuntimeError('J5 whole-graph control was mistaken for a scoped boundary')
    undo_preview = call('PreviewTurboMCPAssetUndo', asset=asset)
    confirmation = re.search(r'Confirm=([0-9A-Fa-f-]+)', undo_preview)
    if not confirmation:
        raise RuntimeError('J5 scoped edit has no undo confirmation')
    row['undo'] = call('EditTurboMCPAssetUndo', asset=asset, confirm=confirmation.group(1))
    row['undo_positions'] = read_positions(call, asset, graph)
    row['undo_graph'] = read_complete_graph(call, asset, graph)
    if row['undo_positions'] != row['before_positions'] or row['undo_graph'] != row['before_graph']:
        raise RuntimeError('J5 scoped edit undo did not restore the graph')
    # Reuse the existing fixed-obstacle and containment controls rather than
    # constructing another large cascade. A passing placement is not proof
    # that constrained section growth still refuses safely.
    test_name = 'TurboMCP.GraphAppend.TidyFeedback'
    automation = 'AutomationTestToolset.AutomationTestToolset'
    call('DiscoverTests', _toolset=automation)
    listed = call('ListTests', _toolset=automation, nameFilter=test_name, tagFilter='')
    if test_name not in listed:
        raise RuntimeError('J5 fixed-obstacle control was not registered')
    row['obstacle_control'] = call('RunTests', _toolset=automation, testNames=[test_name])
    parse_report = runpy.run_path(str(Path(__file__).with_name('test-graph-append.py')))['parse_run_tests_report']
    entry, report = parse_report(row['obstacle_control'], test_name)
    if entry['state'] != 'Success' or entry['errors']:
        raise RuntimeError('J5 fixed-obstacle control failed')
    row['passed'] = True
    persist()


def record_comment_regression(call, evidence, persist):
    """J4 records existing titled boxes; containment is checked independently of Tidy's summary."""
    asset, graph = evidence['asset'], 'LoopCascade'
    row = dict(acceptance=False, boxes=[])
    evidence.setdefault('regressions', {})['J4'] = row
    for block_name, title in [('S', 'Validate input'), ('L1B', 'Flatten path points')]:
        labels = next(block['labels'] for block in evidence['blocks'] if block['name'] == block_name)
        members = [evidence['label_ids'][label] for label in labels]
        args = dict(asset=asset, graphName=graph, action='create', nodes=','.join(members),
                    box='', text=title, color='', size='')
        call('PreviewTurboMCPComment', **args)
        answer = call('EditTurboMCPComment', **args)
        match = re.search(r'created comment box (\S+) in ', answer)
        if not match:
            raise RuntimeError('J4 comment creation returned no identity: ' + answer)
        row['boxes'].append(dict(id=match.group(1), title=title, original_members=members))
        persist()
    row['before'] = read_positions(call, asset, graph)
    args = dict(asset=asset, graphName=graph, bForce=False, nodeIds=[], mainPath='',
                horizontalGap=120.0, verticalGap=100.0, lanePitch=280.0, wrapAfter='')
    row['preview'] = call('PreviewTurboMCPGraphTidy', **args)
    row['edit'] = call('EditTurboMCPGraphTidy', **args)
    row['after'] = read_positions(call, asset, graph)
    persist()
    if set(row['before']) != set(row['after']):
        raise RuntimeError('J4 changed the node or comment identity set')
    for box in row['boxes']:
        matches = [key for key in row['after'] if key == box['id'] or key.endswith(':' + box['id'])]
        if len(matches) != 1:
            raise RuntimeError('J4 cannot resolve comment geometry: ' + box['id'])
        rect = row['after'][matches[0]]
        box['readback'] = call('ReadTurboMCPCommentList', asset=asset, graphName=graph,
                               scope='box:' + matches[0], offset=0, limit=80)
        if box['title'] not in box['readback']:
            raise RuntimeError('J4 lost comment title: ' + box['title'])
        box['contained_original_members'] = [identity for identity in box['original_members']
            if row['after'][identity]['x'] >= rect['x']
            and row['after'][identity]['y'] >= rect['y']
            and row['after'][identity]['x'] + row['after'][identity]['width'] <= rect['x'] + rect['width']
            and row['after'][identity]['y'] + row['after'][identity]['height'] <= rect['y'] + rect['height']]
        persist()
        if not box['contained_original_members']:
            raise RuntimeError('J4 left a titled box empty of its original nodes')
    row['titles_and_nonempty_passed'] = True
    row['capture'] = call('LookTurboMCPCapture', asset=asset, graphName=graph,
                          mode='graph', target='', size='1600x900')
    row['remaining'] = 'Inspect box fit and candidate layout; title and nonempty checks alone are not visual acceptance.'
    persist()



def record_break_regression(call, evidence, persist):
    """J3 uses a real loop Break input, retaining that edge as execution feedback."""
    asset, graph = evidence['asset'], 'J3BreakFeedback'
    row = dict(acceptance=False)
    evidence.setdefault('regressions', {})['J3'] = row
    call('EditTurboMCPFunctionAdd', asset=asset, name=graph, inputs='Items: array<double>', outputs='Done: bool')
    call('EditTurboMCPCompile', assets=asset)
    initial = call('ReadTurboMCPGraphLoad', asset=asset, graphName=graph, format='read',
                   detail='full', offset=0, maxChars=24000, revision='', entryFilter='')
    entry = re.search(r'\(fn J3BreakFeedback ; ([^\s;]+)', initial).group(1)
    result = re.search(r'\(Function\|Result ; ([^\s;]+)', initial).group(1)
    discovery = call('PreviewTurboMCPNodeType', asset=asset, graphName=graph, words='ForEachLoop', limit='40')
    choices = [line for line in discovery.splitlines() if line == 'Utilities|Array|ForEachLoopwithBreak']
    if len(choices) != 1:
        raise RuntimeError('J3 cannot discover the array loop with Break')
    loop_type = choices[0]
    row['loop_pins'] = call('PreviewTurboMCPNodePins', asset=asset, graphName=graph, typeId=loop_type)
    for pin in ('Exec', 'Array', 'Break', 'LoopBody', 'Completed'):
        if not re.search(r'\b' + pin + r'\b', row['loop_pins']):
            raise RuntimeError('J3 missing discovered loop pin ' + pin)
    input_discovery = call('PreviewTurboMCPNodeType', asset=asset, graphName=graph, words='Items', limit='40')
    inputs = [line for line in input_discovery.splitlines() if line.startswith('FunctionInput|') and line.endswith('|J3BreakFeedback|Items')]
    if len(inputs) != 1:
        raise RuntimeError('J3 input getter discovery is ambiguous')
    call('EditTurboMCPGraphRemove', asset=asset, graphName=graph, nodes='[]',
         links=json.dumps([dict(sourceNodeId=entry,sourcePin='then',targetNodeId=result,targetPin='execute')]))
    text = ('(' + inputs[0] + ' ; label ItemsRead\n)\n('
            + loop_type + ' ; label Loop\n "Exec" (-> ' + entry
            + ':"then")\n "Array" (-> ItemsRead:"Items")\n)\n'
            '(Utilities|FlowControl|Branch ; label Stop\n "execute" (-> Loop:"LoopBody")\n "Condition" true\n)')
    args = dict(asset=asset,graphName=graph,text=text,continuation='auto',placement='local', containingCommentId='')
    row['append_preview'] = call('PreviewTurboMCPGraphAppend', **args)
    if not row['append_preview'].startswith('preview:'):
        raise RuntimeError('J3 construction preview refused: ' + row['append_preview'])
    appended = call('EditTurboMCPGraphAppend', previewToken='', **args)
    created = re.findall(r'^created (.*?): type (.*)$', appended, re.M)
    if [kind for _,kind in created] != [inputs[0],loop_type,'Utilities|FlowControl|Branch']:
        raise RuntimeError('J3 construction identities do not match the recipe')
    loop, stop = created[1][0], created[2][0]
    row['loop_id'], row['stop_id'] = loop, stop
    call('EditTurboMCPGraphConnect',asset=asset,graphName=graph,sourceNodeId=stop,sourcePin='then',targetNodeId=loop,targetPin='Break')
    call('EditTurboMCPGraphConnect',asset=asset,graphName=graph,sourceNodeId=loop,sourcePin='Completed',targetNodeId=result,targetPin='execute')
    row['compile'] = call('EditTurboMCPCompile',assets=asset)
    row['before'] = call('ReadTurboMCPNodeResolve',nodeIds=loop)
    persist()
    args = dict(asset=asset,graphName=graph,bForce=False,nodeIds=[],mainPath='',horizontalGap=120.0,verticalGap=100.0,lanePitch=280.0,wrapAfter='')
    row['preview'] = call('PreviewTurboMCPGraphTidy',**args)
    row['edit'] = call('EditTurboMCPGraphTidy',**args)
    row['after'] = call('ReadTurboMCPNodeResolve',nodeIds=loop)
    row['positions'] = read_positions(call,asset,graph)
    row['capture'] = call('LookTurboMCPCapture',asset=asset,graphName=graph,mode='graph',target='',size='1600x900')
    row['remaining'] = 'Compare exact feedback and Completed links, Body pin alignment and Completed placement; no automatic visual acceptance.'
    persist()


def record_event_regressions(call, evidence, persist, only_case=None):
    """Record J1/J2/J6 on fresh Actor event graphs, never substitute functions."""
    engine_tools = 'editor_toolset.toolsets.blueprint.BlueprintTools'
    for case, entries, calls_per_entry in [('J1', 1, 3), ('J2', 1, 2), ('J6A', 3, 2), ('J6B', 3, 2)]:
        if only_case is not None and case != only_case:
            continue
        name = 'TidyFixture_' + case + '_' + uuid.uuid4().hex
        asset, graph = '/Game/Tests/' + name, 'EventGraph'
        row = dict(asset=asset, graph=graph, acceptance=False, chains=[])
        evidence.setdefault('regressions', {})[case] = row
        call('create', _toolset=engine_tools, folder_path='/Game/Tests', asset_name=name,
             asset_type={'refPath': '/Script/Engine.Actor'})
        factory_nodes = read_positions(call, asset, graph)
        if factory_nodes:
            call('EditTurboMCPGraphRemove',asset=asset,graphName=graph,nodes=json.dumps(list(factory_nodes)),links='[]')
        call('EditTurboMCPVarsAdd', asset=asset, variables='Message: string')
        call('EditTurboMCPCompile', assets=asset)
        def discover(words, predicate):
            answer = call('PreviewTurboMCPNodeType',asset=asset,graphName=graph,words=words,limit='80')
            choices = [line for line in answer.splitlines() if predicate(line)]
            if len(choices) != 1:
                raise RuntimeError(case + ': ambiguous node discovery for ' + words)
            call('PreviewTurboMCPNodePins',asset=asset,graphName=graph,typeId=choices[0])
            return choices[0]
        getter = discover('Message',lambda line: line.startswith('Variable|' + asset + '.') and line.endswith('|Get|Message'))
        printer = discover('PrintString',lambda line: line.startswith('Function|/Script/Engine.') and line.endswith(':PrintString'))
        def append(forms, expected):
            args=dict(asset=asset,graphName=graph,text='\n'.join(forms),continuation='auto',placement='local', containingCommentId='')
            preview=call('PreviewTurboMCPGraphAppend',**args)
            if not preview.startswith('preview:'):
                raise RuntimeError(case + ': append preview refused: ' + preview)
            answer=call('EditTurboMCPGraphAppend',previewToken='',**args)
            created=re.findall(r'^created (.*?): type (.*)$',answer,re.M)
            if [kind for _,kind in created] != expected:
                raise RuntimeError(case + ': created node types differ from request')
            return [identity for identity,_ in created]
        for index in range(entries):
            event=call('EditTurboMCPCustomEventAdd',asset=asset,graphName=graph,name='Start'+str(index),inputs='')
            match=re.search(r'^node: (\S+)',event,re.M)
            if not match:
                raise RuntimeError(case + ': missing event identity')
            entry=match.group(1)
            starts=[(entry,'then')]
            if case == 'J2':
                sequence=discover('Sequence',lambda line: line == 'Utilities|FlowControl|Sequence')
                seq=append(['('+sequence+' ; label Sequence\n "execute" (-> '+entry+':"then")\n)'],[sequence])[0]
                call('PreviewTurboMCPNodePinGrowth',asset=asset,graphName=graph,nodeId=seq,count='3')
                call('EditTurboMCPNodePinGrowth',asset=asset,graphName=graph,nodeId=seq,count='3')
                row['sequence_readback']=call('ReadTurboMCPNodeResolve',nodeIds=seq)
                starts=[(seq,'then_'+str(i)) for i in range(3)]
            for lane,(source,pin) in enumerate(starts):
                forms, expected=[],[]
                for stage in range(calls_per_entry):
                    label='Call'+str(stage)
                    forms.append('('+getter+' ; label Read'+str(stage)+'\n)')
                    forms.append('('+printer+' ; label '+label+'\n "execute" (-> '+source+':'+json.dumps(pin)+')\n "InString" (-> Read'+str(stage)+':"Message")\n)')
                    expected.extend([getter,printer])
                    source,pin=label,'then'
                row['chains'].append(dict(entry=entry,lane=lane,nodes=append(forms,expected)))
                persist()
        row['compile']=call('EditTurboMCPCompile',assets=asset)
        row['before']=read_positions(call,asset,graph)
        args=dict(asset=asset,graphName=graph,bForce=False,nodeIds=[],mainPath='',horizontalGap=120.0,verticalGap=100.0,lanePitch=280.0,wrapAfter='')
        row['preview']=call('PreviewTurboMCPGraphTidy',**args)
        row['edit']=call('EditTurboMCPGraphTidy',**args)
        row['after']=read_positions(call,asset,graph)
        if set(row['before']) != set(row['after']):
            raise RuntimeError(case + ': Tidy changed the node identity set')
        row['capture']=call('LookTurboMCPCapture',asset=asset,graphName=graph,mode='graph',target='',size='1600x900')
        row['remaining']='Verify exact links, lane order, row alignment and candidate comparison from the recorded baseline.'
        persist()



def read_complete_graph(call, asset, graph):
    """Reconstruct one revision rather than compare a truncated reader page."""
    offset, revision, bodies = 0, '', []
    while True:
        page = json.loads(call('ReadTurboMCPGraphLoad',asset=asset,graphName=graph,
                              format='json',detail='full',offset=offset,maxChars=24000,
                              revision=revision,entryFilter=''))
        bodies.append(page['body'])
        offset, revision = page['nextOffset'], page['revision']
        if offset == -1:
            return ''.join(bodies)


def independently_score_geometry(text, context):
    """Appendix H oracle over exported physical geometry, independent of C++ scoring."""
    result = {}
    for side in ('before', 'after'):
        rows = {}
        for kind, fields in re.findall(r'^score (rect|box|edge|segment) (.+)$', text, re.M):
            values = dict(field.split('=', 1) for field in fields.split())
            if values.pop('side') == side:
                rows.setdefault(kind, []).append(values)
        if not rows.get('rect') or not rows.get('segment'):
            raise RuntimeError(context + ': missing auditable ' + side + ' geometry')
        segments = [(int(x['wire']), int(x['exec']),
                     (float(x['ax']), float(x['ay'])), (float(x['bx']), float(x['by'])))
                    for x in rows['segment']]
        def cross(a, b, c):
            return (b[0]-a[0])*(c[1]-a[1])-(b[1]-a[1])*(c[0]-a[0])
        def intersects(a, b, c, d):
            q = cross(a, b, c), cross(a, b, d), cross(c, d, a), cross(c, d, b)
            return ((q[0] > .001 and q[1] < -.001) or (q[0] < -.001 and q[1] > .001)) and ((q[2] > .001 and q[3] < -.001) or (q[2] < -.001 and q[3] > .001))
        def clips(a, b, rect):
            low, high = 0., 1.
            for axis, start, size in ((0, float(rect['x']), float(rect['w'])), (1, float(rect['y']), float(rect['h']))):
                minimum, maximum = start + .001, start + size - .001
                delta = b[axis] - a[axis]
                if abs(delta) < 1e-6:
                    if not minimum < a[axis] < maximum:
                        return False
                else:
                    first, last = sorted(((minimum-a[axis])/delta, (maximum-a[axis])/delta))
                    low, high = max(low, first), min(high, last)
                    if low >= high:
                        return False
            return low < high
        box_crossings = set()
        score = [0., 0., 0., 0., 0., 0., 0.]
        for i, (wire, execution, a, b) in enumerate(segments):
            length = math.dist(a, b)
            score[5] += length
            # Use the specified angle boundary rather than the C++ tangent comparison.
            angle = math.degrees(math.atan2(abs(b[1]-a[1]), abs(b[0]-a[0])))
            if execution and 10 < angle < 80:
                score[2] += length
            for _, other_execution, c, d in segments[:i]:
                if execution == other_execution and intersects(a, b, c, d):
                    score[1 if execution else 4] += 1
            for box in rows.get('box', []):
                owned = {int(x) for x in box.get('owned', '').split(',') if x}
                if wire not in owned and length > .001 and clips(a, b, box):
                    box_crossings.add((wire, box['index']))
        score[0] = len(box_crossings)
        score[3] = sum(int(x['row']) and abs(float(x['by'])-float(x['ay'])) > 16 for x in rows.get('edge', []))
        rects = [(float(x['x']), float(x['y']), float(x['w']), float(x['h'])) for x in rows['rect']]
        score[6] = (max(x+w for x,y,w,h in rects)-min(x for x,y,w,h in rects)) * (max(y+h for x,y,w,h in rects)-min(y for x,y,w,h in rects))
        result[side] = score
    return result


def read_compact_measures(text, context, geometry_text=None):
    """Read the documented score table; absent or invalid values are failures."""
    names = ('unrelated box crossings', 'execution segment crossings',
             'diagonal execution length', 'within-block row breaks',
             'data segment crossings', 'total segment length', 'bounding area')
    result = {}
    for name in names:
        matches = re.findall(r'^' + re.escape(name) + r': ([0-9]+(?:\.[0-9]+)?) -> ([0-9]+(?:\.[0-9]+)?)(?:;[^\n]*)?$', text, re.M)
        if len(matches) != 1:
            raise RuntimeError(context + ': expected exactly one compact measure: ' + name)
        result[name] = dict(before=float(matches[0][0]), after=float(matches[0][1]))
    oracle = independently_score_geometry(geometry_text if geometry_text is not None else text, context)
    for i, name in enumerate(names):
        for side in ('before', 'after'):
            if abs(result[name][side] - oracle[side][i]) > .11:
                raise RuntimeError(context + ': independent ' + side + ' score mismatch for ' + name + ': ' + str(oracle[side][i]) + ' vs ' + str(result[name][side]))
    return result


def read_compact_preview(call, args, context):
    """Collect every bounded geometry page for one unchanged preview plan.

    Preview replans on each call. Compare its decision, scores and planned
    positions before combining pages, then use those rows as the independent
    score oracle. Edit has no geometry page and must not be replayed for one.
    """
    preview = call('PreviewTurboMCPGraphTidy', **args)

    def plan_signature(text):
        return tuple(line for line in text.splitlines()
                     if line.startswith(('WOULD ', 'ALREADY TIDY', 'changed:',
                                         'unrelated box crossings:',
                                         'execution segment crossings:',
                                         'diagonal execution length:',
                                         'within-block row breaks:',
                                         'data segment crossings:',
                                         'total segment length:',
                                         'bounding area:',
                                         'planned ', 'route knot ', 'generated box ')))

    signature = plan_signature(preview)
    rows = []
    offset = 0
    total = None
    while True:
        page = call('PreviewTurboMCPGraphTidy', **dict(args, GeometryOffset=offset,
                                                       GeometryLimit=40))
        # The tool has a bounded response: a geometry page can consume space
        # otherwise used by trailing planned-node sections. Its retained plan
        # lines must match the same prefix of the default preview.
        page_signature = plan_signature(page)
        if len(page_signature) < 8 or page_signature != signature[:len(page_signature)]:
            raise RuntimeError(context + ': preview plan changed between geometry pages')
        match = re.search(r'^geometry evidence: rows (\d+)\.\.(\d+) of (\d+) for this plan\.$',
                          page, re.M)
        if not match:
            raise RuntimeError(context + ': missing numbered geometry page')
        first, last, count = map(int, match.groups())
        if first != offset or last < first or last - first >= 40 or (total is not None and count != total):
            raise RuntimeError(context + ': inconsistent geometry page range or total')
        total = count
        page_rows = re.findall(r'^score (?:rect|box|edge|segment) .+$', page, re.M)
        if len(page_rows) != last - first + 1:
            raise RuntimeError(context + ': geometry page omitted measured rows')
        rows.extend(page_rows)
        offset = last + 1
        if offset == total:
            if re.search(r'^geometry evidence next:', page, re.M):
                raise RuntimeError(context + ': final geometry page has a continuation')
            break
        next_page = re.search(r'^geometry evidence next: GeometryOffset=(\d+) GeometryLimit=(\d+)\.',
                              page, re.M)
        if not next_page or (int(next_page.group(1)), int(next_page.group(2))) != (offset, 40):
            raise RuntimeError(context + ': invalid geometry continuation')
    # Default preview is intentionally capped. Read the complete coordinate
    # plan in numbered pages before comparing it with the one edit result.
    detail_rows = []
    detail_offset = 0
    detail_total = None
    stable_prefixes = ('WOULD ', 'ALREADY TIDY', 'unrelated box crossings:',
                       'execution segment crossings:', 'diagonal execution length:',
                       'within-block row breaks:', 'data segment crossings:',
                       'total segment length:', 'bounding area:')
    def stable_header(text):
        return tuple(line for line in text.splitlines() if line.startswith(stable_prefixes))
    while True:
        page = call('PreviewTurboMCPGraphTidy', **dict(args, PlanOffset=detail_offset,
                                                       PlanLimit=20))
        if stable_header(page) != stable_header(preview):
            raise RuntimeError(context + ': preview plan changed between coordinate pages')
        match = re.search(r'^plan detail: rows (\d+)\.\.(-?\d+) of (\d+) for this plan\.$',
                          page, re.M)
        if not match:
            raise RuntimeError(context + ': missing numbered plan-detail page')
        first, last, count = map(int, match.groups())
        if first != detail_offset or (detail_total is not None and count != detail_total):
            raise RuntimeError(context + ': inconsistent plan-detail page range or total')
        detail_total = count
        page_rows = [line for line in page.splitlines()
                     if line.startswith(('planned ', 'planned reader copy ',
                                         'generated box ', 'route knot '))]
        expected = max(0, last - first + 1)
        if len(page_rows) != expected or expected > 20:
            raise RuntimeError(context + ': plan-detail page omitted coordinate rows')
        detail_rows.extend(page_rows)
        detail_offset = last + 1
        if detail_offset == detail_total:
            if re.search(r'^plan detail next:', page, re.M):
                raise RuntimeError(context + ': final plan-detail page has a continuation')
            break
        next_page = re.search(r'^plan detail next: PlanOffset=(\d+) PlanLimit=(\d+)\.',
                              page, re.M)
        if not next_page or (int(next_page.group(1)), int(next_page.group(2))) != (detail_offset, 20):
            raise RuntimeError(context + ': invalid plan-detail continuation')
    if len(detail_rows) != detail_total:
        raise RuntimeError(context + ': incomplete coordinate plan')
    preview = '\n'.join(line for line in preview.splitlines()
                        if not line.startswith(('planned ', 'planned reader copy ',
                                                'generated box ', 'route knot ')))
    preview += '\n' + '\n'.join(detail_rows)
    return preview, '\n'.join(rows)


def record_routing_candidate(call, evidence, persist, case='J2'):
    """Exercise J2 routing mechanics; keep Force and visual acceptance explicit."""
    item = evidence['regressions']['J2'] if case == 'J2' else dict(asset=evidence['asset'], graph='LoopCascade')
    asset, graph = item['asset'], item['graph']
    row = dict(acceptance=False, forced_mechanics_check=True)
    evidence['routing_' + case] = row

    def key(identity):
        match = re.search(r'[:.]([0-9a-fA-F]{6,32})$', identity)
        if not match:
            raise RuntimeError('Unrecognized node identity: ' + identity)
        return match.group(1).lower()

    def topology(positions):
        nodes, edges, fanout = {}, set(), {}
        for identity in positions:
            text = call('ReadTurboMCPNodeContext', asset=asset, graphName=graph,
                        nodeId=identity, nextHops=0)
            current = None
            observed = {}
            for line in text.splitlines():
                match = re.match(r'^node (.+) type=(.*)$', line)
                if match:
                    current = key(match.group(1))
                    observed[current] = dict(type=match.group(2), defaults=[])
                    observed[current]['fanout'] = {}
                elif current and line.startswith('  input ') and ' default=' in line:
                    observed[current]['defaults'].append(line)
                elif current:
                    match = re.match(r'^  output (.+?) -> (.+):([^:]+)$', line)
                    if match:
                        edge = (current, match.group(1), key(match.group(2)), match.group(3))
                        edges.add(edge)
                        observed[current]['fanout'].setdefault(match.group(1), []).append(edge[2:])
            for identity, metadata in observed.items():
                metadata['defaults'].sort()
                ordered = metadata.pop('fanout')
                if identity in fanout and fanout[identity] != ordered:
                    raise RuntimeError('Inconsistent fan-out order for ' + identity)
                fanout[identity] = ordered
                if identity in nodes and nodes[identity] != metadata:
                    raise RuntimeError('Inconsistent context snapshots for ' + identity)
                nodes[identity] = metadata
        return dict(nodes=nodes, edges=sorted(edges), fanout=fanout)

    row['before_positions'] = read_positions(call, asset, graph)
    row['before_graph'] = read_complete_graph(call, asset, graph)
    row['before_topology'] = topology(row['before_positions'])
    args = dict(asset=asset, graphName=graph, bForce=True, nodeIds=[], mainPath='',
                horizontalGap=120.0, verticalGap=100.0, lanePitch=280.0, wrapAfter='',
                layoutMode='blocks', routeWires='orthogonal', channelWidth=60.0)
    row['preview'], row['preview_geometry'] = read_compact_preview(call, args, 'routing J2 preview')
    row['preview_measures'] = read_compact_measures(row['preview'], 'routing J2 preview', row['preview_geometry'])
    planned = re.findall(r'^route knot wire=\d+ index=\d+ x=(-?\d+) y=(-?\d+)$', row['preview'], re.M)
    row['planned_knots'] = [tuple(map(int, point)) for point in planned]
    persist()
    if not planned:
        raise RuntimeError('J2 routing planned no knots; the crossing case remains unresolved')
    if read_positions(call, asset, graph) != row['before_positions'] or read_complete_graph(call, asset, graph) != row['before_graph']:
        raise RuntimeError('Routing preview mutated the graph')
    row['edit'] = call('EditTurboMCPGraphTidy', **args)
    row['after_positions'] = read_positions(call, asset, graph)
    row['after_graph'] = read_complete_graph(call, asset, graph)
    persist()
    if not row['edit'].startswith('changed:'):
        raise RuntimeError('J2 routed edit did not apply; inspect retained preview and rollback evidence')
    row['edit_measures'] = read_compact_measures(row['edit'], 'routing J2 edit', row['preview_geometry'])
    if row['preview_measures'] != row['edit_measures']:
        raise RuntimeError('Routing preview/edit score mismatch')
    row['score_regressions'] = [name for name, value in row['edit_measures'].items()
                               if value['after'] > value['before'] * 1.1 + 0.11]
    added = set(row['after_positions']) - set(row['before_positions'])
    if not set(row['before_positions']) <= set(row['after_positions']):
        raise RuntimeError('Routing removed an original node')
    if sorted((row['after_positions'][node]['x'], row['after_positions'][node]['y']) for node in added) != sorted(row['planned_knots']):
        raise RuntimeError('Added knot coordinates differ from the preview')
    row['after_topology'] = topology(row['after_positions'])
    original = row['before_topology']['nodes']
    actual = row['after_topology']['nodes']
    added_keys = {key(node) for node in added}
    # NodeContext may have no creation type ID for a reroute. Resolve its
    # concrete class instead of treating an empty creation ID as a missing node.
    for node in added:
        resolved = call('ReadTurboMCPNodeResolve', nodeIds=node)
        if not re.search(r'^    class: UK2Node_Knot$', resolved, re.M) and not re.search(r'^    class: K2Node_Knot$', resolved, re.M):
            raise RuntimeError('Routing added a node whose concrete class is not K2Node_Knot')
    if {identity: actual.get(identity) for identity in original} != original:
        raise RuntimeError('Routing changed original node types or input defaults')
    edges = row['after_topology']['edges']
    outgoing = {}
    for source, pin, target, target_pin in edges:
        outgoing.setdefault(source, []).append((source, pin, target, target_pin))
    for identity in added_keys:
        if len(outgoing.get(identity, [])) != 1 or sum(edge[2] == identity for edge in edges) != 1:
            raise RuntimeError('Added reroute is orphaned or branches')
    contracted = set()
    for source, pin, target, target_pin in edges:
        if source in added_keys:
            continue
        seen = set()
        while target in added_keys:
            if target in seen:
                raise RuntimeError('Added reroutes form a cycle')
            seen.add(target)
            _, _, target, target_pin = outgoing[target][0]
        contracted.add((source, pin, target, target_pin))
    contracted_fanout = {}
    for source, pins in row['after_topology']['fanout'].items():
        if source in added_keys:
            continue
        contracted_fanout[source] = {}
        for pin, destinations in pins.items():
            restored = []
            for target, target_pin in destinations:
                seen = set()
                while target in added_keys:
                    if target in seen:
                        raise RuntimeError('Added reroutes form a cycle')
                    seen.add(target)
                    _, _, target, target_pin = outgoing[target][0]
                restored.append((target, target_pin))
            contracted_fanout[source][pin] = restored
    row['fanout_order_equal'] = contracted_fanout == row['before_topology']['fanout']
    if not row['fanout_order_equal']:
        raise RuntimeError('Routing changed logical fan-out ordering')
    row['contracted_edges'] = sorted(contracted)
    row['connectivity_equal'] = row['contracted_edges'] == row['before_topology']['edges']
    persist()
    if not row['connectivity_equal']:
        raise RuntimeError('Routing changed logical pin connections')
    row['capture'] = call('LookTurboMCPCapture', asset=asset, graphName=graph, mode='graph', target='', size='1600x900')
    undo = call('PreviewTurboMCPAssetUndo', asset=asset)
    confirm = re.search(r'Confirm=([0-9A-Fa-f-]+)', undo)
    if not confirm:
        raise RuntimeError('Routing undo preview returned no confirmation')
    row['undo'] = call('EditTurboMCPAssetUndo', asset=asset, confirm=confirm.group(1))
    row['undo_restored'] = read_positions(call, asset, graph) == row['before_positions'] and read_complete_graph(call, asset, graph) == row['before_graph']
    persist()
    if not row['undo_restored']:
        raise RuntimeError('Routing undo did not restore original graph and coordinates')


def record_loop_locals(call, evidence, persist, keep_for_layout=False):
    asset, graph = evidence['asset'], 'LoopCascade'
    row = dict(acceptance=False)
    evidence['explicit_layout_locals' if keep_for_layout else 'explicit_loop_local'] = row
    call('EditTurboMCPCompile', assets=asset)
    row['preview'] = call('PreviewTurboMCPLocalVars', asset=asset, graphName=graph, names='{}')
    ids = re.findall(r'^output ID: (.+?); local name:', row['preview'], re.M)
    row['reported_shared_outputs'] = {}
    for label in ('N02', 'N07', 'N27'):
        key = evidence['label_ids'][label].rsplit(':', 1)[-1]
        row['reported_shared_outputs'][label] = [identity for identity in ids if re.search(':' + re.escape(key) + r'[0-9a-fA-F]*:', identity)]
    persist()
    if any(not values for values in row['reported_shared_outputs'].values()):
        raise RuntimeError('Local preview omitted a required shared value; unsafe values must still be reported')
    outputs = [identity for identity in row['reported_shared_outputs']['N27'] if identity.endswith(':Index')]
    if len(outputs) != 1:
        raise RuntimeError('Missing unique explicit loop-index output identity')
    selected = {outputs[0]: 'CurrentIndex'}
    if keep_for_layout:
        counts = [identity for identity in row['reported_shared_outputs']['N02'] if identity.endswith(':ReturnValue')]
        if len(counts) != 1:
            raise RuntimeError('Missing unique stable array-length identity')
        selected[counts[0]] = 'PointCount'
    names = json.dumps(selected)
    row['selected_preview'] = call('PreviewTurboMCPLocalVars', asset=asset, graphName=graph, names=names)
    row['before_positions'] = read_positions(call,asset,graph)
    row['before_graph'] = read_complete_graph(call,asset,graph)
    row['edit'] = call('EditTurboMCPLocalVars', asset=asset, graphName=graph, names=names)
    row['after_positions'] = read_positions(call,asset,graph)
    row['after_graph'] = read_complete_graph(call,asset,graph)
    persist()
    if not row['edit'].startswith('changed:') or 'compile=passed' not in row['edit']:
        raise RuntimeError('Explicit cascade loop promotion did not compile successfully')
    if len(row['after_positions']) != len(row['before_positions']) + 2 * len(selected) or any(row['after_positions'].get(identity) != rect for identity,rect in row['before_positions'].items()):
        raise RuntimeError('Explicit loop promotion changed original geometry or promoted additional values')
    if 'CurrentIndex' not in row['after_graph']:
        raise RuntimeError('Meaningful local name missing from graph readback')
    if keep_for_layout:
        row['undo_pending_for_layout'] = True
        persist()
        return row
    preview = call('PreviewTurboMCPAssetUndo',asset=asset)
    confirm = re.search(r'Confirm=([0-9A-Fa-f-]+)',preview)
    if not confirm:
        raise RuntimeError('Missing explicit-local undo confirmation')
    row['undo'] = call('EditTurboMCPAssetUndo',asset=asset,confirm=confirm.group(1))
    row['undo_restored'] = read_positions(call,asset,graph) == row['before_positions'] and read_complete_graph(call,asset,graph) == row['before_graph']
    persist()
    if not row['undo_restored']:
        raise RuntimeError('Explicit loop local undo did not restore exact original graph')


def record_generated_comments(call, evidence, persist, case='J2'):
    item = evidence['regressions'][case] if case in evidence['regressions'] else dict(asset=evidence['asset'], graph='LoopCascade')
    asset, graph = item.get('asset', evidence.get('asset')), item.get('graph', 'J3BreakFeedback' if case == 'J3' else 'LoopCascade')
    row = dict(acceptance=False, forced_mechanics_check=True)
    evidence.setdefault('generated_comments', {})[case] = row
    row['before_positions'] = read_positions(call, asset, graph)
    row['before_graph'] = read_complete_graph(call, asset, graph)
    args = dict(asset=asset, graphName=graph, bForce=True, nodeIds=[], mainPath='',
                horizontalGap=120., verticalGap=100., lanePitch=280., wrapAfter='',
                layoutMode='blocks', routeWires='orthogonal' if case == 'cascade' else 'off', channelWidth=60., bCreateBoxes=True)
    row['preview'], row['preview_geometry'] = read_compact_preview(call, args, case + ' comments preview')
    row['preview_measures'] = read_compact_measures(row['preview'], case + ' comments preview', row['preview_geometry'])
    plans = re.findall(r'^generated box (\S+) title="(.*?)" x=(-?\d+) y=(-?\d+) width=(\d+) height=(\d+) members=(.*)$', row['preview'], re.M)
    row['plans'] = [dict(key=k,title=t,x=int(x),y=int(y),width=int(w),height=int(h),members=m.split(',')) for k,t,x,y,w,h,m in plans]
    persist()
    if not plans:
        raise RuntimeError(case + ': requested new comments but no plans were produced')
    if read_positions(call,asset,graph) != row['before_positions'] or read_complete_graph(call,asset,graph) != row['before_graph']:
        raise RuntimeError(case + ': comment preview mutated the graph')
    row['edit'] = call('EditTurboMCPGraphTidy', **args)
    row['after_positions'] = read_positions(call,asset,graph)
    if not row['edit'].startswith('changed:'):
        raise RuntimeError(case + ': comment edit refused; retained evidence is not acceptance')
    row['edit_measures'] = read_compact_measures(row['edit'], case + ' comments edit', row['preview_geometry'])
    if row['edit_measures'] != row['preview_measures']:
        raise RuntimeError(case + ': generated-comment preview/edit score mismatch')
    added = set(row['after_positions']) - set(row['before_positions'])
    planned_rects = sorted(tuple(plan[key] for key in ('x','y','width','height')) for plan in row['plans'])
    actual_boxes = {identity for identity in added if tuple(row['after_positions'][identity][key] for key in ('x','y','width','height')) in planned_rects}
    actual_rects = sorted(tuple(row['after_positions'][identity][key] for key in ('x','y','width','height')) for identity in actual_boxes)
    if actual_rects != planned_rects:
        raise RuntimeError(case + ': created comments differ from their planned rectangles')
    planned_knots = sorted(tuple(map(int, point)) for point in re.findall(r'^route knot wire=\d+ index=\d+ x=(-?\d+) y=(-?\d+)$', row['preview'], re.M))
    planned_readers = [tuple(map(int, point)) for point in re.findall(r'^planned reader copy index=\d+ x=(-?\d+) y=(-?\d+) width=', row['preview'], re.M)]
    if sorted((row['after_positions'][identity]['x'],row['after_positions'][identity]['y']) for identity in added-actual_boxes) != sorted(planned_knots + planned_readers):
        if 'Additional Tidy detail omitted after' in row['preview'] and len(planned_knots + planned_readers) < len(added - actual_boxes):
            raise RuntimeError(case + ': preview omitted planned route-knot details; full coordinate comparison is blocked by the bounded response')
        raise RuntimeError(case + ': combined comment/router knot coordinates differ from the preview')
    def contains(a,b):
        return a['x'] <= b['x'] and a['y'] <= b['y'] and a['x']+a['width'] >= b['x']+b['width'] and a['y']+a['height'] >= b['y']+b['height']
    row['nested_pairs'] = []
    for i,a in enumerate(row['plans']):
        for b in row['plans'][i+1:]:
            overlap = a['x'] < b['x']+b['width'] and a['x']+a['width'] > b['x'] and a['y'] < b['y']+b['height'] and a['y']+a['height'] > b['y']
            if overlap and not contains(a,b) and not contains(b,a):
                raise RuntimeError(case + ': generated comments partially overlap')
            if contains(a,b): row['nested_pairs'].append((a['key'],b['key']))
            if contains(b,a): row['nested_pairs'].append((b['key'],a['key']))
    if case == 'cascade':
        def members(plan):
            return {identity.rsplit(':',1)[-1] for identity in plan['members']}
        labels = {label: identity.rsplit(':',1)[-1] for label,identity in evidence['label_ids'].items()}
        outer = [plan for plan in row['plans'] if all(labels[label] in members(plan) for label in ('N26','N27','N30','N33','N56'))]
        falloff = [plan for plan in row['plans'] if labels['N56'] in members(plan) and labels['N26'] not in members(plan)]
        closest = [plan for plan in row['plans'] if labels['N30'] in members(plan) and labels['N33'] in members(plan) and labels['N26'] not in members(plan)]
        if len(outer) != 1 or len(falloff) != 1 or len(closest) != 1 or not contains(outer[0],falloff[0]) or not contains(outer[0],closest[0]):
            raise RuntimeError('Cascade Falloff and Closest segment boxes are not nested in the outer body')
        row['required_nesting_verified'] = True
        if row['edit_measures']['unrelated box crossings']['after'] != 0:
            raise RuntimeError('Combined cascade routing crosses an unrelated generated box')
    row['comment_pages'] = []
    offset = 0
    for _ in range(80):
        page = call('ReadTurboMCPCommentList', asset=asset, graphName=graph, scope='', offset=offset, limit=80)
        if not page.startswith('read:'):
            raise RuntimeError(case + ': comment readback failed')
        row['comment_pages'].append(page)
        continuation = re.search(r'nextOffset=(\d+)', page)
        if not continuation:
            break
        next_offset = int(continuation.group(1))
        if next_offset <= offset:
            raise RuntimeError(case + ': comment pagination did not advance')
        offset = next_offset
    else:
        raise RuntimeError(case + ': comment pagination exceeded its bound')
    row['comments'] = '\n'.join(row['comment_pages'])
    for plan in row['plans']:
        if plan['title'] not in row['comments']:
            raise RuntimeError(case + ': created comment title is missing')
    # Native whole-graph capture can hit its zoom floor. The preview exports
    # an overview and 1:1 tiles; live captures cover each generated region.
    row['captures'] = {}
    for plan in row['plans']:
        target = f"{plan['x']-40},{plan['y']-40},{plan['width']+80},{plan['height']+80}"
        row['captures'][plan['key']] = call('LookTurboMCPCapture', asset=asset, graphName=graph,
                                          mode='region', target=target, size='1600x900')
    undo = call('PreviewTurboMCPAssetUndo', asset=asset)
    confirm = re.search(r'Confirm=([0-9A-Fa-f-]+)',undo)
    if not confirm:
        raise RuntimeError(case + ': missing generated-comment undo confirmation')
    row['undo'] = call('EditTurboMCPAssetUndo', asset=asset, confirm=confirm.group(1))
    row['undo_restored'] = read_positions(call,asset,graph) == row['before_positions'] and read_complete_graph(call,asset,graph) == row['before_graph']
    persist()
    if not row['undo_restored']:
        raise RuntimeError(case + ': generated-comment undo did not restore exact original state')


def record_compact_candidates(call, evidence, persist, comments_only=False):
    """Retain failures and verify read-back/undo; visual acceptance is separate."""
    cases = [('cascade', evidence['asset'], 'LoopCascade', []),
             ('J3', evidence['asset'], 'J3BreakFeedback', [])]
    selected = evidence['regressions']['J5']['selected_ids']
    cases.append(('J5',evidence['asset'],'LoopCascade',selected))
    for name in ('J1','J2','J6A','J6B'):
        item=evidence['regressions'][name]
        cases.append((name,item['asset'],item['graph'],[]))
    if comments_only:
        cases = [('J4', evidence['asset'], 'LoopCascade', [])]
    for name,asset,graph,selection in cases:
        row=dict(acceptance=False)
        evidence.setdefault('compact',{})[name]=row
        row['before_graph']=read_complete_graph(call,asset,graph)
        row['before_positions']=read_positions(call,asset,graph)
        args=dict(asset=asset,graphName=graph,bForce=False,nodeIds=selection,mainPath='',
                  horizontalGap=120.0,verticalGap=100.0,lanePitch=280.0,wrapAfter='',layoutMode='blocks')
        row['preview'], row['preview_geometry'] = read_compact_preview(call, args, name + ' preview')
        row['compact_measures'] = read_compact_measures(row['preview'], name + ' preview', row['preview_geometry'])
        row['after_preview_positions']=read_positions(call,asset,graph)
        row['after_preview_graph']=read_complete_graph(call,asset,graph)
        if row['after_preview_positions'] != row['before_positions'] or row['after_preview_graph'] != row['before_graph']:
            raise RuntimeError(name+': compact preview changed the graph')
        row['edit']=call('EditTurboMCPGraphTidy',**args)
        row['after_graph']=read_complete_graph(call,asset,graph)
        row['after_positions']=read_positions(call,asset,graph)
        row['reader_equal']=row['after_graph']==row['before_graph']
        if name == 'cascade':
            branch = row['after_positions'][evidence['label_ids']['N30']]
            setter = row['after_positions'][evidence['label_ids']['N33']]
            row['shared_calculation_exec_gap'] = setter['x'] - branch['x'] - branch['width']
            if not row['edit'].startswith('changed:') or row['shared_calculation_exec_gap'] > args['horizontalGap'] + 1:
                raise RuntimeError('Cascade re-reserved a shared calculation tree between its branch and setter')
        if name == 'J4':
            row['comment_checks'] = []
            for box in evidence['regressions']['J4']['boxes']:
                matches = [identity for identity in row['after_positions']
                           if identity == box['id'] or identity.endswith(':' + box['id'])]
                if len(matches) != 1:
                    raise RuntimeError('J4 compact comment identity is missing or ambiguous')
                rect = row['after_positions'][matches[0]]
                readback = call('ReadTurboMCPCommentList', asset=asset, graphName=graph,
                                scope='box:' + matches[0], offset=0, limit=80)
                missing = [identity for identity in box['original_members']
                           if identity not in row['after_positions'] or not (
                               row['after_positions'][identity]['x'] >= rect['x']
                               and row['after_positions'][identity]['y'] >= rect['y']
                               and row['after_positions'][identity]['x'] + row['after_positions'][identity]['width'] <= rect['x'] + rect['width']
                               and row['after_positions'][identity]['y'] + row['after_positions'][identity]['height'] <= rect['y'] + rect['height'])]
                row['comment_checks'].append(dict(id=matches[0], title=box['title'],
                                                  readback=readback, missing_members=missing))
                persist()
                if box['title'] not in readback or missing:
                    raise RuntimeError('J4 compact layout lost a comment title or original member')
        planned = {identity: dict(x=int(x), y=int(y)) for identity,x,y in
            re.findall(r'^planned (\S+) x=(-?\d+) y=(-?\d+)$', row['preview'], re.M)}
        resolved = {}
        for identity, point in planned.items():
            matches = [full for full in row['after_positions'] if full == identity or full.endswith('.' + identity)]
            if len(matches) != 1:
                raise RuntimeError(name+': planned identity is missing or ambiguous: '+identity)
            resolved[matches[0]] = point
        planned = resolved
        row['preview_plan_positions'] = planned
        if not planned:
            raise RuntimeError(name+': preview omitted its planned positions')
        if row['edit'].startswith('changed:'):
            row['edit_compact_measures'] = read_compact_measures(row['edit'], name + ' edit', row['preview_geometry'])
            row['preview_edit_score_mismatches'] = [measure for measure, values in row['compact_measures'].items()
                if row['edit_compact_measures'][measure] != values]
            persist()
            if row['preview_edit_score_mismatches']:
                raise RuntimeError(name + ': preview/edit score mismatch: ' + ', '.join(row['preview_edit_score_mismatches']))
            # Force is false in this scenario. Retain every regressed measure,
            # including a zero-to-positive change; never accept a passing rerun
            # as evidence that the earlier regression disappeared.
            row['score_regressions'] = [measure for measure, values in row['compact_measures'].items()
                if values['after'] > values['before'] * 1.10 + 0.11]
            if row['score_regressions']:
                persist()
                raise RuntimeError(name + ': edit accepted >10% compact score regression: ' + ', '.join(row['score_regressions']))
            row['preview_edit_mismatches'] = [identity for identity, point in planned.items()
                if identity not in row['after_positions'] or
                any(row['after_positions'][identity][axis] != point[axis] for axis in ('x','y'))]
            persist()
            if row['preview_edit_mismatches']:
                raise RuntimeError(name+': preview/edit coordinate mismatch')
        row['changed_outside']=[identity for identity in row['before_positions']
            if selection and identity not in selection and row['after_positions'].get(identity)!=row['before_positions'][identity]]
        persist()
        if row['changed_outside']:
            raise RuntimeError(name+': compact layout changed fixed outside geometry')
        if not row['reader_equal']:
            raise RuntimeError(name+': complete graph read-back changed; inspect evidence before accepting semantics')
        # Dense cascades can exceed native whole-graph zoom limits. Capture
        # each recipe block at its measured bounds so evidence remains readable.
        if graph == 'LoopCascade':
            row['captures'] = {}
            for block in evidence['blocks']:
                identities = [evidence['label_ids'][label] for label in block['labels']]
                rects = [row['after_positions'][identity] for identity in identities]
                left = min(r['x'] for r in rects) - 80
                top = min(r['y'] for r in rects) - 80
                right = max(r['x'] + r['width'] for r in rects) + 80
                bottom = max(r['y'] + r['height'] for r in rects) + 80
                target = f'{left},{top},{right-left},{bottom-top}'
                row['captures'][block['name']] = call('LookTurboMCPCapture',
                    asset=asset,graphName=graph,mode='region',target=target,size='1600x900')
                persist()
        else:
            row['capture']=call('LookTurboMCPCapture',asset=asset,graphName=graph,mode='graph',target='',size='1600x900')
        if row['edit'].startswith('changed:'):
            undo_preview=call('PreviewTurboMCPAssetUndo',asset=asset)
            confirm=re.search(r'Confirm=([0-9A-Fa-f-]+)',undo_preview)
            if not confirm:
                raise RuntimeError(name+': undo preview returned no confirmation')
            row['undo']=call('EditTurboMCPAssetUndo',asset=asset,confirm=confirm.group(1))
            row['undo_graph']=read_complete_graph(call,asset,graph)
            row['undo_positions']=read_positions(call,asset,graph)
            row['undo_restored']=row['undo_graph']==row['before_graph'] and row['undo_positions']==row['before_positions']
            if not row['undo_restored']:
                raise RuntimeError(name+': undo did not restore graph and geometry')
        else:
            row['unchanged_verified']=row['after_positions']==row['before_positions']
            if not row['unchanged_verified']:
                raise RuntimeError(name+': unchanged response left geometry modified')
        persist()


def run():
    root = Path(__file__).resolve().parents[1]
    context = json.loads(Path(os.environ['TURBOMCP_EXECUTION_CONTEXT']).read_text(encoding='utf-8'))
    require_context(root, context['engine'])
    if os.environ.get('TURBOMCP_RUNNER_OWNS_FIXTURES') != '1':
        raise RuntimeError('The supervisor must own fixture restoration.')
    directory = root / '.local' / 'tidy-cascade' / uuid.uuid4().hex
    directory.mkdir(parents=True)
    session = Session()
    session.connect()
    if os.environ.get('TMCP_TIDY_FEEDBACK_ONLY') == '1':
        name = 'TurboMCP.GraphAppend.TidyFeedback'
        automation = 'AutomationTestToolset.AutomationTestToolset'
        session.call(automation, 'DiscoverTests', {})
        listed = session.call(automation, 'ListTests', {'nameFilter': name, 'tagFilter': ''})
        if name not in listed:
            raise RuntimeError('Focused Tidy feedback test is not registered')
        answer = session.call(automation, 'RunTests', {'testNames': [name]})
        parse_report = runpy.run_path(str(Path(__file__).with_name('test-graph-append.py')))['parse_run_tests_report']
        entry, report = parse_report(answer, name)
        (directory / 'focused-native.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
        print(answer, flush=True)
        if entry['state'] != 'Success' or entry['errors']:
            raise RuntimeError('Focused Tidy feedback regression failed')
        print('RESULT: PASS (focused Tidy feedback only; not cascade acceptance)', flush=True)
        return
    # A tracked selection survives the supervisor's deliberate environment filter.
    scenario = json.loads((root / 'Tests' / 'tidy-loop-cascade.json').read_text(encoding='utf-8'))
    subset = os.environ.get('TURBOMCP_TIDY_BASELINE_SUBSET', scenario.get('run_subset', 'all'))
    if subset not in ('all', 'events', 'preview', 'routing', 'boundary'):
        raise RuntimeError('Unknown baseline subset; expected all, events, preview, routing or boundary')
    evidence = {'stage': 'discovery', 'layout_acceptance': False, 'subset': subset, 'calls': []}

    def persist():
        (directory / 'evidence.json').write_text(json.dumps(evidence, indent=2), encoding='utf-8')

    def call(tool, **args):
        if tool in ('PreviewTurboMCPGraphTidy', 'EditTurboMCPGraphTidy'):
            args.setdefault('routeWires', 'off')
            args.setdefault('channelWidth', 60.0)
            args.setdefault('bCreateBoxes', False)
        toolset = args.pop('_toolset', None) or toolset_for(tool)
        row = {'toolset': toolset, 'tool': tool, 'arguments': args, 'outcome': 'pending'}
        evidence['calls'].append(row)
        persist()
        answer = session.call(toolset, tool, args)
        row['answer'] = answer
        row['outcome'] = 'returned'
        persist()
        print(tool + ': ' + answer[:250], flush=True)
        if answer.lstrip().lower().startswith(('error:', '!!', 'refused:', 'partial:')):
            raise RuntimeError(tool + ': ' + answer)
        return answer

    try:
        for toolset in (READ_TOOLSET, PREVIEW_TOOLSET, EDIT_TOOLSET, LOOK_TOOLSET):
            response = session.rpc('tools/call', {'name': 'describe_toolset', 'arguments': {'toolset_name': toolset}})
            (directory / (toolset.split('.')[-1] + '.json')).write_text(json.dumps(response, indent=2), encoding='utf-8')
        # Compare compact help with the live schema, not a hand-maintained
        # parameter list. All tools named by X-129 must retain required names.
        evidence['required_help'] = {}
        for tool in ('PreviewTurboMCPGraphTidy', 'EditTurboMCPGraphTidy',
                     'LookTurboMCPGraphPositions', 'PreviewTurboMCPNodeType', 'LookTurboMCPCapture'):
            toolset = toolset_for(tool)
            response = json.loads((directory / (toolset.split('.')[-1] + '.json')).read_text())
            schemas = [json.loads(item['text']) for item in response['content'] if item.get('type') == 'text']
            matches = [item for schema in schemas for item in schema.get('tools', [])
                       if item['name'].split('.')[-1] == tool]
            if len(matches) != 1:
                raise RuntimeError('Missing unique live schema for ' + tool)
            required = matches[0].get('inputSchema', {}).get('required', [])
            help_text = call('ReadTurboMCPToolHelp', toolsetName=toolset, toolName=tool)
            summary = help_text.split('required parameters (live schema):', 1)[-1]
            missing = [name for name in required if not re.search(r'^  ' + re.escape(name) + r': ', summary, re.M)]
            evidence['required_help'][tool] = dict(required=required, missing=missing, text=help_text)
            persist()
            if missing:
                raise RuntimeError(tool + ': compact help omitted required parameters: ' + ', '.join(missing))
        call('ReadTurboMCPStatus')
        if subset == 'routing':
            record_event_regressions(call, evidence, persist, only_case='J2')
            record_routing_candidate(call, evidence, persist)
            record_generated_comments(call, evidence, persist)
            evidence['stage'] = 'focused_routing_checks_passed_visual_acceptance_pending'
            persist()
            print('Evidence: ' + str(directory), flush=True)
            print('RESULT: PASS (focused J2 routing only; full cascade and visual acceptance remain pending)')
            return
        if subset == 'preview':
            # Narrow native rendering check, not a substitute for cascade acceptance.
            record_event_regressions(call, evidence, persist, only_case='J2')
            item = evidence['regressions']['J2']
            asset, graph = item['asset'], item['graph']
            before_positions = read_positions(call, asset, graph)
            before_graph = read_complete_graph(call, asset, graph)
            preview = call('PreviewTurboMCPGraphTidy', asset=asset, graphName=graph,
                           bForce=False, nodeIds=[], mainPath='', horizontalGap=120.0,
                           verticalGap=100.0, lanePitch=280.0, wrapAfter='', layoutMode='blocks')
            evidence['detail_preview'] = preview
            persist()
            for side in ('before', 'after'):
                if not re.search(r'^' + side + r'-block-\d+-row-\d+-col-\d+: ', preview, re.M):
                    raise RuntimeError('Missing native detail image for ' + side)
            if read_positions(call, asset, graph) != before_positions or read_complete_graph(call, asset, graph) != before_graph:
                raise RuntimeError('Detail preview mutated the live graph')
            evidence['stage'] = 'detail_preview_immutability_passed_image_review_pending'
            persist()
            print('Evidence: ' + str(directory), flush=True)
            print('RESULT: PASS (focused J2 preview only; screenshot inspection and cascade acceptance remain pending)')
            return
        if subset == 'events':
            record_event_regressions(call, evidence, persist)
            evidence['stage'] = 'event_baselines_recorded_acceptance_pending'
            persist()
            print('Evidence: ' + str(directory), flush=True)
            print('RESULT: PASS (J1/J2/J6 baseline checkpoint only; full fixture and acceptance not run)')
            return
        asset = '/Game/Tests/TidyFixture_LoopCascade_' + uuid.uuid4().hex
        evidence['asset'] = asset
        call('PreviewTurboMCPFunctionLibraryCreate', packagePath=asset)
        call('EditTurboMCPFunctionLibraryCreate', packagePath=asset)
        call('EditTurboMCPFunctionAdd', asset=asset, name='LoopCascade', inputs='PathPoints: array<vector>, PathTransform: transform, SamplePoints: array<vector>, InnerDistance: double, FalloffDistance: double, FallbackWeight: double, Scale: double', outputs='Weights: array<double>')
        # Use explicit doubles consistently; report float/math-double ambiguity is retained.
        evidence['recipe_corrections'] = ['Use explicit double scalar/array types to match the listed double operators.']
        for name, kind in [('FlatPoints', 'array<vector>'), ('WorkWeights', 'array<double>'), ('BestDistSq', 'double'), ('ResultScale', 'double')]:
            call('EditTurboMCPLocalVariableAdd', asset=asset, function='LoopCascade', name=name, type=kind, default='')
        call('EditTurboMCPCompile', assets=asset)
        # Stock macro actions may be absent until their owning Blueprint is loaded.
        # This is an explicit read-only prerequisite probe, not direct node construction.
        call('ReadTurboMCPBlueprintLoad', assets='/Engine/EditorBlueprintResources/StandardMacros.StandardMacros', detail='manifest', graphFilter='', format='read', tokenBudget='4000')
        binary = root / 'TestProject/Binaries/Win64/UnrealEditor-TurboMCP.dll'
        binary_hash = hashlib.sha256(binary.read_bytes()).hexdigest()
        cache_path = root / '.local/tidy-cascade' / ('discovery-' + binary_hash + '.json')
        cached = json.loads(cache_path.read_text()) if cache_path.exists() else None
        if cached and cached.get('signature_version') == 1:
            old_name = cached['asset'].rsplit('/', 1)[-1]
            new_name = asset.rsplit('/', 1)[-1]
            for field in ('discovery', 'types', 'pins'):
                evidence[field] = json.loads(json.dumps(cached[field]).replace(old_name, new_name))
            evidence['discovery_reused_from'] = cached['evidence']
            evidence['discovery_binary_sha256'] = binary_hash
        else:
            queries = ['PathPoints', 'PathTransform', 'SamplePoints', 'InnerDistance', 'FalloffDistance', 'FallbackWeight', 'Scale', 'FlatPoints', 'WorkWeights', 'BestDistSq', 'ResultScale', 'Length', 'MakeLiteralInt', 'GreaterEqual', 'Branch', 'Clear', 'ForEachLoop', 'TransformLocation', 'BreakVector', 'MakeVector', 'Array_Add', 'Subtract', 'ForLoop', 'Get (a copy)', 'Add', 'Equal', 'Select', 'FindClosestPointOnSegment', 'Multiply', 'Less', 'Sqrt', 'MapRangeClamped', 'MakeLiteralDouble', 'FClamp']
            evidence['discovery'] = {}
            for query in queries:
                answer = call('PreviewTurboMCPNodeType', asset=asset, graphName='LoopCascade', words=query, limit='200' if query == 'Scale' else '40')
                evidence['discovery'][query] = answer
            # Resolve exact IDs from returned discovery rows, then inspect actual pins.
            exact = {
                'Length': 'Utilities|Array|Length', 'GreaterEqual': 'Utilities|Operators|GreaterEqual(>=)',
                'Branch': 'Utilities|FlowControl|Branch', 'Clear': 'Utilities|Array|Clear',
                'ForEachLoop': 'Utilities|Array|ForEachLoop', 'Array_Add': 'Utilities|Array|Add',
                'Subtract': 'Utilities|Operators|Subtract', 'ForLoop': 'Utilities|FlowControl|ForLoop',
                'Get (a copy)': 'Utilities|Array|Get(acopy)', 'Add': 'Utilities|Operators|Add',
                'Equal': 'Utilities|Operators|Equal(==)', 'Select': 'Utilities|Select',
                'Multiply': 'Utilities|Operators|Multiply', 'Less': 'Utilities|Operators|Less(<)'}
            evidence['types'] = {}
            evidence['pins'] = {}
            for query, answer in evidence['discovery'].items():
                rows = answer.splitlines()
                if query in exact:
                    candidates = [x for x in rows if x == exact[query]]
                elif query in ('PathPoints', 'PathTransform', 'SamplePoints', 'InnerDistance', 'FalloffDistance', 'FallbackWeight', 'Scale'):
                    candidates = [x for x in rows if x.startswith('FunctionInput|') and x.endswith('|LoopCascade|' + query)]
                elif query in ('FlatPoints', 'WorkWeights', 'BestDistSq', 'ResultScale'):
                    candidates = [x for x in rows if x.startswith('Variable|' + asset) and x.endswith(('|Get|' + query, '|Set|' + query))]
                else:
                    candidates = [x for x in rows if x.startswith('Function|/Script/Engine.') and x.endswith(':' + query)]
                if not candidates:
                    raise RuntimeError('Missing unambiguous discovered creation ID: ' + query)
                evidence['types'][query] = candidates
                for candidate in candidates:
                    evidence['pins'][candidate] = call('PreviewTurboMCPNodePins', asset=asset, graphName='LoopCascade', typeId=candidate)
            cache_path.write_text(json.dumps(dict(signature_version=1, asset=asset, evidence=str(directory), **{k:evidence[k] for k in ('discovery','types','pins')}), indent=2), encoding='utf-8')
        evidence['initial_graph'] = call('ReadTurboMCPGraphLoad', asset=asset, graphName='LoopCascade', format='read', detail='full', offset=0, maxChars=24000, revision='', entryFilter='')
        build_graph(call, evidence, persist)
        if os.environ.get('TMCP_TIDY_CASCADE_ROUTING_ONLY') == '1':
            # Reuse the real field fixture, but skip unrelated J1-J6 and local
            # promotion checks. The routing checkpoint retains topology and undo.
            record_routing_candidate(call, evidence, persist, case='cascade')
            evidence['stage'] = 'focused_cascade_routing_passed_visual_pending'
            persist()
            print('Evidence: ' + str(directory), flush=True)
            print('RESULT: PASS (cascade routing only; visual review pending)', flush=True)
            return
        record_baseline(call, evidence, persist)
        record_scoped_loop_boundary(call, evidence, persist)
        if subset == 'boundary':
            evidence['stage'] = 'scoped_loop_boundary_passed_full_cascade_pending'
            persist()
            print('Evidence: ' + str(directory), flush=True)
            print('RESULT: PASS (focused J5 boundary only; full cascade and visual acceptance remain pending)')
            return
        record_loop_locals(call, evidence, persist)
        record_scoped_regression(call, evidence, persist)
        record_break_regression(call, evidence, persist)
        record_event_regressions(call, evidence, persist)
        record_compact_candidates(call, evidence, persist)
        record_routing_candidate(call, evidence, persist, case='cascade')
        record_loop_locals(call, evidence, persist, keep_for_layout=True)
        record_generated_comments(call, evidence, persist, case='cascade')
        local_row = evidence['explicit_layout_locals']
        undo_preview = call('PreviewTurboMCPAssetUndo', asset=evidence['asset'])
        confirmation = re.search(r'Confirm=([0-9A-Fa-f-]+)', undo_preview)
        if not confirmation:
            raise RuntimeError('Missing combined promotion undo confirmation')
        local_row['undo'] = call('EditTurboMCPAssetUndo', asset=evidence['asset'], confirm=confirmation.group(1))
        local_row['undo_restored'] = read_positions(call,evidence['asset'],'LoopCascade') == local_row['before_positions'] and read_complete_graph(call,evidence['asset'],'LoopCascade') == local_row['before_graph']
        persist()
        if not local_row['undo_restored']:
            raise RuntimeError('Combined promotion/layout undo did not restore the original graph')
        record_generated_comments(call, evidence, persist, case='J3')
        record_comment_regression(call, evidence, persist)
        record_compact_candidates(call, evidence, persist, comments_only=True)
        record_routing_candidate(call, evidence, persist)
        evidence['stage'] = 'workflow_checks_passed_visual_acceptance_pending'
        persist()
        print('Evidence: ' + str(directory), flush=True)
        print('RESULT: PASS (baseline evidence checkpoint only; layout acceptance remains false)')
    except Exception as error:
        evidence['failure'] = str(error)
        evidence['stage'] = 'failed'
        persist()
        print('Evidence: ' + str(directory), flush=True)
        raise
