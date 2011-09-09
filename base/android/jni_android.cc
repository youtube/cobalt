// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_android.h"

#include "base/logging.h"

namespace {
JavaVM* g_jvm = 0;
jobject g_application_context = NULL;
}

namespace base {
namespace android {

JNIEnv* AttachCurrentThread() {
  if (!g_jvm)
    return NULL;

  JNIEnv* env = NULL;
  jint ret = g_jvm->AttachCurrentThread(&env, NULL);
  DCHECK_EQ(ret, JNI_OK);
  return env;
}

void DetachFromVM() {
  // Ignore the return value, if the thread is not attached, DetachCurrentThread
  // will fail. But it is ok as the native thread may never be attached.
  if (g_jvm)
    g_jvm->DetachCurrentThread();
}

void InitVM(JavaVM* vm) {
  DCHECK(!g_jvm);
  g_jvm = vm;
}

void InitApplicationContext(jobject context) {
  DCHECK(!g_application_context);
  g_application_context = context;
}

jobject GetApplicationContext() {
  DCHECK(g_application_context);
  return g_application_context;
}

jmethodID GetMethodID(JNIEnv* env,
                      jclass clazz,
                      const char* const method,
                      const char* const jni_signature) {
  jmethodID id = env->GetMethodID(clazz, method, jni_signature);
  DCHECK(id) << method;
  CheckException(env);
  return id;
}

jmethodID GetStaticMethodID(JNIEnv* env,
                            jclass clazz,
                            const char* const method,
                            const char* const jni_signature) {
  jmethodID id = env->GetStaticMethodID(clazz, method, jni_signature);
  DCHECK(id) << method;
  CheckException(env);
  return id;
}

bool CheckException(JNIEnv* env) {
  if (env->ExceptionCheck() == JNI_FALSE)
    return false;
  env->ExceptionDescribe();
  env->ExceptionClear();
  return true;
}

}  // namespace android
}  // namespace base
