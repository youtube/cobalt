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

#include <jni.h>

#include "base/android/feature_map.h"
#include "base/feature_list.h"
#include "base/features.h"
#include "base/no_destructor.h"

// Must come after headers that provide symbols used by @JniType.
#include "cobalt/android/cobalt_feature_map_jni/CobaltFeatureMap_jni.h"

namespace cobalt::features {

namespace {

// Array of features exposed through the Java CobaltFeatureMap API, e.g., for
// the CachedFeatureParams in CobaltFeatureList.java.
const base::Feature* const kFeaturesExposedToJava[] = {
    &base::features::kLowEndMemoryExperiment,
};

base::android::FeatureMap* GetFeatureMap() {
  static base::NoDestructor<base::android::FeatureMap> kFeatureMap(
      kFeaturesExposedToJava);
  return kFeatureMap.get();
}

}  // namespace

static jlong JNI_CobaltFeatureMap_GetNativeMap(JNIEnv* env) {
  return reinterpret_cast<jlong>(GetFeatureMap());
}

}  // namespace cobalt::features
