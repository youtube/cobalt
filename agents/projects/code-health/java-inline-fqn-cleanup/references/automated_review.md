# Automated Review Protocol: Clean up inline FQNs

Use this prompt when delegating a final review of a java-inline-fqn-cleanup
patch to the **`generalist`** sub-agent.

## Review Prompt

Delegate the review to the **`generalist`** sub-agent with this exact prompt:

> "Follow the **Shared Automated Review Protocol** in
> `../../hub/references/shared_automated_review.md`.
>
> In addition to the generic checks, you MUST evaluate the patch against these
> **Specific Cleanup Rules** for Clean up inline FQNs:
>
> - **Collision Avoidance:** Verify that replacing an inline FQN (e.g., `a.b.C`)
>   did not introduce a collision with another class `C` imported from a
>   different package (e.g., `x.y.C`).
> - **Completeness:** Verify that all occurrences of the inline FQN in the
>   modified files have been replaced with the simple class name. "

## Handling Findings

Follow the **Handling Findings** section in
`../../hub/references/shared_automated_review.md` to resolve any issues reported
by the reviewer.
