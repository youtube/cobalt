// Copyright 2024 The Cobalt Authors. All Rights Reserved.
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

#include <jni.h>

#include "jni_state.h"

/*
  This module stores shared JNI state variables.
  It's contained in a standalone shared library, sharing the same state
  across all calling modules.
  Do not add any functionality here, besides setting and retrieving shared
  state variables.
  Also do not add any other dependencies besides <jni.h>

  Unifying Starboard/Chrome JNI will remove the need for this.
*/

namespace {
JavaVM* g_vm = NULL;
jobject g_application_class_loader = NULL;
jobject g_starboard_bridge = NULL;
}  // namespace

namespace starboard {
namespace android {
namespace shared {

void JNIState::SetVM(JavaVM* vm) {
  g_vm = vm;
}

JavaVM*& JNIState::GetVM() {
  return g_vm;
}

void JNIState::SetStarboardBridge(jobject value) {
  g_starboard_bridge = value;
}

jobject& JNIState::GetStarboardBridge() {
  return g_starboard_bridge;
}

void JNIState::SetApplicationClassLoader(jobject value) {
  g_application_class_loader = value;
}

jobject& JNIState::GetApplicationClassLoader() {
  return g_application_class_loader;
}

}  // namespace shared
}  // namespace android
}  // namespace starboard
