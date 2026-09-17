# Agent Guidelines & Repository Policies

## Source Code Control Policy

- [ ] **Codeowners Review**: Codeowners review required (via codeowners critic reviewers resolved from `.github/CODEOWNERS`).
- [ ] **Style Guide Review**: Style guide review (via `styleguide-reviewer` critic reviewer).
- [ ] **MISSY Review**: Missy review required (via `missy-reviewer` critic reviewer).
- [ ] **Critic Review**: Add 'critic_review skill' at the end of each coding task.
- [ ] **TPS Review**: Add 'tps_review' skill for larger tasks and projects or if directly prompted.

### Mandatory Code Review Policy for CODEOWNERS

Per the project's Source Code Control Policy and Code Submission Policy (CSP), **CODEOWNERS reviews are mandatory for all code changes**.

- **Single Source of Truth**: `.github/CODEOWNERS` strictly defines all domain ownership rules.
- **Deterministic Resolution**: Mandatory codeowner reviewers for any changeset or PR MUST be resolved deterministically using `resolve_codeowners.py`.
- **Non-Bypassable Review Gate**: All resolved codeowner critic subagents MUST be included in the review panel. Omitting or bypassing a resolved codeowner reviewer is prohibited.
- **Unowned File Coverage**: Files in a changeset that do not match an explicit pattern in `.github/CODEOWNERS` are classified as `unowned_files` and MUST be reviewed by general critics (`adversarial-reviewer`, `kissy-reviewer`, `styleguide-reviewer`, `cobalt-ci-reviewer`).
- **Mandatory Sign-off**: The Arbiter MUST NOT grant final approval or sign-off unless ALL mandatory codeowner reviewers have provided explicit approval (or an explicit N/A/decline).
