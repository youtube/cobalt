---
name: cobalt-ci-results-triage
description: >-
  Triages downloaded CI results in a source-idempotent way.
  Analyzes logs, groups failures by branch, and generates a unified report.
---

# CI Results Triage Skill

This skill analyzes downloaded CI results stored in a shared incoming directory and generates a unified triage report.

## Prerequisites
- Python 3

## Detailed Steps

- [ ] **Triage Results**

    Run the unified analyzer on the incoming directory containing the results JSON files.

    ```bash
    python3 {skill_dir}/scripts/unified_analyzer.py --incoming-dir /tmp/cobalt_gardener_${USER}/incoming --output gardener_report.md
    ```

    Replace `${USER}` with your username.

- [ ] **Verify Report**

    For each failure in the report:
    - Verify that the detected error signature is the actual cause of the failure.
        *   **Constraint**: Limit verification to log analysis. Do NOT view local source files, run code search for symbols, or check git branches/history to debug the failure or check if fixes are present.
    - If no signature was matched, inspect the log file manually:
        *   Open the cached log file.
        *   Inspect the log (especially near the end) to find the lines detailing the failure.
        *   Note the root cause and the specific error line for the report.
        *   Do NOT attempt to debug the code.
    - Check Buganizer for existing bugs matching the failure:
        *   Search for the specific error line or test name. Limit search to the first few highly relevant queries.
        *   Prioritize searching the [Gardener Hotlist](https://b.corp.google.com/hotlists/7245259) and the Cobalt component (componentid:114154).
        *   If a match is found, record the bug ID/status. If the matching bug is not on the [Gardener Hotlist](https://b.corp.google.com/hotlists/7245259), suggest adding it in the Next Steps.
        *   If no match is found, mark it as `Bug: NEW` in the report.
        *   Do NOT attempt to verify if the bug fix is merged in the current workspace.

- [ ] **Summarize and Report**

    Present the verified report to the user.

## Report Structure and Sorting

The generated raw report (also referred to as the Script Report) groups failures by branch:
- **Branch Health Report**: A high-level status summary for each branch.
- **Detailed Branch Failures**: Detailed failure logs grouped by run for each unhealthy branch.

In both sections, branches are sorted in **release version descending** order (e.g., `main` or `master` appears first, followed by release branches in descending version order like `25.lts.10.master`, `25.lts.2.master`, `19.lts.1.master`, etc.) to ensure that the most active and relevant branches are displayed at the top.
