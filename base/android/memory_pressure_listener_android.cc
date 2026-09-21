// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/memory_pressure_listener_android.h"

#include "base/android/pre_freeze_background_memory_trimmer.h"
#include "base/feature_list.h"
#include "base/features.h"
#include "base/logging.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/no_destructor.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "base/memory_jni/MemoryPressureListener_jni.h"

using base::android::JavaParamRef;

namespace {
base::android::MemoryPressureListenerAndroid::MemoryPressureForwarderCallback&
GetForwarderCallback() {
  static base::NoDestructor<
      base::android::MemoryPressureListenerAndroid::MemoryPressureForwarderCallback>
      forwarder_callback;
  return *forwarder_callback;
}
}  // namespace

// Defined and called by JNI.
static void JNI_MemoryPressureListener_OnMemoryPressure(
    JNIEnv* env,
    jint memory_pressure_level) {
  auto level = static_cast<base::MemoryPressureListener::MemoryPressureLevel>(
      memory_pressure_level);
  auto& forwarder = GetForwarderCallback();
  if (forwarder) {
    LOG(INFO) << "[CobaltMemoryPressure] JNI received level=" << level
              << ", forwarding to AndroidOsSignalEvaluator";
    forwarder.Run(level);
    return;
  }
  LOG(INFO) << "[CobaltMemoryPressure] JNI received level=" << level
            << ", no forwarder registered, calling NotifyMemoryPressure directly";
  base::MemoryPressureListener::NotifyMemoryPressure(level);
}

static void JNI_MemoryPressureListener_OnPreFreeze(JNIEnv* env) {
  base::android::PreFreezeBackgroundMemoryTrimmer::OnPreFreeze();
}

static jboolean JNI_MemoryPressureListener_IsTrimMemoryBackgroundCritical(
    JNIEnv* env) {
  return base::android::PreFreezeBackgroundMemoryTrimmer::
      IsTrimMemoryBackgroundCritical();
}

static jboolean JNI_MemoryPressureListener_IsModerateMemoryPressureEnabled(
    JNIEnv* env) {
#if BUILDFLAG(IS_COBALT)
  return base::FeatureList::IsEnabled(
      base::features::kCobaltEnableModerateMemoryPressure);
#else
  return false;
#endif
}

static jint JNI_MemoryPressureListener_GetMemoryPressureCooldownSeconds(
    JNIEnv* env) {
#if BUILDFLAG(IS_COBALT)
  // When AndroidOsSignalEvaluator is registered, MultiSourceMemoryPressureMonitor
  // in C++ enforces the unified cooldown and immediate escalation override.
  // Return 0 so Java passes signals directly through to C++.
  if (GetForwarderCallback()) {
    return 0;
  }
  if (!base::FeatureList::IsEnabled(
          base::features::kCobaltMemoryPressureCooldown)) {
    return 60;
  }
  return base::features::kCobaltMemoryPressureCooldownSeconds.Get();
#else
  return 60;
#endif
}

namespace base::android {

void MemoryPressureListenerAndroid::SetMemoryPressureForwarderCallback(
    MemoryPressureForwarderCallback callback) {
  GetForwarderCallback() = std::move(callback);
}

void MemoryPressureListenerAndroid::Initialize(JNIEnv* env) {
  Java_MemoryPressureListener_addNativeCallback(env);
}

}  // namespace base::android
