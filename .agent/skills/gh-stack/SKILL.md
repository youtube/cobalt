---
name: gh-stack
description: >-
  Manage stacked pull requests using the 'gh stack' CLI extension effectively
  and efficiently.
---

# GitHub Stack Skill (`gh stack`)

> [!IMPORTANT]
> **When to Use This Skill**:
> - **Chained / Dependent PRs Only**: Use `gh stack` exclusively when managing
>   a series of interdependent/chained pull requests, or when the user
>   **explicitly requests** creating or managing a stack.
> - **Do NOT Use for Single PRs**: Never use `gh stack` for single, standalone
>   pull requests. For standard single PRs, use `gh pr create`.

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

If `gh-stack` extension is not installed:

```bash
gh extension install github/gh-stack
```

> [!IMPORTANT]
> **Enable Rerere Before Operations**:
> Always enable `git rerere` to record and reuse recorded resolutions of
> conflicts during cascade rebases, avoiding interactive prompts:
> ```bash
> git config rerere.enabled true
> ```

---

## Remote Ref & Branch Naming Convention

> [!IMPORTANT]
> **Only Stack Branches on `origin` via `gh stack`**:
> - Stack branches pushed to `origin` must ALWAYS use `gh stack submit --auto`.
>   **Never push them manually** (e.g. `git push origin`).
> - All regular (non-stack) branches must be pushed to your `fork` remote.
> - New stack branch names must strictly follow the pattern:
>   `stack/<GitHub username>/<feature_branch>`

---

## Core Operations

### 1. Initialize a Stack

Initializes stack tracking locally. Always pass explicit branch names (and
optional base trunk):

```bash
# Set prerequisite
git config rerere.enabled true

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

# Stage all changes, commit, and add branch to stack
gh stack add -Am "Implement user authentication endpoint" \
  stack/<username>/feature-auth

# Stage tracked changes only, commit, and add branch
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

# Continue rebase after resolving merge conflicts
git add <resolved_files>
gh stack rebase --continue

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

### 8. Submit Stacked Pull Requests

Push stack branches to `origin` and create/update linked pull requests.
**MUST** include `--auto`:

```bash
# Non-interactive submission (creates/updates draft PRs with auto titles)
gh stack submit --auto

# Non-interactive submission, marking PRs as ready for review
gh stack submit --auto --open
```

---

## Best Practices

- **No Interactive Commands**: Never run `gh stack modify`, `gh stack switch`,
  `gh stack checkout` (without target), `gh stack submit` (without `--auto`),
  `gh stack view` (without `--short`/`--json`), or `gh stack init` (without
  branch args).
- **Branch Naming**: Always prefix stack branches with `stack/<username>/`.
- **Remote Push Targets**: Regular branches must be pushed to the `fork`
  remote only. Stack branches (`stack/<GitHub username>/<branch_name>`) are
  created on `origin` strictly via `gh stack submit --auto`, never manually
  (`git push origin`).
- **Regular Synchronization**: Run `gh stack sync --prune` frequently to pull
  trunk changes, prune merged branches, and cascade-rebase dependencies
  without manual merge conflicts.
- **Enable `rerere`**: Always set `git config rerere.enabled true` before
  running `gh stack init` or rebase operations.
