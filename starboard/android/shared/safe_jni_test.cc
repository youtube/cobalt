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

#include "starboard/android/shared/safe_jni.h"

#include <string>
#include <vector>

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/jni_zero/jni_zero.h"

namespace starboard {
namespace {

using base::android::ConvertJavaStringToUTF8;
using base::android::ToJavaArrayOfStrings;
using jni_zero::AttachCurrentThread;
using jni_zero::ScopedJavaLocalRef;

TEST(SafeJniTest, JavaObjectArrayToVector_ValidArray) {
  JNIEnv* env = AttachCurrentThread();
  ASSERT_NE(env, nullptr);

  std::vector<std::string> input_strings = {"apple", "banana", "cherry"};
  ScopedJavaLocalRef<jobjectArray> j_array =
      ToJavaArrayOfStrings(env, input_strings);
  ASSERT_FALSE(j_array.is_null());

  std::vector<ScopedJavaLocalRef<jobject>> result =
      JavaObjectArrayToVector(env, j_array);

  EXPECT_EQ(result.size(), input_strings.size());

  for (size_t i = 0; i < input_strings.size(); ++i) {
    ASSERT_FALSE(result[i].is_null());
    std::string result_str =
        ConvertJavaStringToUTF8(env, static_cast<jstring>(result[i].obj()));
    EXPECT_EQ(result_str, input_strings[i]);
  }
}

TEST(SafeJniTest, JavaObjectArrayToVector_NullArray) {
  JNIEnv* env = AttachCurrentThread();
  ASSERT_NE(env, nullptr);

  ScopedJavaLocalRef<jobjectArray> j_array;  // Null ref
  std::vector<ScopedJavaLocalRef<jobject>> result =
      JavaObjectArrayToVector(env, j_array);

  EXPECT_TRUE(result.empty());
}

TEST(SafeJniTest, JavaObjectArrayToVector_EmptyArray) {
  JNIEnv* env = AttachCurrentThread();
  ASSERT_NE(env, nullptr);

  std::vector<std::string> input_strings = {};
  ScopedJavaLocalRef<jobjectArray> j_array =
      ToJavaArrayOfStrings(env, input_strings);
  ASSERT_FALSE(j_array.is_null());

  std::vector<ScopedJavaLocalRef<jobject>> result =
      JavaObjectArrayToVector(env, j_array);

  EXPECT_TRUE(result.empty());
}

TEST(SafeJniTest, JavaByteBufferToSpan_ValidBuffer) {
  JNIEnv* env = AttachCurrentThread();
  ASSERT_NE(env, nullptr);

  uint8_t c_buffer[16] = {0};
  ScopedJavaLocalRef<jobject> j_buffer(
      env, env->NewDirectByteBuffer(c_buffer, sizeof(c_buffer)));
  ASSERT_FALSE(j_buffer.is_null());

  Span<uint8_t> result = JavaByteBufferToSpan(env, j_buffer);

  EXPECT_EQ(result.data(), c_buffer);
  EXPECT_EQ(result.size(), sizeof(c_buffer));
  EXPECT_FALSE(result.empty());
}

TEST(SafeJniTest, JavaByteBufferToSpan_NullBuffer) {
  JNIEnv* env = AttachCurrentThread();
  ASSERT_NE(env, nullptr);

  ScopedJavaLocalRef<jobject> j_buffer;  // Null
  Span<uint8_t> result = JavaByteBufferToSpan(env, j_buffer);

  EXPECT_EQ(result.data(), nullptr);
  EXPECT_EQ(result.size(), 0U);
  EXPECT_TRUE(result.empty());
}

TEST(SafeJniTest, JavaByteBufferToSpan_NonDirectBuffer) {
  JNIEnv* env = AttachCurrentThread();
  ASSERT_NE(env, nullptr);

  // Create a regular Java byte array, which is NOT a direct ByteBuffer
  ScopedJavaLocalRef<jbyteArray> j_array(env, env->NewByteArray(10));
  ASSERT_FALSE(j_array.is_null());

  // Pass the byte array (cast to jobject) to the helper
  Span<uint8_t> result = JavaByteBufferToSpan(env, j_array);

  EXPECT_EQ(result.data(), nullptr);
  EXPECT_EQ(result.size(), 0U);
  EXPECT_TRUE(result.empty());
}

}  // namespace
}  // namespace starboard
