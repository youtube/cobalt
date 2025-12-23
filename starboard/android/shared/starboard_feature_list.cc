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

#include "base/android/jni_string.h"
#include "base/notreached.h"
#include "starboard/shared/starboard/feature_list.h"
#include "starboard/shared/starboard/features.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "cobalt/android/jni_headers/StarboardFeatureList_jni.h"

using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

namespace starboard::features {
namespace {

// Array of features exposed through the Java StarboardFeatureList API. Entries
// in this array refer to features defined in
// starboard/extension/feature_config.h.
const SbFeature* const kFeaturesExposedToJava[] = {
    &kNonTunneledDecodeOnly,
};

const SbFeature* FindFeatureExposedToJava(const std::string& feature_name) {
  for (const SbFeature* feature : kFeaturesExposedToJava) {
    if (feature->name == feature_name) {
      return feature;
    }
  }
  NOTREACHED() << "Queried feature cannot be found in StarboardFeatureList: "
               << feature_name;
  return nullptr;
}
}  // namespace

static jboolean JNI_StarboardFeatureList_IsEnabled(
    JNIEnv* env,
    const JavaParamRef<jstring>& jfeature_name) {
  const SbFeature* feature =
      FindFeatureExposedToJava(ConvertJavaStringToUTF8(env, jfeature_name));
  return FeatureList::IsEnabled(*feature);
}
}  // namespace starboard::features
