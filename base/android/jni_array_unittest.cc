// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_array.h"

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace android {

TEST(JniArray, BasicConversions) {
  const uint8 kBytes[] = { 0, 1, 2, 3 };
  const size_t kLen = arraysize(kBytes);
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jbyteArray> bytes = ToJavaByteArray(env, kBytes, kLen);
  ASSERT_TRUE(bytes.obj());

  std::vector<uint8> vec(5);
  JavaByteArrayToByteVector(env, bytes.obj(), &vec);
  EXPECT_EQ(4U, vec.size());
  EXPECT_EQ(std::vector<uint8>(kBytes, kBytes + kLen), vec);

  AppendJavaByteArrayToByteVector(env, bytes.obj(), &vec);
  EXPECT_EQ(8U, vec.size());
}

}  // namespace android
}  // namespace base
