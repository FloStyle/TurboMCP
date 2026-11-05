"""Evidence-first builder for the neutral loop-cascade field scenario.

All editor calls use the public registry. A lost response is never replayed.
Discovery is a checkpoint, not successful acceptance of the layout scenario.
"""
import json
import os
from pathlib import Path
import re
import uuid
from execution_runtime import require_context
from tmcp import Session, toolset_for, READ_TOOLSET, PREVIEW_TOOLSET, EDIT_TOOLSET, LOOK_TOOLSET


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
        evidence['initial_graph'] = call('ReadTurboMCPGraphLoad', asset=asset, graphName='LoopCascade', format='read', detail='full', offset=0, maxChars=24000, revision='', entryFilter='')
        evidence['stage'] = 'pins_verified_fixture_not_built' 
        persist()
        print('Evidence: ' + str(directory), flush=True)
        print('RESULT: PASS (discovery checkpoint only; layout acceptance remains false)')
    except Exception:
        evidence['stage'] = 'failed'
        persist()
        print('Evidence: ' + str(directory), flush=True)
        raise
