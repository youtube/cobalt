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

#include "starboard/android/shared/jni_state.h"

namespace {

pthread_key_t g_tls_key = 0;

void Destroy(void* value) {
  if (value != NULL) {
    starboard::android::shared::JniOnThreadShutdown();
  }
}

}  // namespace

namespace starboard::android::shared {

// Warning: use __android_log_write for logging in this file.

// static
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

// static
void JniOnThreadShutdown() {
  // We must call DetachCurrentThread() before exiting, if we have ever
  // previously called AttachCurrentThread() on it.
  //   http://docs.oracle.com/javase/7/docs/technotes/guides/jni/spec/invocation.html
  if (pthread_getspecific(g_tls_key)) {
    JNIState::GetVM()->DetachCurrentThread();
    pthread_setspecific(g_tls_key, NULL);
  }
}

jfieldID JniGetStaticFieldIDOrAbort(JNIEnv* env,
                                    jclass clazz,
                                    const char* name,
                                    const char* sig) {
  SB_CHECK(env);
  jfieldID field = env->GetStaticFieldID(clazz, name, sig);
  JniAbortOnException(env);
  return field;
}

jfieldID JniGetFieldIDOrAbort(JNIEnv* env,
                              jobject obj,
                              const char* name,
                              const char* sig) {
  SB_CHECK(env);
  jclass clazz = env->GetObjectClass(obj);
  JniAbortOnException(env);
  jfieldID field = env->GetFieldID(clazz, name, sig);
  JniAbortOnException(env);
  env->DeleteLocalRef(clazz);
  return field;
}

jint JniGetEnumValueOrAbort(JNIEnv* env, jclass clazz, const char* name) {
  SB_CHECK(env);
  jfieldID field = JniGetStaticFieldIDOrAbort(env, clazz, name, "I");
  jint enum_value = env->GetStaticIntField(clazz, field);
  JniAbortOnException(env);
  return enum_value;
}

jmethodID JniGetObjectMethodIDOrAbort(JNIEnv* env,
                                      jobject obj,
                                      const char* name,
                                      const char* sig) {
  SB_CHECK(env);
  jclass clazz = env->GetObjectClass(obj);
  JniAbortOnException(env);
  jmethodID method_id = env->GetMethodID(clazz, name, sig);
  JniAbortOnException(env);
  env->DeleteLocalRef(clazz);
  return method_id;
}

jmethodID JniGetStaticMethodIDOrAbort(JNIEnv* env,
                                      jclass clazz,
                                      const char* name,
                                      const char* sig) {
  SB_CHECK(env);
  jmethodID method = env->GetStaticMethodID(clazz, name, sig);
  JniAbortOnException(env);
  return method;
}

jobject JniGetObjectArrayElementOrAbort(JNIEnv* env,
                                        jobjectArray array,
                                        jsize index) {
  SB_CHECK(env);
  jobject result = env->GetObjectArrayElement(array, index);
  JniAbortOnException(env);
  return result;
}

jclass JniFindClassExtOrAbort(JNIEnv* env, const char* name) {
  // Convert the JNI FindClass name with slashes to the "binary name" with dots
  // for ClassLoader.loadClass().
  ::std::string dot_name = name;
  ::std::replace(dot_name.begin(), dot_name.end(), '/', '.');
  jstring jname = env->NewStringUTF(dot_name.c_str());
  JniAbortOnException(env);
  jobject clazz_obj = JniCallObjectMethodOrAbort(
      env, JNIState::GetApplicationClassLoader(), "loadClass",
      "(Ljava/lang/String;)Ljava/lang/Class;", jname);
  env->DeleteLocalRef(jname);
  return static_cast<jclass>(clazz_obj);
}

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
  if (!env->ExceptionCheck()) {
    return;
  }
  env->ExceptionDescribe();
  SbSystemBreakIntoDebugger();
}

}  // namespace starboard::android::shared
