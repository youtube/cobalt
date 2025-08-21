// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

#include "jni_state.h"

namespace {

pthread_key_t g_tls_key = 0;

void Destroy(void* value) {
  if (value != NULL) {
    starboard::android::shared::JniEnvExt::OnThreadShutdown();
  }
}

}  // namespace

namespace starboard {
namespace android {
namespace shared {

// Warning: use __android_log_write for logging in this file.

// static
void JniEnvExt::Initialize(JniEnvExt* env, jobject starboard_bridge) {
  SB_DCHECK(g_tls_key == 0);
  pthread_key_create(&g_tls_key, Destroy);

  // This must be initialized separately from JNI_OnLoad
  SB_DCHECK(JNIState::GetVM() != NULL);

  SB_DCHECK(JNIState::GetApplicationClassLoader() == NULL);
  JNIState::SetApplicationClassLoader(
      env->ConvertLocalRefToGlobalRef(env->CallObjectMethodOrAbort(
          env->GetObjectClass(starboard_bridge), "getClassLoader",
          "()Ljava/lang/ClassLoader;")));

  SB_DCHECK(JNIState::GetStarboardBridge() == NULL);
  JNIState::SetStarboardBridge(env->NewGlobalRef(starboard_bridge));
}

// static
void JniEnvExt::OnThreadShutdown() {
  // We must call DetachCurrentThread() before exiting, if we have ever
  // previously called AttachCurrentThread() on it.
  //   http://docs.oracle.com/javase/7/docs/technotes/guides/jni/spec/invocation.html
  if (pthread_getspecific(g_tls_key)) {
    JNIState::GetVM()->DetachCurrentThread();
    pthread_setspecific(g_tls_key, NULL);
  }
}

JniEnvExt* JniEnvExt::Get() {
  JNIEnv* env = nullptr;
  if (JNI_OK != JNIState::GetVM()->GetEnv(reinterpret_cast<void**>(&env),
                                          JNI_VERSION_1_4)) {
    // Tell the JVM our thread name so it doesn't change it.
    char thread_name[16];
    pthread_getname_np(pthread_self(), thread_name, sizeof(thread_name));
    JavaVMAttachArgs args{JNI_VERSION_1_4, thread_name, NULL};
    JNIState::GetVM()->AttachCurrentThread(&env, &args);
    // We don't use the value, but any non-NULL means we have to detach.
    pthread_setspecific(g_tls_key, env);
  }
  // The downcast is safe since we only add methods, not fields.
  return static_cast<JniEnvExt*>(env);
}

jobject JniEnvExt::GetStarboardBridge() {
  return JNIState::GetStarboardBridge();
}

jclass JniEnvExt::FindClassExtOrAbort(const char* name) {
  // Convert the JNI FindClass name with slashes to the "binary name" with dots
  // for ClassLoader.loadClass().
  ::std::string dot_name = name;
  ::std::replace(dot_name.begin(), dot_name.end(), '/', '.');
  jstring jname = NewStringUTF(dot_name.c_str());
  AbortOnException();
  jobject clazz_obj = CallObjectMethodOrAbort(
      JNIState::GetApplicationClassLoader(), "loadClass",
      "(Ljava/lang/String;)Ljava/lang/Class;", jname);
  DeleteLocalRef(jname);
  return static_cast<jclass>(clazz_obj);
}

}  // namespace shared
}  // namespace android
}  // namespace starboard
