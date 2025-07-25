// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/feature_map.h"
#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "ui/android/ui_android_features.h"
#include "ui/android/ui_android_jni_headers/UiAndroidFeatureMap_jni.h"

namespace ui {

namespace {

// Array of features exposed through the Java UiAndroidFeatureMap API.
const base::Feature* const kFeaturesExposedToJava[] = {
    &ui::kConvertTrackpadEventsToMouse,
    &ui::kDeprecatedExternalPickerFunction,
    &ui::kPwaRestoreUi,
    &ui::kReportAllAvailablePointerTypes,
};

// static
base::android::FeatureMap* GetFeatureMap() {
  static base::NoDestructor<base::android::FeatureMap> kFeatureMap(std::vector(
      std::begin(kFeaturesExposedToJava), std::end(kFeaturesExposedToJava)));
  return kFeatureMap.get();
}

}  // namespace

static jlong JNI_UiAndroidFeatureMap_GetNativeMap(JNIEnv* env) {
  return reinterpret_cast<jlong>(GetFeatureMap());
}

}  // namespace ui
