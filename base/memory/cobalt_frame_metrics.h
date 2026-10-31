// Copyright 2026 The Chromium Authors and Cobalt Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MEMORY_COBALT_FRAME_METRICS_H_
#define BASE_MEMORY_COBALT_FRAME_METRICS_H_

#include "base/base_export.h"

namespace base {
namespace cobalt {

// Sets and gets the active CPU frame preparation duration in milliseconds
// (time from previous frame swap end to current frame swap start).
BASE_EXPORT void SetLastCpuFramePrepTimeMs(double ms);
BASE_EXPORT double GetLastCpuFramePrepTimeMs();

}  // namespace cobalt
}  // namespace base

#endif  // BASE_MEMORY_COBALT_FRAME_METRICS_H_
