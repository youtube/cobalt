// Copyright 2017 Google Inc. All Rights Reserved.
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

#include <string>

#include "starboard/android/shared/jni_env_ext.h"
#include "starboard/configuration.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace android {
namespace shared {
namespace {

// UTF-16, UTF-8, and Modified UTF-8 test strings, all "𐆖€£$"
// 𐆖 U+10196 -> U16: D800 DD96  U8: F0 90 86 96  MU8: ED A0 80 ED B6 96
// € U+020AC -> U16: 20AC       U8: E2 82 AC     MU8: E2 82 AC
// £ U+000A3 -> U16: 00A3       U8: C2 A3        MU8: C2 A3
// $ U+00024 -> U16: 0024       U8: 24           MU8: 24
const char16_t kU16[] = u"\U00010196\u20AC\u00A3\u0024";
const char kU8[] = "\xF0\x90\x86\x96\xE2\x82\xAC\xC2\xA3\x24";
const char kMU8[] = "\xED\xA0\x80\xED\xB6\x96\xE2\x82\xAC\xC2\xA3\x24";

// Subtract one from the array size to not count the null terminator.
const int kU16Length = SB_ARRAY_SIZE(kU16) - 1;
const int kU8Length = SB_ARRAY_SIZE(kU8) - 1;
const int kMU8Length = SB_ARRAY_SIZE(kMU8) - 1;

// Note: there is no test for getting the string back as modified UTF-8 since
// on some Android devices GetStringUTFChars() may return standard UTF-8.
// (e.g. Nexus Player returns modified UTF-8, but Shield returns standard UTF-8)
// see: https://github.com/android-ndk/ndk/issues/283

TEST(JniEnvExtTest, NewStringStandardUTF) {
  JniEnvExt* env = JniEnvExt::Get();
  jstring j_str = env->NewStringStandardUTFOrAbort(kU8);

  EXPECT_EQ(kU16Length, env->GetStringLength(j_str));
  const jchar* u16_chars = env->GetStringChars(j_str, NULL);
  std::u16string u16_string(
      reinterpret_cast<const char16_t*>(u16_chars), kU16Length);
  EXPECT_EQ(std::u16string(kU16), u16_string);
  env->ReleaseStringChars(j_str, u16_chars);
}

TEST(JniEnvExtTest, NewStringModifiedUTF) {
  JniEnvExt* env = JniEnvExt::Get();
  jstring j_str = env->NewStringUTF(kMU8);

  EXPECT_EQ(kU16Length, env->GetStringLength(j_str));
  const jchar* u16_chars = env->GetStringChars(j_str, NULL);
  std::u16string u16_string(
      reinterpret_cast<const char16_t*>(u16_chars), kU16Length);
  EXPECT_EQ(std::u16string(kU16), u16_string);
  env->ReleaseStringChars(j_str, u16_chars);
}

TEST(JniEnvExtTest, EmptyNewStringStandardUTF) {
  JniEnvExt* env = JniEnvExt::Get();
  jstring j_str = env->NewStringStandardUTFOrAbort("");

  EXPECT_EQ(0, env->GetStringLength(j_str));
}

TEST(JniEnvExtTest, GetStringStandardUTF) {
  JniEnvExt* env = JniEnvExt::Get();
  jstring j_str =
      env->NewString(reinterpret_cast<const jchar*>(kU16), kU16Length);

  std::string str = env->GetStringStandardUTFOrAbort(j_str);
  EXPECT_EQ(kU8Length, str.length());
  EXPECT_EQ(std::string(kU8), str);
  env->DeleteLocalRef(j_str);
}

TEST(JniEnvExtTest, EmptyGetStringStandardUTF) {
  JniEnvExt* env = JniEnvExt::Get();
  jchar empty[] = {};
  jstring j_str = env->NewString(empty, 0);

  std::string str = env->GetStringStandardUTFOrAbort(j_str);
  EXPECT_EQ(0, str.length());
  EXPECT_EQ(std::string(), str);
  env->DeleteLocalRef(j_str);
}

}  // namespace
}  // namespace shared
}  // namespace android
}  // namespace starboard
