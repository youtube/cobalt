// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_array.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/logging.h"

namespace base {
namespace android {

ScopedJavaLocalRef<jbyteArray> ToJavaByteArray(
    JNIEnv* env, const uint8* bytes, size_t len) {
  jbyteArray byte_array = env->NewByteArray(len);
  CheckException(env);
  DCHECK(byte_array);

  jbyte* elements = env->GetByteArrayElements(byte_array, NULL);
  memcpy(elements, bytes, len);
  env->ReleaseByteArrayElements(byte_array, elements, 0);
  CheckException(env);

  return ScopedJavaLocalRef<jbyteArray>(env, byte_array);
}

ScopedJavaLocalRef<jobjectArray> ToJavaArrayOfByteArray(
    JNIEnv* env, const std::vector<std::string>& v) {
  ScopedJavaLocalRef<jclass> byte_array_clazz = GetClass(env, "[B");
  jobjectArray joa = env->NewObjectArray(v.size(),
                                         byte_array_clazz.obj(), NULL);
  CheckException(env);

  for (size_t i = 0; i < v.size(); ++i) {
    ScopedJavaLocalRef<jbyteArray> byte_array = ToJavaByteArray(env,
        reinterpret_cast<const uint8*>(v[i].data()), v[i].length());
    env->SetObjectArrayElement(joa, i, byte_array.obj());
  }
  return ScopedJavaLocalRef<jobjectArray>(env, joa);
}

ScopedJavaLocalRef<jobjectArray> ToJavaArrayOfStrings(
    JNIEnv* env, const std::vector<std::string>& v) {
  ScopedJavaLocalRef<jclass> string_clazz = GetClass(env, "java/lang/String");
  jobjectArray joa = env->NewObjectArray(v.size(), string_clazz.obj(), NULL);
  CheckException(env);

  for (size_t i = 0; i < v.size(); ++i) {
    ScopedJavaLocalRef<jstring> item = ConvertUTF8ToJavaString(env, v[i]);
    env->SetObjectArrayElement(joa, i, item.obj());
  }
  return ScopedJavaLocalRef<jobjectArray>(env, joa);
}

ScopedJavaLocalRef<jobjectArray> ToJavaArrayOfStrings(
    JNIEnv* env, const std::vector<string16>& v) {
  ScopedJavaLocalRef<jclass> string_clazz = GetClass(env, "java/lang/String");
  jobjectArray joa = env->NewObjectArray(v.size(), string_clazz.obj(), NULL);
  CheckException(env);

  for (size_t i = 0; i < v.size(); ++i) {
    ScopedJavaLocalRef<jstring> item = ConvertUTF16ToJavaString(env, v[i]);
    env->SetObjectArrayElement(joa, i, item.obj());
  }
  return ScopedJavaLocalRef<jobjectArray>(env, joa);
}

void AppendJavaStringArrayToStringVector(JNIEnv* env,
                                         jobjectArray array,
                                         std::vector<string16>* out) {
  DCHECK(out);
  if (!array)
    return;
  jsize len = env->GetArrayLength(array);
  for (jsize i = 0; i < len; ++i) {
    ScopedJavaLocalRef<jstring> str(env,
        static_cast<jstring>(env->GetObjectArrayElement(array, i)));
    out->push_back(ConvertJavaStringToUTF16(str));
  }
}

void AppendJavaByteArrayToByteVector(JNIEnv* env,
                                     jbyteArray byte_array,
                                     std::vector<uint8>* out) {
  DCHECK(out);
  if (!byte_array)
    return;
  jsize len = env->GetArrayLength(byte_array);
  jbyte* bytes = env->GetByteArrayElements(byte_array, NULL);
  out->insert(out->end(), bytes, bytes + len);
  env->ReleaseByteArrayElements(byte_array, bytes, JNI_ABORT);
}

void JavaByteArrayToByteVector(JNIEnv* env,
                               jbyteArray byte_array,
                               std::vector<uint8>* out) {
  DCHECK(out);
  out->clear();
  AppendJavaByteArrayToByteVector(env, byte_array, out);
}

}  // namespace android
}  // namespace base
