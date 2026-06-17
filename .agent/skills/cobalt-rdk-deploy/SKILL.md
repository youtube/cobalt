---
name: cobalt-rdk-deploy
description: >-
  Deploy, run, test, reset, and inspect logs for Cobalt on RDK devices using deploy_rdk.py. Use when building or launching Cobalt as a plugin or executable, running NPLB/GTest suites, resetting inactive display sessions on the box, reverting the active loader to Cobalt 25, setting up the RDK cross-compilation toolchain, or streaming/following journalctl logs from the target device. Don't use for generic Android, Linux, or Raspberry Pi builds.
---

# Cobalt RDK Deploy Skill

A skill for cross-compiling, deploying, testing, and debugging Cobalt on RDK devices using the `deploy_rdk.py` CLI script.

## Quick Start

Create an alias for the deployment script:

```bash
alias deploy_rdk='python3 starboard/contrib/rdk/src/third_party/starboard/rdk/arm/scripts/deploy_rdk.py'
```

## Execution Protocol

Instead of guessing flags or following hardcoded task menus, **always run `--help` first** to inspect the available arguments and execution modes:

```bash
deploy_rdk --help
```

1. **Inspect the help output** to identify the correct flag(s) for your target task (e.g., `--setup-toolchain`, `--run`, `--reset`, `--logs`, or `--revert-c25`).
2. **Formulate and run the command** directly based on the self-documenting options.

*Note: Remote Build Execution (RBE) is enabled by default. Only pass `--no-rbe` if RBE compilation commands fail.*
