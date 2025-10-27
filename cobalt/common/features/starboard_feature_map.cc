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

#include "base/android/feature_map.h"
#include "base/no_destructor.h"
#include "cobalt/android/jni_headers/StarboardFeatureMap_jni.h"
#include "cobalt/common/features/features.h"

namespace cobalt::features {
namespace {
// Array of features exposed through the Java StarboardFeatureMap API. Entries
// in this array refer to features defined in starboard/common/feature_config.h.
const base::Feature* const kFeaturesExposedToJava[] = {
    &features::kNonTunneledDecodeOnly,
};
// static
base::android::FeatureMap* GetFeatureMap() {
  static base::NoDestructor<base::android::FeatureMap> kFeatureMap(
      kFeaturesExposedToJava);
  return kFeatureMap.get();
}
}  // namespace

static jlong JNI_StarboardFeatureMap_GetNativeMap(JNIEnv* env) {
  return reinterpret_cast<jlong>(GetFeatureMap());
}
}  // namespace cobalt::features
