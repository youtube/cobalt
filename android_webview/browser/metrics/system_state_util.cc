// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/metrics/system_state_util.h"

#include "android_webview/browser_jni_headers/SystemStateUtil_jni.h"

namespace android_webview {

MultipleUserProfilesState GetMultipleUserProfilesState() {
  static MultipleUserProfilesState multiple_user_profiles_state =
      static_cast<MultipleUserProfilesState>(
          Java_SystemStateUtil_getMultipleUserProfilesState(
              base::android::AttachCurrentThread()));
  return multiple_user_profiles_state;
}

PrimaryCpuAbiBitness GetPrimaryCpuAbiBitness() {
  static PrimaryCpuAbiBitness primary_cpu_abi_bitness =
      static_cast<PrimaryCpuAbiBitness>(
          Java_SystemStateUtil_getPrimaryCpuAbiBitness(
              base::android::AttachCurrentThread()));
  return primary_cpu_abi_bitness;
}

}  // namespace android_webview
