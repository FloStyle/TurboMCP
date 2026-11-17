"""Negative controls for duplicate discovery and stale binary acceptance."""
import hashlib
import json
from pathlib import Path
import tempfile
import unittest
from plugin_identity import (check_discovery, check_installed_discovery, verify_binary,
                             verify_consumer_listener, verify_consumer_process)


class IdentityTests(unittest.TestCase):
    def test_duplicate_descriptor_is_not_accepted(self):
        with tempfile.TemporaryDirectory() as temp:
            root=Path(temp);repo=root/'repo';engine=root/'engine'
            plugin=repo/'Plugins/TurboMCP/TurboMCP.uplugin';plugin.parent.mkdir(parents=True)
            descriptor={'Modules':[{'Name':'TurboMCP'}]};plugin.write_text(json.dumps(descriptor))
            project=repo/'TestProject/TurboMCPTest.uproject';project.parent.mkdir()
            project.write_text(json.dumps({'AdditionalPluginDirectories':['../Plugins']}))
            check_discovery(repo,engine)
            duplicate=engine/'Engine/Plugins/Marketplace/Other/Other.uplugin';duplicate.parent.mkdir(parents=True)
            duplicate.write_text(json.dumps(descriptor))
            with self.assertRaises(ValueError):check_discovery(repo,engine)

    def test_same_name_or_bytes_do_not_replace_the_expected_artifact(self):
        with tempfile.TemporaryDirectory() as temp:
            root=Path(temp);built=root/'built.dll';other=root/'old.dll'
            built.write_bytes(b'built');other.write_bytes(b'built')
            provenance={'ArtifactPath':str(built),'ArtifactSha256':hashlib.sha256(b'built').hexdigest()}
            verify_binary(provenance,built)

    def test_installed_consumer_requires_exact_discovery_and_loaded_path(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            project = root/'Consumer With Spaces'/'Consumer.uproject'
            engine = root/'engine'
            descriptor = project.parent/'Plugins/TurboMCP/TurboMCP.uplugin'
            dll = descriptor.parent/'Binaries/Win64/UnrealEditor-TurboMCP.dll'
            dll.parent.mkdir(parents=True)
            data = json.dumps({'Modules': [{'Name': 'TurboMCP'}]})
            descriptor.write_text(data)
            dll.write_bytes(b'reviewed')
            check_installed_discovery(project, engine)
            for path, filename, contents in (
                ('Plugins', 'Renamed.uplugin', '{"Modules":[{"Name":"TurboMCP"},],}'),
                ('Platforms/Win64/Plugins', 'Renamed.uplugin', data),
                ('Restricted/NoRedist/Platforms/Win64/Plugins', 'TurboMCP_Win64.uplugin', '{"bIsPluginExtension":true}'),
            ):
                duplicate = engine/'Engine'/path/'Other'/filename
                duplicate.parent.mkdir(parents=True)
                duplicate.write_text(contents)
                with self.subTest(path=path), self.assertRaisesRegex(ValueError, 'exactly one'):
                    check_installed_discovery(project, engine)
                duplicate.unlink()

            exe = engine/'UnrealEditor.exe'
            expected = {dll: hashlib.sha256(b'reviewed').hexdigest()}
            observed = {'process': {'ProcessId': 7, 'ExecutablePath': str(exe),
                                    'CommandLine': f'"{exe}" "{project}" -unattended'},
                        'modules': [str(dll)]}
            self.assertEqual(verify_consumer_process(observed, 7, exe, project, expected)[dll.name]['path'], str(dll))
            alternate = root/dll.name
            alternate.write_bytes(b'reviewed')
            with self.assertRaisesRegex(ValueError, 'differs from the build artifact'):
                verify_consumer_process({**observed, 'modules': [str(alternate)]}, 7, exe, project, expected)
            with self.assertRaisesRegex(RuntimeError, 'could not be enumerated'):
                verify_consumer_process({**observed, 'modules': None}, 7, exe, project, expected)
            dll.write_bytes(b'changed!')
            with self.assertRaisesRegex(ValueError, 'hash differs'):
                verify_consumer_process(observed, 7, exe, project, expected)

    def test_consumer_listener_rejects_foreign_pid_and_wildcard(self):
        own = {'LocalAddress': '127.0.0.1', 'OwningProcess': 7}
        self.assertTrue(verify_consumer_listener([own, {**own, 'LocalAddress': '::1'}], 7))
        self.assertFalse(verify_consumer_listener([], 7))
        for foreign in ({**own, 'OwningProcess': 8}, {**own, 'LocalAddress': '0.0.0.0'}):
            with self.subTest(foreign=foreign), self.assertRaises(ValueError):
                verify_consumer_listener([own, foreign], 7)
        with self.assertRaisesRegex(RuntimeError, 'already has a listener'):
            verify_consumer_listener([own], None)
            with self.assertRaises(ValueError):verify_binary(provenance,other)
            built.write_bytes(b'changed')
            with self.assertRaises(ValueError):verify_binary(provenance,built)


if __name__=='__main__':unittest.main()
