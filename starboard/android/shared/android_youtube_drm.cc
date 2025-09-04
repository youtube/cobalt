// Copyright 2019 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/android/shared/android_youtube_drm.h"

#include "starboard/android/shared/jni_env_ext.h"
#include "starboard/android/shared/jni_utils.h"
#include "starboard/common/log.h"

namespace starboard {
namespace android {
namespace shared {

using starboard::android::shared::JniEnvExt;
using starboard::android::shared::ScopedLocalJavaRef;

namespace {

constexpr size_t kSHA256DigestSize = 32;
constexpr int64_t kYoutubeUuidMostSigBits = 0xB8AD1DF990B54468L;
constexpr int64_t kYoutubeUuidLeastSigBits = 0x92B8F0A9F0A66623L;

struct MethodCache {
  jmethodID uuid_constructor;
  jmethodID mediadrm_constructor;
  jmethodID get_property_byte_array;
  jmethodID open_session;
  jmethodID get_crypto_session;
  jmethodID sign_method;
};

bool InitializeMethodIds(JniEnvExt* env, MethodCache* cache) {
  if (cache->uuid_constructor && cache->mediadrm_constructor) {
    return true;
  }

  ScopedLocalJavaRef<jclass> uuid_class(env->FindClass("java/util/UUID"));
  if (!uuid_class.Get()) {
    SB_DLOG(ERROR) << "Failed to find UUID class";
    return false;
  }

  cache->uuid_constructor =
      env->GetMethodID(uuid_class.Get(), "<init>", "(JJ)V");
  if (!cache->uuid_constructor) {
    SB_DLOG(ERROR) << "Failed to get UUID constructor";
    return false;
  }

  ScopedLocalJavaRef<jclass> mediadrm_class(
      env->FindClass("android/media/MediaDrm"));
  if (!mediadrm_class.Get()) {
    SB_DLOG(ERROR) << "Failed to find MediaDrm class";
    return false;
  }

  cache->mediadrm_constructor =
      env->GetMethodID(mediadrm_class.Get(), "<init>", "(Ljava/util/UUID;)V");
  if (!cache->mediadrm_constructor) {
    SB_DLOG(ERROR) << "Failed to get MediaDrm constructor";
    return false;
  }

  cache->get_property_byte_array = env->GetMethodID(
      mediadrm_class.Get(), "getPropertyByteArray", "(Ljava/lang/String;)[B");
  cache->open_session =
      env->GetMethodID(mediadrm_class.Get(), "openSession", "()[B");
  cache->get_crypto_session =
      env->GetMethodID(mediadrm_class.Get(), "getCryptoSession",
                       "([BLjava/lang/String;Ljava/lang/String;)Landroid/media/"
                       "MediaDrm$CryptoSession;");

  ScopedLocalJavaRef<jclass> crypto_session_class(
      env->FindClass("android/media/MediaDrm$CryptoSession"));
  cache->sign_method =
      env->GetMethodID(crypto_session_class.Get(), "sign", "([B[B)[B");

  return cache->get_property_byte_array && cache->open_session &&
         cache->get_crypto_session && cache->sign_method;
}

ScopedLocalJavaRef<jobject> CreateYoutubeUuid(JniEnvExt* env,
                                              MethodCache* cache) {
  if (!InitializeMethodIds(env, cache)) {
    return ScopedLocalJavaRef<jobject>();
  }

  ScopedLocalJavaRef<jclass> uuid_class(env->FindClass("java/util/UUID"));
  jobject uuid =
      env->NewObject(uuid_class.Get(), cache->uuid_constructor,
                     kYoutubeUuidMostSigBits, kYoutubeUuidLeastSigBits);
  if (env->ExceptionCheck()) {
    env->ExceptionClear();
    SB_DLOG(ERROR) << "Failed to create youtube sign UUID";
    return ScopedLocalJavaRef<jobject>();
  }

  return ScopedLocalJavaRef<jobject>(uuid);
}

ScopedLocalJavaRef<jobject> CreateMediaDrm(JniEnvExt* env,
                                           MethodCache* cache,
                                           jobject uuid) {
  if (!InitializeMethodIds(env, cache)) {
    return ScopedLocalJavaRef<jobject>();
  }

  ScopedLocalJavaRef<jclass> mediadrm_class(
      env->FindClass("android/media/MediaDrm"));
  jobject mediadrm =
      env->NewObject(mediadrm_class.Get(), cache->mediadrm_constructor, uuid);
  if (env->ExceptionCheck()) {
    env->ExceptionClear();
    SB_DLOG(INFO) << "Failed to create MediaDrm";
    return ScopedLocalJavaRef<jobject>();
  }

  return ScopedLocalJavaRef<jobject>(mediadrm);
}

ScopedLocalJavaRef<jbyteArray> GetKeySetId(JniEnvExt* env,
                                           MethodCache* cache,
                                           jobject mediadrm) {
  ScopedLocalJavaRef<jstring> property_name(
      env->NewStringUTF("youtube_KdSetId"));
  if (!property_name.Get()) {
    return ScopedLocalJavaRef<jbyteArray>();
  }

  jobject result = env->CallObjectMethod(
      mediadrm, cache->get_property_byte_array, property_name.Get());
  if (env->ExceptionCheck()) {
    env->ExceptionClear();
    SB_DLOG(ERROR) << "Exception in youtube sign getPropertyByteArray";
    return ScopedLocalJavaRef<jbyteArray>();
  }

  return ScopedLocalJavaRef<jbyteArray>(static_cast<jbyteArray>(result));
}

ScopedLocalJavaRef<jbyteArray> OpenDrmSession(JniEnvExt* env,
                                              MethodCache* cache,
                                              jobject mediadrm) {
  jobject result = env->CallObjectMethod(mediadrm, cache->open_session);
  if (env->ExceptionCheck()) {
    env->ExceptionClear();
    SB_DLOG(ERROR) << "Exception in youtube sign openSession";
    return ScopedLocalJavaRef<jbyteArray>();
  }

  return ScopedLocalJavaRef<jbyteArray>(static_cast<jbyteArray>(result));
}

ScopedLocalJavaRef<jobject> CreateCryptoSession(JniEnvExt* env,
                                                MethodCache* cache,
                                                jobject mediadrm,
                                                jbyteArray session_id) {
  ScopedLocalJavaRef<jstring> cipher(env->NewStringUTF("AES/CBC/PKCS5Padding"));
  ScopedLocalJavaRef<jstring> mac(env->NewStringUTF("HmacSHA256"));
  if (!cipher.Get() || !mac.Get()) {
    return ScopedLocalJavaRef<jobject>();
  }

  jobject result = env->CallObjectMethod(mediadrm, cache->get_crypto_session,
                                         session_id, cipher.Get(), mac.Get());
  if (env->ExceptionCheck()) {
    env->ExceptionClear();
    SB_DLOG(ERROR) << "Exception in youtube sign getCryptoSession";
    return ScopedLocalJavaRef<jobject>();
  }

  return ScopedLocalJavaRef<jobject>(result);
}

ScopedLocalJavaRef<jbyteArray> SignMessage(JniEnvExt* env,
                                           MethodCache* cache,
                                           jobject crypto_session,
                                           jbyteArray key_id,
                                           jbyteArray message) {
  jobject result = env->CallObjectMethod(crypto_session, cache->sign_method,
                                         key_id, message);
  if (env->ExceptionCheck()) {
    env->ExceptionClear();
    SB_DLOG(ERROR) << "Exception in sign";
    return ScopedLocalJavaRef<jbyteArray>();
  }

  return ScopedLocalJavaRef<jbyteArray>(static_cast<jbyteArray>(result));
}

}  // namespace

bool YbMediaDrmSignMessage(const uint8_t* message,
                           size_t message_size_in_bytes,
                           uint8_t* digest,
                           size_t digest_size_in_bytes) {
  if (!message || message_size_in_bytes <= 0 || !digest ||
      digest_size_in_bytes < kSHA256DigestSize) {
    SB_DLOG(ERROR) << "Invalid parameters";
    return false;
  }

  JniEnvExt* env = JniEnvExt::Get();
  if (!env) {
    SB_DLOG(ERROR) << "Failed to get JNI env";
    return false;
  }

  MethodCache cache = {0};
  ScopedLocalJavaRef<jobject> uuid = CreateYoutubeUuid(env, &cache);
  if (!uuid.Get()) {
    SB_DLOG(INFO) << "Device not support youtube mediadrm service";
    return false;
  }

  ScopedLocalJavaRef<jobject> mediadrm =
      CreateMediaDrm(env, &cache, uuid.Get());
  if (!mediadrm.Get()) {
    SB_DLOG(INFO) << "Create youtube sign drm plugin failed";
    return false;
  }

  ScopedLocalJavaRef<jbyteArray> session_id =
      OpenDrmSession(env, &cache, mediadrm.Get());
  if (!session_id.Get() || env->GetArrayLength(session_id.Get()) <= 0) {
    SB_DLOG(ERROR) << "Open youtube sign session failed";
    return false;
  }

  ScopedLocalJavaRef<jbyteArray> key_id =
      GetKeySetId(env, &cache, mediadrm.Get());
  if (!key_id.Get() || env->GetArrayLength(key_id.Get()) <= 0) {
    SB_DLOG(ERROR) << "Acquire youtube sign key set id failed";
    return false;
  }

  ScopedLocalJavaRef<jobject> crypto_session =
      CreateCryptoSession(env, &cache, mediadrm.Get(), session_id.Get());
  if (!crypto_session.Get()) {
    SB_DLOG(ERROR) << "Create youtube sign crypto session failed";
    return false;
  }

  ScopedLocalJavaRef<jbyteArray> message_array(
      env->NewByteArray(message_size_in_bytes));
  if (!message_array.Get()) {
    SB_DLOG(ERROR) << "Failed to create message array";
    return false;
  }

  env->SetByteArrayRegion(message_array.Get(), 0, message_size_in_bytes,
                          reinterpret_cast<const jbyte*>(message));
  if (env->ExceptionCheck()) {
    env->ExceptionClear();
    SB_DLOG(ERROR) << "Failed to set byte array region";
    return false;
  }

  ScopedLocalJavaRef<jbyteArray> signature = SignMessage(
      env, &cache, crypto_session.Get(), key_id.Get(), message_array.Get());
  if (!signature.Get()) {
    SB_DLOG(ERROR) << "Youtube sign SignMessage failed";
    return false;
  }

  jsize signature_length = env->GetArrayLength(signature.Get());
  if (signature_length <= 0 || signature_length > digest_size_in_bytes) {
    SB_DLOG(ERROR) << "Invalid signature length";
    return false;
  }

  env->GetByteArrayRegion(signature.Get(), 0, signature_length,
                          reinterpret_cast<jbyte*>(digest));
  if (env->ExceptionCheck()) {
    env->ExceptionClear();
    SB_DLOG(ERROR) << "Failed to get byte array region";
    return false;
  }

  return true;
}

}  // namespace shared
}  // namespace android
}  // namespace starboard
