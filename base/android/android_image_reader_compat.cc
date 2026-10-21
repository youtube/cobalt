// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/android_image_reader_compat.h"

#include "build/build_config.h"
#include "build/buildflag.h"

namespace base {
namespace android {

bool EnableAndroidImageReader() {
  // Cobalt completely bypasses Chromium's native video decoding pipeline
  // (which is the sole instantiator of AImageReader/ImageReaderGLOwner)
  // and renders all media directly through the Starboard player.
  //
  // Disabling AImageReader here also safely prevents the GPU compositor
  // from attempting to initialize advanced multi-threaded scheduling
  // features like DrDc (which has a hard hardware dependency on AImageReader
  // buffer sharing), ensuring maximum rendering stability on TV platforms.
  // See b/508072838.
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
