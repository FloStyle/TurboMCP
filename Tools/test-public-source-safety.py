"""Exercise public export boundaries in disposable Git repositories."""

import hashlib
import importlib.util
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch


ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location('public_export', ROOT / 'Tools/export-public.py')
EXPORT = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(EXPORT)


class ExportSafety(unittest.TestCase):
    def setUp(self):
        scratch = tempfile.TemporaryDirectory(prefix='tmcp-public-source-')
        self.addCleanup(scratch.cleanup)
        self.scratch = Path(scratch.name)
        self.repo = self.scratch / 'source'
        self.repo.mkdir()
        self.git('init', '--quiet')
        for key, value in (('user.name', 'Export Fixture'), ('user.email', 'export@example.invalid'),
                           ('core.autocrlf', 'false'), ('commit.gpgsign', 'false')):
            self.git('config', key, value)
        for path in ('LICENSE', 'Plugins/TurboMCP/LICENSE'):
            self.write(path, b'Synthetic license\n')
        notice = {'license': 'MPL-2.0', 'copyright': 'Fixture',
                  'provenance': 'Synthetic fixture', 'status': 'repository_notice'}
        self.write_json('Plugins/TurboMCP/NOTICES.json',
                        {'schema': 1, 'files': dict.fromkeys(('LICENSE', 'NOTICES.json'), notice)})
        self.write('release/export-policy.json', (ROOT / 'release/export-policy.json').read_bytes())
        self.write('Tools/run-example.sh', b'#!/bin/sh\n')
        self.git('add', '-A')
        self.git('update-index', '--chmod=+x', 'Tools/run-example.sh')
        self.git('commit', '--quiet', '-m', 'Synthetic source')
        scope = patch.multiple(EXPORT, ROOT=self.repo, POLICY=self.repo / 'release/export-policy.json')
        scope.start()
        self.addCleanup(scope.stop)

    def git(self, *args, data=None):
        return subprocess.run(['git', *args], cwd=self.repo, input=data, capture_output=True,
                              check=True).stdout

    def write(self, relative, data):
        path = self.repo / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)
        return path

    def write_json(self, relative, data):
        return self.write(relative, json.dumps(data).encode('utf8'))

    def preview(self, output):
        with patch.object(sys, 'argv', ['export-public.py', '--preview', '--output', str(output)]):
            EXPORT.main()

    def test_preview_copies_one_staged_tree_and_writes_identity(self):
        head = self.git('rev-parse', 'HEAD').decode().strip()
        staged = b'new staged bytes \r\n'
        self.write('docs/example.md', staged)
        self.git('add', 'docs/example.md')
        self.write('untracked.txt', b'local only')
        source_tree = self.git('write-tree').decode().strip()
        output = self.scratch / 'preview'
        self.preview(output)
        manifest = json.loads(output.with_suffix('.manifest.json').read_bytes())
        self.assertTrue(manifest['preview'])
        self.assertEqual((manifest['source_commit'], manifest['source_tree']), (head, source_tree))
        entries = {row['path']: row for row in manifest['files']}
        paths = {p.relative_to(output).as_posix() for p in output.rglob('*') if p.is_file()}
        self.assertEqual(paths, set(entries))
        expected = set(self.git('ls-tree', '-r', '--name-only', '-z', source_tree)
                       .decode().strip('\0').split('\0'))
        self.assertEqual(paths, expected)
        self.assertNotIn('untracked.txt', paths)
        self.assertFalse((output / '.git').exists())
        self.assertEqual((output / 'docs/example.md').read_bytes(), staged)
        for name, row in entries.items():
            data = (output / name).read_bytes()
            self.assertEqual(data, self.git('cat-file', 'blob', f'{source_tree}:{name}'))
            self.assertEqual(row['sha256'], hashlib.sha256(data).hexdigest())
            self.assertEqual(row['bytes'], len(data))
        self.assertEqual(entries['Tools/run-example.sh']['git_mode'], '100755')
        if os.name != 'nt':
            self.assertTrue(os.access(output / 'Tools/run-example.sh', os.X_OK))

    def test_unsafe_git_tree_paths_are_refused(self):
        original = self.git('ls-tree', 'HEAD')
        blob = self.git('rev-parse', 'HEAD:LICENSE').strip()
        subgroup = self.git('mktree', data=b'100644 blob ' + blob + b'\texample.txt\n').strip()
        for extra, reason in (
            (b'100644 blob ' + blob + b'\t.ENV\n', 'Forbidden tracked content'),
            (b'100644 blob ' + blob + b'\tNUL.txt\n', 'Unsafe source path'),
            (b'040000 tree ' + subgroup + b'\ttools\n', 'Case-colliding export paths'),
        ):
            with self.subTest(reason=reason):
                tree = self.git('mktree', data=original + extra).decode().strip()
                with self.assertRaisesRegex(ValueError, reason):
                    EXPORT.source_entries(tree, release=False)

    def test_sensitive_bytes_and_path_are_refused_without_echo(self):
        for relative, canary in (
            ('docs/canary.txt', b'HTTPS://' + b'fixture:never-a-secret@example.invalid'),
            ('docs/canary.txt', b'-----BEGIN ' + b'ENCRYPTED PRIVATE KEY-----'),
            ('docs/' + 'ghp_' + 'A' * 36, b'synthetic filename check'),
        ):
            with self.subTest(kind=relative.endswith('.txt')):
                path = self.write(relative, canary)
                self.git('add', relative)
                output = self.scratch / 'refused'
                with self.assertRaisesRegex(ValueError, 'matching content suppressed') as error:
                    self.preview(output)
                self.assertNotIn(canary.decode(), str(error.exception))
                if not relative.endswith('.txt'):
                    self.assertNotIn(path.name, str(error.exception))
                self.assertFalse(output.exists())
                path.unlink()
                self.git('add', '-A')

    def test_sidecar_is_exclusive_and_git_metadata_is_not_an_output(self):
        output = self.scratch / 'preview'
        manifest = output.with_suffix('.manifest.json')
        with self.assertRaisesRegex(ValueError, 'Git metadata'):
            self.preview(self.repo / '.git' / 'candidate')

        # Simulate another caller winning the sidecar name after preflight.
        source_entries = EXPORT.source_entries

        def entries_then_competing_sidecar(tree, release):
            entries = source_entries(tree, release)
            manifest.write_bytes(b'earlier candidate')
            return entries

        with patch.object(EXPORT, 'source_entries', entries_then_competing_sidecar):
            with self.assertRaises(FileExistsError):
                self.preview(output)
        self.assertEqual(manifest.read_bytes(), b'earlier candidate')

    def test_dangling_manifest_link_does_not_create_its_target(self):
        output = self.scratch / 'preview'
        manifest = output.with_suffix('.manifest.json')
        target = self.scratch / 'missing'
        try:
            manifest.symlink_to(target)
        except OSError as exc:
            self.skipTest(f'Host cannot create symlinks: {exc}')
        with self.assertRaisesRegex(ValueError, 'Manifest output already exists'):
            self.preview(output)
        self.assertTrue(manifest.is_symlink())
        self.assertFalse(target.exists())
        self.assertFalse(output.exists())

    def test_release_refuses_staging_during_real_evidence_check(self):
        engine = '0' * 40
        plugin_tree = self.git('rev-parse', 'HEAD:Plugins/TurboMCP').decode().strip()
        self.write_json('release/status.json', {'readiness': 'ready', 'public_export_approved': True,
                                               'validated_tree': plugin_tree})
        self.write_json('Tests/acceptance.json', {'cases': [{'id': 'TEST', 'required': True,
                        'command': 'fixture', 'existingEquivalent': 'fixture'}]})
        self.write_json('engine.lock.json', {'commit': engine})
        shutil.copyfile(ROOT / 'Tools/release-check.py', self.repo / 'Tools/release-check.py')
        self.git('add', '-A')
        self.git('commit', '--quiet', '-m', 'Synthetic gates')
        head = self.git('rev-parse', 'HEAD').decode().strip()
        data = b'Synthetic acceptance output'
        (self.scratch / 'test.log').write_bytes(data)
        digest = hashlib.sha256(data).hexdigest()
        evidence = self.scratch / 'evidence.json'
        evidence.write_text(json.dumps({
            'schema': 1, 'source_commit': head, 'engine_commit': engine,
            'cases': [{'id': 'TEST', 'status': 'passed', 'exit_code': 0, 'command': 'fixture',
                       'log': 'test.log', 'log_sha256': digest}],
            'artifact': {'path': 'test.log', 'sha256': digest},
        }), encoding='utf8')
        approved = self.scratch / 'approved-synthetic-release'
        with patch.object(sys, 'argv', ['export-public.py', '--output', str(approved),
                                       '--evidence', str(evidence)]):
            EXPORT.main()
        manifest = json.loads(approved.with_suffix('.manifest.json').read_bytes())
        self.assertFalse(manifest['preview'])
        self.assertEqual(manifest['source_tree'], self.git('rev-parse', 'HEAD^{tree}').decode().strip())
        output = self.scratch / 'release'
        run = subprocess.run

        def run_with_concurrent_stage(args, *positional, **kwargs):
            result = run(args, *positional, **kwargs)
            if args[0] == sys.executable and args[1].endswith('release-check.py'):
                self.write('docs/not-in-evidence.md', b'new staged bytes')
                run(['git', 'add', 'docs/not-in-evidence.md'], cwd=self.repo, check=True)
            return result

        with patch.object(sys, 'argv', ['export-public.py', '--output', str(output),
                                       '--evidence', str(evidence)]):
            with patch.object(subprocess, 'run', run_with_concurrent_stage):
                with self.assertRaisesRegex(ValueError, 'Source changed during release verification'):
                    EXPORT.main()
        self.assertFalse(output.exists())


if __name__ == '__main__':
    unittest.main()
