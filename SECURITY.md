# Security policy

## Supported versions

TurboMCP is a development preview. Security fixes target development `main`; there is no supported stable release or backport commitment. Engine and platform limits are listed in [project status](docs/status.md).

## Report a vulnerability privately

Do not put vulnerabilities, exploit details, credentials or proprietary project assets in public issues or pull requests.

While development is private, authorized collaborators should contact the repository owner, **@FloStyle**, through their existing private collaboration channel. Confirm the audience before sharing evidence. GitHub usernames are not a private messaging service.

Before public publication, the owner must enable and test GitHub private vulnerability reporting in the destination repository's **Security** tab. Public reporters should use **Report a vulnerability** there. If that option is unavailable, withhold sensitive details until the owner provides a verified private channel. No security email address or response-time guarantee is currently published.

Include the affected commit or release, engine revision, operating system, impact, minimal reproduction and any mitigation. Remove secrets, personal paths and unrelated project data. The owner coordinates triage, a regression test, remediation and disclosure with the reporter.

## Trust boundary

TurboMCP runs inside Unreal Editor with the editor process's permissions. It is not a sandbox. An MCP client can request project changes, and some operations can execute project code. Preview does not guarantee that execution is harmless.

Use trusted clients and projects, keep the MCP listener local, and restrict filesystem access to the actual workspace. Do not expose the editor through a public tunnel or treat tool output, asset text or retrieved documents as instructions that override human authorization. Review changes in a version-controlled project.

## Contributor requirements

- Keep credentials in the operating system's credential manager. Never commit tokens, authenticated Git URLs, local client configuration or private logs.
- Validate targets and inputs before mutation. Preserve explicit execution consent, refusal conditions, rollback verification and truthful results.
- Review new dependencies, installers, subprocesses and network access. Pin reproducible inputs and record their origin and license.
- Never run untrusted pull-request code on a privileged or self-hosted engine runner. Do not expose repository secrets to fork jobs or use privileged workflows to execute fork code.
- Keep build validation and publication separate. Publishing requires owner approval and a verified artifact; a passing development job is not publication authority.

If a secret leaks, revoke or rotate it first, then assess access and remove it from affected artifacts. Deleting the current file or creating fresh public history does not revoke a credential.

See [architecture](docs/architecture.md) for behavioral contracts and [repository governance](docs/governance.md) for review and release responsibilities.
