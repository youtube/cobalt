---
name: pr-shepherd
description: >-
  Shepherds PRs to green by delegating monitoring, triage, and fixes to a
  Watch Coordinator subagent until all CI checks pass and review comments are
  addressed.
---

# Pull Request Shepherd

The **PR Shepherd** skill autonomously drives Pull Requests to **All Green**
through delegation so the Main Agent's context stays minimal.

- **Main Agent** (you): Spawns the Watch Coordinator and handles final user
  reporting or escalations.
- **Watch Coordinator** (`pr-shepherd-watcher`): Runs `shepherd_pr.py --watch`,
  maintains `shepherding_report.md`, and dispatches **Issue Workers**
  (`pr-shepherd-fixer`) in isolated workspaces for any CI failure, unresolved
  review thread, or merge conflict.

---

## Main Agent Rules

### 1. Delegate Everything (Conserve Context)
- **MUST** spawn a **Watch Coordinator** subagent
  (`Role: "pr-shepherd-watcher"`) to monitor the PR(s).
- **MUST NOT** read [resources/triage_agent.md][triage-agent-ref] directly.
- **MUST NOT** run `shepherd_pr.py --watch`, inspect CI logs, download
  artifacts, perform failure triage, or edit PR code yourself.
- Only read `shepherding_report.md` when summarizing progress for the user or
  presenting an escalation report.

### 2. Spawning the Watch Coordinator
When launching `pr-shepherd-watcher`, provide:
1. Target repository (`<owner>/<repo>`), PR number(s), and base branch.
2. Instruction to read [resources/triage_agent.md][triage-agent-ref] **first**
   and follow its watch loop, active interruption dispatch, triage/escalation
   protocols, and `shepherding_report.md` Action Ledger rules.
3. Explicit GitHub comment-reply policy: autonomous replies are **prohibited**
   unless the user explicitly granted single-use consent in chat for a specific
   reply.

### 3. Completion & Escalation
- Wait for the Watch Coordinator to report either:
  - `SUPER GREEN! Multipass authorized.` (all checks green, mergeable, zero
    unresolved review threads), or
  - An escalation (persistent non-PR base/infra failure, context-limit handoff,
    or pending review-comment replies awaiting user consent).
- Present the final status or escalation summary (from `shepherding_report.md`)
  to the user.

---

## Strict Prohibitions

- **Never Merge PRs**: Your boundary ends at **All Green** / **SUPER GREEN**.
  Never run `gh pr merge` or trigger GitHub merge APIs; merging is strictly
  reserved for the user.
- **Never Comment on GitHub Autonomously**: Never post PR comments or inline
  review replies without explicit single-use user consent in chat. Every
  granted consent expires immediately after one use.

[triage-agent-ref]: resources/triage_agent.md
