"""Exercise the repository client's retry control flow without network or editor access.

HISTORICAL PROBE. This file produced the `DEFECT_CONFIRMED` record in
`audit-verification.json`: one simulated effect followed by a lost response caused a reconnect and a
SECOND effect. It asserted `p.effects == 2`, because that was the defect.

RELEASE FINDING R02 HAS SINCE BEEN REPAIRED, so the assertion below is now the regression guard in the
opposite direction: exactly one effect, and an unknown outcome reported instead of a replay. The current
acceptance for C01/C02 lives in `Tools/test-client-retry.py`, which measures the same control flow with
more cases; this file is kept because it is the historical evidence for the finding and because deleting it
would erase the record of what was measured.
"""

import importlib.util, json
p = r'C:\Users\Example\Desktop\TurboMCP\Tools\tmcp.py'
spec = importlib.util.spec_from_file_location('audit_tmcp', p)
m = importlib.util.module_from_spec(spec)
spec.loader.exec_module(m)


class Probe(m.Session):
    def __init__(self):
        super().__init__()
        self.effects = 0
        self.reconnects = 0

    def connect(self):
        self.reconnects += 1
        return {}

    def _rpc_once(self, method, params=None, request_id=1):
        self.effects += 1
        if self.effects == 1:
            raise TimeoutError('Simulated response loss AFTER server-side effect')
        return {'ok': True}


p = Probe()
unknown = None
try:
    p.rpc('tools/call',
          {'name': 'call_tool',
           'arguments': {'toolset_name': m.EDIT_TOOLSET,
                         'tool_name': 'EditTurboMCPGraphRepoint',
                         'arguments': {}}},
          2)
except m.UnknownOutcomeError as exc:
    unknown = str(exc)

print(json.dumps({
    'test': 'client does not replay after an ambiguous failure (R02, repaired)',
    'simulated_effects': p.effects,
    'reconnects': p.reconnects,
    'unknown_outcome': bool(unknown),
    'scope': 'Real rpc control flow with mocked I/O. No editor calls.'}, indent=2))
assert p.effects == 1, "a mutation was replayed after its response was lost"
assert p.reconnects == 1, "the reconnect was lost along with the replay"
assert unknown, "the caller was not told the outcome is unknown"
