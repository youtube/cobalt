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

#include "base/android/jni_android.h"
#include "starboard/android/shared/jni_state.h"
#include "starboard/common/check_op.h"
#include "starboard/thread.h"

<<<<<<< HEAD
#include "jni_state.h"

=======
>>>>>>> 9cd2db17c5a (starboard: JniEnvExt logs JNI exception on abort  (#7440))
namespace {

pthread_key_t g_tls_key = 0;

void Destroy(void* value) {
  if (value != NULL) {
    starboard::android::shared::JniEnvExt::OnThreadShutdown();
  }
}

}  // namespace

namespace starboard::android::shared {

// Warning: use __android_log_write for logging in this file.

// static
void JniEnvExt::Initialize(JniEnvExt* env, jobject starboard_bridge) {
  SB_DCHECK_EQ(g_tls_key, 0);
  pthread_key_create(&g_tls_key, Destroy);

  // This must be initialized separately from JNI_OnLoad
  SB_DCHECK_NE(JNIState::GetVM(), nullptr);

  SB_DCHECK_EQ(JNIState::GetApplicationClassLoader(), nullptr);
  JNIState::SetApplicationClassLoader(
      env->ConvertLocalRefToGlobalRef(env->CallObjectMethodOrAbort(
          env->GetObjectClass(starboard_bridge), "getClassLoader",
          "()Ljava/lang/ClassLoader;")));

  SB_DCHECK_EQ(JNIState::GetStarboardBridge(), nullptr);
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
#if __ANDROID_API__ < 26
    prctl(PR_GET_NAME, thread_name, 0L, 0L, 0L);
#else
    pthread_getname_np(pthread_self(), thread_name, sizeof(thread_name));
#endif  // __ANDROID_API__ < 26
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

<<<<<<< HEAD
}  // namespace starboard::android::shared
=======
jclass JniFindClassOrAbort(JNIEnv* env, const char* name) {
  SB_CHECK(env);
  jclass result = env->FindClass(name);
  JniAbortOnException(env);
  return result;
}

jobject JniNewObjectOrAbort(JNIEnv* env,
                            const char* class_name,
                            const char* sig,
                            ...) {
  SB_CHECK(env);
  va_list argp;
  va_start(argp, sig);
  jclass clazz = JniFindClassExtOrAbort(env, class_name);
  jmethodID methodID = env->GetMethodID(clazz, "<init>", sig);
  JniAbortOnException(env);
  jobject result = env->NewObjectV(clazz, methodID, argp);
  JniAbortOnException(env);
  env->DeleteLocalRef(clazz);
  va_end(argp);
  return result;
}

jstring JniNewStringStandardUTFOrAbort(JNIEnv* env, const char* bytes) {
  SB_CHECK(env);
  const jstring charset = env->NewStringUTF("UTF-8");
  JniAbortOnException(env);
  const jbyteArray byte_array = JniNewByteArrayFromRaw(
      env, reinterpret_cast<const jbyte*>(bytes), (bytes ? strlen(bytes) : 0U));
  JniAbortOnException(env);
  jstring result = static_cast<jstring>(JniNewObjectOrAbort(
      env, "java/lang/String", "([BLjava/lang/String;)V", byte_array, charset));
  env->DeleteLocalRef(byte_array);
  env->DeleteLocalRef(charset);
  return result;
}

std::string JniGetStringStandardUTFOrAbort(JNIEnv* env, jstring str) {
  SB_CHECK(env);
  if (str == nullptr) {
    return std::string();
  }
  const jstring charset = env->NewStringUTF("UTF-8");
  JniAbortOnException(env);
  const jbyteArray byte_array =
      static_cast<jbyteArray>(JniCallObjectMethodOrAbort(
          env, str, "getBytes", "(Ljava/lang/String;)[B", charset));
  jsize array_length = env->GetArrayLength(byte_array);
  JniAbortOnException(env);
  void* bytes = env->GetPrimitiveArrayCritical(byte_array, nullptr);
  JniAbortOnException(env);
  std::string result(static_cast<const char*>(bytes), array_length);
  env->ReleasePrimitiveArrayCritical(byte_array, bytes, JNI_ABORT);
  JniAbortOnException(env);
  env->DeleteLocalRef(byte_array);
  env->DeleteLocalRef(charset);
  return result;
}

void JniCallVoidMethod(JNIEnv* env,
                       jobject obj,
                       const char* name,
                       const char* sig,
                       ...) {
  SB_CHECK(env);
  va_list argp;
  va_start(argp, sig);
  env->CallVoidMethodV(obj, JniGetObjectMethodIDOrAbort(env, obj, name, sig),
                       argp);
  va_end(argp);
}

void JniCallVoidMethodOrAbort(JNIEnv* env,
                              jobject obj,
                              const char* name,
                              const char* sig,
                              ...) {
  SB_CHECK(env);
  va_list argp;
  va_start(argp, sig);
  JniCallVoidMethodVOrAbort(
      env, obj, JniGetObjectMethodIDOrAbort(env, obj, name, sig), argp);
  va_end(argp);
}

void JniCallVoidMethodVOrAbort(JNIEnv* env,
                               jobject obj,
                               jmethodID methodID,
                               va_list args) {
  SB_CHECK(env);
  env->CallVoidMethodV(obj, methodID, args);
  JniAbortOnException(env);
}

void JniCallStaticVoidMethod(JNIEnv* env,
                             const char* class_name,
                             const char* method_name,
                             const char* sig,
                             ...) {
  SB_CHECK(env);
  va_list argp;
  va_start(argp, sig);
  jclass clazz = JniFindClassExtOrAbort(env, class_name);
  env->CallStaticVoidMethodV(
      clazz, JniGetStaticMethodIDOrAbort(env, clazz, method_name, sig), argp);
  env->DeleteLocalRef(clazz);
  va_end(argp);
}

void JniCallStaticVoidMethodOrAbort(JNIEnv* env,
                                    const char* class_name,
                                    const char* method_name,
                                    const char* sig,
                                    ...) {
  SB_CHECK(env);
  va_list argp;
  va_start(argp, sig);
  jclass clazz = JniFindClassExtOrAbort(env, class_name);
  env->CallStaticVoidMethodV(
      clazz, JniGetStaticMethodIDOrAbort(env, clazz, method_name, sig), argp);
  JniAbortOnException(env);
  env->DeleteLocalRef(clazz);
  va_end(argp);
}

jobject JniConvertLocalRefToGlobalRef(JNIEnv* env, jobject local) {
  SB_CHECK(env);
  jobject global = env->NewGlobalRef(local);
  env->DeleteLocalRef(local);
  return global;
}

void JniAbortOnException(JNIEnv* env) {
  SB_CHECK(env);
  base::android::CheckException(env);
}

}  // namespace starboard
>>>>>>> 9cd2db17c5a (starboard: JniEnvExt logs JNI exception on abort  (#7440))
