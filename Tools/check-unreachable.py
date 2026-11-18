# Copyright (c) 2026 FloStyle
# SPDX-License-Identifier: MPL-2.0
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.
"""Refuse statements left dead by an unconditional terminator in the same block.

A `return`, `continue` or `break` that always runs makes the rest of its block
unreachable. That is an error for projects on BuildSettingsVersion V7 or Latest
(/we4702), so the break reaches a user who builds the plugin from source.

The scan is deliberately conservative and works on source text:

* string, character and raw string literals, `//` comments, `/* */` comments and
  preprocessor lines are ignored. Raw strings are tracked across lines, with
  their delimiter, so a `(` or `return` inside one never counts;
* statements are tracked per brace depth, so a terminator inside an `if` block
  does not mark the rest of the function dead;
* a label (`case X:`, `default:`, `name:`) ends the dead region, because control
  can arrive there from elsewhere. Access specifiers do not count, and `std::` or
  `Name::` are not labels;
* `break` is allowed directly inside a `switch` block.

It reports the line of the terminator and the line of the first dead statement.
"""

import argparse
import pathlib
import re
import sys

SOURCE = pathlib.Path('Plugins/TurboMCP/Source')
TERMINATOR = re.compile(r'^(return|continue|break)\b')
CASE_LABEL = re.compile(r'^(?:case\b[^:]*|default\s*:)\s*')
NAMED_LABEL = re.compile(r'^([A-Za-z_]\w*)\s*:(?!:)')
ACCESS = {'public', 'private', 'protected'}
RAW_START = re.compile(r'(?:u8|u|U|L)?R"([^ ()\\]{0,16})\(')


def strip_label(text):
    """Remove a leading label. Return (rest, is_label)."""
    match = CASE_LABEL.match(text)
    if match:
        return text[match.end():].strip(), True
    match = NAMED_LABEL.match(text)
    if match:
        if match.group(1) in ACCESS:
            return text, False
        return text[match.end():].strip(), True
    return text, False


def strip_code(line, state):
    """Blank literals and comments in one line. Return (code, state).

    ``state`` carries two things between lines: whether a block comment is open,
    and the delimiter of an open raw string so its content stays blank across
    lines.
    """
    out = []
    index = 0
    length = len(line)
    block = state.get('block', False)
    raw = state.get('raw')
    while index < length:
        if raw is not None:
            end = line.find(')' + raw + '"', index)
            if end < 0:
                return ''.join(out), {'block': block, 'raw': raw}
            index = end + len(raw) + 2
            raw = None
            continue
        if block:
            end = line.find('*/', index)
            if end < 0:
                return ''.join(out), {'block': True, 'raw': None}
            index = end + 2
            block = False
            continue
        char = line[index]
        if char == '/' and index + 1 < length and line[index + 1] == '/':
            break
        if char == '/' and index + 1 < length and line[index + 1] == '*':
            block = True
            index += 2
            continue
        if char == '"' or char == '\'':
            quote = char
            index += 1
            while index < length:
                if line[index] == '\\':
                    index += 2
                    continue
                if line[index] == quote:
                    index += 1
                    break
                index += 1
            out.append(' ')
            continue
        match = RAW_START.match(line, index)
        if match and (index == 0 or not (line[index - 1].isalnum() or line[index - 1] == '_')):
            delimiter = match.group(1)
            index = match.end()
            end = line.find(')' + delimiter + '"', index)
            if end < 0:
                return ''.join(out), {'block': False, 'raw': delimiter}
            index = end + len(delimiter) + 2
            continue
        out.append(char)
        index += 1
    return ''.join(out), {'block': block, 'raw': None}


CONTROL = re.compile(r'^(if|for|while|switch|else|do)\b')


def scan_text(text, name='<text>'):
    """Return [(terminator_line, statement_line, terminator)] for dead statements."""
    findings = []
    depth = 0
    kinds = []
    dead = {}
    expected = {}
    buffer = []
    buffer_line = None
    buffer_parens = None
    parens = 0
    state = {'block': False, 'raw': None}
    branches = []

    def take():
        nonlocal buffer_line, buffer_parens
        chunk = ''.join(buffer).strip()
        first = buffer_line
        start = buffer_parens
        buffer.clear()
        buffer_line = None
        buffer_parens = None
        return chunk, first, start

    def handle(chunk, line, start_parens):
        while True:
            chunk, label = strip_label(chunk)
            if not label:
                break
            dead.pop(depth, None)
            expected.pop(depth, None)
            if not chunk:
                return
        if not chunk:
            return
        if start_parens:
            # Inside an expression: a lambda body or an initializer, not a
            # statement of the enclosing block.
            return
        if depth in dead:
            findings.append((dead[depth][0], line, dead[depth][1]))
            del dead[depth]
            return
        if expected.pop(depth, False):
            # The body of a control statement that is not braced.
            return
        if CONTROL.match(chunk):
            if chunk.rstrip().endswith(')'):
                expected[depth] = True
            return
        match = TERMINATOR.match(chunk)
        if match and not (match.group(1) == 'break' and 'switch' in kinds):
            dead[depth] = (line, match.group(1))

    for number, raw_line in enumerate(text.splitlines(), 1):
        code, state = strip_code(raw_line, state)
        directive = code.strip()
        if directive.startswith('#'):
            word = directive[1:].strip().split(' ', 1)[0] if directive[1:].strip() else ''
            if word in ('if', 'ifdef', 'ifndef'):
                branches.append((depth, list(kinds), dict(dead), dict(expected), parens))
            elif word in ('else', 'elif') and branches:
                depth, kinds, dead, expected, parens = branches[-1]
                kinds, dead, expected = list(kinds), dict(dead), dict(expected)
                buffer.clear()
                buffer_line = buffer_parens = None
            elif word == 'endif' and branches:
                depth, kinds, dead, expected, parens = branches.pop()
                kinds, dead, expected = list(kinds), dict(dead), dict(expected)
                buffer.clear()
                buffer_line = buffer_parens = None
            continue
        if not code.strip():
            continue
        for char in code:
            if buffer_line is None and not char.isspace():
                buffer_line = number
                buffer_parens = parens
            if char == '(':
                parens += 1
            elif char == ')':
                parens = max(0, parens - 1)
            elif char == ';' and parens == 0:
                chunk, line, start = take()
                handle(chunk, line, start)
            elif char == '{':
                chunk, line, start = take()
                handle(chunk, line, start)
                if parens == 0:
                    expected.pop(depth, None)
                depth += 1
                kinds.append('switch' if 'switch' in chunk else 'body')
            elif char == '}':
                chunk, line, start = take()
                handle(chunk, line, start)
                expected.pop(depth, None)
                dead.pop(depth, None)
                if kinds:
                    kinds.pop()
                depth = max(0, depth - 1)
                dead.pop(depth, None)
            else:
                buffer.append(char)
    chunk, line, start = take()
    handle(chunk, line, start)
    return findings


def scan_file(path):
    text = path.read_text(encoding='utf-8', errors='replace')
    return [(path, terminator, statement, word)
            for terminator, statement, word in scan_text(text, str(path))]


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', default='.', help='repository root')
    args = parser.parse_args(argv)
    root = pathlib.Path(args.root)
    base = root / SOURCE
    if not base.is_dir():
        print('FAIL: %s is not a directory' % base)
        return 2
    files = sorted(list(base.rglob('*.cpp')) + list(base.rglob('*.h')))
    found = []
    for path in files:
        found.extend(scan_file(path))
    for path, terminator, statement, word in found:
        print('%s:%d: %s makes code dead at line %d'
              % (path.relative_to(root).as_posix(), terminator, word, statement))
    if found:
        print('FAIL: %d dead statement(s) in %d file(s)' % (len(found), len({f[0] for f in found})))
        return 1
    print('PASS: no dead statement after an unconditional terminator (%d files scanned)' % len(files))
    return 0


if __name__ == '__main__':
    sys.exit(main())
