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

#include "base/android/jni_string.h"
#include "cobalt/browser/cobalt_crash_annotations.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "cobalt/android/jni_headers/CrashAnnotatorImplFirstParty_jni.h"

void JNI_CrashAnnotatorImplFirstParty_SetAnnotation(
    JNIEnv* env,
    const base::android::JavaParamRef<jstring>& j_name,
    const base::android::JavaParamRef<jstring>& j_value) {
  std::string name = base::android::ConvertJavaStringToUTF8(env, j_name);
  std::string value = base::android::ConvertJavaStringToUTF8(env, j_value);
  cobalt::browser::CobaltCrashAnnotations::GetInstance()->SetAnnotation(name,
                                                                        value);
}
