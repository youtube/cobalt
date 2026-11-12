// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CONTROLLER_MEMORY_SAVER_CONTROLLER_H_
#define THIRD_PARTY_BLINK_RENDERER_CONTROLLER_MEMORY_SAVER_CONTROLLER_H_

#include "base/timer/timer.h"
#include "build/build_config.h"
#include "third_party/blink/renderer/controller/controller_export.h"

#if BUILDFLAG(IS_COBALT)
#include <memory>
#include "base/memory/memory_pressure_listener.h"
#endif  // BUILDFLAG(IS_COBALT)

namespace blink {

// Tracks available memory to toggle memory saver mode on v8 isolates.
class CONTROLLER_EXPORT MemorySaverController {
 public:
  static void Initialize();

 private:
  MemorySaverController();

  void Sample();
  void SetMemorySaverModeForAllIsolates(bool memory_saver_mode_enabled);

#if BUILDFLAG(IS_COBALT)
  // Turns memory saver mode on when memory pressure is signaled
  // (kCobaltV8MemorySaverOnMemoryPressure).
  void OnMemoryPressure(
      base::MemoryPressureListener::MemoryPressureLevel level);
  // Called once no memory pressure has been signaled for the hold time.
  void OnMemoryPressureHoldExpired();
  // Applies memory saver mode if either input asks for it.
  void UpdateMemorySaverMode();

  std::unique_ptr<base::MemoryPressureListener> memory_pressure_listener_;
  base::OneShotTimer memory_pressure_hold_timer_;
  // Set by Sample() when available memory is low (MemorySaverModeRenderTuning).
  bool available_memory_low_ = false;
  // Set while memory pressure was signaled within the hold time.
  bool memory_pressure_active_ = false;
#endif  // BUILDFLAG(IS_COBALT)

  base::RepeatingTimer sample_timer_;
  bool memory_saver_enabled_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CONTROLLER_MEMORY_SAVER_CONTROLLER_H_
