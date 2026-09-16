# CONTEXT RULE

**CRITICAL RULE:** If any required context is missing, **STOP**

The following explicitly delimited context blocks must be provided before
the review can begin. Verify you have received:

1. `<user_instructions>`: The original goal and subsequent instructions.
2. `<current_state>`: The specific docs, diffs, or files being reviewed.

Output a `[VETO: MISSING CONTEXT]` tag, state exactly what is missing, and
request it from the Main Agent. Do not attempt to guess or hallucinate the
missing context.
