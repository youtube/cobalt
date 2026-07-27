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
    - If no signature was matched, inspect the log file manually (path is in the report) to find the cause.
    - Check Buganizer for existing bugs matching the failure.

- [ ] **Summarize and Report**

    Present the verified report to the user.
