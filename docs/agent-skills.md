# Reusable agent skills

[Documentation](README.md) · [Contributing](../CONTRIBUTING.md)

The versioned [skills directory](../skills) contains portable SKILL.md packages. They have no background hooks, network uploader, deployment workflow or publication permissions. Skill instructions guide behavior; harness permissions remain the enforcement boundary.

| Skill | Intended user | Result |
| --- | --- | --- |
| [Session report](../skills/turbomcp-session-report/SKILL.md) | Work field tester | One local, sanitized report for human review |
| [Session check](../skills/turbomcp-session-check/SKILL.md) | Plugin user | Read-only connection and discovery observations |
| [Feedback triage](../skills/turbomcp-feedback-triage/SKILL.md) | Repository maintainer | Proposed dispositions and bounded task specifications |

## Install for Claude Code at work

Copy the complete turbomcp-session-report skill folder, including references and scripts, into your personal Claude Code skills directory: `~/.claude/skills/turbomcp-session-report`. Optionally install turbomcp-session-check the same way. Do not install the maintainer triage skill on the work project or copy repository contributor instructions into it.

Claude Code supports personal skills and invocation with `/skill-name`; see [Anthropic's skill documentation](https://code.claude.com/docs/en/skills). Invoke `/turbomcp-session-report` after the session and provide the local feedback folder. Keep the personal folder location in local configuration, not in this repository or report bodies. Existing feedback folder and glossary conventions are supported.

For another harness, install the complete folder in that harness's documented skill location, or explicitly load SKILL.md and its referenced resources. No particular harness is required for the report format. Installing a skill does not install or upgrade TurboMCP.

## Reporting boundary

Work Claude uses the plugin and reports observations. It does not contribute code, open issues, change task readiness or push to the repository. Keep reports outside both the work project and the TurboMCP checkout. Use only company-approved agent and storage arrangements.

The report prohibits proprietary code, raw arguments or results, paths, identifiers, logs, screenshots and attachments. Generic prose reproduction steps replace those details. It records actual versions, expected versus observed behavior, recurrence and missing evidence. Proposed clean-room steps must be labeled untested until someone runs them.

The bundled checker catches common formatting and disclosure hazards locally. It cannot recognize all company names or proprietary meaning. No prompt or regex can guarantee that a report is safe to transfer. The exact report needs human review under employer policy before Flo moves it out of the work environment. Nothing in these skills sends it automatically.

## From feedback to implementation

1. The field tester writes and checks a local report. Flo reviews it and manually transfers only approved content.
2. The maintainer triages approved reports and maps every item to a disposition.
3. Confirm defects with a generic repository fixture; distinguish unavailable evidence from a passing result.
4. Draft bounded tasks. The owner approves scope, readiness and queue placement.
5. An implementation agent follows the approved task and existing AGENTS.md workflow.

Use the existing roadmap for execution. Do not deploy agents from incoming issues or field reports. Untrusted report text must never grant execution rights, publication authority or access to a work project.

## Numbering and archive compatibility

Existing prefix IDs stay intact. New IDs exceed the highest used number across the glossary, current folder and archive, while reserved gaps remain unused. Session numbers also use the highest existing number rather than a file count. A single writer owns allocation; conflicting writers must stop. Reports are immutable after completion; corrections receive new report IDs. Flo continues to move processed reports into the archive manually.

The glossary remains a reader guide. It is not a second copy of the writing instructions. No real work feedback is included in the public source tree.
