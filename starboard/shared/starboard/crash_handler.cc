// Copyright 2021 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/shared/starboard/crash_handler.h"

#include <pthread.h>

#include "starboard/common/check_op.h"
#include "starboard/common/log.h"
#include "starboard/crashpad_wrapper/wrapper.h"
#include "starboard/extension/crash_handler.h"

namespace starboard {

namespace {

SetStringCallback g_set_string_callback = nullptr;
pthread_mutex_t g_set_string_callback_mutex = PTHREAD_MUTEX_INITIALIZER;

bool OverrideCrashpadAnnotations(CrashpadAnnotations* crashpad_annotations) {
  return false;  // Deprecated
}

bool SetString(const char* key, const char* value) {
  bool result = false;

  SB_CHECK_EQ(pthread_mutex_lock(&g_set_string_callback_mutex), 0);
  if (g_set_string_callback) {
    result = g_set_string_callback(key, value);
  }
  SB_CHECK_EQ(pthread_mutex_unlock(&g_set_string_callback_mutex), 0);

  return result;
}

void RegisterSetStringCallback(SetStringCallback callback) {
  SB_CHECK_EQ(pthread_mutex_lock(&g_set_string_callback_mutex), 0);
  g_set_string_callback = callback;
  SB_CHECK_EQ(pthread_mutex_unlock(&g_set_string_callback_mutex), 0);
}

const CobaltExtensionCrashHandlerApi kCrashHandlerApi = {
    kCobaltExtensionCrashHandlerName, 3,
    &OverrideCrashpadAnnotations,     &SetString,
    &RegisterSetStringCallback,
};

}  // namespace

const void* GetCrashHandlerApi() {
  return &kCrashHandlerApi;
}

}  // namespace starboard
