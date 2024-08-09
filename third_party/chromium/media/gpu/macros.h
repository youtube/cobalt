// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_CHROMIUM_MEDIA_GPU_MACROS_H_
#define THIRD_PARTY_CHROMIUM_MEDIA_GPU_MACROS_H_

#include "base/logging.h"

#define DVLOGF(level) DVLOG(level) << __func__ << "(): "
#define VLOGF(level) VLOG(level) << __func__ << "(): "
#define VPLOGF(level) VPLOG(level) << __func__ << "(): "

namespace media_m96 {

// Copy the memory between arrays with checking the array size.
template <typename T, size_t N>
inline void SafeArrayMemcpy(T (&to)[N], const T (&from)[N]) {
  memcpy(to, from, sizeof(T[N]));
}

}  // namespace media_m96

#endif  // THIRD_PARTY_CHROMIUM_MEDIA_GPU_MACROS_H_
