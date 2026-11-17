"""Check the consumer reply parser against a real edit reply.

Edit replies gained a dirty-package report. The consumer must still compare the
operation's own answer exactly, and must not let the report hide a changed
message. These cases use the reply captured from the bp-only consumer run.
"""

import pathlib
import sys

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

import package_consumer  # noqa: E402

ASSET = '/Game/TurboMCPConsumer/BFL_InstallSmoke'

SUCCESS = '\n'.join([
    'changed',
    'Created Blueprint function library: %s.BFL_InstallSmoke' % ASSET,
    'Parent: /Script/Engine.BlueprintFunctionLibrary',
    'Generated class: %s.BFL_InstallSmoke_C' % ASSET,
    'Unsaved changes: yes. No disk save was requested. Inspect the library before saving; '
    'do not automatically retry creation.',
    'dirty_packages: before=2 after=3 newly_dirty=1 no_longer_dirty=0',
    '  newly_dirty: %s' % ASSET,
    package_consumer.DIRTY_REVIEW,
])

REFUSAL = '\n'.join([
    'unchanged',
    'refused: The destination package already exists in memory, the registry or on disk: %s' % ASSET,
])


def expect_error(callable_, label):
    try:
        callable_()
    except ValueError:
        return 0
    print('FAIL: %s was accepted' % label)
    return 1


def main():
    failures = 0

    message, dirty = package_consumer.split_edit_reply(SUCCESS)
    if len(message) != 5:
        failures += 1
        print('FAIL: success message has %d line(s), expected 5' % len(message))
    if message[:1] != ['changed']:
        failures += 1
        print('FAIL: success message does not start with changed')
    names = package_consumer.check_dirty_block(dirty, ASSET, require=True)
    if names != [ASSET]:
        failures += 1
        print('FAIL: dirty block named %r' % (names,))

    changed, _ = package_consumer.split_edit_reply('changed\nan extra line\n' + SUCCESS.split('\n', 1)[1])
    if len(changed) != 6:
        failures += 1
        print('FAIL: an added message line was hidden by the report')

    failures += expect_error(
        lambda: package_consumer.check_dirty_block([], ASSET, require=True),
        'a missing dirty report')

    broken = [dirty[0].replace('newly_dirty=1', 'newly_dirty=2')] + dirty[1:]
    failures += expect_error(
        lambda: package_consumer.check_dirty_block(broken, ASSET, require=True),
        'a dirty report whose row count disagrees')

    failures += expect_error(
        lambda: package_consumer.check_dirty_block(dirty, '/Game/Other', require=True),
        'a dirty report that names another asset')

    refusal_message, refusal_dirty = package_consumer.split_edit_reply(REFUSAL)
    if len(refusal_message) != 2 or package_consumer.check_dirty_block(refusal_dirty) != []:
        failures += 1
        print('FAIL: a refusal without a dirty report did not parse')

    if failures:
        print('RESULT: FAIL (%d case(s))' % failures)
        return 1
    print('RESULT: PASS (success, added line, missing report, row mismatch, other asset, refusal)')
    return 0


if __name__ == '__main__':
    sys.exit(main())
