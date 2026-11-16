# Feedback and contributions

[Report feedback](https://github.com/FloStyle/TurboMCP/issues/new?template=feedback.md) | [Open issues](https://github.com/FloStyle/TurboMCP/issues) | [Website](https://flostyle.github.io/TurboMCP/)

Use one workflow for feedback from a single tool call or a complete work session. Each actionable finding belongs to one of these types:

| Type | What to describe |
| --- | --- |
| Bug | Existing behavior is wrong, fails or reports a misleading result. |
| Tool improvement | An existing tool needs better coverage, output, speed or usability. |
| New feature | A capability or workflow is missing. |

## From observation to issue

1. **Search existing issues.** Add evidence to a matching issue instead of opening a duplicate.
2. **Prepare the finding.** Describe the user task, tool names, versions, observed behavior, desired result and impact. Keep unknown causes and untested proposals explicit.
3. **Review the content.** Remove credentials, private project data, identifiers and raw logs. Use English and generic or synthetic examples. Report vulnerabilities privately through [SECURITY.md](SECURITY.md).
4. **Submit the [shared feedback form](https://github.com/FloStyle/TurboMCP/issues/new?template=feedback.md).** Use one issue per distinct actionable finding. Related observations that need the same change can stay together. Preserve any existing report and item IDs.

A session report supplies evidence for this same workflow. Do not open an extra issue just to duplicate the report. Positive observations can support a finding; they do not need their own implementation issue.

## Let an agent help

Use the [session-report skill](skills/turbomcp-session-report/SKILL.md) and its [report format](skills/turbomcp-session-report/references/report-format.md) to turn an observed session into a sanitized local report. The skill covers failures, tool limitations and missing capabilities. It does not submit issues or change the plugin.

Ask the agent to read the skill, use the configured local feedback folder, preserve report IDs and run its checker:

```powershell
python skills/turbomcp-session-report/scripts/check_report.py <report-file>
```

Review the exact content before submitting the relevant findings through the shared form. A checker pass is not privacy approval. You can also fill in the form directly without an agent or session report. See [skill setup](docs/agent-skills.md).

## From issue to change

The maintainer reviews evidence, duplicates, scope and priority. An issue is not implementation approval. For code changes, follow [AGENTS.md](AGENTS.md) and the [contributor workflow](docs/contributing.md) for an approved scope, base commit and task claim. Include checks run, results and missing evidence in the pull request.

Keep discussions useful and respectful. You are responsible for submitted content, including AI-assisted work.
