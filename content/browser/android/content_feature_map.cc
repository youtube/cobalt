// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/feature_map.h"
#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "content/common/features.h"
#include "content/public/android/content_jni_headers/ContentFeatureMap_jni.h"
#include "content/public/common/content_features.h"
#include "third_party/blink/public/common/features.h"
#include "ui/accessibility/accessibility_features.h"

namespace content::android {

namespace {

// Array of features exposed through the Java ContentFeatureMap API. Entries in
// this array may either refer to features defined in the header of this file or
// in other locations in the code base (e.g. content_features.h).
const base::Feature* const kFeaturesExposedToJava[] = {
    &blink::features::kStylusPointerAdjustment,
    &blink::features::kStylusRichGestures,
    &features::kAccessibilityPageZoom,
    &features::kAccessibilityPerformanceFiltering,
    &features::kAccessibilityPerformanceTesting,
    &features::kAutoDisableAccessibilityV2,
    &features::kBackgroundMediaRendererHasModerateBinding,
    &features::kFedCm,
    &features::kMouseAndTrackpadDropdownMenu,
    &features::kOptimizeImmHideCalls,
    &features::kProcessSharingWithStrictSiteInstances,
    &features::kReduceGpuPriorityOnBackground,
    &features::kRequestDesktopSiteAdditions,
    &features::kRequestDesktopSiteWindowSetting,
    &features::kSmartZoom,
    &features::kTouchDragAndContextMenu,
    &features::kWebBluetoothNewPermissionsBackend,
    &features::kWebNfc,
};

// static
base::android::FeatureMap* GetFeatureMap() {
  static base::NoDestructor<base::android::FeatureMap> kFeatureMap(std::vector(
      std::begin(kFeaturesExposedToJava), std::end(kFeaturesExposedToJava)));
  return kFeatureMap.get();
}

}  // namespace

static jlong JNI_ContentFeatureMap_GetNativeMap(JNIEnv* env) {
  return reinterpret_cast<jlong>(GetFeatureMap());
}

}  // namespace content::android
