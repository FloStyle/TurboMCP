"""Recovery negative controls use disposable synthetic files, never Unreal assets."""
import json
import os
import subprocess
import sys
import time
from execution_runtime import ResourceLocks
from pathlib import Path
import tempfile
import unittest
from execution_recovery import snapshot,restore,pending,collect_failure


class RecoveryTests(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory(prefix='tmcp-recovery-')
        self.repo=Path(self.temp.name)
        self.run=self.repo/'.local/executions/run';self.run.mkdir(parents=True)
        self.content=self.repo/'TestProject/Content';self.content.mkdir(parents=True)
        (self.content/'fixture').write_bytes(b'original')

    def tearDown(self):self.temp.cleanup()

    @unittest.skipUnless(os.name=='nt','Windows Job Object crash test')
    def test_supervisor_death_preserves_recovery_and_kills_owned_children(self):
        # Launch the real supervisor primitive in another process, then kill its owner.
        child="from pathlib import Path;import time;Path('TestProject/Content/fixture').write_bytes(b'changed');Path('ready').write_text('ready');time.sleep(2);Path('escaped').write_text('escaped');time.sleep(30)"
        driver="""import os,sys
from pathlib import Path
from execution_runtime import ResourceLocks,run_owned
from execution_recovery import snapshot
root=Path(sys.argv[1]);run=root/'.local/executions/run'
with ResourceLocks(root/'locks',['shared-engine']):
    snapshot(root,run)
    run_owned([sys.executable,'-c',sys.argv[2]],str(root),dict(os.environ),str(run/'log'),30)
"""
        env=dict(os.environ,PYTHONPATH=str(Path(__file__).resolve().parent))
        process=subprocess.Popen([sys.executable,'-c',driver,str(self.repo),child],env=env,
                                 stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
        try:
            deadline=time.monotonic()+10
            while not (self.repo/'ready').exists() and time.monotonic()<deadline:
                if process.poll() is not None:break
                time.sleep(0.05)
            self.assertTrue((self.repo/'ready').exists(),'child did not reach the mutation')
            process.kill();process.wait(timeout=5)
            time.sleep(2.5)
            self.assertFalse((self.repo/'escaped').exists(),'owned descendant survived owner death')
            self.assertEqual(pending(self.repo),['run'])
            with ResourceLocks(self.repo/'locks',['shared-engine']):
                restore(self.repo,self.run)
            self.assertEqual((self.content/'fixture').read_bytes(),b'original')
        finally:
            if process.poll() is None:process.kill();process.wait(timeout=5)

    def test_exact_restore_after_changes_deletion_and_addition(self):
        (self.content/'deleted').write_bytes(b'keep')
        unrelated=self.repo/'unrelated';unrelated.write_bytes(b'untouched')
        snapshot(self.repo,self.run)
        (self.content/'fixture').write_bytes(b'changed')
        (self.content/'deleted').unlink()
        (self.content/'new').write_bytes(b'new')
        self.assertEqual(pending(self.repo),['run'])
        self.assertEqual(restore(self.repo,self.run),2)
        self.assertEqual((self.content/'fixture').read_bytes(),b'original')
        self.assertEqual((self.content/'deleted').read_bytes(),b'keep')
        self.assertFalse((self.content/'new').exists())
        self.assertEqual(unrelated.read_bytes(),b'untouched')
        self.assertEqual(pending(self.repo),[])

    def test_completed_recovery_cannot_overwrite_later_work(self):
        snapshot(self.repo,self.run)
        restore(self.repo,self.run)
        (self.content/'fixture').write_bytes(b'later work')
        with self.assertRaises(ValueError):restore(self.repo,self.run)
        self.assertEqual((self.content/'fixture').read_bytes(),b'later work')

    def test_corrupt_backup_refuses_before_writing(self):
        snapshot(self.repo,self.run)
        (self.run/'fixture-backup/fixture').write_bytes(b'corrupt')
        (self.content/'fixture').write_bytes(b'current')
        with self.assertRaises(ValueError):restore(self.repo,self.run)
        self.assertEqual((self.content/'fixture').read_bytes(),b'current')
        self.assertEqual(pending(self.repo),['run'])

    def test_foreign_checkout_refused(self):
        snapshot(self.repo,self.run)
        with self.assertRaises(ValueError):restore(self.repo/'other',self.run)

    def test_corrupt_journal_blocks_new_run(self):
        (self.run/'recovery.json').write_text('broken')
        self.assertEqual(pending(self.repo),['run'])

    def test_private_artifact_never_enters_feedback_and_draft_is_immutable(self):
        logs=self.repo/'TestProject/Saved/Logs';logs.mkdir(parents=True)
        secret='SECRET_PROJECT proprietary_function C:/private/work'
        (logs/'session.log').write_text(secret)
        report={'status':'failed','reason':secret,'command':secret}
        summary=collect_failure(self.repo,self.run,report)
        self.assertEqual(summary['artifact_count'],1)
        draft=(self.run/'feedback-draft.md').read_text(encoding='ascii')
        for word in secret.split():self.assertNotIn(word,draft)
        self.assertIn('Not confirmed',draft)
        with self.assertRaises(FileExistsError):collect_failure(self.repo,self.run,report)


if __name__=='__main__':unittest.main()
