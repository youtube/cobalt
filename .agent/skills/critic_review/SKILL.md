---
name: critic-review
description: >-
  Perform multi-perspective code reviews using a pool of specialized
  reviewer personas. Use at the end of each coding task in an
  implementation plan or when reviewing diffs.
disable-model-invocation: false
disable-slash-command: false
---

# Critic Review: Multi-Persona Code Review

A rigorous multi-agent code review workflow executed at the end of each task
to verify correctness, robustness, security, style, and project guidelines.

## When to Use

- At the end of each task in an implementation plan before proceeding.
- When performing a comprehensive review of a PR, diff, or patch.
- Whenever multi-perspective validation (e.g. style, security, zero-trust,
  adversarial testing) is requested.

## Skill Folder Structure

```text
skills/critic_review/
├── SKILL.md                 # Main workflow & instructions (this file)
├── README.md                # Reference for enabling mandatory critic review
├── references/              # Procedural guidelines & context rules
│   └── context_rule.md      # Mandatory context verification checklist
└── resources/               # Assets, templates, and reviewer configs
    ├── codeowners/          # Domain-specific CODEOWNERS reviewers (*.md)
    └── reviewers/           # General reviewer personas (*.md)
```

## Reviewer Subagents Pool & Discovery

1. `~/.gemini/critics/{,codeowners/}{name}.md` (highest priority).
2. `SKILL_DIR/resources/{reviewers,codeowners}/{name}.md`.
3. `.agent/critics/{,codeowners/}{name}.md`.
4. `critic-reviewer` in `.agent/agents/`, `AGENTS_DIR/`, `~/.gemini/**/agents/`.

## Workflow
### 1. Group Selection
Select all pool critics matching the review area of expertise (up to 5,
matched via `description` and `tags`) plus all resolved CODEOWNERS critics
(from `.github/CODEOWNERS`):
```bash
# Diff vs HEAD
python3 SKILL_DIR/scripts/resolve_codeowners.py
# PR / stdin
git diff --name-only origin/main | \
  python3 SKILL_DIR/scripts/resolve_codeowners.py
# Explicit paths
python3 SKILL_DIR/scripts/resolve_codeowners.py <file1> <file2> ...
```
Outputs matching `<name>-reviewer.md` personas (one per line). If
`.github/CODEOWNERS` exists and no domain critics match (or a resolved persona
file is missing), cover with general pool reviewers and escalate to a human if
CODEOWNERS coverage is unclear.

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

### 4. Review Synthesis
- Evaluate findings across all reviewer reports.
- Synthesize actionable feedback, prioritizing security and correctness.
