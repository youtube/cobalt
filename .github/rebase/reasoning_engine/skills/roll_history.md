# Cobalt Chromium Roll & Rebase Ground-Truth Catalog

Historical catalog of upstream Chromium rolls into Cobalt staging (`M139` – `M141`).

In each merged **Bot Roll PR**, the initial commits represent the conflicted upstream roll baseline created by infra bots, while the trailing commits represent the human ground-truth resolution.

---

## Rebase PR Catalog

| Milestone | Roll Target | Bot Roll PR | Vertex AI PR |
| :--- | :--- | :--- | :--- |
| **M141** | **M141.7364** | [#12257](https://github.com/youtube/cobalt/pull/12257) *(Active)* | — |
| | **M141.7351** | [#12228](https://github.com/youtube/cobalt/pull/12228) | [#12259](https://github.com/youtube/cobalt/pull/12259) |
| **M140** | **M140.7339** | [#12161](https://github.com/youtube/cobalt/pull/12161) | [#12176](https://github.com/youtube/cobalt/pull/12176) |
| | **M140.7318** | [#12155](https://github.com/youtube/cobalt/pull/12155) | — |
| | **M140.7298** | [#12086](https://github.com/youtube/cobalt/pull/12086) | — |
| | **M140.7278** | [#12051](https://github.com/youtube/cobalt/pull/12051) | [#12136](https://github.com/youtube/cobalt/pull/12136) |
| **M139** | **M139.7244** | [#11890](https://github.com/youtube/cobalt/pull/11890) | [#12038](https://github.com/youtube/cobalt/pull/12038) |
| | **M139.7217** | [#11722](https://github.com/youtube/cobalt/pull/11722) | [#12258](https://github.com/youtube/cobalt/pull/12258) |

---

## Comparative Analysis Guidelines

1. **Self-Contained Bot Roll PR Analysis**:
   - For any merged **Bot Roll PR**, partition the commits:
     - **Infra Baseline Commits**: Initial bot commits bringing in upstream Chromium changes.
     - **Human Ground-Truth Fix Commits**: Trailing commits resolving merge conflicts, GN build breaks, and compiler errors.
2. **AI Comparison**:
   - When a corresponding **Vertex AI PR** is listed, compare the AI's resolution diff against the trailing human fix commits of the **Bot Roll PR** to analyze discrepancies and extract reusable lessons.


---

## Expert Review Insights

### Diff-Tooling Sanity Check Protocol

Before drawing any comparative conclusions from `TOOL_DIFF_FILE` or `TOOL_UPSTREAM_DIFF` results, validate the tool dispatcher's health:

1. **Argument Hygiene**: Issue tool calls as a single bare token (e.g. `TOOL_DIFF_FILE: DEPS`), never appended with analytical commentary on the same line.
2. **Echo Verification**: Confirm the returned `=== Target File: <X> ===` header exactly matches the single-token argument supplied. If `<X>` contains prose, multi-sentence commentary, or run-on punctuation, the dispatcher mis-parsed the argument — discard the result and do not draw conclusions from it.
3. **Implausibility Check for DEPS**: For any named/versioned Chromium milestone roll PR (e.g. `Update to 139.7217`), DEPS changes (submodule pins, `checkout_cobalt_internal`, Cobalt revision overrides) are near-certain. If both `TOOL_DIFF_FILE: DEPS` and `TOOL_UPSTREAM_DIFF: DEPS` report zero changes simultaneously across Human, AI, and upstream Commit #3, halt immediately and flag a probable commit-range/harness defect — do NOT report this as evidence of Human/AI equivalence.
4. **Zero-Inventory Check**: If the pre-supplied Modified Files Inventory reports 0 files across all three categories (Shared/Human-only/AI-only) for a milestone roll PR, treat this as a strong signal of an incorrect merge-base or commit-range computation in the inventory-generation step, and request inventory regeneration before proceeding with file-level analysis.


---

## Expert Review Insights

### Zero-Inventory Hard Stop (Enforcement Escalation)

If a Modified Files Inventory for a named/versioned Chromium milestone roll PR (e.g., "Update to 140.7298") reports **0 files in all three categories** (Shared/Human-only/AI-only), this MUST be treated as a **blocking defect**, not merely a cautionary signal:

1. Immediately cross-check against the bot roll commit's own CONFLICTED file manifest (present in every roll commit message under a fenced `


---

## Expert Review Insights

### Zero-Inventory + Zero-Canary-Diff Compound Failure Signature

When BOTH of the following occur simultaneously for a named/versioned milestone roll PR pair:
1. The Modified Files Inventory reports 0 files across all three categories (Shared/Human-only/AI-only), AND
2. Canary files (`DEPS`, `.github/AUTOROLL_CHROMIUM`, and at least one historically high-churn source file like `media/starboard/sbplayer_bridge.cc`) all report zero diff in both Human and AI PRs across repeated tool invocations,

this is definitive evidence of an **incorrect commit-range or merge-base computation** in the review harness — not evidence of Human/AI parity, and not a partial/truncated diff issue.

**Mandatory reviewer response:**
- Do NOT produce a "no differences found" or "AI matched Human" conclusion under any framing.
- Mark the entire comparative review as **UNVERIFIED** in the Executive Summary.
- Still surface the milestone-appropriate known breaking-pattern checklist (from Section 4 of `cobalt_rebase.md`, extended by milestone-specific Expert Review Insights) as a manual fallback checklist.
- Explicitly recommend escalation to fix PR ref/commit-range resolution (e.g., verify the diff tool is pointed at the correct base/head SHAs for both PR #s, not a stale or default branch) before any further automated comparative review is attempted for this PR pair.
- Do not spend additional tool-call budget re-probing the same files with reworded arguments once this compound signature is detected after 2-3 corroborating probes; escalate immediately instead.
