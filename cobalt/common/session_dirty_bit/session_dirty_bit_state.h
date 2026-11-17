// Copyright 2026 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef COBALT_COMMON_SESSION_DIRTY_BIT_SESSION_DIRTY_BIT_STATE_H_
#define COBALT_COMMON_SESSION_DIRTY_BIT_SESSION_DIRTY_BIT_STATE_H_

#include <cstdint>
#include <string_view>

#include "base/component_export.h"
#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"

namespace cobalt {

// File persisted under `DIR_CACHE` (or `SHELL_DIR_USER_DATA` in browser tests)
// tracking the active Cobalt session's dirty-bit stage across process exits.
COMPONENT_EXPORT(COBALT_SESSION_DIRTY_BIT)
extern const base::FilePath::CharType kSessionDirtyBitFilename[];

// Internal command-line switch forwarded to out-of-process renderer
// subprocesses when the previous session terminated with a dirty bit set.
inline constexpr char kPreviousSessionDirtyBitStageSwitch[] =
    "cobalt-previous-session-dirty-bit-stage";

// Two-stage session dirty bit comparable to YouTube Living Room's
// UnexpectedRestartService dirty bit, extended with pre-JS startup coverage:
// - `kClean`: Previous session exited cleanly (blur, conceal, stop, destroy,
//             explicit reload/exit) or no prior dirty bit existed.
// - `kEarly`: Previous session terminated uncleanly during early startup before
//             the web application promoted the dirty bit to `kLate`.
// - `kLate`:  Previous session terminated uncleanly during an active foreground
//             session after the web application promoted the dirty bit.
enum class SessionDirtyBitStage : uint8_t {
  kClean = 0,
  kEarly = 1,
  kLate = 2,
};

COMPONENT_EXPORT(COBALT_SESSION_DIRTY_BIT)
std::string_view SessionDirtyBitStageToString(SessionDirtyBitStage stage);

COMPONENT_EXPORT(COBALT_SESSION_DIRTY_BIT)
SessionDirtyBitStage SessionDirtyBitStageFromString(std::string_view value);

// Returns true when Browser and Renderer share the same OS process
// (`--single-process`), meaning `GetPreviousSessionDirtyBitStage()` sees
// browser-side updates directly in memory without Mojo listener notifications.
COMPONENT_EXPORT(COBALT_SESSION_DIRTY_BIT)
bool IsSessionDirtyBitSharedInProcess();

// Maps `<storage_dir>/CobaltSessionDirtyBit` once at early browser startup
// (before UI-thread blocking restrictions take effect) to snapshot the previous
// session's exit stage, and arms `kEarly` if `is_initially_visible` is true.
// Subsequent runtime transitions update the `MAP_SHARED` byte in memory with
// zero syscalls and immediate kernel page-cache durability across crashes.
COMPONENT_EXPORT(COBALT_SESSION_DIRTY_BIT)
void InitializeSessionDirtyBit(const base::FilePath& storage_dir,
                               bool is_initially_visible);

// Lock-free O(1) read of the previous session's dirty-bit stage for Blink JS
// bindings and child-process launch switch forwarding. Falls back once to
// `--cobalt-previous-session-dirty-bit-stage` in out-of-process renderers.
COMPONENT_EXPORT(COBALT_SESSION_DIRTY_BIT)
SessionDirtyBitStage GetPreviousSessionDirtyBitStage();

// Explicitly sets the in-memory previous-session stage snapshot (used by
// renderer-side `clearSessionDirtyBit()`, Mojo listener updates, and tests).
COMPONENT_EXPORT(COBALT_SESSION_DIRTY_BIT)
void SetPreviousSessionDirtyBitStage(SessionDirtyBitStage stage);

// Promotes the active session's dirty bit from `kEarly` to `kLate` once the web
// application has bootstrapped (marking any prior-session crash as consumed).
COMPONENT_EXPORT(COBALT_SESSION_DIRTY_BIT)
void PromoteSessionDirtyBit();

// Marks any prior-session dirty bit as consumed once the primary main frame
// finishes bootstrap (e.g., `hideSplashScreen()` / `exit()`), ensuring devices
// in Mendel Control arms or pre-rollout builds cleanly disarm to `kClean` on
// exit even without calling `promoteSessionDirtyBit()`.
COMPONENT_EXPORT(COBALT_SESSION_DIRTY_BIT)
void MarkPreviousSessionDirtyBitConsumed();

// Explicitly clears both the on-disk dirty bit and the previous-session stage
// snapshot ahead of an intentional web app reload or paused preload cleanup,
// marking any prior-session crash as consumed and resetting `kLate` promotion.
COMPONENT_EXPORT(COBALT_SESSION_DIRTY_BIT)
void ClearSessionDirtyBit();

// Disarms the dirty bit on clean platform transitions away from active
// foreground (`Blur`/`onPause`, `Conceal`/`onStop`, `Freeze`, `Stop`,
// `onDestroy`, `h5vcc.system.exit()`). If the prior session crashed and JS has
// not yet consumed it via `PromoteSessionDirtyBit()` or
// `ClearSessionDirtyBit()`, restores the unconsumed prior crash stage on disk
// and in memory so transient early-boot blurs or pre-bootstrap exits never
// silently drop a previous crash.
COMPONENT_EXPORT(COBALT_SESSION_DIRTY_BIT)
void OnSessionDirtyBitPlatformDisarm(bool reset_promotion);

// Re-arms the on-disk dirty bit when entering active foreground (`Focus` /
// `onResume`), writing `kLate` if the current document already promoted or
// `kEarly` if still bootstrapping.
COMPONENT_EXPORT(COBALT_SESSION_DIRTY_BIT)
void OnSessionDirtyBitPlatformForeground();

// Invoked when a committed cross-document navigation finishes on the primary
// main frame. Resets `kLate` promotion for the new document and arms `kEarly`
// on disk if currently in the foreground, without altering the previous-session
// snapshot.
COMPONENT_EXPORT(COBALT_SESSION_DIRTY_BIT)
void OnSessionDirtyBitMainFrameNavigationCommitted();

// Registers a browser-side callback invoked whenever the previous-session stage
// transitions to `kClean` due to disarming, allowing out-of-process renderers
// to stay synchronized.
COMPONENT_EXPORT(COBALT_SESSION_DIRTY_BIT)
void SetPreviousSessionDirtyBitClearedCallback(base::RepeatingClosure callback);

COMPONENT_EXPORT(COBALT_SESSION_DIRTY_BIT)
SessionDirtyBitStage ReadSessionDirtyBitFileForTesting(
    const base::FilePath& storage_dir);

COMPONENT_EXPORT(COBALT_SESSION_DIRTY_BIT)
void WriteSessionDirtyBitFileForTesting(const base::FilePath& storage_dir,
                                        SessionDirtyBitStage stage);

COMPONENT_EXPORT(COBALT_SESSION_DIRTY_BIT)
void ResetSessionDirtyBitForTesting();

}  // namespace cobalt

#endif  // COBALT_COMMON_SESSION_DIRTY_BIT_SESSION_DIRTY_BIT_STATE_H_
