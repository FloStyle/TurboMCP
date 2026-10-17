"""Exercise report lint against synthetic disclosure hazards only."""
import importlib.util
import pathlib
import unittest
import subprocess
import sys
import tempfile

root = pathlib.Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('report_check', root / 'skills/turbomcp-session-report/scripts/check_report.py')
checker = importlib.util.module_from_spec(spec)
spec.loader.exec_module(checker)


def report():
    fields = '\n'.join('**' + f + ':** Unknown' for f in checker.FIELDS)
    fields = fields.replace('**Review state:** Unknown', '**Review state:** Pending human review')
    return '# TurboMCP session report\n' + fields + '\n' + '\n'.join('## ' + s + '\nNone observed.\n' for s in checker.SECTIONS)


class ReportTests(unittest.TestCase):
    def test_generic_report(self):
        self.assertEqual(checker.check(report()), [])

    def test_disclosure_hazards(self):
        samples = (r'C:\Synthetic\asset.txt', '/Game/Synthetic', 'BP_Synthetic',
                   'https://example.invalid', 'person@example.invalid', '```python',
                   '0123456789abcdef0123456789abcdef', '{"input": 1}', chr(0x4E2D))
        for sample in samples:
            with self.subTest(sample=ascii(sample)):
                self.assertTrue(checker.check(report() + sample))

    def test_duplicate_ids_and_missing_structure(self):
        self.assertTrue(checker.check(report() + '\n**GL-07: One**\n**GL-07: Two**\n'))
        self.assertTrue(checker.check('# Empty report\n'))

    def test_cli_does_not_echo_rejected_content(self):
        with tempfile.TemporaryDirectory() as folder:
            candidate = pathlib.Path(folder) / 'candidate.md'
            marker = 'secret-person@example.invalid'
            candidate.write_text(report() + marker, encoding='ascii')
            result = subprocess.run([sys.executable, str(checker.__file__), str(candidate)], capture_output=True, text=True)
            self.assertNotEqual(result.returncode, 0)
            self.assertNotIn(marker, result.stdout + result.stderr)
            candidate.write_text(report(), encoding='ascii')
            result = subprocess.run([sys.executable, str(checker.__file__), str(candidate)], capture_output=True, text=True)
            self.assertEqual(result.returncode, 0)
            self.assertIn('Human confidentiality review', result.stdout)

    def test_no_confidentiality_certificate(self):
        # Generic ASCII proprietary meaning cannot be detected by a character or pattern check.
        self.assertEqual(checker.check(report() + 'A confidential concept with ordinary words.\n'), [])
        self.assertTrue(checker.check(report().replace('Pending human review', 'Approved')))


if __name__ == '__main__':
    unittest.main()
