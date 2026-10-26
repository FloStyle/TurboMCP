"""Exercise the actual stop function with synthetic process records only."""
import json
import os
from pathlib import Path
import shutil
import subprocess
import unittest

ROOT=Path(__file__).resolve().parents[1]
PWSH=shutil.which('pwsh')

class CleanupTests(unittest.TestCase):
    def probe(self, lease):
        script=r"""
$ErrorActionPreference='Stop'
$tokens=$null;$errors=$null
$tree=[System.Management.Automation.Language.Parser]::ParseFile($env:TMCP_SCRIPT,[ref]$tokens,[ref]$errors)
if($errors.Count){throw 'Invalid PowerShell syntax'}
$fn=$tree.Find({param($n) $n -is [System.Management.Automation.Language.FunctionDefinitionAst] -and $n.Name -eq 'Stop-TurboMCPOwnedEditor'},$true)
Invoke-Expression $fn.Extent.Text
$script:lease=$env:TMCP_LEASE | ConvertFrom-Json
$script:inventory=0;$script:stopped=0;$script:removed=0
$OwnerId='test-owner';$RepoRoot='synthetic'
function Read-TurboMCPLease { param($RepoRoot) return $script:lease }
function Get-TurboMCPRunningEditors { param($ProjectPath,$EnginePath) $script:inventory++; return @{Owned=@();Foreign=@()} }
function Get-Process { return [pscustomobject]@{Id=123} }
function Stop-Process { $script:stopped++ }
function Remove-TurboMCPLease { param($RepoRoot,$OwnerId) $script:removed++ }
function Warn { param($message) }
function Fail { param($message) }
$result=Stop-TurboMCPOwnedEditor -ProjectPath synthetic -EnginePath synthetic
@{result=$result;inventory=$script:inventory;stopped=$script:stopped;removed=$script:removed} | ConvertTo-Json -Compress
"""
        env=dict(os.environ,TMCP_SCRIPT=str(ROOT/'Tools/start-editor.ps1'),TMCP_LEASE=json.dumps(lease))
        result=subprocess.run([PWSH,'-NoProfile','-Command',script],env=env,capture_output=True,text=True,timeout=20)
        self.assertEqual(result.returncode,0,result.stderr)
        return json.loads(result.stdout.splitlines()[-1])

    def test_foreign_or_corrupt_lease_refuses_before_process_access(self):
        for lease in [{'OwnerId':'another-owner'},{'Unreadable':True}]:
            result=self.probe(lease)
            self.assertFalse(result['result'])
            self.assertEqual(result['inventory'],0)
            self.assertEqual(result['stopped'],0)
            self.assertEqual(result['removed'],0)

    def test_owned_idle_lease_releases_without_global_livecoding_stop(self):
        result=self.probe({'OwnerId':'test-owner'})
        self.assertTrue(result['result'])
        self.assertEqual(result['stopped'],0)
        self.assertEqual(result['removed'],1)

    def test_suite_cleanup_does_not_kill_pid_inferred_descendants(self):
        source=(ROOT/'Tools/run-suites.ps1').read_text(encoding='utf-8-sig')
        body=source.split('function Stop-Editor {',1)[1].split('function ',1)[0]
        self.assertNotIn('Get-TurboMCPOwnedDescendants',body)
        self.assertNotIn('Stop-Process',body)

    def test_changed_powershell_files_parse(self):
        script=r"""
$bad=@()
Get-ChildItem -LiteralPath $env:TMCP_TOOLS -Filter '*.ps1' | ForEach-Object {
  $tokens=$null;$errors=$null
  [void][System.Management.Automation.Language.Parser]::ParseFile($_.FullName,[ref]$tokens,[ref]$errors)
  if($errors.Count){$bad+=$_.Name}
}
if($bad.Count){throw ($bad -join ', ')}
"""
        result=subprocess.run([PWSH,'-NoProfile','-Command',script],env=dict(os.environ,TMCP_TOOLS=str(ROOT/'Tools')),capture_output=True,text=True,timeout=20)
        self.assertEqual(result.returncode,0,result.stderr)

if __name__=='__main__':
    if not PWSH:raise SystemExit('BLOCKED: PowerShell 7 required')
    unittest.main()
