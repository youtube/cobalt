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

#ifndef STARBOARD_ANDROID_SHARED_JNI_ENV_EXT_H_
#define STARBOARD_ANDROID_SHARED_JNI_ENV_EXT_H_

#include <cstdarg>
#include <cstring>
#include <memory>
#include <string>

#include "base/android/jni_android.h"
#include "starboard/common/check_op.h"
#include "starboard/common/log.h"
#include "starboard/system.h"

namespace starboard::android::shared {

// One-time initialization to be called before starting the application.
void JniInitialize(JNIEnv* jni_env, jobject starboard_bridge);

// Called right before each native thread is about to be shutdown.
void JniOnThreadShutdown();

inline void JniAbortOnException(JNIEnv* env) {
  if (!env->ExceptionCheck()) {
    return;
  }
  env->ExceptionDescribe();
  SbSystemBreakIntoDebugger();
}

// Lookup the class of an object and find a field in it.
inline jfieldID JniGetStaticFieldIDOrAbort(JNIEnv* env,
                                           jclass clazz,
                                           const char* name,
                                           const char* sig) {
  SB_CHECK(env);
  jfieldID field = env->GetStaticFieldID(clazz, name, sig);
  JniAbortOnException(env);
  return field;
}

inline jfieldID JniGetFieldIDOrAbort(JNIEnv* env,
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

inline jint JniGetEnumValueOrAbort(JNIEnv* env,
                                   jclass clazz,
                                   const char* name) {
  SB_CHECK(env);
  jfieldID field = JniGetStaticFieldIDOrAbort(env, clazz, name, "I");
  jint enum_value = env->GetStaticIntField(clazz, field);
  JniAbortOnException(env);
  return enum_value;
}

// Lookup the class of an object and find a method in it.
inline jmethodID JniGetObjectMethodIDOrAbort(JNIEnv* env,
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

inline jmethodID JniGetStaticMethodIDOrAbort(JNIEnv* env,
                                             jclass clazz,
                                             const char* name,
                                             const char* sig) {
  SB_CHECK(env);
  jmethodID method = env->GetStaticMethodID(clazz, name, sig);
  JniAbortOnException(env);
  return method;
}

inline jobject JniGetObjectArrayElementOrAbort(JNIEnv* env,
                                               jobjectArray array,
                                               jsize index) {
  SB_CHECK(env);
  jobject result = env->GetObjectArrayElement(array, index);
  JniAbortOnException(env);
  return result;
}

// Find a class, aborting if it isn't found.
jclass JniFindClassExtOrAbort(JNIEnv* env, const char* name);

inline jclass JniFindClassOrAbort(JNIEnv* env, const char* name) {
  SB_CHECK(env);
  jclass result = env->FindClass(name);
  JniAbortOnException(env);
  return result;
}

// Create a new object, aborting on failure.
inline jobject JniNewObjectOrAbort(JNIEnv* env,
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

// Create a new jstring from a standard UTF-8 string, aborting on failure.
// Note that this function creates a new jstring from a char* that is in
// standard UTF-8 encoding. This differs from JNIEnv::NewStringUTF() which
// takes JNI modified UTF-8.
inline jstring JniNewStringStandardUTFOrAbort(JNIEnv* env, const char* bytes) {
  SB_CHECK(env);
  const jstring charset = env->NewStringUTF("UTF-8");
  JniAbortOnException(env);
  const jbyteArray byte_array = JniNewByteArrayFromRaw(
      env, reinterpret_cast<const jbyte*>(bytes), (bytes ? strlen(bytes) : 0U));
  JniAbortOnException(env);
  jobject result = JniNewObjectOrAbort(
      env, "java/lang/String", "([BLjava/lang/String;)V", byte_array, charset);
  env->DeleteLocalRef(byte_array);
  env->DeleteLocalRef(charset);
  return static_cast<jstring>(result);
}

// Get a standard UTF-8 std::string from a jstring, aborting on failure.
inline std::string JniGetStringStandardUTFOrAbort(JNIEnv* env, jstring str) {
  SB_CHECK(env);
  if (str == NULL) {
    return std::string();
  }
  const jstring charset = env->NewStringUTF("UTF-8");
  JniAbortOnException(env);
  const jbyteArray byte_array =
      static_cast<jbyteArray>(JniCallObjectMethodOrAbort(
          env, str, "getBytes", "(Ljava/lang/String;)[B", charset));
  jsize array_length = env->GetArrayLength(byte_array);
  JniAbortOnException(env);
  void* bytes = env->GetPrimitiveArrayCritical(byte_array, NULL);
  JniAbortOnException(env);
  std::string result(static_cast<const char*>(bytes), array_length);
  env->ReleasePrimitiveArrayCritical(byte_array, bytes, JNI_ABORT);
  JniAbortOnException(env);
  env->DeleteLocalRef(byte_array);
  env->DeleteLocalRef(charset);
  return result;
}

#define DEFINE_JNI_GET_FIELD_OR_ABORT(_jtype, _jname)                          \
  inline _jtype JniGet##_jname##FieldOrAbort(                                  \
      JNIEnv* env, jobject obj, const char* name, const char* sig) {           \
    SB_CHECK(env);                                                             \
    _jtype result = env->Get##_jname##Field(                                   \
        obj, JniGetFieldIDOrAbort(env, obj, name, sig));                       \
    JniAbortOnException(env);                                                  \
    return result;                                                             \
  }                                                                            \
                                                                               \
  inline _jtype JniGetStatic##_jname##FieldOrAbort(                            \
      JNIEnv* env, const char* class_name, const char* name,                   \
      const char* sig) {                                                       \
    SB_CHECK(env);                                                             \
    jclass clazz = JniFindClassExtOrAbort(env, class_name);                    \
    _jtype result = JniGetStatic##_jname##FieldOrAbort(env, clazz, name, sig); \
    env->DeleteLocalRef(clazz);                                                \
    return result;                                                             \
  }                                                                            \
                                                                               \
  inline _jtype JniGetStatic##_jname##FieldOrAbort(                            \
      JNIEnv* env, jclass clazz, const char* name, const char* sig) {          \
    SB_CHECK(env);                                                             \
    _jtype result = env->GetStatic##_jname##Field(                             \
        clazz, JniGetStaticFieldIDOrAbort(env, clazz, name, sig));             \
    JniAbortOnException(env);                                                  \
    return result;                                                             \
  }

#define DEFINE_JNI_CALL_METHOD_OR_ABORT(_jtype, _jname)                        \
  inline _jtype JniCall##_jname##MethodOrAbort(                                \
      JNIEnv* env, jobject obj, const char* name, const char* sig, ...) {      \
    SB_CHECK(env);                                                             \
    va_list argp;                                                              \
    va_start(argp, sig);                                                       \
    _jtype result = JniCall##_jname##MethodVOrAbort(                           \
        env, obj, JniGetObjectMethodIDOrAbort(env, obj, name, sig), argp);     \
    va_end(argp);                                                              \
    return result;                                                             \
  }                                                                            \
                                                                               \
  inline _jtype JniCallStatic##_jname##MethodOrAbort(                          \
      JNIEnv* env, const char* class_name, const char* method_name,            \
      const char* sig, ...) {                                                  \
    SB_CHECK(env);                                                             \
    va_list argp;                                                              \
    va_start(argp, sig);                                                       \
    jclass clazz = JniFindClassExtOrAbort(env, class_name);                    \
    _jtype result = JniCallStatic##_jname##MethodVOrAbort(                     \
        env, clazz, JniGetStaticMethodIDOrAbort(env, clazz, method_name, sig), \
        argp);                                                                 \
    env->DeleteLocalRef(clazz);                                                \
    va_end(argp);                                                              \
    return result;                                                             \
  }                                                                            \
                                                                               \
  inline _jtype JniCall##_jname##MethodVOrAbort(                               \
      JNIEnv* env, jobject obj, jmethodID methodID, va_list args) {            \
    SB_CHECK(env);                                                             \
    _jtype result = env->Call##_jname##MethodV(obj, methodID, args);           \
    JniAbortOnException(env);                                                  \
    return result;                                                             \
  }                                                                            \
                                                                               \
  inline _jtype JniCallStatic##_jname##MethodVOrAbort(                         \
      JNIEnv* env, jclass clazz, jmethodID methodID, va_list args) {           \
    SB_CHECK(env);                                                             \
    _jtype result = env->CallStatic##_jname##MethodV(clazz, methodID, args);   \
    JniAbortOnException(env);                                                  \
    return result;                                                             \
  }

DEFINE_JNI_GET_FIELD_OR_ABORT(jobject, Object)
DEFINE_JNI_GET_FIELD_OR_ABORT(jboolean, Boolean)
DEFINE_JNI_GET_FIELD_OR_ABORT(jbyte, Byte)
DEFINE_JNI_GET_FIELD_OR_ABORT(jchar, Char)
DEFINE_JNI_GET_FIELD_OR_ABORT(jshort, Short)
DEFINE_JNI_GET_FIELD_OR_ABORT(jint, Int)
DEFINE_JNI_GET_FIELD_OR_ABORT(jlong, Long)
DEFINE_JNI_GET_FIELD_OR_ABORT(jfloat, Float)
DEFINE_JNI_GET_FIELD_OR_ABORT(jdouble, Double)
#undef DEFINE_JNI_GET_FIELD_OR_ABORT

DEFINE_JNI_CALL_METHOD_OR_ABORT(jobject, Object)
DEFINE_JNI_CALL_METHOD_OR_ABORT(jboolean, Boolean)
DEFINE_JNI_CALL_METHOD_OR_ABORT(jbyte, Byte)
DEFINE_JNI_CALL_METHOD_OR_ABORT(jchar, Char)
DEFINE_JNI_CALL_METHOD_OR_ABORT(jshort, Short)
DEFINE_JNI_CALL_METHOD_OR_ABORT(jint, Int)
DEFINE_JNI_CALL_METHOD_OR_ABORT(jlong, Long)
DEFINE_JNI_CALL_METHOD_OR_ABORT(jfloat, Float)
DEFINE_JNI_CALL_METHOD_OR_ABORT(jdouble, Double)
#undef DEFINE_JNI_CALL_METHOD_OR_ABORT

inline void JniCallVoidMethod(JNIEnv* env,
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

inline void JniCallVoidMethodOrAbort(JNIEnv* env,
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

inline void JniCallVoidMethodVOrAbort(JNIEnv* env,
                                      jobject obj,
                                      jmethodID methodID,
                                      va_list args) {
  SB_CHECK(env);
  env->CallVoidMethodV(obj, methodID, args);
  JniAbortOnException(env);
}

inline void JniCallStaticVoidMethod(JNIEnv* env,
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

inline void JniCallStaticVoidMethodOrAbort(JNIEnv* env,
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

#define DEFINE_JNI_NEW_ARRAY_FROM_RAW(j_type, j_name)      \
  inline j_type##Array JniNew##j_name##ArrayFromRaw(       \
      JNIEnv* env, const j_type* data, jsize size) {       \
    SB_CHECK(env);                                         \
    SB_DCHECK(data);                                       \
    SB_DCHECK_GE(size, 0);                                 \
    j_type##Array j_array = env->New##j_name##Array(size); \
    SB_CHECK(j_array) << "Out of memory making new array"; \
    env->Set##j_name##ArrayRegion(j_array, 0, size, data); \
    return j_array;                                        \
  }
DEFINE_JNI_NEW_ARRAY_FROM_RAW(jboolean, Boolean)
DEFINE_JNI_NEW_ARRAY_FROM_RAW(jbyte, Byte)
DEFINE_JNI_NEW_ARRAY_FROM_RAW(jchar, Char)
DEFINE_JNI_NEW_ARRAY_FROM_RAW(jshort, Short)
DEFINE_JNI_NEW_ARRAY_FROM_RAW(jint, Int)
DEFINE_JNI_NEW_ARRAY_FROM_RAW(jlong, Long)
DEFINE_JNI_NEW_ARRAY_FROM_RAW(jfloat, Float)
DEFINE_JNI_NEW_ARRAY_FROM_RAW(jdouble, Double)
#undef DEFINE_JNI_NEW_ARRAY_FROM_RAW

inline jobject JniConvertLocalRefToGlobalRef(JNIEnv* env, jobject local) {
  SB_CHECK(env);
  jobject global = env->NewGlobalRef(local);
  env->DeleteLocalRef(local);
  return global;
}

}  // namespace starboard::android::shared

#endif  // STARBOARD_ANDROID_SHARED_JNI_ENV_EXT_H_
