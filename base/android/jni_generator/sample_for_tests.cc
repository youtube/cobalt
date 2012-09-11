// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"

// The main purpose of this is to ensure sample_for_tests_jni.h
// compiles and the functions declared in it as expected.

using base::android::AttachCurrentThread;
using base::android::ScopedJavaLocalRef;

namespace base {
namespace android {

class CPPClass {
 public:
  class InnerClass {
   public:
    jdouble MethodOtherP0(JNIEnv* env, jobject obj) {
      return 0.0;
    }
  };

  void Destroy(JNIEnv* env, jobject obj) {
    delete this;
  }

  jint Method(JNIEnv* env, jobject obj) {
    return 0;
  }

  ScopedJavaLocalRef<jstring> InnerMethod(JNIEnv* env, jobject obj) {
    return ScopedJavaLocalRef<jstring>();
  }
};

static jint Init(JNIEnv* env, jobject obj) {
  return 0;
}

static jdouble GetDoubleFunction(JNIEnv*, jobject) {
  return 0;
}

static jfloat GetFloatFunction(JNIEnv*, jclass) {
  return 0;
}

static void SetNonPODDatatype(JNIEnv*, jobject, jobject) {}

static jobject GetNonPODDatatype(JNIEnv*, jobject) {
  return NULL;
}

static jint InnerFunction(JNIEnv*, jclass) {
  return 0;
}

} // namespace android
} // namespace base

#include "jni/SampleForTests_jni.h"

int main() {
  // On a regular application, you'd call AttachCurrentThread(). This sample is
  // not yet linking with all the libraries.
  JNIEnv* env = /* AttachCurrentThread() */ NULL;

  // This is how you call a java static method from C++.
  bool foo = base::android::Java_SampleForTests_staticJavaMethod(env);

  // This is how you call a java method from C++. Note that you must have
  // obtained the jobject somehow.
  jobject my_java_object = NULL;
  int bar = base::android::Java_SampleForTests_javaMethod(
      env, my_java_object, 1, 2);

  return 0;
}
