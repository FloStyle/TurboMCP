"""Compatibility entry point for the maintained disposable batch workflow.

The historical shared-asset baseline was superseded by native sequential,
refusal, preservation and one-undo checks in test-batch-sequence.py.
Post-mutation failure and recovery blocking are in test-batch-rollback.py.
This entry point does not imply that all B01-B05 release cases are mapped.
"""
import runpy
from pathlib import Path

runpy.run_path(str(Path(__file__).with_name("test-batch-sequence.py")), run_name="__main__")
