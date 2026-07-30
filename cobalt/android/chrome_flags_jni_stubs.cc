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

// Native side of //chrome/browser/flags:jni_headers for Cobalt. The real
// implementations in //chrome/browser/flags:flags_android{,_impl} pull in
// //chrome/browser, and Cobalt never calls into them. The Java classes only
// reach Cobalt's Java graph through //components/thin_webview/internal ->
// //components/browser_ui/modaldialog/android -> //components/browser_ui/widget
// (https://crrev.com/c/7763465), and Cobalt only uses CompositorViewFactory
// from thin_webview.

#include <jni.h>

#include <cstdint>

#include "base/notreached.h"
#include "chrome/browser/flags/jni_headers/ChromeFeatureMap_jni.h"
#include "chrome/browser/flags/jni_headers/ChromeSessionState_jni.h"

static int64_t JNI_ChromeFeatureMap_GetNativeMap(JNIEnv* env) {
  NOTREACHED();
}

static void JNI_ChromeSessionState_SetActivityType(JNIEnv* env, int32_t type) {
  NOTREACHED();
}

static void JNI_ChromeSessionState_SetDarkModeState(JNIEnv* env,
                                                    int32_t state) {
  NOTREACHED();
}

static void JNI_ChromeSessionState_SetIsInMultiWindowMode(
    JNIEnv* env,
    bool j_is_in_multi_window_mode) {
  NOTREACHED();
}

DEFINE_JNI(ChromeFeatureMap)
DEFINE_JNI(ChromeSessionState)
