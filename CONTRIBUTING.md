# Reports and contributions

[Open issues](https://github.com/FloStyle/TurboMCP/issues) | [1.0.0 plan](docs/release-1.0.0.md)

Start with an issue. Search existing reports and add new evidence to a matching issue before creating another one. Use English and keep examples small.

## Report a bug

Use the [bug report form](https://github.com/FloStyle/TurboMCP/issues/new?template=bug_report.md). Include:

- TurboMCP version or commit, Unreal version and source/installed build, OS and MCP client.
- Tool name and the shortest synthetic reproduction.
- Expected result, observed result and impact.
- Whether it happens once or repeatedly, any workaround, and what remains unknown.

Describe memory changes, compilation and saving separately. A successful call or compile does not prove the asset was saved. Do not guess the cause.

## Report a work session with an agent

Use the [session-report skill](skills/turbomcp-session-report/SKILL.md) and its [report format](skills/turbomcp-session-report/references/report-format.md). The format covers bugs, limitations, workarounds, useful behavior and feature ideas. Keep existing report and item IDs.

Ask your agent to read the skill, write a local report and run its checker:

```powershell
python skills/turbomcp-session-report/scripts/check_report.py <report-file>
```

The skill does not submit issues. Review the exact report and remove private project data before you or an explicitly authorized maintainer submit it. A checker pass is not privacy approval. See [skill setup](docs/agent-skills.md) for installation and the reporting boundary.

## Propose a feature

Use the [feature request form](https://github.com/FloStyle/TurboMCP/issues/new?template=feature_request.md). Explain the user task, current workaround and smallest useful improvement. List the native Unreal or TurboMCP tools you tried. Describe an observable success case and any behavior that must stay unchanged.

A proposal is not an approved implementation task. The maintainer reviews scope, duplicates and priority before assigning work.

## Submit a fix

Discuss the issue first. Follow [AGENTS.md](AGENTS.md) and the [contributor workflow](docs/contributing.md) for an approved scope, base commit and task claim. Keep the change focused. Include checks run, results and missing evidence in the pull request. Humans remain responsible for AI-assisted changes.

Use synthetic or approved public examples. Keep credentials, workplace code, raw project logs and personal data out of issues and commits. Report vulnerabilities privately through [SECURITY.md](SECURITY.md).
