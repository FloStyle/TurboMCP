"""Negative controls for duplicate discovery and stale binary acceptance."""
import hashlib
import json
from pathlib import Path
import tempfile
import unittest
from plugin_identity import check_discovery,verify_binary


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
            with self.assertRaises(ValueError):verify_binary(provenance,other)
            built.write_bytes(b'changed')
            with self.assertRaises(ValueError):verify_binary(provenance,built)


if __name__=='__main__':unittest.main()
