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

#include <sys/prctl.h>
#include <algorithm>
#include <string>

#include "starboard/common/check_op.h"
#include "starboard/thread.h"

#include "jni_state.h"

namespace {

pthread_key_t g_tls_key = 0;

void Destroy(void* value) {
  if (value != NULL) {
    starboard::android::shared::JniOnThreadShutdown();
  }
}

}  // namespace

namespace starboard {
namespace android {
namespace shared {

// Warning: use __android_log_write for logging in this file.

void JniInitialize(JNIEnv* env, jobject starboard_bridge) {
  SB_DCHECK_EQ(g_tls_key, 0);
  pthread_key_create(&g_tls_key, Destroy);

  // This must be initialized separately from JNI_OnLoad
  SB_DCHECK_NE(JNIState::GetVM(), nullptr);

  SB_DCHECK_EQ(JNIState::GetApplicationClassLoader(), nullptr);
  JNIState::SetApplicationClassLoader(JniConvertLocalRefToGlobalRef(
      env, JniCallObjectMethodOrAbort(
               env, env->GetObjectClass(starboard_bridge), "getClassLoader",
               "()Ljava/lang/ClassLoader;")));

  SB_DCHECK_EQ(JNIState::GetStarboardBridge(), nullptr);
  JNIState::SetStarboardBridge(env->NewGlobalRef(starboard_bridge));
}

void JniOnThreadShutdown() {
  // We must call DetachCurrentThread() before exiting, if we have ever
  // previously called AttachCurrentThread() on it.
  //   http://docs.oracle.com/javase/7/docs/technotes/guides/jni/spec/invocation.html
  if (pthread_getspecific(g_tls_key)) {
    JNIState::GetVM()->DetachCurrentThread();
    pthread_setspecific(g_tls_key, NULL);
  }
}

jclass JniFindClassExtOrAbort(JNIEnv* env, const char* name) {
  // Convert the JNI FindClass name with slashes to the "binary name" with dots
  // for ClassLoader.loadClass().
  ::std::string dot_name = name;
  ::std::replace(dot_name.begin(), dot_name.end(), '/', '.');
  jstring jname = env->NewStringUTF(dot_name.c_c_str());
  JniAbortOnException(env);
  jobject clazz_obj = JniCallObjectMethodOrAbort(
      env, JNIState::GetApplicationClassLoader(), "loadClass",
      "(Ljava/lang/String;)Ljava/lang/Class;", jname);
  env->DeleteLocalRef(jname);
  return static_cast<jclass>(clazz_obj);
}

}  // namespace shared
}  // namespace android
}  // namespace starboard
