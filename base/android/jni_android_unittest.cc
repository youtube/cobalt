// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_android.h"

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

JNINativeInterface g_previous_functions = {0};

jmethodID GetMethodIDWrapper(JNIEnv* env, jclass clazz, const char* method,
                             const char* jni_signature) {
  g_last_method = method;
  g_last_jni_signature = jni_signature;
  g_last_method_id = g_previous_functions.GetMethodID(env, clazz, method,
                                                      jni_signature);
  return g_last_method_id;
}

}  // namespace

class JNIAndroidTest : public testing::Test {
 protected:
  virtual void SetUp() {
    JNIEnv* env = AttachCurrentThread();
    g_previous_functions = *env->functions;
    JNINativeInterface* native_interface =
        const_cast<JNINativeInterface*>(env->functions);
    native_interface->GetMethodID = &GetMethodIDWrapper;
  }

  virtual void TearDown() {
    JNIEnv* env = AttachCurrentThread();
    *(const_cast<JNINativeInterface*>(env->functions)) = g_previous_functions;
  }

  void Reset() {
    g_last_method = 0;
    g_last_jni_signature = 0;
    g_last_method_id = NULL;
  }
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
