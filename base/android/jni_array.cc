// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_array.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/logging.h"

namespace base {
namespace android {

jbyteArray ToJavaByteArray(JNIEnv* env,
                           const unsigned char* bytes,
                           size_t len) {
  jbyteArray byte_array = env->NewByteArray(len);
  CheckException(env);
  CHECK(byte_array);

  jbyte* elements = env->GetByteArrayElements(byte_array, NULL);
  memcpy(elements, bytes, len);
  env->ReleaseByteArrayElements(byte_array, elements, 0);
  CheckException(env);

  return byte_array;
}

jobjectArray ToJavaArrayOfByteArray(JNIEnv* env,
                                    const std::vector<std::string>& v) {
  ScopedJavaLocalRef<jclass> byte_array_clazz(env, env->FindClass("[B"));
  jobjectArray joa = env->NewObjectArray(v.size(),
                                         byte_array_clazz.obj(), NULL);
  CheckException(env);

  for (size_t i = 0; i < v.size(); ++i) {
    ScopedJavaLocalRef<jobject> byte_array(env, ToJavaByteArray(env,
            reinterpret_cast<const uint8*>(v[i].data()), v[i].length()));
    env->SetObjectArrayElement(joa, i, byte_array.obj());
  }
  return joa;
}

jobjectArray ToJavaArrayOfStrings(JNIEnv* env,
                                  const std::vector<std::string>& v) {
  ScopedJavaLocalRef<jclass> string_clazz(env,
                                          env->FindClass("java/lang/String"));
  jobjectArray joa = env->NewObjectArray(v.size(), string_clazz.obj(), NULL);
  CheckException(env);

  for (size_t i = 0; i < v.size(); ++i) {
    ScopedJavaLocalRef<jstring> item(env,
                                     ConvertUTF8ToJavaString(env, v[i]));
    env->SetObjectArrayElement(joa, i, item.obj());
  }
  return joa;
}

}  // namespace android
}  // namespace base
