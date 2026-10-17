"""Provenance distinguishes engine/repository commits and includes staged changes."""
import json,os,pathlib,shutil,subprocess,tempfile
ROOT=pathlib.Path(__file__).resolve().parents[1]
pwsh=shutil.which('pwsh')
if not pwsh:raise SystemExit('RESULT: BLOCKED (PowerShell 7 required)')
def git(root,*args):return subprocess.check_output(['git','-C',str(root),*args],stderr=subprocess.STDOUT,text=True).strip()
with tempfile.TemporaryDirectory(prefix='tmcp-provenance-') as temp:
    root=pathlib.Path(temp);repo=root/'repo';engine=root/'engine'
    for p in [repo,engine]:
        p.mkdir();git(p,'init');(p/'identity.txt').write_text(p.name)
        git(p,'add','.');git(p,'-c','user.name=Test','-c','user.email=test@example.invalid','commit','-m',p.name)
    (repo/'identity.txt').write_text('staged change');git(repo,'add','identity.txt')
    artifact=repo/'plugin.dll';artifact.write_bytes(b'MZ-fixture')
    env=os.environ.copy();env.update(TMCP_PROVENANCE_MODULE=str(ROOT/'Tools/ownership.ps1'),TMCP_PROVENANCE_REPO=str(repo),TMCP_PROVENANCE_ENGINE=str(engine),TMCP_PROVENANCE_ARTIFACT=str(artifact))
    script='''$ErrorActionPreference='Stop'; . $env:TMCP_PROVENANCE_MODULE; $a=Get-TurboMCPArtifactVerdict -Path $env:TMCP_PROVENANCE_ARTIFACT; New-TurboMCPBuildProvenance -RepoRoot $env:TMCP_PROVENANCE_REPO -Engine $env:TMCP_PROVENANCE_ENGINE -Target TestEditor -Configuration Development -Platform Win64 -Artifact $a | ConvertTo-Json -Depth 5 -Compress'''
    result=subprocess.run([pwsh,'-NoProfile','-NonInteractive','-Command',script],env=env,text=True,capture_output=True,check=True)
    data=json.loads(result.stdout)
    assert data['SourceCommit']==git(repo,'rev-parse','HEAD')
    assert data['EngineIdentity']==git(engine,'rev-parse','HEAD')
    assert data['SourceCommit']!=data['EngineIdentity']
    assert data['DirtyDiffPresent'] and len(data['DirtyDiffSha256'])==64
    assert pathlib.Path(data['ArtifactPath'])==artifact
    assert data['ArtifactBytes']==len(artifact.read_bytes())
print('RESULT: PASS (distinct engine identity, staged changes, artifact path and bytes)')
