"""Evidence-first builder for the neutral loop-cascade field scenario.

All editor calls use the public registry. A lost response is never replayed.
Discovery is a checkpoint, not successful acceptance of the layout scenario.
"""
import json
import hashlib
import os
from pathlib import Path
import re
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
        args = dict(asset=asset, graphName=graph, text=text, continuation='auto', placement='local')
        preview = call('PreviewTurboMCPGraphAppend', **args)
        if not preview.startswith('preview:'):
            raise RuntimeError('Append preview did not approve construction: ' + preview)
        result = call('EditTurboMCPGraphAppend', **args)
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



def record_scoped_regression(call, evidence, persist):
    """J5: record a scoped baseline and require every outside rectangle to stay fixed.

    This checks the scope boundary, not compact-layout acceptance. Restore the
    original baseline coordinates after recording the scoped result.
    """
    asset, graph = evidence['asset'], 'LoopCascade'
    labels = next(block['labels'] for block in evidence['blocks'] if block['name'] == 'L3B')
    selected = [evidence['label_ids'][label] for label in labels]

    def positions():
        result, offset = {}, 0
        while True:
            page = call('LookTurboMCPGraphPositions', asset=asset, graphName=graph,
                        nodeIds='', maxNodes=256, offset=offset)
            for match in re.finditer(r'^(\S+) x=(-?\d+) y=(-?\d+) width=(\d+) height=(\d+)$', page, re.M):
                identity, *values = match.groups()
                result[identity] = dict(zip(('x', 'y', 'width', 'height'), map(int, values)))
            continuation = re.search(r'next_offset=(-?\d+)', page)
            if not continuation:
                raise RuntimeError('J5 positions have no explicit continuation')
            offset = int(continuation.group(1))
            if offset == -1:
                return result

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
    evidence = {'stage': 'discovery', 'layout_acceptance': False, 'calls': []}

    def persist():
        (directory / 'evidence.json').write_text(json.dumps(evidence, indent=2), encoding='utf-8')

    def call(tool, **args):
        row = {'tool': tool, 'arguments': args, 'outcome': 'pending'}
        evidence['calls'].append(row)
        persist()
        answer = session.call(toolset_for(tool), tool, args)
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
        call('ReadTurboMCPStatus')
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
        record_baseline(call, evidence, persist)
        record_scoped_regression(call, evidence, persist)
        persist()
        print('Evidence: ' + str(directory), flush=True)
        print('RESULT: PASS (baseline evidence checkpoint only; layout acceptance remains false)')
    except Exception as error:
        evidence['failure'] = str(error)
        evidence['stage'] = 'failed'
        persist()
        print('Evidence: ' + str(directory), flush=True)
        raise
