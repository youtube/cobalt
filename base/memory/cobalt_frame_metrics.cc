// Copyright 2026 The Chromium Authors and Cobalt Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/cobalt_frame_metrics.h"

#include <atomic>

namespace base {
namespace cobalt {

namespace {
std::atomic<double> g_last_cpu_frame_prep_time_ms{0.0};
std::atomic<double> g_last_paint_damage_percentage{0.0};
}  // namespace

void SetLastCpuFramePrepTimeMs(double ms) {
  g_last_cpu_frame_prep_time_ms.store(ms, std::memory_order_relaxed);
}

double GetLastCpuFramePrepTimeMs() {
  return g_last_cpu_frame_prep_time_ms.load(std::memory_order_relaxed);
}

void SetLastPaintDamagePercentage(double pct) {
  g_last_paint_damage_percentage.store(pct, std::memory_order_relaxed);
}

double GetLastPaintDamagePercentage() {
  return g_last_paint_damage_percentage.load(std::memory_order_relaxed);
}

}  // namespace cobalt
}  // namespace base
