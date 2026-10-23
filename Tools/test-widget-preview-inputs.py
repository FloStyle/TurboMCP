"""Validate preview refusal order; never construct or render a widget."""
from tmcp import Session, LOOK_TOOLSET
s = Session()
s.connect()
def call(size, execution='allow', samples=''):
    return s.call(LOOK_TOOLSET, 'LookTurboMCPWidgetPreview', {'widget':'/Game/Tests/DefinitelyMissingQualityWidget', 'size':size, 'execution':execution, 'sampleValuesJson':samples, 'functionName':''})
for size in ('1.5x720', '1280x0.5', '-1x720', '+1x720', '0x720', '4097x720', '999999999999999999999x720', '1x2x3'):
    result=call(size)
    assert result.startswith('error:') and 'whole numbers from 1 to 4096' in result and 'no widget was loaded or constructed' in result, (size,result)
for consent in ('', 'deny', 'maybe'):
    result=call('1280x720',consent)
    assert result.startswith('error:') and 'no widget was loaded or constructed' in result, result
# Valid size forms must reach JSON validation, proving they passed size validation
# without relying on an asset lookup failure or executing project code.
for size in ('', '1x1', '4096x4096', ' 1280 x 720 ', '640,360'):
    result=call(size,samples='[]')
    assert 'sample values are not a JSON object' in result, (size,result)
print('RESULT: PASS (size and consent refusals; no rendering or mutation coverage)')
