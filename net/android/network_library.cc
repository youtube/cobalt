// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/android/network_library.h"

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/logging.h"
#include "jni/AndroidNetworkLibrary_jni.h"

using base::android::AttachCurrentThread;
using base::android::ClearException;
using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF8ToJavaString;
using base::android::GetApplicationContext;
using base::android::ScopedJavaLocalRef;
using base::android::ToJavaArrayOfByteArray;
using base::android::ToJavaByteArray;

namespace net {
namespace android {

VerifyResult VerifyX509CertChain(const std::vector<std::string>& cert_chain,
                                 const std::string& auth_type) {
  JNIEnv* env = AttachCurrentThread();
  if (!env || !g_AndroidNetworkLibrary_verifyServerCertificates) {
    // TODO(bulach): Remove when we initialize the JVM in unit tests.
    LOG(WARNING) << "JNI initialization failed";
    return VERIFY_INVOCATION_ERROR;
  }

  ScopedJavaLocalRef<jobjectArray> chain_byte_array =
      ToJavaArrayOfByteArray(env, cert_chain);
  DCHECK(!chain_byte_array.is_null());

  ScopedJavaLocalRef<jstring> auth_string =
      ConvertUTF8ToJavaString(env, auth_type);
  DCHECK(!auth_string.is_null());

  jboolean trusted = Java_AndroidNetworkLibrary_verifyServerCertificates(
      env, chain_byte_array.obj(), auth_string.obj());
  if (ClearException(env))
    return VERIFY_INVOCATION_ERROR;

  return trusted ? VERIFY_OK : VERIFY_NO_TRUSTED_ROOT;
}

bool StoreKeyPair(const uint8* public_key,
                  size_t public_len,
                  const uint8* private_key,
                  size_t private_len) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jbyteArray> public_array =
      ToJavaByteArray(env, public_key, public_len);
  ScopedJavaLocalRef<jbyteArray> private_array =
      ToJavaByteArray(env, private_key, private_len);
  jboolean ret = Java_AndroidNetworkLibrary_storeKeyPair(env,
      GetApplicationContext(), public_array.obj(), private_array.obj());
  LOG_IF(WARNING, !ret) <<
      "Call to Java_AndroidNetworkLibrary_storeKeyPair failed";
  return ret;
}

bool HaveOnlyLoopbackAddresses() {
  JNIEnv* env = AttachCurrentThread();
  return Java_AndroidNetworkLibrary_haveOnlyLoopbackAddresses(env);
}

std::string GetNetworkList() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jstring> ret =
      Java_AndroidNetworkLibrary_getNetworkList(env);
  return ConvertJavaStringToUTF8(ret);
}

bool GetMimeTypeFromExtension(const std::string& extension,
                              std::string* result) {
  JNIEnv* env = AttachCurrentThread();

  ScopedJavaLocalRef<jstring> extension_string =
      ConvertUTF8ToJavaString(env, extension);
  ScopedJavaLocalRef<jstring> ret =
      Java_AndroidNetworkLibrary_getMimeTypeFromExtension(
          env, extension_string.obj());

  if (!ret.obj()) {
    LOG(WARNING) << "Call to getMimeTypeFromExtension failed for extension: "
        << extension;
    return false;
  }
  *result = ConvertJavaStringToUTF8(ret);
  return true;
}

bool RegisterNetworkLibrary(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace android
}  // namespace net
