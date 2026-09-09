---
name: gh-stack
description: >-
  Manage stacked pull requests using the 'gh stack' CLI extension effectively
  and efficiently. Use ONLY when creating, navigating, rebasing, restructuring,
  or submitting chains of two or more dependent pull requests (never for single
  changes/PRs).
---

# GitHub Stack Skill (`gh stack`)

> [!CAUTION]
> **STRICT PROHIBITION: NEVER USE `gh stack` FOR A SINGLE CHANGE ("STACK OF ONE")**:
> - **Default is ALWAYS Standard PR**: For any single, standalone change or PR,
>   **DO NOT USE `gh stack`**. Use standard Git and GitHub CLI:
>   ```bash
>   git checkout -b <branch_name>
>   git push -u fork <branch_name>
>   gh pr create --repo <owner>/<repo>
>   ```
> - **A Stack MUST Have 2 or More Dependent PRs**: A stack is ONLY valid when you
>   are actively creating and managing **two or more interdependent layers** (e.g.,
>   Layer 2 based on Layer 1). A stack with only 1 PR is an anti-pattern that
>   violates repository conventions.
> - **Only Two Legitimate Triggers**: You may ONLY activate this skill if:
>   1. The user **explicitly instructed** you to create or manage a stack (e.g.,
>      using the words *"stack"*, *"stacked PR"*, or *"`gh stack`"* in their prompt).
>   2. The task actively requires splitting a large feature into **two or more
>      dependent PRs** submitted in sequence right now.

### ⛔ Agent Rationalization Checklist: Stop Yourself!

AI agents frequently overcomplicate tasks by rationalizing why a single change should be a stack. If you catch yourself thinking any of the following thoughts, **STOP**:

| 🧠 Agent's Internal Rationalization | 🛑 Reality & Correction |
| :--- | :--- |
| *"This could be the first part of a future multi-part project."* | **False.** Do NOT anticipate future PRs that do not exist yet. If you are submitting one PR now, submit it as a standard PR. |
| *"Using `gh stack` makes rebasing or syncing cleaner."* | **False.** A stack of 1 PR adds useless metadata, pollutes `origin` with unnecessary `stack/` branches, and complicates review without any benefit. |
| *"The user asked for a new feature or complex bugfix, so it warrants a stack."* | **False.** Feature complexity does NOT dictate a stack. Only creating **multiple interdependent pull requests** dictates a stack. |
| *"It doesn't hurt to use `gh stack` just in case."* | **False.** Pushing a single PR via `gh stack` pushes to `origin` instead of your `fork`, violating standard single-branch repository policy. |
| *"The task touches multiple files or includes multiple commits."* | **False.** Multiple files or commits belong in a single standard PR unless each layer represents an independently reviewable PR based on the previous layer. |

This skill provides instructions for managing stacked pull requests using the
`gh-stack` CLI extension (`gh stack`). Stacked pull requests break large feature
changes into an ordered chain of smaller, dependent PRs that can be reviewed,
rebased, and merged independently.

```
stack/<user>/frontend      → PR #3 (base: stack/<user>/api-endpoints) ← top
stack/<user>/api-endpoints → PR #2 (base: stack/<user>/auth-layer)
stack/<user>/auth-layer    → PR #1 (base: main)                       ← bottom
─────────────────────────────────────────────────────────────
main (trunk)
```

## 🚨 CRITICAL: Interactive GUI/TUI Commands & Agent Execution Safety

> [!CAUTION]
> **DO NOT RUN INTERACTIVE `gh stack` COMMANDS**:
> Several `gh stack` commands open interactive Terminal User Interfaces (TUIs),
> graphical interactive pickers, or terminal text editors (`$EDITOR` / `vim`).
> In automated or non-interactive agent execution contexts, these commands will
> spawn a GUI/TUI, wait indefinitely for user input, and **HANG FOREVER**.

### Commands That Spawn UIs / Editors and HANG

- **`gh stack modify`**
  - *Reason for Hanging*: Spawns interactive TUI for reordering/editing stack
    layers.
  - *Safe Alternative*: Use git commands manually (e.g. `git rebase --onto`)
    or `gh stack modify --continue`/`--abort`.
- **`gh stack switch`**
  - *Reason for Hanging*: Spawns interactive TUI branch picker to switch active
    stack.
  - *Safe Alternative*: Use
    `gh stack checkout <stack_number | pr_number | branch_name>`.
- **`gh stack checkout` (without args)**
  - *Reason for Hanging*: Spawns interactive branch picker.
  - *Safe Alternative*: Specify target branch or PR:
    `gh stack checkout <target>`.
- **`gh stack submit` (without `--auto`)**
  - *Reason for Hanging*: Opens interactive text editor (`$EDITOR`) to edit PR
    titles and descriptions.
  - *Safe Alternative*: Always pass `--auto`: `gh stack submit --auto`
    (or `gh stack submit --auto --open`).
- **`gh stack view` (without `--short`/`--json`)**
  - *Reason for Hanging*: Opens interactive pager / TUI view.
  - *Safe Alternative*: Always pass `--short` or `--json`:
    `gh stack view --short` or `gh stack view --json`.
- **`gh stack init` (without args)**
  - *Reason for Hanging*: Opens interactive TUI prompt to select base and
    branches.
  - *Safe Alternative*: Specify branches explicitly:
    `gh stack init [--base main] stack/<user>/b1 stack/<user>/b2`.

---

## Installation & Prerequisites

Ensure the `gh-stack` extension is installed:

```bash
gh extension list | grep -q 'gh-stack' || gh extension install github/gh-stack
```

> [!IMPORTANT]
> **Enable Rerere Before Operations**:
> Always enable `git rerere` and `rerere.autoupdate` to record and automatically
> stage resolutions of conflicts during cascade rebases, avoiding interactive prompts:
> ```bash
> git config rerere.enabled true
> git config rerere.autoupdate true
> ```

---

## Remote Ref & Branch Naming Convention

> [!IMPORTANT]
> **Only Stack Branches on `origin` via `gh stack`**:
> - Stack branches pushed to `origin` must ALWAYS use `gh stack submit --auto`.
>   **Never push them manually** (e.g., `git push origin` or `git push --force origin`).
> - All regular (non-stack) branches must be pushed to your `fork` remote.
> - New stack branch names must strictly follow the pattern:
>   `stack/<GitHub username>/<feature_branch>`
> - Pushing stack branches directly to `origin` requires repository write permissions.
>   If contributing from an external fork without upstream write access, maintain
>   the stack against your own fork remote (`origin`).
> - Determine your GitHub login dynamically rather than guessing:
>   ```bash
>   GH_USER=$(gh api user -q .login)
>   ```

---

## Core Operations

### 1. Initialize a Stack

Initializes stack tracking locally. Always pass explicit branch names (and
optional base trunk):

```bash
# Set prerequisites
git config rerere.enabled true
git config rerere.autoupdate true

# Initialize with explicit branch names (non-interactive)
gh stack init stack/<username>/feature-auth stack/<username>/feature-api

# Initialize against a specific trunk base branch
gh stack init --base main stack/<username>/feature-auth
```

### 2. Add Layers to the Stack

Create a new branch on top of the current stack layer:

```bash
# Add a new branch to the top of the stack
gh stack add stack/<username>/<new_layer_branch>

# Prefer explicit file staging before committing and adding layer:
git add <modified_files>
gh stack add -m "Implement user authentication endpoint" \
  stack/<username>/feature-auth

# Alternatively stage tracked changes only:
gh stack add -um "Refactor database migrations" stack/<username>/feature-db
```

### 3. Navigate the Stack

Move non-interactively between layers in the active stack:

```bash
gh stack up        # Move to the layer above current (away from trunk)
gh stack down      # Move to the layer below current (towards trunk)
gh stack top       # Jump to the topmost layer
gh stack bottom    # Jump to the bottom layer (closest to trunk)
gh stack trunk     # Check out the base trunk branch
```

### 4. View Stack Structure & Status

Always pass `--short` or `--json` to prevent interactive TUI pager output:

```bash
# Compact text view (non-interactive)
gh stack view --short

# Output stack details as JSON (ideal for parsing in scripts/subagents)
gh stack view --json
```

### 5. Rebase and Synchronize Stack

Keep all branches in the stack rebased and synchronized with trunk and
dependent layers:

```bash
# Cascading rebase of all branches in the stack from trunk upward
gh stack rebase

# Rebase only branches below or above current branch
gh stack rebase --downstack
gh stack rebase --upstack

# Continue rebase headlessly after resolving merge conflicts:
git add <resolved_files>
GIT_EDITOR=true gh stack rebase --continue

# Abort rebase and restore stack to pre-rebase state
gh stack rebase --abort

# All-in-one sync: fetch remote, cascade rebase, prune merged branches,
# and reconcile PR state (non-interactive)
gh stack sync --prune
```

### 6. Check Out an Existing Stack

Switch to an existing stack using an explicit identifier (never without
arguments):

```bash
# Check out by stack number or PR number
gh stack checkout <stack_number>
gh stack checkout <pr_number>

# Check out by branch name
gh stack checkout stack/<username>/<branch_name>
```

### 7. Restructure a Stack (Non-Interactive)

Because `gh stack modify` opens an interactive TUI, perform restructuring using
standard non-interactive git commands (e.g., `git rebase --onto`) or
resolve/abort ongoing sessions:

```bash
# Continue or abort modify session if already in progress
gh stack modify --continue
gh stack modify --abort
```

> [!IMPORTANT]
> **Reconcile Metadata After Manual Rebasing**:
> `gh stack` stores branch hierarchy metadata in `.git/gh-stack`. If you restructure
> branches using raw Git commands (`git rebase --onto`), re-register the updated stack
> ordering to synchronize metadata before running `gh stack rebase` or `gh stack submit`:
> ```bash
> gh stack init [--base main] stack/<username>/b1 stack/<username>/b2
> ```

### 8. Submit Stacked Pull Requests

Push stack branches to `origin` and create/update linked pull requests.
**MUST** include `--auto`:

```bash
# Non-interactive submission (creates/updates draft PRs with auto titles)
gh stack submit --auto

# Non-interactive submission, marking PRs as ready for review
gh stack submit --auto --open
```

> [!TIP]
> **Post-Submission PR Description Updates**:
> Because `--auto` populates PR descriptions directly from commit messages without
> opening an interactive editor, update individual PR descriptions non-interactively
> after submission if needed:
> ```bash
> gh pr edit <pr_number> --body "Updated PR description..."
> ```

---

## Best Practices

- **Never Create Single-PR Stacks (Minimum Stack Size $\ge 2$)**: A stack of one is
  an anti-pattern and strictly prohibited. If you only have one change or PR to
  submit, always use standard Git workflow (branch + push to `fork` + `gh pr create`).
- **No Interactive Commands**: Never run `gh stack modify`, `gh stack switch`,
  `gh stack checkout` (without target), `gh stack submit` (without `--auto`),
  `gh stack view` (without `--short`/`--json`), or `gh stack init` (without
  branch args).
- **Branch Naming**: Always prefix stack branches with `stack/<username>/`.
- **Remote Push Targets**: Regular branches must be pushed to the `fork`
  remote only. Stack branches (`stack/<GitHub username>/<branch_name>`) are
  created on `origin` strictly via `gh stack submit --auto`, never manually
  (`git push origin` or `git push --force origin`).
- **Regular Synchronization**: Run `gh stack sync --prune` frequently to pull
  trunk changes, prune merged branches, and cascade-rebase dependencies
  without manual merge conflicts.
- **Enable `rerere`**: Always set `git config rerere.enabled true` and
  `git config rerere.autoupdate true` before running `gh stack init` or rebase
  operations.
