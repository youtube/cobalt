# Tooling Guidance & Production Hardening

## Production Hardening Checklist

Synthesis MUST ensure:

1. **Lifetime Safety:** Use `base::RefCountedDeleteOnSequence` for timers.
2. **Zero-Copy:** Prefer `std::move` and `base::RefCountedString`.
3. **DoS Mitigation:** Enforce strict length limits (e.g., 64KB).
4. **Atomic State:** Ensure callback checks (e.g., `if (callback_)`) are
   atomically sound or strictly sequence-enforced to prevent double-runs.

## Infrastructure & Tooling Guidance

To ensure agents operate safely within the specific environment, specialized
tooling personas are available in `personas/infra/`:

- **`infra/jj_git.json`**: Expert in `jj` on Git workflow. Agents performing
  file operations or commit management in a `JJ` environment SHOULD consult this
  persona to avoid losing Gerrit `Change-Id`s or mishandling detached HEAD
  states.
- **`infra/chromium_build.json`**: Expert in Chromium build tools. Agents
  performing builds or adding new files SHOULD consult this persona to ensure
  correct target discovery and usage of `autoninja`.
