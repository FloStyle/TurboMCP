"""Regression tests for bounded execution. No Unreal process is started."""
import importlib.util
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import time
import unittest
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

    def test_default_plan_and_engine_execution_refusal(self):
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


if __name__=='__main__':
    unittest.main()
