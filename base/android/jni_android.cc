// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_android.h"

#include <map>

#include "base/android/scoped_java_ref.h"
#include "base/atomicops.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/threading/platform_thread.h"

namespace {
JavaVM* g_jvm = 0;
jobject g_application_context = NULL;

struct MethodIdentifier {
  const char* class_name;
  const char* method;
  const char* jni_signature;

  bool operator<(const MethodIdentifier& other) const {
    int r = strcmp(class_name, other.class_name);
    if (r < 0) {
      return true;
    } else if (r > 0) {
      return false;
    }

    r = strcmp(method, other.method);
    if (r < 0) {
      return true;
    } else if (r > 0) {
      return false;
    }

    return strcmp(jni_signature, other.jni_signature) < 0;
  }
};

typedef std::map<MethodIdentifier, jmethodID> MethodIDMap;
base::LazyInstance<MethodIDMap, base::LeakyLazyInstanceTraits<MethodIDMap> >
    g_method_id_map = LAZY_INSTANCE_INITIALIZER;
const base::subtle::AtomicWord kUnlocked = 0;
const base::subtle::AtomicWord kLocked = 1;
base::subtle::AtomicWord g_method_id_map_lock = kUnlocked;

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

jmethodID GetMethodIDFromClassName(JNIEnv* env,
                                   const char* class_name,
                                   const char* method,
                                   const char* jni_signature) {
  MethodIdentifier key;
  key.class_name = class_name;
  key.method = method;
  key.jni_signature = jni_signature;

  MethodIDMap* map = g_method_id_map.Pointer();
  bool found = false;

  while (base::subtle::Acquire_CompareAndSwap(&g_method_id_map_lock,
                                              kUnlocked,
                                              kLocked) != kUnlocked) {
    base::PlatformThread::YieldCurrentThread();
  }
  MethodIDMap::const_iterator iter = map->find(key);
  if (iter != map->end()) {
    found = true;
  }
  base::subtle::Release_Store(&g_method_id_map_lock, kUnlocked);

  // Addition to the map does not invalidate this iterator.
  if (found) {
    return iter->second;
  }

  ScopedJavaLocalRef<jclass> clazz(env, env->FindClass(class_name));
  jmethodID id = GetMethodID(env, clazz.obj(), method, jni_signature);

  while (base::subtle::Acquire_CompareAndSwap(&g_method_id_map_lock,
                                              kUnlocked,
                                              kLocked) != kUnlocked) {
    base::PlatformThread::YieldCurrentThread();
  }
  // Another thread may have populated the map already.
  std::pair<MethodIDMap::const_iterator, bool> result =
      map->insert(std::make_pair(key, id));
  DCHECK_EQ(id, result.first->second);
  base::subtle::Release_Store(&g_method_id_map_lock, kUnlocked);

  return id;
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

jfieldID GetFieldID(JNIEnv* env,
                    jclass clazz,
                    const char* field,
                    const char* jni_signature) {
  jfieldID id = env->GetFieldID(clazz, field, jni_signature);
  DCHECK(id) << field;
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
