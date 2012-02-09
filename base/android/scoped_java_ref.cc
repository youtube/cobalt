// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/scoped_java_ref.h"

#include "base/logging.h"

namespace base {
namespace android {

JavaRef<jobject>::JavaRef() : env_(NULL), obj_(NULL) {}

JavaRef<jobject>::JavaRef(JNIEnv* env, jobject obj)
    : env_(env),
      obj_(obj) {
  if (obj) {
    DCHECK(env);
    DCHECK_EQ(JNILocalRefType, env->GetObjectRefType(obj));
  }
}

JavaRef<jobject>::~JavaRef() {
}

void JavaRef<jobject>::SetNewLocalRef(JNIEnv* env, jobject obj) {
  if (obj)
    obj = env->NewLocalRef(obj);
  if (obj_)
    env_->DeleteLocalRef(obj_);
  env_ = env;
  obj_ = obj;
}

void JavaRef<jobject>::SetNewGlobalRef(JNIEnv* env, jobject obj) {
  if (obj)
    obj = env->NewGlobalRef(obj);
  if (obj_)
    env_->DeleteGlobalRef(obj_);
  env_ = env;
  obj_ = obj;
}

void JavaRef<jobject>::ResetLocalRef() {
  if (obj_)
    env_->DeleteLocalRef(obj_);
  env_ = NULL;
  obj_ = NULL;
}

void JavaRef<jobject>::ResetGlobalRef() {
  if (obj_)
    env_->DeleteGlobalRef(obj_);
  env_ = NULL;
  obj_ = NULL;
}

jobject JavaRef<jobject>::ReleaseInternal() {
  jobject obj = obj_;
  env_ = NULL;
  obj_ = NULL;
  return obj;
}

}  // namespace android
}  // namespace base
