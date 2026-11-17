# Security

TurboMCP 1.0.0 is stable for Windows on the pinned Unreal Engine 5.8.4 source build. Fixes target `main`, and the `stable` branch carries released versions. Launcher builds and Unreal 5.9 are not certified yet.

## Report privately

Use [Report a vulnerability](https://github.com/FloStyle/TurboMCP/security/advisories/new) to send a private report. Do not post sensitive details in a public issue.

Include the affected version, engine build, impact, minimal reproduction and any workaround. Remove credentials and unrelated project data. The maintainer will coordinate a fix and disclosure.

## Use safely

TurboMCP runs with Unreal Editor's permissions. Some tools modify assets or execute project code. Use trusted clients, keep the MCP listener local and review changes in version control. It is not a sandbox.

Never commit credentials or run untrusted pull requests on an engine machine with private projects. If a credential leaks, revoke or rotate it first; removing Git history does not revoke it.

Stable releases require reviewed source, tests and package evidence. Daily snapshots are development builds of the source, not stable releases. See the [release process](docs/release.md).
