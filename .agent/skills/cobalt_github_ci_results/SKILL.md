---
name: cobalt-github-ci-results
description: >-
  Retrieves GitHub Actions CI results (discovers failed runs and downloads logs)
  and outputs them in the unified schema format.
  Implements: cobalt gardener results gathering.
---

# GitHub Actions CI Results Retriever Skill

This skill discovers recent failed GitHub Actions runs for the Cobalt repository, downloads their logs, and saves them in the unified results schema format.

## Prerequisites
- `gh` CLI installed and authenticated.

## Detailed Steps

- [ ] **Retrieve GHA Results**

    Run the retrieval script to discover failed runs and download their logs.

    ```bash
    python3 {skill_dir}/scripts/github_download.py --output /tmp/cobalt_gardener_${USER}/incoming/github_results.json
    ```

    You can optionally specify `--run-id` to retrieve results for a specific run:

    ```bash
    python3 {skill_dir}/scripts/github_download.py --run-id <run_id> --output /tmp/cobalt_gardener_${USER}/incoming/github_results.json
    ```
