"""Regression tests for bounded execution. No Unreal process is started."""
import importlib.util
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
from execution_runtime import ResourceLocks, run_owned

TOOLS=Path(__file__).resolve().parent


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
        with patch.object(executor.shutil, 'which', return_value='pwsh'):
            self.assertEqual(executor.commands(plan, 'owned-engine'), [
                [sys.executable, 'Tools/doctor.py'],
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


if __name__=='__main__':
    unittest.main()
