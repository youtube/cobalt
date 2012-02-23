// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/android/network_library.h"

#include "base/android/auto_jobject.h"
#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/logging.h"
#include "jni/android_network_library_jni.h"

using base::android::AttachCurrentThread;
using base::android::AutoJObject;
using base::android::CheckException;
using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF8ToJavaString;
using base::android::GetApplicationContext;
using base::android::ToJavaArrayOfByteArray;
using base::android::ToJavaByteArray;

namespace net {
namespace android {

VerifyResult VerifyX509CertChain(const std::vector<std::string>& cert_chain,
                                 const std::string& hostname,
                                 const std::string& auth_type) {
  JNIEnv* env = AttachCurrentThread();
  if (!env) {
    // TODO(husky): Maybe initialize the JVM in unit tests?
    LOG(WARNING) << "JNI initialization failed";
    return VERIFY_INVOCATION_ERROR;
  }

  AutoJObject chain_byte_array = AutoJObject::FromLocalRef(
      env, ToJavaArrayOfByteArray(env, cert_chain));
  DCHECK(chain_byte_array.obj());

  AutoJObject host_string = AutoJObject::FromLocalRef(
      env, ConvertUTF8ToJavaString(env, hostname));
  DCHECK(host_string.obj());

  AutoJObject auth_string = AutoJObject::FromLocalRef(
      env, ConvertUTF8ToJavaString(env, auth_type));
  DCHECK(auth_string.obj());

  jint error = Java_AndroidNetworkLibrary_verifyServerCertificates(
      env, static_cast<jobjectArray>(chain_byte_array.obj()),
      static_cast<jstring>(host_string.obj()),
      static_cast<jstring>(auth_string.obj()));

  switch (error) {
    case 0:
      return VERIFY_OK;
    case 1:
      return VERIFY_BAD_HOSTNAME;
    case 2:
      return VERIFY_NO_TRUSTED_ROOT;
  }
  return VERIFY_INVOCATION_ERROR;
}

bool StoreKeyPair(const uint8* public_key,
                  size_t public_len,
                  const uint8* private_key,
                  size_t private_len) {
  JNIEnv* env = AttachCurrentThread();
  AutoJObject public_array = AutoJObject::FromLocalRef(
      env, ToJavaByteArray(env, public_key, public_len));
  AutoJObject private_array = AutoJObject::FromLocalRef(
      env, ToJavaByteArray(env, private_key, private_len));
  jboolean ret = Java_AndroidNetworkLibrary_storeKeyPair(env,
      GetApplicationContext(),
      static_cast<jbyteArray>(public_array.obj()),
      static_cast<jbyteArray>(private_array.obj()));
  if (CheckException(env) || !ret) {
    LOG(WARNING) << "Call to Java_AndroidNetworkLibrary_storeKeyPair failed";
    return false;
  }
  return true;
}

bool GetMimeTypeFromExtension(const std::string& extension,
                              std::string* result) {
  JNIEnv* env = AttachCurrentThread();

  AutoJObject extension_string = AutoJObject::FromLocalRef(
      env, ConvertUTF8ToJavaString(env, extension));
  AutoJObject ret = AutoJObject::FromLocalRef(
      env, Java_AndroidNetworkLibrary_getMimeTypeFromExtension(
              env, static_cast<jstring>(extension_string.obj())));

  if (CheckException(env) || !ret.obj()) {
    LOG(WARNING) << "Call to getMimeTypeFromExtension failed";
    return false;
  }
  *result = ConvertJavaStringToUTF8(env, static_cast<jstring>(ret.obj()));
  return true;
}

bool RegisterNetworkLibrary(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace android
}  // namespace net


