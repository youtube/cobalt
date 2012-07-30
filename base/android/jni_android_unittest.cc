// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_android.h"

#include "base/at_exit.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace android {

namespace {

const char kJavaLangObject[] = "java/lang/Object";
const char kGetClass[] = "getClass";
const char kToString[] = "toString";
const char kReturningJavaLangClass[] = "()Ljava/lang/Class;";
const char kReturningJavaLangString[] = "()Ljava/lang/String;";

const char* g_last_method;
const char* g_last_jni_signature;
jmethodID g_last_method_id;

const JNINativeInterface* g_previous_functions;

jmethodID GetMethodIDWrapper(JNIEnv* env, jclass clazz, const char* method,
                             const char* jni_signature) {
  g_last_method = method;
  g_last_jni_signature = jni_signature;
  g_last_method_id = g_previous_functions->GetMethodID(env, clazz, method,
                                                       jni_signature);
  return g_last_method_id;
}

}  // namespace

class JNIAndroidTest : public testing::Test {
 protected:
  virtual void SetUp() {
    JNIEnv* env = AttachCurrentThread();
    g_previous_functions = env->functions;
    hooked_functions = *g_previous_functions;
    env->functions = &hooked_functions;
    hooked_functions.GetMethodID = &GetMethodIDWrapper;
  }

  virtual void TearDown() {
    JNIEnv* env = AttachCurrentThread();
    env->functions = g_previous_functions;
    Reset();
  }

  void Reset() {
    g_last_method = 0;
    g_last_jni_signature = 0;
    g_last_method_id = NULL;
  }
  // Needed to cleanup the cached method map in the implementation between
  // runs (e.g. if using --gtest_repeat)
  base::ShadowingAtExitManager exit_manager;
  // From JellyBean release, the instance of this struct provided in JNIEnv is
  // read-only, so we deep copy it to allow individual functions to be hooked.
  JNINativeInterface hooked_functions;
};

TEST_F(JNIAndroidTest, GetMethodIDFromClassNameCaching) {
  JNIEnv* env = AttachCurrentThread();

  Reset();
  jmethodID id1 = GetMethodIDFromClassName(env, kJavaLangObject, kGetClass,
                                           kReturningJavaLangClass);
  EXPECT_STREQ(kGetClass, g_last_method);
  EXPECT_STREQ(kReturningJavaLangClass, g_last_jni_signature);
  EXPECT_EQ(g_last_method_id, id1);

  Reset();
  jmethodID id2 = GetMethodIDFromClassName(env, kJavaLangObject, kGetClass,
                                           kReturningJavaLangClass);
  EXPECT_STREQ(0, g_last_method);
  EXPECT_STREQ(0, g_last_jni_signature);
  EXPECT_EQ(NULL, g_last_method_id);
  EXPECT_EQ(id1, id2);

  Reset();
  jmethodID id3 = GetMethodIDFromClassName(env, kJavaLangObject, kToString,
                                           kReturningJavaLangString);
  EXPECT_STREQ(kToString, g_last_method);
  EXPECT_STREQ(kReturningJavaLangString, g_last_jni_signature);
  EXPECT_EQ(g_last_method_id, id3);
}

}  // namespace android
}  // namespace base
