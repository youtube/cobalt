// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/test_view_android_delegate.h"

#include "base/android/scoped_java_ref.h"
#include "content/test/content_unittests_jni_headers/TestViewAndroidDelegate_jni.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/android/view_android.h"
#include "ui/gfx/geometry/rect.h"

namespace content {

TestViewAndroidDelegate::TestViewAndroidDelegate() {}
TestViewAndroidDelegate::~TestViewAndroidDelegate() {}

void TestViewAndroidDelegate::SetupTestDelegate(ui::ViewAndroid* view_android) {
  JNIEnv* env = base::android::AttachCurrentThread();
  auto test_delegate = Java_TestViewAndroidDelegate_create(env);
  view_android->SetDelegate(test_delegate);
  j_delegate_.Reset(test_delegate);
}

void TestViewAndroidDelegate::InsetViewportBottom(int bottom) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_TestViewAndroidDelegate_insetViewportBottom(env, j_delegate_, bottom);
}

void TestViewAndroidDelegate::SetDisplayFeatureForTesting(
    const gfx::Rect& display_feature) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_TestViewAndroidDelegate_setDisplayFeature(
      env, j_delegate_, display_feature.x(), display_feature.y(),
      display_feature.right(), display_feature.bottom());
}

}  // namespace content
