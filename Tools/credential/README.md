# Credentials

Makes `git push` work without an interactive prompt, so a push can run
unattended.

## The problem this solves

Windows holds **several** `github.com` credentials:

```
git:https://github.com                      the generic entry git writes
git:https://archived-contact@example.invalid             per-account
git:https://archived-contact@example.invalid       per-account
gh:github.com:FloStyle                      written by GitHub CLI
gh:github.com:                              written by GitHub CLI
sourcetree-rest:https://archived-contact@example.invalid written by SourceTree
```

Git Credential Manager is configured machine-wide and handles all of them
correctly. But when more than one matches, it opens a **"Select an account"**
dialog, which appeared on every push and made an unattended push impossible.

All three `git:` entries were checked against `https://api.github.com/user` and
all three authenticate as `FloStyle`, so the credentials were never the issue: the
choosing was.

## What this does instead

`git-credential-turbomcp.ps1` reads **one named entry** out of the Windows
Credential Manager through `[archived item]` and prints it to git. There
is nothing to choose, so nothing is asked.

It adds **no new secret**. It reads a credential that was already stored and
already verified.

| File | Job |
|---|---|
| `[archived item]` | Reads a named credential from the Windows Credential Manager |
| `GitCredentialHelper.cs` | Its source, so the binary can be rebuilt from nothing |
| `git-credential-turbomcp.ps1` | The git helper: reads stdin, prints the credential on stdout |

## Setting it up on this repository

Already done. It is repository-local, so the machine-wide setup is untouched and
every other repository keeps using Git Credential Manager:

```powershell
$helper = "$PWD/Tools/credential/git-credential-turbomcp.ps1"
git config --local --replace-all credential.helper ""
git config --local --add credential.helper "!powershell -NoProfile -NonInteractive -ExecutionPolicy Bypass -File `"$helper`""
```

The **empty first value** matters: git runs every configured helper in order, and
an empty value clears the ones already configured. Without it, GCM from the
machine-wide config would still run first and still prompt.

## Rebuilding the helper

The `.exe` is committed so a push works with no build step. If it is ever lost:

```powershell
cd Tools\credential
C:\Windows\Microsoft.NET\Framework64\v4.0.30319\csc.exe /nologo /target:exe `
    /out:[archived item] GitCredentialHelper.cs
```

## Checking it

```powershell
# What does git actually get? The password is 40 characters, a GitHub token.
"protocol=https`nhost=github.com`n`n" | git credential fill

# Which target names exist? Never prints a secret.
.\[archived item] get 'git:https://github.com'
.\[archived item] list 'github*'
```

`list` uses `CredEnumerate`, which returns error 1168 on this machine because the
git entries live in a vault that enumeration does not cover. Reading by name
works, which is why the helper reads by name.

## What is deliberately not done

- **Nothing is stored, overwritten or erased.** `store` and `erase` are accepted
  and do nothing. Overwriting would destroy a credential that was verified by
  hand, and erasing would put the prompt back.
- **It answers only for `github.com`.** A helper that answered for any host would
  hand a GitHub token to whatever host asked for it.
- **The token is never printed in full** by any tool here. `get` masks it, and
  `list` prints target names and lengths only.
