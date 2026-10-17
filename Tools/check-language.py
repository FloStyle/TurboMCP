"""Check English-source character conventions; see docs/language.md for limits."""
import pathlib
import subprocess
import sys
import unicodedata

ROOT = pathlib.Path(__file__).resolve().parents[1]
BINARY_SUFFIXES = {'.png', '.uasset', '.umap'}


def inspect_text(text):
    issues = []
    for number, line in enumerate(text.split('\n'), 1):
        for char in line:
            category = unicodedata.category(char)
            if (ord(char) > 127 and category[0] in 'LM') or category == 'Cf':
                issues.append(f'line {number}: disallowed character U+{ord(char):04X}')
            elif category == 'Cc' and char not in '\t\r':
                issues.append(f'line {number}: control character U+{ord(char):04X}')
    return issues


def inspect_file(path):
    raw = path.read_bytes()
    suffix = path.suffix.lower()
    if suffix in BINARY_SUFFIXES:
        valid = (suffix == '.png' and raw.startswith(b'\x89PNG\r\n\x1a\n')) or (
            suffix in {'.uasset', '.umap'} and raw.startswith(b'\xc1\x83\x2a\x9e'))
        if not valid:
            return ['invalid binary signature'], False
        return [], True
    try:
        text = raw.decode('utf-8-sig')
    except UnicodeError:
        return ['invalid UTF-8; unknown binary types require explicit review'], False
    return inspect_text(text), False


def main():
    names = subprocess.check_output(
        ['git', '-C', str(ROOT), 'ls-files', '--cached', '--others', '--exclude-standard', '-z']
    ).decode('utf8').split('\0')
    issues, binaries = [], []
    count = 0
    for name in sorted(set(names) - {''}):
        if not name.isascii():
            issues.append(ascii(name) + ': non-ASCII filename')
        path = ROOT / name
        if not path.is_file():
            continue
        failures, binary = inspect_file(path)
        issues.extend(f'{ascii(name)}: {failure}' for failure in failures)
        if binary:
            binaries.append(name)
        else:
            count += 1
    for name in binaries:
        print('BINARY REVIEW REQUIRED: ' + name)
    if issues:
        raise ValueError('\n'.join(issues))
    print(f'PASS: character/encoding checks for {count} candidate text files and all candidate paths.')
    print(f'{len(binaries)} binary files require content review. English prose, Git history and hosted metadata are not certified by character checks.')


if __name__ == '__main__':
    try:
        main()
    except (ValueError, OSError, subprocess.SubprocessError) as exc:
        sys.exit(str(exc))
