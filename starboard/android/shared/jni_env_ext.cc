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

#include "starboard/android/shared/jni_env_ext.h"

#include <android/native_activity.h>
#include <jni.h>

#include <algorithm>
#include <string>

#include "starboard/thread.h"

namespace {

SbThreadLocalKey g_tls_key = kSbThreadLocalKeyInvalid;
ANativeActivity* g_native_activity = NULL;
jobject g_application_class_loader = NULL;
jobject g_starboard_bridge = NULL;

void Destroy(void* value) {
  // OnThreadShutdown() must be called on each thread before it is destroyed.
  SB_DCHECK(value == NULL);
}

}  // namespace

namespace starboard {
namespace android {
namespace shared {

// static
void JniEnvExt::Initialize(ANativeActivity* native_activity) {
  g_tls_key = SbThreadCreateLocalKey(Destroy);
  g_native_activity = native_activity;

  JniEnvExt* env = JniEnvExt::Get();
  jobject loader =
      env->CallObjectMethodOrAbort(env->GetObjectClass(native_activity->clazz),
                                   "getClassLoader",
                                   "()Ljava/lang/ClassLoader;");
  g_application_class_loader = env->ConvertLocalRefToGlobalRef(loader);

  jfieldID bridge_field = env->GetFieldID(
      env->GetObjectClass(native_activity->clazz),
      "starboardBridge", "Lfoo/cobalt/coat/StarboardBridge;");
  g_starboard_bridge = env->ConvertLocalRefToGlobalRef(
      env->GetObjectField(native_activity->clazz, bridge_field));
}

// static
void JniEnvExt::OnThreadShutdown() {
  // We must call DetachCurrentThread() before exiting, if we have ever
  // previously called AttachCurrentThread() on it.
  //   http://docs.oracle.com/javase/7/docs/technotes/guides/jni/spec/invocation.html
  if (SbThreadGetLocalValue(g_tls_key)) {
    g_native_activity->vm->DetachCurrentThread();
    SbThreadSetLocalValue(g_tls_key, NULL);
  }
}

JniEnvExt* JniEnvExt::Get() {
  JNIEnv* env;
  // Always attach in case someone detached. This is a no-op if still attached.
  g_native_activity->vm->AttachCurrentThread(&env, NULL);
  // We don't actually use the value, but any non-NULL means we have to detach.
  SbThreadSetLocalValue(g_tls_key, env);
  // The downcast is safe since we only add methods, not fields.
  return static_cast<JniEnvExt*>(env);
}

jobject JniEnvExt::GetStarboardBridge() {
  return g_starboard_bridge;
}

jclass JniEnvExt::FindClassExtOrAbort(const char* name) {
  // Convert the JNI FindClass name with slashes to the "binary name" with dots
  // for ClassLoader.loadClass().
  ::std::string dot_name = name;
  ::std::replace(dot_name.begin(), dot_name.end(), '/', '.');
  jstring jname = NewStringUTF(dot_name.c_str());
  AbortOnException();
  jobject clazz_obj =
      CallObjectMethodOrAbort(g_application_class_loader, "loadClass",
                              "(Ljava/lang/String;)Ljava/lang/Class;", jname);
  DeleteLocalRef(jname);
  return static_cast<jclass>(clazz_obj);
}

}  // namespace shared
}  // namespace android
}  // namespace starboard
