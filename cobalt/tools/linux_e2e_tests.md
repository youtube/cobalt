// Copyright 2026 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

# Linux End-to-End (E2E) Test Suite

Comprehensive documentation for Cobalt's Linux end-to-end (E2E) test suite, covering application preloading, revealing, and runtime lifecycle state transitions.

---

## 1. Overview

The Linux E2E test suite (`cobalt/tools/test_lifecycle_e2e.py` and `cobalt/tools/test_common.py`) validates that Cobalt behaves correctly across all Starboard lifecycle state transitions on Linux platforms (both Modular and Evergreen).

The test suite uses a platform-agnostic `LifecycleController` abstraction layer defined in `cobalt/tools/test_common.py`. On Linux platforms, `PosixSignalLifecycleController` is used, which relies on the platform using [`starboard/shared/signal/suspend_signals.cc`](file:///usr/local/google/home/jfoks/cobalt.main3/src/starboard/shared/signal/suspend_signals.cc) (installed via `InstallSuspendSignalHandlers()`) to intercept POSIX OS signals and dispatch the corresponding Starboard application lifecycle events (`SbSystemRequestConceal`, `Application::Blur`, `Application::Focus`, `SbSystemRequestFreeze`, `Application::Stop`).

Tests interact with the running Cobalt instance using:
1. **LifecycleController**: Dispatches semantic lifecycle transitions (`blur()`, `focus()`, `conceal()`, `freeze()`, `resume()`, `stop()`), specialized per platform.
2. **Chrome DevTools Protocol (CDP)**: Inspects DOM properties and captures JavaScript lifecycle events in real time.

---

## 2. Preconditions & Requirements

### 2.1 Build Artifacts
Before running the tests, Cobalt must be built in `qa` or `devel` configuration.

- **Linux Modular** (`linux-x64x11-modular`):
  - Executable: `out/linux-x64x11-modular_qa/cobalt_loader`
  - Target: `//cobalt:cobalt_loader`
- **Linux Evergreen** (`evergreen-x64`):
  - Executable: `out/evergreen-x64_qa/loader_app`
  - Library: `out/evergreen-x64_qa/libcobalt.so`
  - Targets: `//cobalt:cobalt_loader`, `//starboard/loader_app:loader_app`, `//cobalt/elf_loader:elf_loader_sandbox`

### 2.2 System & Runtime Dependencies
- **Python 3**: Requires Python 3.8+ using **standard library only** (`socket`, `struct`, `base64`, `json`, `subprocess`). No external pip packages (e.g., `websockets`) are required.
- **Display**:
  - A functional X11 `$DISPLAY` session, **OR**
  - `xvfb-run` installed on the system (the test runner automatically detects headless or broken displays and transparently relaunches under `xvfb-run`).
- **Network / Ports**:
  - Local port `9223` (or custom port via `--port`) available for DevTools HTTP and WebSocket connections.

---

## 3. Test Cases & Verification Procedures

### 3.1 Preload & Reveal Integration Test (`preload`)
Validates that Cobalt can launch in the background in a preloaded state without taking focus or rendering, and later be revealed cleanly.

1. **Launch with `--preload`**:
   - Launches `cobalt_loader` (or `loader_app`) with `--preload --remote-debugging-port=9223`.
2. **Verify Initial Preloaded State**:
   - Polls CDP until `document.visibilityState == 'hidden'` and `document.hasFocus() == false`.
3. **Trigger Reveal**:
   - Calls `controller.resume()` (sends `SIGCONT` on Linux).
4. **Verify Revealed State**:
   - Polls CDP until `document.visibilityState == 'visible'` and `document.hasFocus() == true`.
5. **Clean Termination**:
   - Calls `controller.stop()` (sends `SIGPWR` on Linux) and verifies the process terminates cleanly with exit code `0`.

---

### 3.2 Full Lifecycle State Transitions Test (`lifecycle`)
Validates that the Starboard lifecycle state machine dispatches the expected W3C DOM events in exact sequence when receiving OS signals.

#### In-Page Event Logger
Upon connection, the test runner injects an event queue into the page:
```javascript
window.event_log = [];
['focus', 'blur', 'freeze', 'resume'].forEach(name => {
  window.addEventListener(name, () => window.event_log.push({type: name}));
});
document.addEventListener('visibilitychange', () => {
  window.event_log.push({type: 'visibilitychange', visibility: document.visibilityState});
});
```

#### Transition Sequence & Signal Mapping (via `suspend_signals.cc`)
Signal mappings implemented in [`starboard/shared/signal/suspend_signals.cc`](file:///usr/local/google/home/jfoks/cobalt.main3/src/starboard/shared/signal/suspend_signals.cc):

| Step | Controller Method | Linux Signal (`suspend_signals.cc`) | Expected Starboard State | DOM Assertions | Events Popped from Queue |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **0. Initial** | Launch | App Launch | `kSbLifecycleStateStarted` | `visibilityState == 'visible'`, `hasFocus() == true` | Initial render |
| **1. Blur** | `controller.blur()` | `SIGWINCH` | `kSbLifecycleStateBlurred` | `document.hasFocus() == false` | `{'type': 'blur'}` |
| **2. Focus** | `controller.focus()` | `SIGCONT` | `kSbLifecycleStateStarted` | `document.hasFocus() == true` | `{'type': 'focus'}` |
| **3. Conceal** | `controller.conceal()` | `SIGUSR1` | `kSbLifecycleStateConcealed` | `visibilityState == 'hidden'`, `hasFocus() == false` | `{'type': 'blur'}`, `{'type': 'visibilitychange', 'visibility': 'hidden'}` |
| **4. Freeze** | `controller.freeze()` | `SIGTSTP` | `kSbLifecycleStateFrozen` | Process suspended | `{'type': 'freeze'}` |
| **5. Resume & Reveal** | `controller.resume()` | `SIGCONT` | `kSbLifecycleStateStarted` | `visibilityState == 'visible'`, `hasFocus() == true` | `{'type': 'resume'}`, `{'type': 'visibilitychange', 'visibility': 'visible'}`, `{'type': 'focus'}` |
| **6. Stop** | `controller.stop()` | `SIGPWR` | `kSbLifecycleStateStopped` | Process exits | Clean exit (Code 0) |


---

## 4. Usage & CLI Options

### 4.1 Running via Python Runner
```bash
# Run both preload and lifecycle tests on Modular QA:
python3 cobalt/tools/test_lifecycle_e2e.py \
  --platform linux-x64x11-modular \
  --config qa \
  --out-dir out/linux-x64x11-modular_qa

# Run on Evergreen QA:
python3 cobalt/tools/test_lifecycle_e2e.py \
  --platform evergreen-x64 \
  --config qa \
  --out-dir out/evergreen-x64_qa

# Run only the preload test:
python3 cobalt/tools/test_lifecycle_e2e.py --tests preload

# Run with a custom binary:
python3 cobalt/tools/test_lifecycle_e2e.py --executable /path/to/cobalt_loader
```

### 4.2 Available Flags
- `--platform`: Target platform (`modular`, `linux-x64x11-modular`, `evergreen`, `evergreen-x64`).
- `--config`: Build configuration (`qa` [default], `devel`).
- `--out-dir`: Directory containing compiled build outputs.
- `--executable`: Explicit path or command to launch Cobalt.
- `--port`: DevTools port (default: `9223`).
- `--tests`: Comma-separated list of tests to run: `all` [default], `preload`, `lifecycle`.
- `--timeout`: Maximum seconds to wait for each transition assertion (default: `120.0`).
- `--verbose`: Enable debug logging output.
