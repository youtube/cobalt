// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "url/origin.h"

#include <cstdint>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/memory/ptr_util.h"
#include "url/android/gurl_android.h"
#include "url/url_jni_headers/Origin_jni.h"

namespace url {

// friend
Origin CreateOpaqueOriginForAndroid(const std::string& scheme,
                                    const std::string& host,
                                    uint16_t port,
                                    const base::UnguessableToken& nonce_token) {
  return Origin::CreateOpaqueFromNormalizedPrecursorTuple(
      scheme, host, port, Origin::Nonce(nonce_token));
}

base::android::ScopedJavaLocalRef<jobject> Origin::ToJavaObject() const {
  JNIEnv* env = base::android::AttachCurrentThread();
  const base::UnguessableToken* token = GetNonceForSerialization();
  return Java_Origin_Constructor(
      env, base::android::ConvertUTF8ToJavaString(env, tuple_.scheme()),
      base::android::ConvertUTF8ToJavaString(env, tuple_.host()), tuple_.port(),
      opaque(), token ? token->GetHighForSerialization() : 0,
      token ? token->GetLowForSerialization() : 0);
}

// static
Origin Origin::FromJavaObject(
    const base::android::JavaRef<jobject>& java_origin) {
  JNIEnv* env = base::android::AttachCurrentThread();
  std::unique_ptr<Origin> origin = base::WrapUnique<Origin>(
      reinterpret_cast<Origin*>(Java_Origin_toNativeOrigin(env, java_origin)));
  return std::move(*origin);
}

static base::android::ScopedJavaLocalRef<jobject> JNI_Origin_CreateOpaque(
    JNIEnv* env) {
  return Origin().ToJavaObject();
}

static base::android::ScopedJavaLocalRef<jobject> JNI_Origin_CreateFromGURL(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_gurl) {
  return Origin::Create(*GURLAndroid::ToNativeGURL(env, j_gurl)).ToJavaObject();
}

static jlong JNI_Origin_CreateNative(
    JNIEnv* env,
    const base::android::JavaParamRef<jstring>& java_scheme,
    const base::android::JavaParamRef<jstring>& java_host,
    jshort port,
    jboolean is_opaque,
    jlong token_high_bits,
    jlong token_low_bits) {
  const std::string& scheme = ConvertJavaStringToUTF8(env, java_scheme);
  const std::string& host = ConvertJavaStringToUTF8(env, java_host);

  Origin origin;
  if (is_opaque) {
    absl::optional<base::UnguessableToken> nonce_token =
        base::UnguessableToken::Deserialize(token_high_bits, token_low_bits);
    origin =
        CreateOpaqueOriginForAndroid(scheme, host, port, nonce_token.value());
  } else {
    origin = Origin::CreateFromNormalizedTuple(scheme, host, port);
  }
  return reinterpret_cast<intptr_t>(new Origin(origin));
}

}  // namespace url
