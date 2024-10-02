// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_string.h"
#include "components/page_info/android/jni_headers/PageInfoFeatures_jni.h"
#include "components/page_info/core/features.h"

namespace page_info {

namespace {

// Array of features exposed through the Java Features brdige class. Entries in
// this array may either refer to features defined in the header of this file or
// in other locations in the code base (e.g. content_features.h), and must be
// replicated in the same order in PageInfoFeatures.java.
const base::Feature* kFeaturesExposedToJava[] = {
    &kPageInfoStoreInfo,
};

}  // namespace

static jlong JNI_PageInfoFeatures_GetFeature(JNIEnv* env, jint ordinal) {
  return reinterpret_cast<jlong>(kFeaturesExposedToJava[ordinal]);
}

}  // namespace page_info
