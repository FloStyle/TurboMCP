# MPL-2.0 setup: the three things, in one place

Everything below is already in the repository. This file is a summary so the three pieces can be read
together without opening three others.

---

## 1. The license text — `LICENSE`

The full official Mozilla Public License 2.0, **not retyped and not reworded**.

**Verified verbatim.** The canonical plain-text copy mozilla.org publishes was downloaded and compared
to the file with a string comparison:

| | |
|---|---|
| Downloaded from | `https://www.mozilla.org/media/MPL/2.0/index.815ca599c9df.txt` |
| Official | 16,726 characters |
| `LICENSE` | 16,726 characters |
| Comparison | **identical, byte for byte** |
| Re-checked after a fresh `git checkout` | **still identical** |

The source is the plain-text form rather than the HTML page, because a license file is the plain-text form
and the HTML page reformats the numbered lists into markup.

`.gitattributes` pins it to LF, so `core.autocrlf=true` cannot rewrite the verified text on checkout. That
was not cosmetic: git warned it would convert the file until the entry was added, and a license altered by
a line-ending conversion is not the license that was verified.

---

## 2. The file notice — `Tools/license-header.txt`

This is **Exhibit A** of the license itself, which is the form the license asks a file to carry.

```cpp
// Copyright (c) 2026 Florian ESCHALIER (FloStyle)
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.
```

The `SPDX-License-Identifier` line is added on top of Exhibit A, because it is the machine-readable form and
lets tooling identify the license without parsing prose. The copyright line is the one the project already
used, so nothing was renamed.

### Applying it: `Tools/apply-license-header.ps1`

```powershell
pwsh -File Tools\apply-license-header.ps1           # apply
pwsh -File Tools\apply-license-header.ps1 -Check    # fail if a file lacks it (use in a build gate)
pwsh -File Tools\apply-license-header.ps1 -Root <dir>   # try it on a copy first
```

**Applied to all 189 `.h`, `.cpp` and `.cs` files** - the plugin module under `Source/` and the test module under `TestProject/Source`. The diff is the proof it did nothing else:

```
189 files, one notice and one copyright line each
```

Five added lines per file, **nothing removed**, and **every file carries exactly one notice**.

*** THIS FILE SAID 111 FOR A LONG TIME, AND THE NUMBER HAD STOPPED BEING TRUE. *** *111 was correct when
`Source/` was the only tree the script walked. `TestProject/Source` was added later, the script grew to cover
it, and this paragraph was not updated.* **A count written in prose drifts the moment somebody adds a file,
so the number below is the one the script prints - run it rather than trusting this sentence:**

```powershell
pwsh -File Tools\apply-license-header.ps1 -Check
# 189 file(s): 0 prepended, 0 replaced, 189 already correct
# RESULT: PASS (every source file carries the notice)
```

Three properties make it safe to run again, each verified on a scratch copy before the real tree was
touched:

- **Idempotent.** A second run reports `0 prepended, 0 replaced, 189 already correct`, leaves every MD5 identical, and writes nothing.
  The natural failure of a header script is to *stack* notices, and a file with three copies of one notice
  is a file nobody reads.
- **It replaces, not duplicates.** Every file already had a copyright line; the script consumes it and
  writes the full notice in its place, so the count stays one. It also had to be TAUGHT the current spelling:
  its first pattern required `All Rights Reserved`, which the line
  `// Copyright (c) 2026 Florian ESCHALIER (FloStyle)` does not end with, so it stopped recognising the line
  it was supposed to consume and **prepended the notice above it, leaving two copyright lines in two files**.
  The pattern now matches any `// Copyright` line.
- **It detects the line ending instead of choosing one.** This repository pins LF in `.gitattributes`. The
  first version hard-coded a CRLF join *and* PowerShell's `-split "\`n"` silently failed to split an LF-only
  file — so each file arrived as one line, no pattern matched the top of it, and the script announced it
  would **prepend a notice to files that already had one**. Detecting the ending per file fixed both.
  After the run: **zero files contain a CR.**

It also keeps a leading `#pragma once` on the first line, because a notice above it is legal but reads as if
it were part of the guard.

---

## 3. The README section — `README.md`, "## License"

```markdown
## License

**Mozilla Public License 2.0** (`MPL-2.0`). The full text is in [`LICENSE`](LICENSE).

**You can use this plugin in a commercial game and ship that game.** MPL-2.0 is a file-level copyleft
license, not a viral one: it does not reach into your game's own code, your Blueprints, or your assets. Your
game stays yours, under whatever license you choose.

**If you modify the plugin's own files, those files stay under MPL-2.0.** That is the one condition, and it
is deliberately narrow:

| You do this | What MPL-2.0 asks of you |
|---|---|
| Ship a commercial game that uses the plugin | Nothing. No fee, no notice in your game. |
| Modify a plugin `.cpp` or `.h` and distribute it | Publish **that file's** source under MPL-2.0. |
| Add a **new** file that includes plugin code | That new file is also covered, because it contains Covered Software. |
| Add a **new**, separate file that only calls the plugin | Yours. Keep it closed if you want. |
| Combine the plugin with your own code | Allowed, under terms of your choice, as long as the plugin files keep their license. |

Two things MPL-2.0 asks for in practice:

- **Keep the notices.** The MPL-2.0 header at the top of each source file is a license notice. Section 3.4
  says you may not remove or alter it.
- **Tell recipients where to get the license.** If you distribute modified plugin source, point them at
  `https://mozilla.org/MPL/2.0/`, or include the `LICENSE` file. Section 3.1.

Distributing the plugin as a compiled `.dll` inside a shipped game is **Executable Form**, and MPL-2.0
allows it under any terms as long as the source stays available as above (Section 3.2). In practice the
plugin is editor-only and does not ship in a game build at all.

Every source file carries this header, and `Tools/apply-license-header.ps1` reapplies it:

```cpp
// Copyright (c) 2026 Florian ESCHALIER (FloStyle)
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.
```

The `SPDX-License-Identifier` line is the machine-readable form, so tooling can identify the license
without parsing prose. Run `pwsh -File Tools\apply-license-header.ps1 -Check` in a build to fail when a new
file is missing the notice; the script is idempotent, so running it twice changes nothing.

This is a plain description of what the license says, not legal advice. For a commercial release, read
[`LICENSE`](LICENSE) itself.
```
