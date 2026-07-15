// Copyright 2026 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <string>

#include "base/android/jni_string.h"
#include "base/time/time.h"
#include "cobalt/android/browser_jni_headers/AppEventBridge_jni.h"
#include "starboard/event.h"

// Forward declaration of the platform lifecycle event handler.
void SbEventHandle(const SbEvent* event);

void JNI_AppEventBridge_HandleLifecycleEvent(JNIEnv* env,
                                             jint type,
                                             jlong timestamp) {
  SbEvent event;
  event.type = static_cast<SbEventType>(type);
  event.timestamp = timestamp;
  event.data = nullptr;

  SbEventHandle(&event);
}

void JNI_AppEventBridge_HandleStartEvent(
    JNIEnv* env,
    const base::android::JavaParamRef<jstring>& jlink,
    jlong timestamp) {
  SbEvent event;
  event.type = kSbEventTypeStart;
  event.timestamp = timestamp;

  std::string link_str;
  SbEventStartData start_data = {};
  if (jlink) {
    link_str = base::android::ConvertJavaStringToUTF8(env, jlink);
    if (!link_str.empty()) {
      start_data.link = link_str.c_str();
    }
  }
  event.data = &start_data;

  SbEventHandle(&event);
}

void JNI_AppEventBridge_HandleOsNetworkEvent(JNIEnv* env, jboolean online) {
  SbEvent event;
  event.type = online ? kSbEventTypeOsNetworkConnected
                      : kSbEventTypeOsNetworkDisconnected;
  event.timestamp = base::Time::Now().ToTimeT() * 1000000;
  event.data = nullptr;

  SbEventHandle(&event);
}
