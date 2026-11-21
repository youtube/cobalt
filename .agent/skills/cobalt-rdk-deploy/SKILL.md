---
name: cobalt-rdk-deploy
description: >-
  Builds, deploys, runs, resets, and inspects logs for Cobalt on RDK devices using deploy_rdk.py. Also handles flashing RDK system images via flash_rdk.py when explicitly requested. Use when compiling or launching Cobalt, running GTest suites, resetting display sessions, fetching journalctl logs, or flashing RDK boxes. Don't use for generic Linux, Android, or Raspberry Pi builds.
---

# Cobalt RDK Deploy Skill

A skill for building, deploying, running, and debugging Cobalt on RDK devices using the `deploy_rdk.py` CLI script, and flashing system software using `flash_rdk.py`.

## Quick Start

Create aliases for RDK scripts:

```bash
alias deploy_rdk='python3 starboard/contrib/rdk/src/third_party/starboard/rdk/arm/scripts/deploy_rdk.py'
alias flash_rdk='python3 starboard/contrib/rdk/src/third_party/starboard/rdk/arm/scripts/flash_rdk.py'
```

## Execution Protocol

1. **Always query `--help` first** to inspect the available arguments and execution modes of the script:
   ```bash
   deploy_rdk --help
   ```

2. **Formulate and run the command** directly based on the self-documenting options. Do not guess flags or hardcode command paths.

3. **Avoid manual device interaction:** Do NOT run raw shell commands (via `adb shell` or `ssh`) directly on the target device to perform pre-flight checks (such as verifying device properties, OS versions, symlinks, or config status). The `deploy_rdk.py` script automatically manages all version checks, configuration switches, display resets, and reboots internally. Trust the script and execute your formulated command directly.

4. **Flashing RDK Device System Software:**
   - If (and ONLY if) the user explicitly asks to flash or update the RDK system image, use `flash_rdk.py`:
     ```bash
     flash_rdk --help
     ```
   - **IMPORTANT:** Do NOT refer to, mention, or suggest `flash_rdk.py` for general building, deployment, testing, resetting, or debugging tasks unless the user explicitly requests flashing or re-flashing the image / RDK box.

5. **Identify the connection method:**
   - **ADB (Default):** The script defaults to ADB for device interaction.
   - **SSH/SCP:** If you need to target a remote device over SSH, check `--help` for the IP-based configuration flags (e.g., `--device-ip`).

6. **Do NOT use `--no-rbe` by default:** Remote Build Execution (RBE) is enabled by default and must be used for compilation. Only pass the `--no-rbe` flag if RBE build commands fail.

7. **Assume the toolchain is present:** Do NOT pre-emptively check for the cross-compilation toolchain (do not run `echo $RDK_HOME` or list directories to verify sysroots). Assume it is fully configured and present by default. Only troubleshoot the toolchain or run `--setup-toolchain` if the compilation step (`autoninja`) fails with compiler/toolchain errors, or if the user explicitly requests it.

8. **Side-by-side RDK6 and RDK7 support:** `deploy_rdk.py` defaults to RDK6 (`out/evergreen-arm-hardfp-rdk_<config>`). When targeting an RDK7 device, pass `--rdk-version 7` (which builds into `out/evergreen-arm-hardfp-rdk7_<config>` and forwards `--rdk-version 7` to `gn.py`; pass `--rdk-home` or `--toolchain-installer` when overriding the SDK path or installer). The script enforces per-version `/version.txt` minimums and rejects deploying RDK7 native binaries (`GLIBC_2.34` / `GLIBCXX_3.4.29`) onto RDK6 devices automatically.
