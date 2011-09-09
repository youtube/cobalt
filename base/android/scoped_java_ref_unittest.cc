// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/scoped_java_ref.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace android {

namespace {
int g_local_refs = 0;
int g_global_refs = 0;

jobject NewGlobalRef(JNIEnv* env, jobject obj) {
  ++g_global_refs;
  return AttachCurrentThread()->NewGlobalRef(obj);
}

void DeleteGlobalRef(JNIEnv* env, jobject obj) {
  --g_global_refs;
  return AttachCurrentThread()->DeleteGlobalRef(obj);
}

jobject NewLocalRef(JNIEnv* env, jobject obj) {
  ++g_local_refs;
  return AttachCurrentThread()->NewLocalRef(obj);
}

void DeleteLocalRef(JNIEnv* env, jobject obj) {
  --g_local_refs;
  return AttachCurrentThread()->DeleteLocalRef(obj);
}
}  // namespace

class ScopedJavaRefTest : public testing::Test {
 protected:
  virtual void SetUp() {
    g_local_refs = 0;
    g_global_refs = 0;
    JNIEnv* env = AttachCurrentThread();
    counting_env = *env;
    counting_functions = *counting_env.functions;
    counting_functions.NewGlobalRef = &NewGlobalRef;
    counting_functions.DeleteGlobalRef = &DeleteGlobalRef;
    counting_functions.NewLocalRef = &NewLocalRef;
    counting_functions.DeleteLocalRef = &DeleteLocalRef;
    counting_env.functions = &counting_functions;
  }

  // Special JNI env configured in SetUp to count in and out all local & global
  // reference instances. Be careful to only use this with the ScopedJavaRef
  // classes under test, else it's easy to get system references counted in
  // here too.
  JNIEnv counting_env;
  JNINativeInterface counting_functions;
};

// The main purpose of this is testing the various conversions compile.
TEST_F(ScopedJavaRefTest, Conversions) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jstring> str(env, ConvertUTF8ToJavaString(env, "string"));
  ScopedJavaGlobalRef<jstring> global(str);
  {
    ScopedJavaGlobalRef<jobject> global_obj(str);
    ScopedJavaLocalRef<jobject> local_obj(global);
    const JavaRef<jobject>& obj_ref1(str);
    const JavaRef<jobject>& obj_ref2(global);
    EXPECT_TRUE(env->IsSameObject(obj_ref1.obj(), obj_ref2.obj()));
    EXPECT_TRUE(env->IsSameObject(global_obj.obj(), obj_ref2.obj()));
  }
  global.Reset(str);
  const JavaRef<jstring>& str_ref = str;
  EXPECT_EQ("string", ConvertJavaStringToUTF8(env, str_ref.obj()));
  str.Reset();
}

TEST_F(ScopedJavaRefTest, RefCounts) {
  ScopedJavaLocalRef<jstring> str;
  str.Reset(&counting_env, ConvertUTF8ToJavaString(AttachCurrentThread(),
                                                   "string"));
  EXPECT_EQ(1, g_local_refs);
  EXPECT_EQ(0, g_global_refs);

  {
    ScopedJavaGlobalRef<jstring> global_str(str);
    ScopedJavaGlobalRef<jobject> global_obj(global_str);
    EXPECT_EQ(1, g_local_refs);
    EXPECT_EQ(2, g_global_refs);

    ScopedJavaLocalRef<jstring> str2(&counting_env, str.Release());
    EXPECT_EQ(1, g_local_refs);
    {
      ScopedJavaLocalRef<jstring> str3(str2);
      EXPECT_EQ(2, g_local_refs);
    }
    EXPECT_EQ(1, g_local_refs);
    str2.Reset();
    EXPECT_EQ(0, g_local_refs);
    global_str.Reset();
    EXPECT_EQ(1, g_global_refs);
    ScopedJavaGlobalRef<jobject> global_obj2(global_obj);
    EXPECT_EQ(2, g_global_refs);
  }

  EXPECT_EQ(0, g_local_refs);
  EXPECT_EQ(0, g_global_refs);
}

}  // namespace android
}  // namespace base
