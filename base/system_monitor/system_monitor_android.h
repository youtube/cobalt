// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_SYSTEM_MONITOR_SYSTEM_MONITOR_ANDROID_H_
#define BASE_SYSTEM_MONITOR_SYSTEM_MONITOR_ANDROID_H_

#include <jni.h>

namespace base {

// Registers the JNI bindings for SystemMonitor.
bool RegisterSystemMonitor(JNIEnv* env);

}  // namespace base

#endif  // BASE_SYSTEM_MONITOR_SYSTEM_MONITOR_ANDROID_H_
