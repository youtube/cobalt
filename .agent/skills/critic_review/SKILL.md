---
name: critic-review
description: >-
  Perform multi-perspective code reviews using a pool of specialized
  reviewer personas and an arbiter for consensus. Use at the end of each
  coding task in an implementation plan or when reviewing diffs.
disable-model-invocation: false
disable-slash-command: false
---

# Critic Review: Multi-Persona Code Review & Arbiter Sign-off

A rigorous multi-agent code review workflow executed at the end of each task
to verify correctness, robustness, security, style, and project guidelines.

## When to Use

- At the end of each task in an implementation plan before proceeding.
- When performing a comprehensive review of a PR, diff, or patch.
- Whenever multi-perspective validation (e.g. style, security, zero-trust,
  adversarial testing) is requested.

## Skill Folder Structure

This skill adheres to the recommended Jetski skill layout:

```text
skills/critic_review/
├── SKILL.md                 # Main workflow & instructions (this file)
├── references/              # Procedural guidelines & arbiter rules
│   ├── arbiter.md           # Conflict resolution & final consensus
│   └── context_rule.md      # Mandatory context verification checklist
└── resources/               # Assets, templates, and reviewer configs
    └── reviewers/           # Reviewer personas (*.md)
        ├── codeowners/      # Domain-specific CODEOWNERS reviewers (*.md)
        ├── adversarial-reviewer.md
        ├── cobalt-ci-reviewer.md
        ├── kissy-reviewer.md
        └── styleguide-reviewer.md
```

## Reviewer Subagents Pool & Discovery

Reviewer personas participating in the review pool are identified by the
`critic-reviewer` tag in their YAML frontmatter or agent manifest.
Reviewers are discovered across the following locations (in priority order):

1. **User Custom Critics**: `~/.gemini/critics/{name}.md` (or `~/.gemini/critics/codeowners/{name}.md`)
   - Highest priority. Personas defined here allow any user to add or override reviewers globally across projects.
2. **Workspace Project Critics**: `.agent/critics/{name}.md`
   - Repo-specific custom critics checked into the workspace.
3. **Skill Reviewer Resources**: `SKILL_DIR/resources/reviewers/{name}.md` and
   `SKILL_DIR/resources/reviewers/codeowners/{name}.md`
4. **Workspace / Global Agents**: `AGENTS_DIR/{name}/agent.json`,
   `AGENTS_DIR/{name}.md`, or `~/.gemini/config/agents/`

## Defining Custom Review Agents

Users can define custom review personas by creating a Markdown file in
`~/.gemini/critics/{reviewer-name}.md` (or `.agent/critics/{reviewer-name}.md`).

### Format Specification

Every custom reviewer must include a YAML frontmatter block containing `tags` with `critic-reviewer`:

```markdown
---
name: Custom Reviewer
description: "Review changes against custom domain criteria or guidelines."
tags:
  - critic-reviewer
  - custom-reviewer
---

Before beginning your review, you must read the context verification procedure in [context_rule.md](SKILL_DIR/references/context_rule.md).

You are the **Custom Reviewer**, specializing in...

# INSTRUCTIONS
Evaluate `<current_state>` against the following criteria:

## Review Checklist
- [ ] Item 1...
- [ ] Item 2...
```

## Workflow

### 1. Group Selection
Select 3–5 appropriate reviewers from the pool based on the scope and nature
of the changes (e.g., styleguide, security/adversarial, minimalist/kissy, CI/GitHub Actions, domain, or custom user critics):
- Custom user critics in `~/.gemini/critics/*.md` and `~/.gemini/critics/codeowners/*.md`
- Workspace critics in `.agent/critics/*.md`
- Reviewers advertised in `SKILL_DIR/resources/reviewers/*.md` and
  `SKILL_DIR/resources/reviewers/codeowners/*.md`
- Custom subagents tagged with `critic-reviewer`

#### Codeowner Reviewer Determination (GitHub CLI & Resolver)
Per the repository Code Submission Policy (CSP), mandatory codeowner reviewers are determined deterministically from `.github/CODEOWNERS` (the single source of truth) using the resolver CLI tool:

```bash
# 1. Resolve codeowner reviewers for local working tree diff:
python3 SKILL_DIR/scripts/resolve_codeowners.py --diff

# 2. Or resolve codeowners for a specific PR via GitHub CLI (gh):
python3 SKILL_DIR/scripts/resolve_codeowners.py --pr <PR_NUMBER>

# 3. Or pass explicit file paths from <current_state>:
python3 SKILL_DIR/scripts/resolve_codeowners.py <file1> <file2> ... --format markdown
```

The resolver strictly adheres to GitHub CODEOWNERS specifications:
- Sequential evaluation with **last-matching-line-wins** precedence.
- Directory inheritance and path anchoring (`/`).
- Multi-team co-ownership (e.g. Android TV + Media).
- Explicit unowned and empty-owner clearing rules.
- Maps discovered GitHub teams directly to reviewer personas in `resources/reviewers/codeowners/` and `~/.gemini/critics/`.
- Surfaces `unowned_files` so general reviewers (`adversarial`, `kissy`, `styleguide`) cover them without blocking on domain owners.

### 2. Context Preparation & Verification
Before dispatching reviews, verify that all mandatory context is assembled
per [`context_rule.md`](SKILL_DIR/references/context_rule.md):
- `<user_instructions>`: Original goal and all subsequent user prompts.
- `<current_state>`: Exact diffs, changed files, and relevant context.
- `<user_feedback>`: Any specific user feedback or constraints.

### 3. Review Execution
- Invoke the selected reviewer subagents in parallel with the review context.
- Each reviewer evaluates the changes against their domain criteria and
  produces an evidence-backed report.
- Remind reviewers if any response is pending.

### 4. Arbiter Synthesis & Sign-off
- The **Arbiter** ([`arbiter.md`](SKILL_DIR/references/arbiter.md)) evaluates
  findings across all reviewer reports.
- If reviewers disagree, the Arbiter resolves conflicts with clear rationale.
- If any required review is missing without explicit decline, the Arbiter
  invokes VETO to request completion before approving.
