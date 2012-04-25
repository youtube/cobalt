// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ANDROID_JNI_HELPER_H_
#define BASE_ANDROID_JNI_HELPER_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"

// Manages WeakGlobalRef lifecycle.
class JavaObjectWeakGlobalRef {
 public:
  JavaObjectWeakGlobalRef(JNIEnv* env, jobject obj);
  virtual ~JavaObjectWeakGlobalRef();

  base::android::ScopedJavaLocalRef<jobject> get(JNIEnv* env) const;

 private:
  jweak obj_;

  DISALLOW_COPY_AND_ASSIGN(JavaObjectWeakGlobalRef);
};

// Get the real object stored in the weak reference returned as a
// ScopedJavaLocalRef.
base::android::ScopedJavaLocalRef<jobject> GetRealObject(
    JNIEnv* env, jobject obj);

#endif  // BASE_ANDROID_JNI_HELPER_H_
