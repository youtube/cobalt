---
name: gh-stack
description: >-
  Manage stacked pull requests using the 'gh stack' CLI extension effectively
  and efficiently, including analyzing PRs for layering and splitting PRs.
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

### ⏱️ Mandatory 1-Minute Safety Timer & Hang Detection

Because unexpected interactive prompts (curses TUIs, pager traps, credential or branch ambiguities) can block execution:

- **Enforce 1-Minute Timeout**: Whenever executing a `gh stack` command, enforce setting a timer or notification for 1 minute (e.g., setting `NotificationTimeoutSeconds: 60` or using a 1-minute schedule).
- **TUI Verification**: Once the timer fires or 1 minute elapses, inspect the command execution and task logs. If the command has stalled waiting for terminal user interaction, terminate or kill the task immediately.
- **Headless Rebase Editor Bypass**: When continuing a cascade rebase (`gh stack rebase --continue`), Git will invoke `$EDITOR` for commit messages unless overridden. Always prefix with `GIT_EDITOR=true`:
  ```bash
  GIT_EDITOR=true gh stack rebase --continue
  ```

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

### 7. Restructure or Reorder a Stack (Non-Interactive & PR Safety)

Because `gh stack modify` opens an interactive TUI, perform restructuring using
standard non-interactive git commands (e.g., `git rebase --onto`) or
resolve/abort ongoing sessions:

```bash
# Continue or abort modify session if already in progress
gh stack modify --continue
gh stack modify --abort
```

> [!CAUTION]
> **PREVENT ACCIDENTAL PR AUTO-CLOSING ("MERGED INTO WRONG BASE")**:
> - **How GitHub Auto-Merges PRs**: GitHub monitors PR base branches. If commit $C$
>   (the head of PR $A$) ever becomes reachable in the history of PR $A$'s base branch
>   $B$, GitHub will **immediately mark PR $A$ as MERGED into $B$, close it, and lock
>   it**. Once marked merged, a PR cannot be reopened or edited via GitHub's API!
> - **The Reordering Pitfall**:
>   If you have a stack:
>   `main` → $B$ (PR 2, base `main`) → $A$ (PR 1, base $B$)
>   And you reorder or move $B$ to the **top** of the stack:
>   `main` → $A$ (PR 1) → $B$ (PR 2)
>   On GitHub, PR 1's base is STILL pointing to branch $B$. If you push branch $B$
>   (which now contains commit $A$ in its git history) before updating PR 1's base on
>   GitHub, **GitHub will instantly mark PR 1 as MERGED into $B$ and delete/close it!**
> - **Mandatory Safe Reordering Protocol**:
>   1. **Do NOT push top-of-stack branches before bottom PR bases are updated**:
>      Never run `gh stack submit --auto` when the base relationships on GitHub are
>      inverted or cyclic.
>   2. **Update GitHub PR bases from bottom to top first**:
>      Before pushing rewritten or reordered branches to remote, update each PR's base
>      branch on GitHub using `gh pr edit <PR> --base <new_parent>` (working from trunk
>      upwards) so that no branch contains its own PR's base in its future commits.
>   3. **If PR bases cannot be changed due to existing stack constraints**:
>      Use new branch names (e.g. `stack/<user>/<feature>-v2`) and link them cleanly
>      instead of pushing an inverted commit tree into existing remote branches.

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

> [!IMPORTANT]
> **Draft State vs. Ready for Review (`--open`)**:
> - `gh stack submit --auto` submits PRs in **Draft** state by default.
> - In many CI systems, Draft PRs do not trigger full build/test matrix workflows,
>   and reviewers may not be notified.
> - Pass `--open` (`gh stack submit --auto --open`) when submitting PRs that are
>   ready for review and CI execution.
> - To mark an already submitted PR as ready for review later:
>   ```bash
>   gh pr ready <pr_number>
>   ```

> [!TIP]
> **Post-Submission PR Description Updates & Trailer Preservation**:
> Because `--auto` populates PR descriptions directly from commit messages without
> opening an interactive editor, update individual PR descriptions non-interactively
> after submission if needed:
> ```bash
> gh pr edit <pr_number> --body "Updated PR description..."
> ```
> Ensure that critical commit trailers (e.g. `Tag: agy`, `Conv: <id>`, `Bug: <id>`)
> are preserved in the PR body.

### 9. Address Review Comments & CI Failures Across Stack Layers

In stacked pull requests, review comments or CI failures frequently target intermediate
or bottom layers while you are working near the top of the stack.

Follow this protocol to apply fixes cleanly and propagate them to all dependent layers:

1. **Check Out the Target Layer**:
   ```bash
   gh stack checkout stack/<username>/<target_layer_branch>
   # Or navigate using: gh stack down / gh stack checkout <pr_number>
   ```
2. **Apply the Fix & Verify Locally**:
   Edit the affected files and verify that local unit tests pass.
3. **Commit the Fix**:
   Commit the change following repository commit guidelines (e.g. creating a separate
   incremental commit with proper trailers):
   ```bash
   git add <modified_files>
   git commit -m "Fix review feedback for <component>"
   ```
4. **Propagate Changes Upstack**:
   Rebase all dependent branches sitting above the current layer:
   ```bash
   gh stack rebase --upstack
   ```
   *(With `git rerere.enabled true` and `rerere.autoupdate true`, any conflict resolutions
   are automatically applied across all subsequent layers).*
5. **Resubmit the Updated Stack**:
   ```bash
   gh stack submit --auto
   ```
6. **Cross-Layer Verification**:
   Jump to the top layer and verify that dependent changes compile and tests pass:
   ```bash
   gh stack top
   # Run tests for topmost dependent changes
   ```

---

## 🔍 Analyzing Changes for Layering Opportunities

Before splitting any change, analyze the diff and commit history to determine
whether stacking is advantageous and how layers should be organized.

### 1. When to Consider Layering

Consider proposing a stacked PR when a change meets one or more criteria:
- **Excessive Size / Complexity**: The pull request exceeds reviewable limits
  (typically >300-500 lines or >10-15 files).
- **Multiple Disparate Concerns**: The diff mixes refactoring, core logic,
  API plumbing, tests, and documentation into a single commit or branch.
- **Cross-Subsystem Boundaries**: Changes cross codeowner or subsystem domains
  (e.g. Starboard OS abstractions, Cobalt media pipeline, and build config)
  that benefit from independent, scoped review by different reviewers.
- **Reviewer Feedback**: Reviewers request breaking down a large change into
  smaller, incremental, and easily digestible steps.

### 2. Layering Principles & Decomposition Dimensions

Decompose changes bottom-up according to these core dimensions:

1. **Dependency Order (Bottom-Up Architectural Hierarchy)**:
   - **Layer 1 (Foundational / Schema)**: Data models, protobuf definitions,
     interfaces, constants, enums, configuration types, and schema migrations.
   - **Layer 2 (Internal / Backend Logic)**: Service implementations,
     internal helpers, algorithms, and core domain logic relying on Layer 1.
   - **Layer 3 (Surface / API / Wiring)**: Public endpoints, controllers, UI
     components, CLI options, and operational wiring exposing Layer 2.
   - **Layer 4 (Validation / Integration Tests / Tooling)**: End-to-end tests,
     integration test suites, sample data, and dev tooling.

2. **Refactoring vs. Behavioral Changes**:
   - Extract preparatory refactoring (e.g. renaming, helper extractions, dead
     code pruning, moving files, cleanups) into early standalone layers.
   - Subsequent layers containing actual feature additions or bug fixes become
     clean, concise, and focused on behavioral intent rather than noise.

3. **Domain / Codeowner Alignment**:
   - Align layers with repository ownership boundaries so each PR has a
     concise list of codeowner reviewers who can review their subsystem in
     isolation without wading through unrelated files.

4. **Risk & Blast Radius Partitioning**:
   - Place inert, dormant code (e.g. new methods or feature implementations
     guarded by disabled flags or uncalled helpers) in lower layers.
   - Place active runtime activation (enabling flags, flipping defaults, wiring
     production entrypoints) in the final top layer to isolate operational risk.

5. **Atomicity & Bisectability Invariant**:
   - **CRITICAL**: Every single layer in the stack MUST compile, link, and
     pass unit tests independently (`git bisect` invariant). Never create an
     intermediate layer that leaves the codebase in a broken state.

---

## ✂️ Splitting an Existing PR into a Stack

> [!CAUTION]
> **MANDATORY POLICY: Explicit User Instruction Required**:
> Splitting an existing PR into a stack restructures git branches, creates
> multiple pull requests on GitHub, and alters active code review workflows.
> - An agent **MUST NOT** split a PR autonomously or speculatively.
> - Splitting a PR **MUST ONLY** be executed upon explicit user instruction in
>   the chat.
> - If an agent observes layering opportunities in a large PR, it may suggest or
>   propose a layering plan to the user, but it must wait for explicit user
>   approval before modifying any branches or creating a stack.

When explicitly instructed by the user to split an existing PR, follow this
step-by-step workflow:

### Step 1: Analyze PR State & Diff

1. Inspect current branch, working directory status, and commit log:
   ```bash
   git status
   git log --oneline <trunk>..HEAD
   ```
2. Inspect the complete diff against trunk (e.g. `main`):
   ```bash
   git diff <trunk>...HEAD --stat
   ```
3. Enable `rerere`:
   ```bash
   git config rerere.enabled true
   ```

### Step 2: Formulate the Layering Plan

Define an ordered list of layers from bottom (closest to trunk) to top. Each
layer must have:
- A descriptive branch name adhering to the stack namespace:
  `stack/<username>/<layer_name>`
- A clear scope (e.g. `Layer 1: Refactor auth helpers`,
  `Layer 2: Add token validation schema`, `Layer 3: Implement login endpoint`)
- Identified commits, files, or diff hunks mapped to that layer.

### Step 3: Construct the Stack Layers Non-Interactively

Choose either Method A (if granular commits already exist) or Method B (if
decomposing a single large commit or unstructured diff):

#### Method A: Splitting via Cherry-Picking Existing Commits

If the PR branch already has logical commits that just need grouping into
stack layers:

```bash
# 1. Start from base trunk
git checkout <trunk>

# 2. Create the first layer branch
git checkout -b stack/<username>/layer-1-models
git cherry-pick <commit_hash_1> <commit_hash_2>

# 3. Initialize the stack on the first layer
gh stack init --base <trunk> stack/<username>/layer-1-models

# 4. Add subsequent layers using gh stack add
gh stack add stack/<username>/layer-2-backend
git cherry-pick <commit_hash_3>

gh stack add stack/<username>/layer-3-api
git cherry-pick <commit_hash_4> <commit_hash_5>
```

#### Method B: Splitting a Monolithic Diff or Squashed PR

If the PR consists of a single large commit or unstructured changes, extract
files or patches layer-by-layer:

```bash
# Save original PR branch name
ORIGINAL_PR_BRANCH="$(git rev-parse --abbrev-ref HEAD)"

# 1. Start from trunk
git checkout <trunk>

# 2. Create and populate Layer 1
git checkout -b stack/<username>/layer-1-foundation
# Check out only foundational files from original PR branch
git checkout "$ORIGINAL_PR_BRANCH" -- path/to/models/ path/to/interfaces/
# Verify build & tests pass
git commit -m "foundation: Add core data models and interfaces"

# 3. Initialize stack with Layer 1
gh stack init --base <trunk> stack/<username>/layer-1-foundation

# 4. Create and populate Layer 2 on top of Layer 1
gh stack add stack/<username>/layer-2-service
# Check out backend service files
git checkout "$ORIGINAL_PR_BRANCH" -- path/to/services/ path/to/logic/
# Verify build & tests pass
git commit -m "service: Implement core business logic"

# 5. Create and populate Layer 3 on top of Layer 2
gh stack add stack/<username>/layer-3-api
# Check out API and consumer files
git checkout "$ORIGINAL_PR_BRANCH" -- path/to/api/ path/to/ui/
# Verify build & tests pass
git commit -m "api: Expose public endpoints and UI"

# 6. Verify full decomposition: diff against original PR branch should be empty
git diff "$ORIGINAL_PR_BRANCH" HEAD
```

### Step 4: Verify Intermediate Build & Test Invariants

Traverse every layer in the stack and verify that each compiles, builds, and
passes tests independently:

```bash
# Navigate to bottom layer
gh stack bottom
# Run local build / test command for layer 1 (e.g. cobalt build or pytest)

# Navigate upward through each layer
gh stack up
# Run build / test command for layer 2

gh stack top
# Run full test suite for final layer
```

### Step 5: Submit the Stack to GitHub

Always apply the 1-minute timer rule before submitting:

```bash
# Set 1-minute timeout on agent execution
# Submit non-interactively to create chained PRs on GitHub
gh stack submit --auto

# Or open PRs directly for review if ready
gh stack submit --auto --open
```

### Step 6: Present Stack to User & Preserve Original PR Delegation

After `gh stack submit --auto` completes:
1. Provide the user with a summary table listing all created stack layers,
   their branch names, target base branches, and newly assigned PR numbers/URLs.
2. **Do Not Autonomously Close Original PR**: Never close, abandon, or modify
   the original PR without explicit user instructions. Inform the user of the
   new stack so they can decide whether to close the original PR or supersede
   it.

---

## Best Practices

- **Explicit User Instruction for Splitting**: Never split an existing PR or branch autonomously. Propose the layering plan and wait for explicit user instruction before creating a stack or modifying branches.
- **Never Create Single-PR Stacks (Minimum Stack Size $\ge 2$)**: A stack of one is
  an anti-pattern and strictly prohibited. If you only have one change or PR to
  submit, always use standard Git workflow (branch + push to `fork` + `gh pr create`).
- **No Interactive Commands**: Never run `gh stack modify`, `gh stack switch`,
  `gh stack checkout` (without target), `gh stack submit` (without `--auto`),
  `gh stack view` (without `--short`/`--json`), or `gh stack init` (without
  branch args).
- **Enforce 1-Minute Safety Timer**: Always run `gh stack` commands with a 1-minute
  timeout or notification (`NotificationTimeoutSeconds: 60`). If the command stalls,
  verify whether a TUI/editor prompt was spawned and terminate immediately.
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
- **Safe Reordering to Avoid Inadvertent PR Closures**: When reordering stack
  layers, never push top layers while downstream PRs on GitHub still list them
  as their base branch. Always verify and update GitHub base branches first, or
  use new branch names if bases cannot be updated.
- **Upstack Propagation & Cross-Layer Verification**: After modifying any intermediate
  layer, always rebase upstack (`gh stack rebase --upstack`), resubmit (`gh stack submit --auto`),
  and verify build health on the top layer.
- **Review Ready State (`--open`)**: Submit with `--open` when PRs should trigger full
  CI runs and request review, avoiding stalled draft PRs.
