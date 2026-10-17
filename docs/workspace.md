# Workspace setup

The repository and Unreal share one working environment but remain separate Git repositories. Choose explicit roots for the plugin checkout and the separate Unreal checkout. Neither must be nested in the other.

1. Copy workspace.example.json to workspace.local.json and set engineRoot to the actual engine root. Alternatively set TURBOMCP_ENGINE. Explicit environment configuration wins; a wrong configured path fails instead of silently selecting another engine.
2. Run `python Tools/doctor.py`. It checks the version and exact commit in engine.lock.json.
3. Open TurboMCP as the primary harness project. Attach the actual Unreal checkout as a second folder. A junction, an IDE workspace, and AGENTS.md do not grant filesystem permission.
4. For Codex CLI use `pwsh -File Tools/start-codex.ps1`; it launches with the repository as --cd and the engine as --add-dir under workspace-write. This grants engine write access needed for builds; engine source edits still require a specific task. For stricter isolation, run implementation agents with engine source read access and send builds to a separate build executor.
5. For Codex desktop add both folders to the local project and make TurboMCP primary. Project instructions are detected from the primary folder. Start a new chat in that project so it receives those roots; an existing projectless chat does not acquire rights merely because files moved.
6. For the DeepSeek harness configure the same two actual filesystem roots and repository working directory. Load AGENTS.md as its instruction entry point if it does not auto-discover it. Do not use a third scratch workspace as the authoritative project and do not enable task-board execution.

The ignored TurboMCP.local.code-workspace is an IDE convenience, not a security policy. Any generated harness-specific configuration must remain local unless it is portable and contains no credentials.

## Build and test isolation

TestProject/TurboMCPTest.uproject uses AdditionalPluginDirectories = ../Plugins. This removes the need to mutate Engine/Plugins/Marketplace for each checkout. Remove stale engine-wide plugin mounts from your local setup only after checking their targets; verify builds and loaded artifacts use this checkout.

Use one editor/build/test owner at a time. Each implementation worker gets a separate Git worktree; a build executor owns the engine outputs and the test editor. Do not let workers regenerate shared fixtures concurrently. Configure each worktree with its own ignored workspace.local.json.

## Credentials

Use Git Credential Manager under the same Windows account as the harness. For multiple GitHub accounts, set the repository-local credential.https://github.com.username to the chosen account. Do not copy a credential helper executable or embed a token in a remote URL. A different Windows account or sandbox identity may need its own authorized credential context.

References: [Codex projects](https://learn.chatgpt.com/docs/projects), [Codex CLI flags](https://learn.chatgpt.com/docs/developer-commands?surface=cli), [GCM account selection](https://github.com/git-ecosystem/git-credential-manager/blob/main/docs/multiple-users.md).
