// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_helper.h"

#include "base/android/jni_android.h"
#include "base/logging.h"

using base::android::AttachCurrentThread;

JavaObjectWeakGlobalRef::JavaObjectWeakGlobalRef(JNIEnv* env, jobject obj)
    : obj_(env->NewWeakGlobalRef(obj)) {
  DCHECK(obj_);
}

JavaObjectWeakGlobalRef::~JavaObjectWeakGlobalRef() {
  DCHECK(obj_);
  AttachCurrentThread()->DeleteWeakGlobalRef(obj_);
}

base::android::ScopedJavaLocalRef<jobject>
    JavaObjectWeakGlobalRef::get(JNIEnv* env) const {
  return GetRealObject(env, obj_);
}

base::android::ScopedJavaLocalRef<jobject> GetRealObject(
    JNIEnv* env, jobject obj) {
  jobject real = env->NewLocalRef(obj);
  if (!real)
    DLOG(ERROR) << "The real object has been deleted!";
  return base::android::ScopedJavaLocalRef<jobject>(env, real);
}
