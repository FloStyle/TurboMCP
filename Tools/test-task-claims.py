"""Concurrent processes in real disposable Git worktrees share one task claim."""
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from task_claim import current,location,release


class ClaimTests(unittest.TestCase):
    def test_two_worktrees_cannot_dispatch_duplicate_tasks(self):
        with tempfile.TemporaryDirectory(prefix='tmcp-task-claim-') as temp:
            root=Path(temp);first=root/'first';second=root/'second';state=root/'state';first.mkdir()
            def git(*args):return subprocess.check_output(['git','-C',str(first),*args],stderr=subprocess.STDOUT)
            git('init');(first/'fixture').write_text('synthetic')
            git('add','.');git('-c','user.name=Test','-c','user.email=test@example.invalid','commit','-m','test')
            git('worktree','add','--detach',str(second),'HEAD')
            self.assertEqual(location(first,state),location(second,state))
            env=dict(os.environ,PYTHONPATH=str(Path(__file__).resolve().parent))
            code='import sys;from task_claim import claim;claim(sys.argv[1],"R-test","worker",sys.argv[2])'
            children=[subprocess.Popen([sys.executable,'-c',code,str(path),str(state)],cwd=path,env=env,
                                        stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL) for path in (first,second)]
            codes=[p.wait(timeout=20) for p in children]
            self.assertEqual(sum(code==0 for code in codes),1,codes)
            active=current(first,state)
            self.assertEqual(active,current(second,state))
            with self.assertRaises(ValueError):release(first,'wrong',state)
            self.assertEqual(current(first,state),active)
            release(second,active['id'],state)
            self.assertIsNone(current(first,state))


if __name__=='__main__':unittest.main()
