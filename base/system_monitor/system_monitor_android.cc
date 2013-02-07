// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/system_monitor/system_monitor.h"
#include "jni/SystemMonitor_jni.h"

namespace base {

namespace android {

// Native implementation of SystemMonitor.java.
void OnBatteryChargingChanged(JNIEnv* env, jclass clazz) {
  SystemMonitor::Get()->ProcessPowerMessage(SystemMonitor::POWER_STATE_EVENT);
}

void OnMainActivityResumed(JNIEnv* env, jclass clazz) {
  SystemMonitor::Get()->ProcessPowerMessage(SystemMonitor::RESUME_EVENT);
}

void OnMainActivitySuspended(JNIEnv* env, jclass clazz) {
  SystemMonitor::Get()->ProcessPowerMessage(SystemMonitor::SUSPEND_EVENT);
}

}  // namespace android

bool SystemMonitor::IsBatteryPower() {
  JNIEnv* env = base::android::AttachCurrentThread();
  return base::android::Java_SystemMonitor_isBatteryPower(env);
}

bool RegisterSystemMonitor(JNIEnv* env) {
  return base::android::RegisterNativesImpl(env);
}

}  // namespace base
