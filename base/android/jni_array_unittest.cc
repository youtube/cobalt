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

TEST(JniArray, JavaArrayOfByteArrayToStringVector) {
  const int kMaxItems = 50;
  JNIEnv* env = AttachCurrentThread();

  // Create a byte[][] object.
  ScopedJavaLocalRef<jclass> byte_array_clazz(env, env->FindClass("[B"));
  ASSERT_TRUE(byte_array_clazz.obj());

  ScopedJavaLocalRef<jobjectArray> array(
      env, env->NewObjectArray(kMaxItems, byte_array_clazz.obj(), NULL));
  ASSERT_TRUE(array.obj());

  // Create kMaxItems byte buffers.
  char text[16];
  for (int i = 0; i < kMaxItems; ++i) {
    snprintf(text, sizeof text, "%d", i);
    ScopedJavaLocalRef<jbyteArray> byte_array = ToJavaByteArray(
        env, reinterpret_cast<uint8*>(text),
        static_cast<size_t>(strlen(text)));
    ASSERT_TRUE(byte_array.obj());

    env->SetObjectArrayElement(array.obj(), i, byte_array.obj());
    ASSERT_FALSE(HasException(env));
  }

  // Convert to std::vector<std::string>, check the content.
  std::vector<std::string> vec;
  JavaArrayOfByteArrayToStringVector(env, array.obj(), &vec);

  EXPECT_EQ(static_cast<size_t>(kMaxItems), vec.size());
  for (int i = 0; i < kMaxItems; ++i) {
    snprintf(text, sizeof text, "%d", i);
    EXPECT_STREQ(text, vec[i].c_str());
  }
}

}  // namespace android
}  // namespace base
