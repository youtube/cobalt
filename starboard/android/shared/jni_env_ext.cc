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
JavaVM* g_vm = NULL;
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
void JniEnvExt::Initialize(JniEnvExt* env, jobject starboard_bridge) {
  SB_DCHECK(g_tls_key == kSbThreadLocalKeyInvalid);
  g_tls_key = SbThreadCreateLocalKey(Destroy);

  SB_DCHECK(g_vm == NULL);
  env->GetJavaVM(&g_vm);

  SB_DCHECK(g_application_class_loader == NULL);
  g_application_class_loader = env->ConvertLocalRefToGlobalRef(
      env->CallObjectMethodOrAbort(env->GetObjectClass(starboard_bridge),
                                   "getClassLoader",
                                   "()Ljava/lang/ClassLoader;"));

  SB_DCHECK(g_starboard_bridge == NULL);
  g_starboard_bridge = env->NewGlobalRef(starboard_bridge);
}

// static
void JniEnvExt::OnThreadShutdown() {
  // We must call DetachCurrentThread() before exiting, if we have ever
  // previously called AttachCurrentThread() on it.
  //   http://docs.oracle.com/javase/7/docs/technotes/guides/jni/spec/invocation.html
  if (SbThreadGetLocalValue(g_tls_key)) {
    g_vm->DetachCurrentThread();
    SbThreadSetLocalValue(g_tls_key, NULL);
  }
}

JniEnvExt* JniEnvExt::Get() {
  JNIEnv* env;
  if (JNI_OK != g_vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6)) {
    // Tell the JVM our thread name so it doesn't change it.
    char thread_name[16];
    SbThreadGetName(thread_name, sizeof(thread_name));
    JavaVMAttachArgs args { JNI_VERSION_1_6, thread_name, NULL };
    g_vm->AttachCurrentThread(&env, &args);
    // We don't use the value, but any non-NULL means we have to detach.
    SbThreadSetLocalValue(g_tls_key, env);
  }
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
