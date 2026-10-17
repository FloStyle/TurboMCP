"""Check the actual engine root and revision without modifying the engine."""
import json, pathlib, subprocess, sys
from tmcp_paths import REPO, find_engine, engine_version
def main():
    root=pathlib.Path(REPO); lock=json.loads((root/'engine.lock.json').read_text())
    engine=pathlib.Path(find_engine()).resolve()
    sha=subprocess.check_output(['git','-C',str(engine),'rev-parse','HEAD'],text=True).strip()
    if sha!=lock['commit']:raise ValueError(f'Engine commit differs from engine.lock.json: {sha}')
    if engine_version(str(engine))!=lock['version']:raise ValueError('Engine version mismatch')
    p=root/'TestProject/TurboMCPTest.uproject';data=json.loads(p.read_text())
    paths=[(p.parent/x).resolve() for x in data.get('AdditionalPluginDirectories',[])]
    if (root/'Plugins').resolve() not in paths:raise ValueError('Wrong plugin discovery directory')
    if not (root/'Plugins/TurboMCP/TurboMCP.uplugin').is_file():raise ValueError('Missing plugin')
    print(f'PASS: engine {lock["version"]} at {engine}; commit {sha}')
    print('PASS: test project discovers the plugin from this checkout')
    print('NOTE: this verifies paths and revision, not sandbox rights or a successful build')
if __name__=='__main__':
    try:main()
    except (OSError,ValueError,RuntimeError,subprocess.SubprocessError) as exc:sys.exit(f'FAIL: {exc}')
