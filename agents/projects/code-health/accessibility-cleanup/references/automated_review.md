# Automated Review Protocol: Histogram Cleanup

Use this prompt when delegating a final review of a histogram cleanup patch to
the **`generalist`** sub-agent.

## Review Prompt

> You are a highly experienced code reviewer specializing in Git patches for
> Code Health and accessibility correctness in Chromium. You task is to analyze
> the provided git patch and provide comprehensive, constructive feedback that
> can be used to verify the efficacy/accuracy of the change. The most important
> aspect of the review is to ensure that the end-user behavior has no
> regressions and is expected to improve from more accurate API usage.
>
> # Step by Step Instructions
>
> 1. Run `git diff HEAD` to generate the patch. Read the patch carefully to
>    understand the removals and changes.
>
> 2. Analyze the `patch` for potential issues across these specific areas:
>
>    - **Functionality & Verification (CRITICAL):** Does the code still work as
>      intended? Use `cs` or `rg` to verify integrity. Ensure any unused
>      methods, variables, or imports resulting from the change are also
>      removed. Check callers, headers, and tests for architectural
>      completeness. Check that we are not creating duplicate code with a
>      reusable component or utility method/class that exists in the code base.
>
>    - **Consistency & Style:** Are there any inconsistencies with existing code
>      or patterns? Ensure that legacy comments have been updated. Ensure that
>      there are no fully qualified paths and instead direct imports are used.
>
>    - **Unused Strings:** Are there any strings that are now unused in the
>      code? If so, ensure that the strings have been removed along with their
>      corresponding png sha1 files if such files exist. For example, when
>      removing the string `example_string`, which is declared in the file:
>      `components/feature_strings.grdp` with the name `IDS_EXAMPLE_STRING`,
>      there may be a corresponding file:
>      `components/feature_strings_grdp/IDS_EXAMPLE_STRING.png.sha1`, which
>      should be deleted.
>
>    - **Correct `BUILD.gn` dependencies:** For files that were modified such
>      that imports were added or removed, find the corresponding `BUILD.gn`
>      file for the modified file, and verify that no changes to the `deps` of
>      that target need to be made. Check whether or not any `deps` can now be
>      deleted. To verify the need for additional deps (or dependency chains
>      that are forbidden), run the `gn check` command.
>
> 3. Formulate concise and constructive feedback for each identified issue.
>    Categorize findings by severity ([Critical], [Major], [Minor]). Provide a
>    clear "Why" and a **numbered list of specific steps** for "Suggested
>    Remediation" for each point.
>
> 4. If all criteria are met and the independent verification confirms no
>    leftover references, output exactly `PASS`. Otherwise, output the complete
>    review.

## Handling Findings

If the review returns findings:

1. **Output Findings:** Display the complete review findings to the user.
2. Analyze the `[Critical]` and `[Major]` issues.
3. Apply the **Suggested Remediation** for each of these issues **sequentially
   (one by one)**.
4. Once all remediations for the identified batch of issues have been applied,
   re-run the review protocol.
5. Repeat until the review returns `PASS`.
6. **Final Validation:** Once `PASS` is achieved, if any changes were made
   during this loop, execute `git cl format` and
   `python3 tools/metrics/histograms/validate_format.py` one last time before
   moving to the next phase in the skill.

IMPORTANT NOTE: Start directly with the output, do not output any delimiters.
Each instruction is crucial and must be executed with utmost care and attention
to detail.
