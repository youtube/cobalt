# Cobalt projet code review style guide

All code submitted to this repository must adhere to the established upstream Chromium style guides.

Primary Reference: When reviewing code, the ultimate source of truth for style, naming conventions, formatting, and best practices resides under the root styleguide/ directory of the repository.

Specific Languages: Reviewers are expected to verify compliance with the relevant style guide for the language in question (e.g., C++, Python, JavaScript).

Automation: While automated tools (linters, formatters) are used, the reviewer must still ensure the spirit of the style guide is followed, especially regarding readability, clarity, and architectural soundness.

# Review Scope Exclusion (Backports and Cherry-Picks)

To prevent redundant reviews and streamline the process for necessary maintenance tasks, the following rule must be observed:

Skip Backports/Cherry-Picks: Do not perform a detailed code style review on pull requests (CLs) that are clearly marked as backports or cherry-picks from an already reviewed upstream change.

Rationale: These changes have already received mandatory review in the original source repository. A review of these CLs should focus only on ensuring the successful and correct application of the patch to the target branch.

Identification: Look for common keywords in the CL title, branch name, or description (e.g., backport, cherry-pick, (cr/123456), or reference to the upstream commit hash).
