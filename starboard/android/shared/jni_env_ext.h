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

#ifndef STARBOARD_ANDROID_SHARED_JNI_ENV_EXT_H_
#define STARBOARD_ANDROID_SHARED_JNI_ENV_EXT_H_

#include <android/native_activity.h>
#include <jni.h>

#include <cstdarg>

#include "starboard/log.h"
#include "starboard/memory.h"

namespace starboard {
namespace android {
namespace shared {

// An extension to JNIEnv to simplify making JNI calls.
//
// Call the static Get() method to get an instance that is already attached to
// the JVM in the current thread.
//
// This extends the JNIEnv structure, which already has a C++ interface for
// calling JNI methods, so any JNIEnv method can be called directly on this.
//
// There are convenience methods to lookup and call Java methods on object
// instances in a single step, with even simpler methods to call Java methods on
// the Activity (the CobaltActivity).
struct JniEnvExt : public JNIEnv {
  // One-time initialization to be called before starting the application.
  static void Initialize(ANativeActivity* native_activity);

  // Returns the thread-specific instance of JniEnvExt.
  static JniEnvExt* Get();

  // Returns the CobaltActivity object.
  jobject GetActivityObject();

  // Lookup the class of an object and find a method in it.
  jmethodID GetObjectMethodID(jobject obj, const char* name, const char* sig) {
    jclass clazz = GetObjectClass(obj);
    jmethodID method_id = GetMethodID(clazz, name, sig);
    DeleteLocalRef(clazz);
    return method_id;
  }

  // Find a class by name using the Activity's class loader. This can load
  // both system classes and application classes, even when not in a JNI
  // stack frame (e.g. in a native thread that was attached the the JVM).
  // https://developer.android.com/training/articles/perf-jni.html#faq_FindClass
  jclass FindClassExt(const char* name);

  // Convienience method to lookup and call a constructor.
  jobject NewObject(const char* class_name, const char* sig, ...) {
    va_list argp;
    va_start(argp, sig);
    jclass clazz = FindClassExt(class_name);
    jmethodID methodID = GetMethodID(clazz, "<init>", sig);
    jobject result = NewObjectV(clazz, methodID, argp);
    DeleteLocalRef(clazz);
    va_end(argp);
    return result;
  }

// Convienience methods to lookup and call a method all at once:
// Call[Type]Method() overloaded to take a jobject of an instance.
// CallActivity[Type]Method() to call methods on the CobaltActivity.
#define X(_jtype, _jname)                                                      \
  _jtype Call##_jname##Method(jobject obj, const char* name, const char* sig,  \
                              ...) {                                           \
    va_list argp;                                                              \
    va_start(argp, sig);                                                       \
    _jtype result =                                                            \
        Call##_jname##MethodV(obj, GetObjectMethodID(obj, name, sig), argp);   \
    va_end(argp);                                                              \
    return result;                                                             \
  }                                                                            \
                                                                               \
  _jtype CallActivity##_jname##Method(const char* name, const char* sig,       \
                                      ...) {                                   \
    va_list argp;                                                              \
    va_start(argp, sig);                                                       \
    jobject obj = GetActivityObject();                                         \
    _jtype result =                                                            \
        Call##_jname##MethodV(obj, GetObjectMethodID(obj, name, sig), argp);   \
    va_end(argp);                                                              \
    return result;                                                             \
  }                                                                            \
                                                                               \
  _jtype CallStatic##_jname##Method(                                           \
      const char* class_name, const char* method_name, const char* sig, ...) { \
    va_list argp;                                                              \
    va_start(argp, sig);                                                       \
    jclass clazz = FindClassExt(class_name);                                   \
    _jtype result = CallStatic##_jname##MethodV(                               \
        clazz, GetStaticMethodID(clazz, method_name, sig), argp);              \
    DeleteLocalRef(clazz);                                                     \
    va_end(argp);                                                              \
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

  void CallVoidMethod(jobject obj, const char* name, const char* sig, ...) {
    va_list argp;
    va_start(argp, sig);
    CallVoidMethodV(obj, GetObjectMethodID(obj, name, sig), argp);
    va_end(argp);
  }

  void CallActivityVoidMethod(const char* name, const char* sig, ...) {
    va_list argp;
    va_start(argp, sig);
    jobject obj = GetActivityObject();
    CallVoidMethodV(obj, GetObjectMethodID(obj, name, sig), argp);
    va_end(argp);
  }

  void CallStaticVoidMethod(const char* class_name,
                            const char* method_name,
                            const char* sig,
                            ...) {
    va_list argp;
    va_start(argp, sig);
    jclass clazz = FindClassExt(class_name);
    CallStaticVoidMethodV(clazz, GetStaticMethodID(clazz, method_name, sig),
                          argp);
    DeleteLocalRef(clazz);
    va_end(argp);
  }

  jobject ConvertLocalRefToGlobalRef(jobject local) {
    jobject global = NewGlobalRef(local);
    DeleteLocalRef(local);
    return global;
  }
};

SB_COMPILE_ASSERT(sizeof(JNIEnv) == sizeof(JniEnvExt),
                  JniEnvExt_must_not_add_fields);

// Wrapper class to manage the lifetime of a local reference to a |jobject|.
// This is necessary for local references to |jobject|s that are obtained in
// native code that was not called into from Java, since they will otherwise
// not be cleaned up.
class ScopedLocalJavaRef {
 public:
  explicit ScopedLocalJavaRef(jobject j_object = NULL) : j_object_(j_object) {}
  ~ScopedLocalJavaRef() {
    if (j_object_) {
      JniEnvExt::Get()->DeleteLocalRef(j_object_);
      j_object_ = NULL;
    }
  }
  jobject Get() const { return j_object_; }
  operator bool() const { return j_object_; }

 private:
  jobject j_object_;

  SB_DISALLOW_COPY_AND_ASSIGN(ScopedLocalJavaRef);
};

// Convenience class to manage the lifetime of a local Java ByteBuffer
// reference, and provide accessors to its properties.
class ScopedJavaByteBuffer {
 public:
  explicit ScopedJavaByteBuffer(jobject j_byte_buffer)
      : j_byte_buffer_(j_byte_buffer) {}
  void* address() const {
    return JniEnvExt::Get()->GetDirectBufferAddress(j_byte_buffer_.Get());
  }
  jint capacity() const {
    return JniEnvExt::Get()->GetDirectBufferCapacity(j_byte_buffer_.Get());
  }
  bool IsNull() const { return !j_byte_buffer_ || !address(); }
  void CopyInto(const void* source, jint count) {
    SB_DCHECK(!IsNull());
    SB_DCHECK(count >= 0 && count <= capacity());
    SbMemoryCopy(address(), source, count);
  }

 private:
  ScopedLocalJavaRef j_byte_buffer_;

  SB_DISALLOW_COPY_AND_ASSIGN(ScopedJavaByteBuffer);
};

}  // namespace shared
}  // namespace android
}  // namespace starboard

#endif  // STARBOARD_ANDROID_SHARED_JNI_ENV_EXT_H_
