"""Require English working text by refusing CJK ideographs in candidate text and paths."""
import pathlib,re,subprocess,sys
ROOT=pathlib.Path(__file__).resolve().parents[1]
CJK=re.compile('[\u3400-\u4dbf\u4e00-\u9fff\uf900-\ufaff\U00020000-\U0003134f]')
def main():
    files=subprocess.check_output(['git','-C',str(ROOT),'ls-files','--cached','--others','--exclude-standard','-z']).decode().split('\0')
    hits=[];count=0
    for rel in sorted(set(files)-{''}):
        if CJK.search(rel):hits.append(rel+': filename')
        p=ROOT/rel
        if not p.is_file():continue
        try:text=p.read_text(encoding='utf-8-sig')
        except UnicodeError:continue
        count+=1
        for n,line in enumerate(text.splitlines(),1):
            if CJK.search(line):hits.append(f'{rel}:{n}')
    if hits:raise ValueError('CJK ideographs in active text: '+', '.join(hits))
    print(f'PASS: {count} candidate text files; no CJK ideographs. Binary assets, ZIP contents and Git history are not language-certified.')
if __name__=='__main__':
    try:main()
    except (ValueError,OSError,subprocess.SubprocessError) as exc:sys.exit(str(exc))
