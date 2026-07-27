---
name: cobalt-kokoro-ci-results
description: >-
  Retrieves Kokoro CI results (discovers failed runs and downloads logs)
  and outputs them in the unified schema format.
  Implements: cobalt gardener results gathering.
---

# Kokoro CI Results Retriever Skill

This skill discovers recent failed Kokoro runs for the Cobalt repository, downloads their logs (including child test logs from Sponge), and saves them in the unified results schema format.

## Prerequisites
- Active LOAS credentials (`gcert`).
- `stubby` and `fileutil` CLI tools.

## Detailed Steps

- [ ] **Retrieve Kokoro Results**

    Run the retrieval script to download failed runs and logs.

    ```bash
    python3 {skill_dir}/scripts/kokoro_download.py --output /tmp/cobalt_gardener_${USER}/incoming/kokoro_results.json
    ```

    You can optionally specify `--build` to retrieve results for a specific build/invocation ID:

    ```bash
    python3 {skill_dir}/scripts/kokoro_download.py --build <build_id> --output /tmp/cobalt_gardener_${USER}/incoming/kokoro_results.json
    ```
