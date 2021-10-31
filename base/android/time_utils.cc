// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/time/time.h"
#include "jni/TimeUtils_jni.h"
#include "starboard/types.h"

namespace base {
namespace android {

static jlong JNI_TimeUtils_GetTimeTicksNowUs(
    JNIEnv* env,
    const JavaParamRef<jclass>& clazz) {
  return (TimeTicks::Now() - TimeTicks()).InMicroseconds();
}

}  // namespace android
}  // namespace base
