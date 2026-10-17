"""Regressions for encoding failures and scripts missed by the previous CJK-only gate."""
import importlib.util
import pathlib
import tempfile
import unittest

spec = importlib.util.spec_from_file_location('language_policy', pathlib.Path(__file__).with_name('check-language.py'))
policy = importlib.util.module_from_spec(spec)
spec.loader.exec_module(policy)


class LanguagePolicyTests(unittest.TestCase):
    def test_english_punctuation(self):
        self.assertEqual(policy.inspect_text('English text \u2014 with punctuation.\n\tNext line.\r\n'), [])

    def test_other_scripts_and_hidden_controls(self):
        for point in (0x4E2D, 0x3042, 0xAC00, 0x0410, 0x0627, 0x00E9, 0x202E, 0x200B, 0):
            with self.subTest(point=point):
                self.assertTrue(policy.inspect_text('Text ' + chr(point)))

    def test_invalid_text_is_not_skipped(self):
        with tempfile.TemporaryDirectory() as folder:
            path = pathlib.Path(folder) / 'README.md'
            path.write_bytes(b'English\xff')
            issues, binary = policy.inspect_file(path)
            self.assertTrue(issues)
            self.assertFalse(binary)

    def test_binary_requires_known_type_and_signature(self):
        with tempfile.TemporaryDirectory() as folder:
            path = pathlib.Path(folder) / 'fixture.uasset'
            path.write_bytes(b'\xc1\x83\x2a\x9e' + b'\0' * 12)
            self.assertEqual(policy.inspect_file(path), ([], True))
            path.write_bytes(b'Not an Unreal asset')
            self.assertTrue(policy.inspect_file(path)[0])


if __name__ == '__main__':
    unittest.main()
