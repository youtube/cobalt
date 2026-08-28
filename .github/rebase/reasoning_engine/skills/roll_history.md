# Cobalt Chromium Roll & Rebase Ground-Truth Catalog

Historical catalog of upstream Chromium rolls into Cobalt staging (`M139` – `M141`).

In each merged **Bot Roll PR**, the initial commits represent the conflicted upstream roll baseline created by infra bots, while the trailing commits represent the human ground-truth resolution.

---

## Rebase PR Catalog

| Milestone | Roll Target | Bot Roll PR | Vertex AI PR |
| :--- | :--- | :--- | :--- |
| **M141** | **M141.7364** | [#12243](https://github.com/youtube/cobalt/pull/12243) *(Active)* | [#12243](https://github.com/youtube/cobalt/pull/12243) |
| | **M141.7351** | [#12228](https://github.com/youtube/cobalt/pull/12228) | [#12259](https://github.com/youtube/cobalt/pull/12259) |
| **M140** | **M140.7339** | [#12161](https://github.com/youtube/cobalt/pull/12161) | [#12176](https://github.com/youtube/cobalt/pull/12176) |
| | **M140.7298** | [#12086](https://github.com/youtube/cobalt/pull/12086) | — |
| | **M140.7278** | [#12051](https://github.com/youtube/cobalt/pull/12051) | [#12136](https://github.com/youtube/cobalt/pull/12136) |
| **M139** | **M139.7244** | [#11722](https://github.com/youtube/cobalt/pull/11722) | [#12038](https://github.com/youtube/cobalt/pull/12038) |
| | **M139.7217** | [#11722](https://github.com/youtube/cobalt/pull/11722) | [#12258](https://github.com/youtube/cobalt/pull/12258) |

---

## Comparative Analysis Guidelines

1. **Self-Contained Bot Roll PR Analysis**:
   - For any merged **Bot Roll PR**, partition the commits:
     - **Infra Baseline Commits**: Initial bot commits bringing in upstream Chromium changes.
     - **Human Ground-Truth Fix Commits**: Trailing commits resolving merge conflicts, GN build breaks, and compiler errors.
2. **AI Comparison**:
   - When a corresponding **Vertex AI PR** is listed, compare the AI's resolution diff against the trailing human fix commits of the **Bot Roll PR** to analyze discrepancies and extract reusable lessons.
