"""Regression tests for bounded execution. No Unreal process is started."""
import importlib.util
import hashlib
import io
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import time
import unittest
from unittest.mock import patch
import zipfile
from execution_runtime import ResourceLocks, run_owned
import package_consumer as consumer

TOOLS=Path(__file__).resolve().parent


class ConsumerAdmissionTests(unittest.TestCase):
    def test_consumer_profile_has_two_fixed_supervised_phases(self):
        spec = importlib.util.spec_from_file_location('executor', TOOLS/'execute-tests.py')
        executor = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(executor)
        root = TOOLS.parent
        head = subprocess.check_output(['git', '-C', str(root), 'rev-parse', 'HEAD'], text=True).strip()
        artifact = {'directory': 'TurboMCP-Test', 'source_commit': '1'*40,
                    'archive_sha256': '2'*64, 'manifest_sha256': '3'*64}
        plan = {'schema': 1, 'commit': head, 'tree_digest': 'fixture', 'profile': 'consumer',
                'suites': [], 'max_seconds': 60, 'consumer_artifact': artifact}
        with patch.object(executor, 'identity', return_value='fixture'):
            executor.validate_request(plan)
            self.assertEqual(executor.commands(plan, 'engine'), [
                [sys.executable, 'Tools/package_consumer.py', phase] for phase in ('bp-only', 'cpp')
            ])
            for change in ({'profile': 'package'}, {'suites': ['test-agent-help']}, {'port': 1234},
                           {'consumer_artifact': {**artifact, 'directory': '../outside'}},
                           {'consumer_artifact': {**artifact, 'archive_sha256': 'missing'}}):
                with self.subTest(change=change), self.assertRaises(ValueError):
                    executor.validate_request({**plan, **change})
        with patch.dict(os.environ, {'TURBOMCP_EXECUTION_CONTEXT': ''}):
            with self.assertRaisesRegex(RuntimeError, 'supervisor context'):
                consumer.admitted_run(root, root/'engine', 'bp-only')

    def test_consumer_archive_validation_and_installed_integrity(self):
        blob = b'known bytes'
        identity = {'bytes': len(blob), 'sha256': hashlib.sha256(blob).hexdigest()}
        files = {f'TurboMCP/{path}': identity for path in consumer.REQUIRED_FILES}
        consumer.validate_file_index(files)
        for path in ('TurboMCP/../escape', 'TurboMCP/nul.txt', 'TurboMCP/foo:stream',
                     'TurboMCP/BINARIES/alternative', 'TurboMCP/Binaries/Win64/UnrealEditor.modules/file'):
            with self.subTest(path=path), self.assertRaises(ValueError):
                consumer.validate_file_index({**files, path: identity})
        buffer = io.BytesIO()
        with zipfile.ZipFile(buffer, 'w') as archive:
            for name in files:
                archive.writestr(name, blob)
        with tempfile.TemporaryDirectory() as temp:
            repo = Path(temp)
            run = repo/'.local/executions'/('a'*32)
            consumer.create_run(repo, run)
            with self.assertRaises(FileExistsError):
                consumer.create_run(repo, run)
            plugins = run/'Plugins'
            plugins.mkdir()
            with zipfile.ZipFile(buffer) as archive:
                with self.assertRaises(ValueError):
                    consumer.validate_zip(archive, {**files, 'TurboMCP/Extra': identity})
                entries = consumer.validate_zip(archive, files)
                consumer.extract(archive, entries, files, plugins)
            self.assertEqual(consumer.verify_installed(plugins, {'files': files}), len(files))
            (plugins/'TurboMCP/LICENSE').write_bytes(b'changed')
            with self.assertRaisesRegex(ValueError, 'file differs'):
                consumer.verify_installed(plugins, {'files': files})

    def test_consumer_build_requires_only_known_module_actions(self):
        log = ('[1/3] Compile [x64] ConsumerCpp.cpp\n[2/3] Link [x64] UnrealEditor-ConsumerCpp.dll\n'
               '[3/3] WriteMetadata ConsumerCppEditor.target [NoUba]\n')
        self.assertEqual(len(consumer.check_build_actions(log, 'ConsumerCpp')), 3)
        with self.assertRaisesRegex(ValueError, 'unexpected compile'):
            consumer.check_build_actions(log.replace('ConsumerCpp.cpp', 'TurboMCP.cpp'), 'ConsumerCpp')
        with self.assertRaisesRegex(RuntimeError, 'Cannot prove'):
            consumer.check_build_actions(log.splitlines()[0], 'ConsumerCpp')

    def test_consumer_metadata_admits_only_stock_owned_project_outputs(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            project = root/'Owned Consumer/ConsumerCpp.uproject'
            engine = root/'engine'
            modules = project.parent/'Binaries/Win64/UnrealEditor.modules'
            receipt = modules.with_name('ConsumerCppEditor.target')
            plugin = project.parent/'Plugins/TurboMCP'/consumer.RECEIPT
            module_outputs = {
                str(modules): {'ModuleNameToFileName': {'ConsumerCpp': 'UnrealEditor-ConsumerCpp.dll'}},
                str(plugin): {'ModuleNameToFileName': {'TurboMCP': Path(consumer.DLL).name}},
            }
            generated = {
                'ProjectFile': str(project), 'Version': None,
                'VersionFile': str(engine/'Engine/Binaries/Win64/UnrealEditor.version'),
                'ReceiptFile': str(receipt), 'MergeModules': False, 'FileToLoadOrderManifest': {},
                'Receipt': {'ProjectFile': str(project), 'TargetName': 'ConsumerCppEditor', 'Platform': 'Win64',
                            'Configuration': 'Development', 'Version': {'BuildId': None}},
                'FileToManifest': module_outputs,
            }
            selected, receipt_path, manifest = consumer.project_metadata_outputs(generated, project, engine, 'engine-id')
            self.assertEqual((receipt_path, manifest), (receipt, modules))
            self.assertEqual(selected, {**generated, 'FileToManifest': {str(modules): module_outputs[str(modules)]}})
            self.assertEqual(generated['FileToManifest'], module_outputs)
            outside = str(engine/'Engine/Binaries/Win64/UnrealEditor.modules')
            for change in ({'ReceiptFile': str(plugin)}, {'Version': {'BuildId': 'write attempt'}},
                           {'FileToManifest': {**module_outputs, outside: module_outputs[str(modules)]}},
                           {'FileToLoadOrderManifest': {outside: {}}}):
                with self.subTest(change=change), self.assertRaises(ValueError):
                    consumer.project_metadata_outputs({**generated, **change}, project, engine, 'engine-id')

    def test_consumer_timeout_preserves_unknown_in_flight_edit(self):
        with tempfile.TemporaryDirectory() as temp:
            repo = Path(temp)
            run = repo/'.local/executions'/('a'*32)
            consumer.create_run(repo, run)
            project = consumer.phase_root(run, 'bp-only')
            project.mkdir()
            ref = {'archive_sha256': 'approved archive', 'manifest_sha256': 'approved manifest'}
            command = consumer.phase_command('bp-only')
            timeout = {'exit_code': 124, 'timed_out': True, 'duration_seconds': 1}
            with (patch.object(consumer, 'artifact_paths', return_value=(repo/'zip', repo/'manifest')),
                  patch.object(consumer, 'verify_artifact_file'),
                  patch.object(consumer, 'validate_file_index'),
                  patch.object(consumer, 'verify_installed', return_value=1),
                  patch.object(consumer, 'consumer_observations', return_value={'listeners': []}),
                  patch.object(consumer, 'check_engine', return_value={}),
                  patch.object(consumer, 'run_owned', side_effect=lambda *args: dict(timeout))):
                (repo/'manifest').write_text('{"files":{}}', encoding='utf-8')
                consumer.record_mutation(project, 'create', 'pending')
                args = (repo, repo/'engine', run, {'consumer_artifact': ref}, command, {}, run/'log', 1)
                pending = consumer.run_phase(*args)
                self.assertTrue(pending['timed_out'])
                self.assertEqual(pending['child_exit_code'], 124)
                self.assertEqual(pending['verdict'], 'outcome_unknown')
                self.assertEqual(pending['after_teardown']['edits'], {'create': 'outcome_unknown'})
                self.assertEqual(pending['after_teardown']['installed_files'], 1)
                consumer.record_mutation(project, 'create', 'completed', 'created')
                self.assertEqual(consumer.run_phase(*args)['verdict'], 'timed_out')


@unittest.skipUnless(os.name=='nt','Windows execution primitives')
class ExecutionTests(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory(prefix='tmcp-execution-')
        self.root=Path(self.temp.name)
        self.env=dict(os.environ, PYTHONPATH=str(TOOLS))

    def tearDown(self):
        self.temp.cleanup()

    def run_child(self,source,timeout=10):
        return run_owned([sys.executable,'-c',source],str(self.root),self.env,str(self.root/'child.log'),timeout)

    def test_exit_codes_are_not_timeouts(self):
        for code in (0,7,124):
            result=self.run_child('raise SystemExit('+str(code)+')')
            self.assertEqual(result['exit_code'],code)
            self.assertFalse(result['timed_out'])

    def test_shared_lock_contends_and_independent_lock_works(self):
        source="from execution_runtime import ResourceLocks; import sys; lock=ResourceLocks(sys.argv[1],[sys.argv[2]]); lock.__enter__()"
        with ResourceLocks(self.root,['engine:shared']):
            blocked=subprocess.run([sys.executable,'-c',source,str(self.root),'engine:shared'],env=self.env,capture_output=True)
            separate=subprocess.run([sys.executable,'-c',source,str(self.root),'engine:other'],env=self.env,capture_output=True)
            self.assertNotEqual(blocked.returncode,0)
            self.assertEqual(separate.returncode,0)
        with ResourceLocks(self.root,['engine:shared']):
            pass

    def test_crashed_owner_releases_lock(self):
        source="from execution_runtime import ResourceLocks; import sys,os; lock=ResourceLocks(sys.argv[1],['engine:shared']); lock.__enter__(); os._exit(9)"
        result=subprocess.run([sys.executable,'-c',source,str(self.root)],env=self.env,capture_output=True)
        self.assertEqual(result.returncode,9)
        with ResourceLocks(self.root,['engine:shared']):
            pass

    def test_timeout_kills_descendants_and_preserves_unrelated_process(self):
        sentinel=subprocess.Popen([sys.executable,'-c','import time;time.sleep(30)'])
        try:
            child="import time;from pathlib import Path;time.sleep(2);Path('escaped.txt').write_text('escaped')"
            source='import subprocess,sys,time;subprocess.Popen([sys.executable,"-c",'+repr(child)+']);time.sleep(30)'
            result=self.run_child(source,0.5)
            self.assertTrue(result['timed_out'])
            self.assertEqual(result['exit_code'],124)
            time.sleep(2.5)
            self.assertFalse((self.root/'escaped.txt').exists())
            self.assertIsNone(sentinel.poll())
        finally:
            sentinel.terminate();sentinel.wait(timeout=5)

    def test_context_requires_live_ancestor_and_matching_engine(self):
        from execution_runtime import canonical
        context=self.root/'context.json'
        context.write_text(json.dumps({'repo':canonical(self.root),'engine':canonical(self.root/'engine'),
                                      'pid':os.getpid(),'token':'test-token'}))
        env=dict(self.env,TURBOMCP_EXECUTION_CONTEXT=str(context),TURBOMCP_EXECUTION_TOKEN='test-token')
        command=[sys.executable,str(TOOLS/'execution_runtime.py'),'--authorize',str(self.root),str(self.root/'engine')]
        valid=subprocess.run(command,env=env,capture_output=True)
        self.assertEqual(valid.returncode,0,valid.stderr)
        bad_engine=subprocess.run(command[:-1]+[str(self.root/'other')],env=env,capture_output=True)
        self.assertNotEqual(bad_engine.returncode,0)
        data=json.loads(context.read_text());data['pid']=0;context.write_text(json.dumps(data))
        stale=subprocess.run(command,env=env,capture_output=True)
        self.assertNotEqual(stale.returncode,0)

    def test_default_plan_and_request_validation(self):
        command=[sys.executable,str(TOOLS/'execute-tests.py')]
        result=subprocess.run(command,capture_output=True,text=True)
        self.assertEqual(result.returncode,0,result.stderr)
        plan=json.loads(result.stdout)
        self.assertEqual(plan['profile'],'fast')
        plan['unexpected_command']='anything'
        spec=importlib.util.spec_from_file_location('executor',TOOLS/'execute-tests.py')
        module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
        with self.assertRaises(ValueError):module.validate_request(plan)
        plan.pop('unexpected_command');plan['tree_digest']='stale'
        with self.assertRaises(ValueError):module.validate_request(plan)

    def test_package_plan_has_fixed_owned_engine_commands(self):
        command = [sys.executable, str(TOOLS/'execute-tests.py'), '--profile', 'package']
        result = subprocess.run(command, capture_output=True, text=True, timeout=30)
        self.assertEqual(result.returncode, 0, result.stderr)
        plan = json.loads(result.stdout)
        self.assertEqual(plan['profile'], 'package')
        self.assertEqual(plan['suites'], [])
        spec = importlib.util.spec_from_file_location('executor', TOOLS/'execute-tests.py')
        executor = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(executor)
        self.assertIn('package', executor.ENGINE_PROFILES)
        with patch.object(executor.shutil, 'which', return_value='pwsh'), \
                patch.dict(os.environ, {'TURBOMCP_ENGINE_DISTRIBUTION': 'source-build'}):
            self.assertEqual(executor.commands(plan, 'owned-engine'), [
                [sys.executable, 'Tools/doctor.py'],
                ['pwsh', '-NoProfile', '-File', 'Tools/package-plugin.ps1', '-Engine', 'owned-engine'],
            ])
        with patch.object(executor.shutil, 'which', return_value='pwsh'), \
                patch.dict(os.environ, {'TURBOMCP_ENGINE_DISTRIBUTION': 'installed-launcher'}):
            self.assertEqual(executor.commands(plan, 'owned-engine'), [
                [sys.executable, 'Tools/engine_distribution.py', '--repo', str(executor.ROOT),
                 '--engine', 'owned-engine', '--json'],
                ['pwsh', '-NoProfile', '-File', 'Tools/package-plugin.ps1', '-Engine', 'owned-engine'],
            ])
        for override in ({'suites': ['test-asset-describe-metadata']}, {'output': 'elsewhere'}):
            with self.subTest(override=override), self.assertRaises(ValueError):
                executor.validate_request({**plan, **override})

    def test_package_helper_refuses_direct_call_before_staging(self):
        tools = self.root / 'Tools'
        tools.mkdir()
        for name in ('package-plugin.ps1', 'execution_runtime.py'):
            shutil.copy2(TOOLS/name, tools/name)
        environment = dict(self.env)
        for name in ('TURBOMCP_EXECUTION_CONTEXT', 'TURBOMCP_EXECUTION_TOKEN'):
            environment.pop(name, None)
        result = subprocess.run(
            ['pwsh', '-NoProfile', '-File', str(tools/'package-plugin.ps1'),
             '-Engine', str(self.root/'unused-engine')],
            cwd=self.root, env=environment, capture_output=True, timeout=20,
        )
        self.assertNotEqual(result.returncode, 0)
        self.assertIn(b'Owned execution context required', result.stdout+result.stderr)
        self.assertFalse((self.root/'release').exists())

    def test_package_stage_preserves_git_blobs_under_crlf_config(self):
        repository = self.root / 'repo'
        plugin = repository / 'Plugin'
        plugin.mkdir(parents=True)
        (repository/'.gitattributes').write_bytes(b'*.txt text eol=crlf\n')
        (plugin/'NOTICE.txt').write_bytes(b'first\r\nsecond\r\n')
        binary = b'\x00\xff\r\n\xfe\n'
        (plugin/'opaque.bin').write_bytes(binary)

        def git(*args):
            return subprocess.check_output(['git', '-C', str(repository), *args])

        git('init', '-q')
        git('config', 'core.autocrlf', 'true')
        git('add', '.')
        git('-c', 'user.name=Test', '-c', 'user.email=test@example.invalid',
            '-c', 'commit.gpgsign=false', 'commit', '-qm', 'raw plugin inputs')
        tree = git('rev-parse', 'HEAD:Plugin').decode('ascii').strip()
        destination = self.root / 'stage'
        driver = self.root / 'stage.ps1'
        # Execute only the production staging functions. No execution context or engine is used.
        driver.write_text("""
param([string] $Helper, [string] $Repository, [string] $Tree, [string] $Destination)
$ErrorActionPreference = 'Stop'
$tokens = $null
$errors = $null
$ast = [System.Management.Automation.Language.Parser]::ParseFile($Helper,[ref]$tokens,[ref]$errors)
if ($errors.Count) { throw 'Package helper parse failed' }
foreach ($definition in $ast.FindAll({
    param($node)
    $node -is [System.Management.Automation.Language.FunctionDefinitionAst]
}, $false)) {
    . ([ScriptBlock]::Create($definition.Extent.Text))
}
[void](Export-TrackedPluginInputs $Repository $Tree $Destination)
""", encoding='utf8')
        result = subprocess.run(['pwsh', '-NoProfile', '-File', str(driver),
                                 str(TOOLS/'package-plugin.ps1'), str(repository), tree, str(destination)],
                                capture_output=True, timeout=30)
        self.assertEqual(result.returncode, 0, result.stdout+result.stderr)
        self.assertEqual(git('cat-file', 'blob', 'HEAD:Plugin/NOTICE.txt'), b'first\nsecond\n')
        self.assertEqual((destination/'NOTICE.txt').read_bytes(), b'first\nsecond\n')
        self.assertEqual((destination/'opaque.bin').read_bytes(), binary)


if __name__=='__main__':
    unittest.main()
