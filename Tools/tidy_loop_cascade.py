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
        queries = ['PathPoints', 'PathTransform', 'SamplePoints', 'InnerDistance', 'FalloffDistance', 'FallbackWeight', 'Scale', 'FlatPoints', 'WorkWeights', 'BestDistSq', 'ResultScale', 'Length', 'MakeLiteralInt', 'GreaterEqual', 'Branch', 'Clear', 'ForEachLoop', 'TransformLocation', 'BreakVector', 'MakeVector', 'Array Add', 'Subtract', 'ForLoop', 'Get (a copy)', 'Add', 'Equal', 'Select', 'FindClosestPointOnSegment', 'Multiply', 'Less', 'Sqrt', 'MapRangeClamped', 'MakeLiteralDouble', 'FClamp']
        evidence['discovery'] = {}
        for query in queries:
            answer = call('PreviewTurboMCPNodeType', asset=asset, graphName='LoopCascade', words=query, limit='40')
            evidence['discovery'][query] = answer
        evidence['stage'] = 'discovery_complete_fixture_not_built'
        persist()
        print('Evidence: ' + str(directory), flush=True)
        print('RESULT: PASS (discovery checkpoint only; layout acceptance remains false)')
    except Exception:
        evidence['stage'] = 'failed'
        persist()
        print('Evidence: ' + str(directory), flush=True)
        raise
