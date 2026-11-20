---
name: pr-shepherd
description: >-
  Shepherds PRs to green by delegating monitoring, triage, and CI/code fixes to
  a Watch Coordinator subagent until all required CI checks pass, while
  escalating any GitHub comment replies or thread resolutions to the user for
  review unless explicitly instructed otherwise.
---

# Pull Request Shepherd

The **PR Shepherd** skill autonomously drives Pull Requests toward **All Green**
through delegation so the Main Agent's context stays minimal.

- **Main Agent** (you): Spawns the Watch Coordinator and handles final user
  reporting or escalations (including draft review comment replies).
- **Watch Coordinator** (`pr-shepherd-watcher`): Runs `shepherd_pr.py --watch`,
  maintains `shepherding_report.md`, and dispatches **Issue Workers**
  (`pr-shepherd-fixer`) in isolated workspaces for any required CI failure,
  unaddressed review thread code change, or merge conflict.

---

## Main Agent Rules

### 1. Delegate Everything (Conserve Context)
- **MUST** spawn a **Watch Coordinator** subagent
  (`Role: "pr-shepherd-watcher"`) to monitor the PR(s).
- **MUST NOT** read [resources/watcher_agent.md][watcher-agent-ref] or
  [resources/fixer_agent.md][fixer-agent-ref] directly.
- **MUST NOT** run `shepherd_pr.py --watch`, inspect CI logs, download
  artifacts, perform failure triage, or edit PR code yourself.
- Only read `shepherding_report.md` when summarizing progress for the user or
  presenting an escalation report.

### 2. Spawning the Watch Coordinator
When launching `pr-shepherd-watcher`, provide:
1. Target repository (`<owner>/<repo>`), PR number(s), and base branch.
2. Instruction to read [resources/watcher_agent.md][watcher-agent-ref] **first**
   and follow its watch loop, active interruption dispatch (spawning
   `pr-shepherd-fixer` with [resources/fixer_agent.md][fixer-agent-ref]),
   context-limit handoff, and `shepherding_report.md` Action Ledger rules.
3. Explicit GitHub comment, review, and thread-resolution policy: autonomously
   posting GitHub comments (`gh pr comment`), in-thread replies
   (`gh api .../replies`), PR reviews (`gh pr review`), or resolving/unresolving
   review threads (`resolveReviewThread`/`unresolveReviewThread`) is **strictly
   prohibited** unless the user explicitly granted single-use consent in chat for
   that specific action. Draft any review comment responses and escalate them to
   the user for review.

### 3. Completion & Escalation
- Wait for the Watch Coordinator to report one of:
  - `SUPER GREEN! Multipass authorized.` (exit `0` — all required CI checks
    green, mergeable, zero unresolved review threads, no `CHANGES_REQUESTED`),
  - `CI GREEN. Awaiting human on review threads/decision (agents: escalate only, never reply or resolve).`
    (exit `3` — all required CI checks green and mergeable, with only
    unresolved review threads / drafted replies or `CHANGES_REQUESTED` awaiting
    human review), or
  - An escalation (persistent non-PR base/infra failure, persistent API error
    exit `4`, or context-limit handoff).
- Present the final status, escalation summary, and any drafted review comment
  replies (from `shepherding_report.md`) to the user.

---

## Strict Prohibitions

- **Never Merge PRs**: Your boundary ends at **SUPER GREEN** or **CI GREEN**
  escalation. Never run `gh pr merge` or trigger GitHub merge APIs; merging is
  strictly reserved for the user.
- **Strict Prohibition — Do Not Comment, Reply, Submit Reviews, or Resolve
  Review Threads**: Never run `gh pr comment`, `gh api .../replies`,
  `gh pr review`, or GraphQL `resolveReviewThread`/`unresolveReviewThread`
  without explicit single-use user consent in chat. Every granted consent
  expires immediately after one use.
- **No PR Shepherd Exemption for Comments or Thread Resolution**: `pr-shepherd`
  and its workers are **NOT** exempt from the commenting restriction. Autonomous
  commits and pushes are scoped strictly to PR-caused CI fixes, merge
  conflicts, and clear requested code fixes (while any comment responses or
  thread resolutions MUST be escalated to the user).

[watcher-agent-ref]: resources/watcher_agent.md
[fixer-agent-ref]: resources/fixer_agent.md
