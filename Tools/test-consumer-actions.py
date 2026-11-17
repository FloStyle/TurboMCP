"""Check the consumer build-action proof.

UnrealBuildTool compiles the module's generated PerModuleInline.gen.cpp beside the
module's own translation unit. The proof must accept that generated compile, still
require exactly one module compile and one link, and still refuse any other
compile or link from the engine.
"""

import pathlib
import sys

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

import package_consumer  # noqa: E402

NAME = 'ConsumerCpp'

GOOD = '\n'.join([
    '[1/4] Compile [x64] PerModuleInline.gen.cpp [NoUba]',
    '[2/4] Compile [x64] ConsumerCpp.cpp [NoUba]',
    '[3/4] Link [x64] UnrealEditor-ConsumerCpp.lib [NoUba]',
    '[4/4] Link [x64] UnrealEditor-ConsumerCpp.dll [NoUba]',
])


def expect(callable_, label, error=Exception):
    try:
        callable_()
    except error:
        return 0
    except Exception as unexpected:  # noqa: BLE001
        print('FAIL: %s raised %s instead of %s' % (label, type(unexpected).__name__, error.__name__))
        return 1
    print('FAIL: %s was accepted' % label)
    return 1


def main():
    failures = 0

    actions = package_consumer.check_build_actions(GOOD, NAME)
    if len(actions) != 4:
        failures += 1
        print('FAIL: expected 4 accepted actions, got %d' % len(actions))

    rogue_engine = GOOD.replace('[4/4] Link', '[5/5] Compile [x64] EngineThing.cpp [NoUba]\n[4/4] Link')
    failures += expect(lambda: package_consumer.check_build_actions(rogue_engine, NAME),
                       'an engine compile', ValueError)

    missing_module = GOOD.replace('Compile [x64] ConsumerCpp.cpp [NoUba]\n', '')
    failures += expect(lambda: package_consumer.check_build_actions(missing_module, NAME),
                       'a build without the module compile', RuntimeError)

    duplicate_module = GOOD + '\n[5/5] Compile [x64] ConsumerCpp.cpp [NoUba]'
    failures += expect(lambda: package_consumer.check_build_actions(duplicate_module, NAME),
                       'a duplicate module compile', RuntimeError)

    indented = GOOD.replace('[1/4]', '  [1/4]')
    failures += expect(lambda: package_consumer.check_build_actions(indented, NAME),
                       'an action line that is not at the start of a line', RuntimeError)

    if failures:
        print('RESULT: FAIL (%d case(s))' % failures)
        return 1
    print('RESULT: PASS (generated compile accepted, engine compile, missing, duplicate and indented refused)')
    return 0


if __name__ == '__main__':
    sys.exit(main())
