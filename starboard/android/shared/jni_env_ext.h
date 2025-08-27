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

#include <android/native_activity.h>
#include <jni.h>

#include <cstdarg>
#include <cstring>
#include <string>

#include "starboard/common/check_op.h"
#include "starboard/common/log.h"
#include "starboard/system.h"

namespace starboard::android::shared {

// An extension to JNIEnv to simplify making JNI calls.
//
// This struct has convenience methods to lookup and call Java methods on object
// instances in a single step.
struct JniExt {
  // Warning: use __android_log_write for logging in this file to astatic void
  // infinite recursion.

  // One-time initialization to be called before starting the application.
  static void Initialize(JNIEnv* env, jobject starboard_bridge);

  // Called right before each native thread is about to be shutdown.
  static void OnThreadShutdown();

  // Lookup the class of an object and find a field in it.
  static jfieldID GetStaticFieldIDOrAbort(JNIEnv* env,
                                          jclass clazz,
                                          const char* name,
                                          const char* sig) {
    SB_CHECK(env);
    jfieldID field = env->GetStaticFieldID(clazz, name, sig);
    AbortOnException(env);
    return field;
  }

  static jfieldID GetFieldIDOrAbort(JNIEnv* env,
                                    jobject obj,
                                    const char* name,
                                    const char* sig) {
    SB_CHECK(env);
    jclass clazz = env->GetObjectClass(obj);
    AbortOnException(env);
    jfieldID field = env->GetFieldID(clazz, name, sig);
    AbortOnException(env);
    env->DeleteLocalRef(clazz);
    return field;
  }

  static jint GetEnumValueOrAbort(JNIEnv* env, jclass clazz, const char* name) {
    SB_CHECK(env);
    jfieldID field = GetStaticFieldIDOrAbort(env, clazz, name, "I");
    jint enum_value = env->GetStaticIntField(clazz, field);
    AbortOnException(env);
    return enum_value;
  }

  // Lookup the class of an object and find a method in it.
  static jmethodID GetObjectMethodIDOrAbort(JNIEnv* env,
                                            jobject obj,
                                            const char* name,
                                            const char* sig) {
    SB_CHECK(env);
    jclass clazz = env->GetObjectClass(obj);
    AbortOnException(env);
    jmethodID method_id = env->GetMethodID(clazz, name, sig);
    AbortOnException(env);
    env->DeleteLocalRef(clazz);
    return method_id;
  }

  static jmethodID GetStaticMethodIDOrAbort(JNIEnv* env,
                                            jclass clazz,
                                            const char* name,
                                            const char* sig) {
    SB_CHECK(env);
    jmethodID method = env->GetStaticMethodID(clazz, name, sig);
    AbortOnException(env);
    return method;
  }

  static jobject GetObjectArrayElementOrAbort(JNIEnv* env,
                                              jobjectArray array,
                                              jsize index) {
    SB_CHECK(env);
    jobject result = env->GetObjectArrayElement(array, index);
    AbortOnException(env);
    return result;
  }

  // Find a class by name using the application's class loader. This can load
  // both system classes and application classes, even when not in a JNI
  // stack frame (e.g. in a native thread that was attached the the JVM).
  // https://developer.android.com/training/articles/perf-jni.html#faq_FindClass
  static jclass FindClassExtOrAbort(JNIEnv* env, const char* name);

  static jclass FindClassOrAbort(JNIEnv* env, const char* name) {
    SB_CHECK(env);
    jclass result = env->FindClass(name);
    AbortOnException(env);
    return result;
  }

  // Convenience method to lookup and call a constructor.
  static jobject NewObjectOrAbort(JNIEnv* env,
                                  const char* class_name,
                                  const char* sig,
                                  ...) {
    SB_CHECK(env);
    va_list argp;
    va_start(argp, sig);
    jclass clazz = FindClassExtOrAbort(env, class_name);
    jmethodID methodID = env->GetMethodID(clazz, "<init>", sig);
    AbortOnException(env);
    jobject result = env->NewObjectV(clazz, methodID, argp);
    AbortOnException(env);
    env->DeleteLocalRef(clazz);
    va_end(argp);
    return result;
  }

  // Constructs a new java.lang.String object from an array of characters in
  // standard UTF-8 encoding. This differs from JNIEnv::NewStringUTF() which
  // takes JNI modified UTF-8.
  static jstring NewStringStandardUTFOrAbort(JNIEnv* env, const char* bytes) {
    const jstring charset = env->NewStringUTF("UTF-8");
    AbortOnException(env);
    const jbyteArray byte_array =
        NewByteArrayFromRaw(env, reinterpret_cast<const jbyte*>(bytes),
                            (bytes ? strlen(bytes) : 0U));
    AbortOnException(env);
    jstring result = static_cast<jstring>(
        NewObjectOrAbort(env, "java/lang/String", "([BLjava/lang/String;)V",
                         byte_array, charset));
    env->DeleteLocalRef(byte_array);
    env->DeleteLocalRef(charset);
    return result;
  }

  // Returns a std::string representing the jstring in standard UTF-8 encoding.
  // This differs from JNIEnv::GetStringUTFChars() which returns modified UTF-8.
  // Also, the buffer of the returned bytes is managed by the std::string object
  // so it is not necessary to release it with JNIEnv::ReleaseStringUTFChars().
  static std::string GetStringStandardUTFOrAbort(JNIEnv* env, jstring str) {
    if (str == nullptr) {
      return std::string();
    }
    const jstring charset = env->NewStringUTF("UTF-8");
    AbortOnException(env);
    const jbyteArray byte_array =
        static_cast<jbyteArray>(CallObjectMethodOrAbort(
            env, str, "getBytes", "(Ljava/lang/String;)[B", charset));
    jsize array_length = env->GetArrayLength(byte_array);
    AbortOnException(env);
    void* bytes = env->GetPrimitiveArrayCritical(byte_array, nullptr);
    AbortOnException(env);
    std::string result(static_cast<const char*>(bytes), array_length);
    env->ReleasePrimitiveArrayCritical(byte_array, bytes, JNI_ABORT);
    AbortOnException(env);
    env->DeleteLocalRef(byte_array);
    env->DeleteLocalRef(charset);
    return result;
  }

// Convenience methods to lookup and read a field or call a method all at once:
// Get[Type]FieldOrAbort() takes a jobject of an instance.
// Call[Type]MethodOrAbort() takes a jobject of an instance.
#define X(_jtype, _jname)                                                      \
  static _jtype Get##_jname##FieldOrAbort(JNIEnv* env, jobject obj,            \
                                          const char* name, const char* sig) { \
    _jtype result =                                                            \
        env->Get##_jname##Field(obj, GetFieldIDOrAbort(env, obj, name, sig));  \
    AbortOnException(env);                                                     \
    return result;                                                             \
  }                                                                            \
                                                                               \
  static _jtype GetStatic##_jname##FieldOrAbort(                               \
      JNIEnv* env, const char* class_name, const char* name,                   \
      const char* sig) {                                                       \
    jclass clazz = FindClassExtOrAbort(env, class_name);                       \
    _jtype result = GetStatic##_jname##FieldOrAbort(env, clazz, name, sig);    \
    env->DeleteLocalRef(clazz);                                                \
    return result;                                                             \
  }                                                                            \
                                                                               \
  static _jtype GetStatic##_jname##FieldOrAbort(                               \
      JNIEnv* env, jclass clazz, const char* name, const char* sig) {          \
    _jtype result = env->GetStatic##_jname##Field(                             \
        clazz, GetStaticFieldIDOrAbort(env, clazz, name, sig));                \
    AbortOnException(env);                                                     \
    return result;                                                             \
  }                                                                            \
                                                                               \
  static _jtype Call##_jname##MethodOrAbort(                                   \
      JNIEnv* env, jobject obj, const char* name, const char* sig, ...) {      \
    va_list argp;                                                              \
    va_start(argp, sig);                                                       \
    _jtype result = Call##_jname##MethodVOrAbort(                              \
        env, obj, GetObjectMethodIDOrAbort(env, obj, name, sig), argp);        \
    va_end(argp);                                                              \
    return result;                                                             \
  }                                                                            \
                                                                               \
  static _jtype CallStatic##_jname##MethodOrAbort(                             \
      JNIEnv* env, const char* class_name, const char* method_name,            \
      const char* sig, ...) {                                                  \
    va_list argp;                                                              \
    va_start(argp, sig);                                                       \
    jclass clazz = FindClassExtOrAbort(env, class_name);                       \
    _jtype result = CallStatic##_jname##MethodVOrAbort(                        \
        env, clazz, GetStaticMethodIDOrAbort(env, clazz, method_name, sig),    \
        argp);                                                                 \
    env->DeleteLocalRef(clazz);                                                \
    va_end(argp);                                                              \
    return result;                                                             \
  }                                                                            \
                                                                               \
  static _jtype Call##_jname##MethodVOrAbort(                                  \
      JNIEnv* env, jobject obj, jmethodID methodID, va_list args) {            \
    _jtype result = env->Call##_jname##MethodV(obj, methodID, args);           \
    AbortOnException(env);                                                     \
    return result;                                                             \
  }                                                                            \
                                                                               \
  static _jtype CallStatic##_jname##MethodVOrAbort(                            \
      JNIEnv* env, jclass clazz, jmethodID methodID, va_list args) {           \
    _jtype result = env->CallStatic##_jname##MethodV(clazz, methodID, args);   \
    AbortOnException(env);                                                     \
    return result;                                                             \
  }

  X(jobject, Object)
  X(jboolean, Boolean)
  X(jbyte, Byte)
  X(jchar, Char)
  X(jshort, Short)
  X(jint, Int)
  X(jlong, Long)
  X(jfloat, Float)
  X(jdouble, Double)

#undef X

  static void CallVoidMethod(JNIEnv* env,
                             jobject obj,
                             const char* name,
                             const char* sig,
                             ...) {
    va_list argp;
    va_start(argp, sig);
    env->CallVoidMethodV(obj, GetObjectMethodIDOrAbort(env, obj, name, sig),
                         argp);
    va_end(argp);
  }

  static void CallVoidMethodOrAbort(JNIEnv* env,
                                    jobject obj,
                                    const char* name,
                                    const char* sig,
                                    ...) {
    va_list argp;
    va_start(argp, sig);
    CallVoidMethodVOrAbort(env, obj,
                           GetObjectMethodIDOrAbort(env, obj, name, sig), argp);
    va_end(argp);
  }

  static void CallVoidMethodVOrAbort(JNIEnv* env,
                                     jobject obj,
                                     jmethodID methodID,
                                     va_list args) {
    env->CallVoidMethodV(obj, methodID, args);
    AbortOnException(env);
  }

  static void CallStaticVoidMethod(JNIEnv* env,
                                   const char* class_name,
                                   const char* method_name,
                                   const char* sig,
                                   ...) {
    va_list argp;
    va_start(argp, sig);
    jclass clazz = FindClassExtOrAbort(env, class_name);
    env->CallStaticVoidMethodV(
        clazz, GetStaticMethodIDOrAbort(env, clazz, method_name, sig), argp);
    env->DeleteLocalRef(clazz);
    va_end(argp);
  }

  static void CallStaticVoidMethodOrAbort(JNIEnv* env,
                                          const char* class_name,
                                          const char* method_name,
                                          const char* sig,
                                          ...) {
    va_list argp;
    va_start(argp, sig);
    jclass clazz = FindClassExtOrAbort(env, class_name);
    env->CallStaticVoidMethodV(
        clazz, GetStaticMethodIDOrAbort(env, clazz, method_name, sig), argp);
    AbortOnException(env);
    env->DeleteLocalRef(clazz);
    va_end(argp);
  }

  jstring GetStringFieldOrAbort(JNIEnv* env, jobject obj, const char* name) {
    return static_cast<jstring>(
        GetObjectFieldOrAbort(env, obj, name, "Ljava/lang/String;"));
  }

  jstring GetStaticStringFieldOrAbort(JNIEnv* env,
                                      const char* class_name,
                                      const char* name) {
    return static_cast<jstring>(GetStaticObjectFieldOrAbort(
        env, class_name, name, "Ljava/lang/String;"));
  }

  jstring GetStaticStringFieldOrAbort(JNIEnv* env,
                                      jclass clazz,
                                      const char* name) {
    return static_cast<jstring>(
        GetStaticObjectFieldOrAbort(env, clazz, name, "Ljava/lang/String;"));
  }

// Convenience method to create a j[Type]Array from raw, native data. It is
// the responsibility of clients to free the returned array when done with it
// by manually calling |DeleteLocalRef| on it.
#define X(_jtype, _jname)                                  \
  static _jtype##Array New##_jname##ArrayFromRaw(          \
      JNIEnv* env, const _jtype* data, jsize size) {       \
    SB_CHECK(env);                                         \
    SB_DCHECK(data);                                       \
    SB_DCHECK_GE(size, 0);                                 \
    _jtype##Array j_array = env->New##_jname##Array(size); \
    SB_CHECK(j_array) << "Out of memory making new array"; \
    env->Set##_jname##ArrayRegion(j_array, 0, size, data); \
    return j_array;                                        \
  }

  X(jboolean, Boolean)
  X(jbyte, Byte)
  X(jchar, Char)
  X(jshort, Short)
  X(jint, Int)
  X(jlong, Long)
  X(jfloat, Float)
  X(jdouble, Double)

#undef X

  static jobject ConvertLocalRefToGlobalRef(JNIEnv* env, jobject local) {
    SB_CHECK(env);
    jobject global = env->NewGlobalRef(local);
    env->DeleteLocalRef(local);
    return global;
  }

  static void AbortOnException(JNIEnv* env) {
    SB_CHECK(env);
    if (!env->ExceptionCheck()) {
      return;
    }
    env->ExceptionDescribe();
    SbSystemBreakIntoDebugger();
  }
};

}  // namespace starboard::android::shared

#endif  // STARBOARD_ANDROID_SHARED_JNI_ENV_EXT_H_
