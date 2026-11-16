# Repository governance

[Documentation](README.md) · [Contributing](../CONTRIBUTING.md) · [Security](../SECURITY.md)

## Contribution strategy

Welcome reproducible bug reports, focused fixes, tests and documentation improvements. Discuss new features and architectural changes before implementation. Proposals should describe a user problem, the current workaround, expected benefit and maintenance cost. Compare native Unreal capabilities before adding overlapping features.

GitHub issues are an intake and discussion channel. Accepted implementation work becomes a bounded repository task; the roadmap remains the execution queue. Reports do not need a root-cause diagnosis. Maintainers reconcile duplicates and translate accepted proposals into ready tasks. Agents must not promote their own proposals into approved work.

## Responsibilities and review

The repository owner, **@FloStyle**, owns scope decisions, architecture exceptions, integration and publication. `.github/CODEOWNERS` identifies the review owner; it does not grant contributors merge permission or enforce review by itself.

Contributors own the correctness of their changes, including AI-generated code. Review the entire diff, disclose material verification gaps and provide reproducible evidence. Do not submit speculative bulk rewrites or claim tests that were not run.

Use focused branches and pull requests. Review changes to tool schemas, mutation behavior, dependencies, permissions, workflows and release tooling explicitly. Rebase or merge current main and rerun affected checks before integration. The owner may use an explicitly authorized local integration during private development; ordinary contributors do not push directly to main.

Use coherent commits containing the task ID where applicable. Avoid force pushes to main and automated merges of unreviewed changes. Preserve copyright notices and only contribute code and assets you have permission to share under the repository license.

## GitHub controls and publication checklist

These settings are separate from versioned policy files. Verify them on the actual destination repository before inviting public contributions:

- Require pull requests, owner review and passing validation on main. Disable force pushes and branch deletion. Where a single owner cannot obtain an independent approval, document the owner integration exception rather than claiming independent review.
- Activate the validation workflow with minimal token permissions. Keep fork checks isolated from secrets and self-hosted engine runners.
- Enable and test private vulnerability reporting. Provide a verified private contact if that feature is unavailable.
- Enable available secret scanning and push protection; verify coverage instead of assuming availability from the plan name.
- Keep publication and release access separate from routine development. Follow the [source and history checks](public-preparation.md) and verify package checksums for stable releases.
- Check issue templates, CODEOWNERS, policy links and repository visibility after copying the source tree.

Private-host checks previously encountered plan restrictions. The checked-in validation workflow has run on private main. Verify permissions, CI and protection independently on the destination. After a visibility change, recheck controls that the host applies according to visibility.

## Community and language

Follow the [community conduct policy](../CODE_OF_CONDUCT.md). Repository-authored prose, comments, task descriptions, filenames and public contribution text must be English. See the [language policy](language.md) for automated coverage and its limits.

GitHub setup references: [code-owner review](https://docs.github.com/en/repositories/managing-your-repositorys-settings-and-features/customizing-your-repository/about-code-owners) and [private vulnerability reporting](https://docs.github.com/en/code-security/how-tos/report-and-fix-vulnerabilities/configure-vulnerability-reporting/configure-for-a-repository).
