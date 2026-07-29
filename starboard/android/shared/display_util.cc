// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/android/shared/display_util.h"

#include "base/android/jni_array.h"
#include "cobalt/android/jni_headers/DisplayUtil_jni.h"
#include "starboard/android/shared/media_capabilities_cache.h"
#include "starboard/shared/starboard/media/mime_supportability_cache.h"
#include "third_party/jni_zero/jni_zero.h"

namespace starboard {
using jni_zero::ScopedJavaLocalRef;

DisplayUtil::Dpi DisplayUtil::GetDisplayDpi() {
  JNIEnv* env = jni_zero::AttachCurrentThread();

  ScopedJavaLocalRef<jobject> display_dpi_obj =
      Java_DisplayUtil_getDisplayDpi(env);

  return {Java_DisplayDpi_getX(env, display_dpi_obj),
          Java_DisplayDpi_getY(env, display_dpi_obj)};
}

// static
std::vector<int> DisplayUtil::GetSupportedHdrTypes(JNIEnv* env) {
  std::vector<int> hdr_types;
  ScopedJavaLocalRef<jintArray> j_hdr_types =
      Java_DisplayUtil_getSupportedHdrTypes(env);
  if (j_hdr_types) {
    base::android::JavaIntArrayToIntVector(env, j_hdr_types, &hdr_types);
  }
  return hdr_types;
}

void JNI_DisplayUtil_OnDisplayChanged(JNIEnv* env) {
  // Display device change could change hdr capabilities.
  MediaCapabilitiesCache::GetInstance()->ClearCache();
  MimeSupportabilityCache::GetInstance()->ClearCachedMimeSupportabilities();
}

}  // namespace starboard
