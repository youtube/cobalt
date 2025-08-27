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

// One-time initialization to be called before starting the application.
void JniInitialize(JNIEnv* env, jobject starboard_bridge);

// Called right before each native thread is about to be shutdown.
void JniOnThreadShutdown();

void JniAbortOnException(JNIEnv* env);

// Lookup the class of an object and find a field in it.
jfieldID JniGetStaticFieldIDOrAbort(JNIEnv* env,
                                    jclass clazz,
                                    const char* name,
                                    const char* sig);

jfieldID JniGetFieldIDOrAbort(JNIEnv* env,
                              jobject obj,
                              const char* name,
                              const char* sig);

jint JniGetEnumValueOrAbort(JNIEnv* env, jclass clazz, const char* name);

// Lookup the class of an object and find a method in it.
jmethodID JniGetObjectMethodIDOrAbort(JNIEnv* env,
                                      jobject obj,
                                      const char* name,
                                      const char* sig);

jmethodID JniGetStaticMethodIDOrAbort(JNIEnv* env,
                                      jclass clazz,
                                      const char* name,
                                      const char* sig);

jobject JniGetObjectArrayElementOrAbort(JNIEnv* env,
                                        jobjectArray array,
                                        jsize index);

// Find a class by name using the application's class loader. This can load
// both system classes and application classes, even when not in a JNI
// stack frame (e.g. in a native thread that was attached the the JVM).
// https://developer.android.com/training/articles/perf-jni.html#faq_FindClass
jclass JniFindClassExtOrAbort(JNIEnv* env, const char* name);

jclass JniFindClassOrAbort(JNIEnv* env, const char* name);

// Convenience method to lookup and call a constructor.
jobject JniNewObjectOrAbort(JNIEnv* env,
                            const char* class_name,
                            const char* sig,
                            ...);

// Constructs a new java.lang.String object from an array of characters in
// standard UTF-8 encoding. This differs from JNIEnv::NewStringUTF() which
// takes JNI modified UTF-8.
jstring JniNewStringStandardUTFOrAbort(JNIEnv* env, const char* bytes);

// Returns a std::string representing the jstring in standard UTF-8 encoding.
// This differs from JNIEnv::GetStringUTFChars() which returns modified UTF-8.
// Also, the buffer of the returned bytes is managed by the std::string object
// so it is not necessary to release it with JNIEnv::ReleaseStringUTFChars().
std::string JniGetStringStandardUTFOrAbort(JNIEnv* env, jstring str);

// Convenience methods to lookup and read a field or call a method all at once:
// JniGet[Type]FieldOrAbort() takes a jobject of an instance.
// JniCall[Type]MethodOrAbort() takes a jobject of an instance.
#define X(_jtype, _jname)                                                      \
  _jtype JniGet##_jname##FieldOrAbort(JNIEnv* env, jobject obj,                \
                                      const char* name, const char* sig);      \
                                                                               \
  _jtype JniGetStatic##_jname##FieldOrAbort(                                   \
      JNIEnv* env, const char* class_name, const char* name, const char* sig); \
                                                                               \
  _jtype JniGetStatic##_jname##FieldOrAbort(                                   \
      JNIEnv* env, jclass clazz, const char* name, const char* sig);           \
                                                                               \
  _jtype JniCall##_jname##MethodOrAbort(                                       \
      JNIEnv* env, jobject obj, const char* name, const char* sig, ...);       \
                                                                               \
  _jtype JniCallStatic##_jname##MethodOrAbort(                                 \
      JNIEnv* env, const char* class_name, const char* method_name,            \
      const char* sig, ...);                                                   \
                                                                               \
  _jtype JniCall##_jname##MethodVOrAbort(JNIEnv* env, jobject obj,             \
                                         jmethodID methodID, va_list args);    \
                                                                               \
  _jtype JniCallStatic##_jname##MethodVOrAbort(                                \
      JNIEnv* env, jclass clazz, jmethodID methodID, va_list args);

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

void JniCallVoidMethod(JNIEnv* env,
                       jobject obj,
                       const char* name,
                       const char* sig,
                       ...);

void JniCallVoidMethodOrAbort(JNIEnv* env,
                              jobject obj,
                              const char* name,
                              const char* sig,
                              ...);

void JniCallVoidMethodVOrAbort(JNIEnv* env,
                               jobject obj,
                               jmethodID methodID,
                               va_list args);

void JniCallStaticVoidMethod(JNIEnv* env,
                             const char* class_name,
                             const char* method_name,
                             const char* sig,
                             ...);

void JniCallStaticVoidMethodOrAbort(JNIEnv* env,
                                    const char* class_name,
                                    const char* method_name,
                                    const char* sig,
                                    ...);

jstring JniGetStringFieldOrAbort(JNIEnv* env, jobject obj, const char* name);

jstring JniGetStaticStringFieldOrAbort(JNIEnv* env,
                                       const char* class_name,
                                       const char* name);

jstring JniGetStaticStringFieldOrAbort(JNIEnv* env,
                                       jclass clazz,
                                       const char* name);

// Convenience method to create a j[Type]Array from raw, native data. It is
// the responsibility of clients to free the returned array when done with it
// by manually calling |DeleteLocalRef| on it.
#define X(_jtype, _jname)                                                     \
  _jtype##Array JniNew##_jname##ArrayFromRaw(JNIEnv* env, const _jtype* data, \
                                             jsize size);

X(jboolean, Boolean)
X(jbyte, Byte)
X(jchar, Char)
X(jshort, Short)
X(jint, Int)
X(jlong, Long)
X(jfloat, Float)
X(jdouble, Double)

#undef X

jobject JniConvertLocalRefToGlobalRef(JNIEnv* env, jobject local);

}  // namespace starboard::android::shared

#endif  // STARBOARD_ANDROID_SHARED_JNI_ENV_EXT_H_
