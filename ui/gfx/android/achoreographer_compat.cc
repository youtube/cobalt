// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/android/achoreographer_compat.h"

#include <dlfcn.h>

#include "base/android/android_info.h"
#include "base/logging.h"

#define LOAD_FUNCTION(obj, lib, func)                              \
  do {                                                             \
    (obj).func##Fn = reinterpret_cast<p##func>(dlsym(lib, #func)); \
    if (!(obj).func##Fn) {                                         \
      (obj).supported = false;                                     \
      LOG(ERROR) << "Unable to load function " << #func;           \
    }                                                              \
  } while (0)

namespace gfx {

namespace {

const AChoreographerCompat* g_test_instance = nullptr;
const AChoreographerCompat33* g_test_instance_33 = nullptr;

}  // namespace

// static
const AChoreographerCompat& AChoreographerCompat::Get() {
  if (g_test_instance) {
    return *g_test_instance;
  }
  static const AChoreographerCompat instance = []() {
    AChoreographerCompat impl;
    impl.supported = true;
    void* main_dl_handle = dlopen("libandroid.so", RTLD_NOW);
    if (!main_dl_handle) {
      LOG(ERROR) << "Couldn't load libandroid.so";
      impl.supported = false;
      return impl;
    }

    LOAD_FUNCTION(impl, main_dl_handle, AChoreographer_getInstance);
    LOAD_FUNCTION(impl, main_dl_handle, AChoreographer_postFrameCallback64);
    LOAD_FUNCTION(impl, main_dl_handle,
                  AChoreographer_registerRefreshRateCallback);
    LOAD_FUNCTION(impl, main_dl_handle,
                  AChoreographer_unregisterRefreshRateCallback);
    return impl;
  }();
  return instance;
}

// static
void AChoreographerCompat::SetForTesting(  // IN-TEST
    const AChoreographerCompat* test_instance) {
  g_test_instance = test_instance;
}

AChoreographerCompat::AChoreographerCompat() = default;

// static
const AChoreographerCompat33& AChoreographerCompat33::Get() {
  if (g_test_instance_33) {
    return *g_test_instance_33;
  }
  static const AChoreographerCompat33 instance = []() {
    AChoreographerCompat33 impl;
    if (base::android::android_info::sdk_int() <
        base::android::android_info::SDK_VERSION_T) {
      impl.supported = false;
      return impl;
    }

    impl.supported = true;
    void* main_dl_handle = dlopen("libandroid.so", RTLD_NOW);
    if (!main_dl_handle) {
      LOG(ERROR) << "Couldn't load libandroid.so";
      impl.supported = false;
      return impl;
    }

    LOAD_FUNCTION(impl, main_dl_handle, AChoreographer_postVsyncCallback);
    LOAD_FUNCTION(impl, main_dl_handle,
                  AChoreographerFrameCallbackData_getFrameTimeNanos);
    LOAD_FUNCTION(impl, main_dl_handle,
                  AChoreographerFrameCallbackData_getFrameTimelinesLength);
    LOAD_FUNCTION(
        impl, main_dl_handle,
        AChoreographerFrameCallbackData_getPreferredFrameTimelineIndex);
    LOAD_FUNCTION(impl, main_dl_handle,
                  AChoreographerFrameCallbackData_getFrameTimelineVsyncId);
    LOAD_FUNCTION(
        impl, main_dl_handle,
        AChoreographerFrameCallbackData_getFrameTimelineExpectedPresentationTimeNanos);
    LOAD_FUNCTION(
        impl, main_dl_handle,
        AChoreographerFrameCallbackData_getFrameTimelineDeadlineNanos);
    return impl;
  }();
  return instance;
}

// static
void AChoreographerCompat33::SetForTesting(  // IN-TEST
    const AChoreographerCompat33* test_instance) {
  g_test_instance_33 = test_instance;
}

AChoreographerCompat33::AChoreographerCompat33() = default;

}  // namespace gfx
