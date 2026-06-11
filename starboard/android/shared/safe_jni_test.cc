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

}  // namespace
}  // namespace starboard
