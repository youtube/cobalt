---
name: experimental-read-code-coverage-metrics
description: >-
  Retrieves and analyzes Chromium per-CL code coverage data from the Findit API.
  Records per-CL coverage metrics for all modified source files and extracts unexecuted code lines.
---

# Read Code Coverage Metrics

This skill fetches and processes code coverage data for a given Gerrit CL and
patchset. It uses the live Findit API to retrieve coverage percentages and
line-by-line details, records coverage metrics for all modified source files,
and extracts the code blocks corresponding to uncovered lines.

## When to Use This Skill

Activate this skill when:

- Establishing a baseline coverage state for a CL before attempting fixes.
- Verifying if coverage has improved after applying changes and re-running try
  jobs.
- You need to know not just the percentage, but *which specific lines* and *code
  blocks* are untested.

## Inputs (Passed directly in prompt)

When invoked as a subagent, the parent agent MUST explicitly pass:

- `cl_url`: The full Gerrit CL URL to fetch coverage for (e.g., `https://chromium-review.googlesource.com/c/chromium/src/+/7916168`).
- `artifact_path`: The destination file path to store the JSON metrics artifact (e.g., `scratch/original_metrics.json`).
- `target_files`: Optional list of specific target file paths to filter coverage for (e.g., `["chrome/browser/.../CommerceBottomSheetContentMediator.java"]`). If omitted, records metrics across all modified source files.

## Workflow

1. **Extract CL Details & Run Coverage Metrics Script**: Use
   `tools/code_coverage/parse_gerrit_url.py` to extract `host`, `project`,
   `change`, and `patchset` from `cl_url`:

   ```bash
   vpython3 tools/code_coverage/parse_gerrit_url.py <cl_url>
   ```

   Execute `fetch_coverage_metrics.py` passing the extracted parameters and optional `--files` filter:

   ```bash
   vpython3 tools/code_coverage/fetch_coverage_metrics.py --host <host> \
     --project <project> --change <change> --patchset <patchset> \
     --output <artifact_path> --files <target_files>
   ```

2. **Verify Coverage Artifact**: Confirm the script generated the artifact at
   `<artifact_path>`. The artifact maps modified source files to their coverage
   metrics and uncovered lines:

   ```json
   {
     "chrome/browser/.../CommerceBottomSheetContentMediator.java": {
       "metrics": {
         "absolute_coverage": 80.0,
         "incremental_coverage": 65.0,
         "absolute_unit_tests_coverage": 80.0,
         "incremental_unit_tests_coverage": 65.0
       },
       "low_coverage_type": [
         "incremental_coverage",
         "incremental_unit_tests_coverage"
       ],
       "uncovered_lines": {
         "77": "    mBottomSheetContent.setConfirmBoxVisible(false);",
         "78": "    mBottomSheetContent.setConfirmButtonEnabled(false);"
       }
     }
   }
   ```

## Output

The skill stores the detailed coverage JSON artifact directly at
`<artifact_path>` (`scratch/original_metrics.json`,
`scratch/control_metrics.json`, or `scratch/fix_metrics.json`). It should return
a summary message indicating the coverage metrics and unexecuted line counts
across modified source files.

## Important Considerations

- **Authentication**: If querying private CLs, pass `--auth-token` or set
  `LUCI_AUTH_TOKEN` in the environment.
- **Error Handling**: If the script exits with non-zero status, check stderr for
  API or decoding failures.
