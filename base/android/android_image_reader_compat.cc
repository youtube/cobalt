// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/android_image_reader_compat.h"

#include "build/build_config.h"

namespace base {
namespace android {

bool EnableAndroidImageReader() {
#if !BUILDFLAG(IS_COBALT)
  // Currently we want to enable AImageReader only for android P+ devices.
  if (__builtin_available(android 28, *)) {
    return true;
  }
#endif
  return false;
}

}  // namespace android
}  // namespace base
