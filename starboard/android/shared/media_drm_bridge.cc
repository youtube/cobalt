// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/android/shared/media_drm_bridge.h"

#include <jni.h>

#include <cstring>
#include <vector>

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/memory/raw_ref.h"
#include "starboard/android/shared/jni_utils.h"
#include "starboard/common/log.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "cobalt/android/jni_headers/MediaDrmBridge_jni.h"

namespace starboard::android::shared {
namespace {

using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF8ToJavaString;
using base::android::ToJavaByteArray;

const char kNoUrl[] = "";

// Using all capital names to be consistent with other Android media statuses.
// They are defined in the same order as in their Java counterparts.  Their
// values should be kept in consistent with their Java counterparts defined in
// android.media.MediaDrm.KeyStatus.
enum class MediaDrmKeyStatus : jint {
  kExpired = 1,
  kInternalError = 4,
  kOutputNotAllowed = 2,
  kPending = 3,
  kUsable = 0,
};

// They must have the same values as defined in MediaDrm.KeyRequest.
enum class RequestType : jint {
  kInitial = 0,
  kRenewal = 1,
  kRelease = 2,
};

SbDrmSessionRequestType ToSbDrmSessionRequestType(RequestType request_type) {
  switch (request_type) {
    case RequestType::kInitial:
      return kSbDrmSessionRequestTypeLicenseRequest;
    case RequestType::kRenewal:
      return kSbDrmSessionRequestTypeLicenseRenewal;
    case RequestType::kRelease:
      return kSbDrmSessionRequestTypeLicenseRelease;
    default:
      SB_NOTREACHED() << "Unknown key request type: "
                      << static_cast<jint>(request_type);
      return kSbDrmSessionRequestTypeLicenseRequest;
  }
}

SbDrmKeyStatus ToSbDrmKeyStatus(MediaDrmKeyStatus status_code) {
  switch (status_code) {
    case MediaDrmKeyStatus::kExpired:
      return kSbDrmKeyStatusExpired;
    case MediaDrmKeyStatus::kInternalError:
      return kSbDrmKeyStatusError;
    case MediaDrmKeyStatus::kOutputNotAllowed:
      return kSbDrmKeyStatusRestricted;
    case MediaDrmKeyStatus::kPending:
      return kSbDrmKeyStatusPending;
    case MediaDrmKeyStatus::kUsable:
      return kSbDrmKeyStatusUsable;
    default:
      SB_NOTREACHED() << "Unknown status=" << static_cast<int>(status_code);
      return kSbDrmKeyStatusError;
  }
}

std::string JavaByteArrayToString(JNIEnv* env,
                                  const JavaRef<jbyteArray>& j_byte_array) {
  std::string out;
  base::android::JavaByteArrayToString(env, j_byte_array, &out);
  return out;
}

std::string JavaByteArrayToString(JNIEnv* env, jbyteArray j_byte_array) {
  return JavaByteArrayToString(
      env, ScopedJavaLocalRef<jbyteArray>(env, j_byte_array));
}

}  // namespace

MediaDrmBridge::MediaDrmBridge(raw_ref<MediaDrmBridge::Host> host,
                               const char* key_system)
    : host_(host) {
  JNIEnv* env = AttachCurrentThread();

  ScopedJavaLocalRef<jstring> j_key_system(
      ConvertUTF8ToJavaString(env, key_system));
  ScopedJavaLocalRef<jobject> j_media_drm_bridge(Java_MediaDrmBridge_create(
      env, j_key_system, reinterpret_cast<jlong>(this)));

  if (j_media_drm_bridge.is_null()) {
    SB_LOG(ERROR) << "Failed to create MediaDrmBridge.";
    return;
  }

  ScopedJavaLocalRef<jobject> j_media_crypto(
      Java_MediaDrmBridge_getMediaCrypto(env, j_media_drm_bridge));

  if (j_media_crypto.is_null()) {
    SB_LOG(ERROR) << "Failed to create MediaCrypto.";
    return;
  }

  j_media_drm_bridge_.Reset(env, j_media_drm_bridge.obj());
  j_media_crypto_.Reset(env, j_media_crypto.obj());

  SB_LOG(INFO) << "Created MediaDrmBridge.";
}

MediaDrmBridge::~MediaDrmBridge() {
  if (!j_media_drm_bridge_.is_null()) {
    Java_MediaDrmBridge_destroy(AttachCurrentThread(), j_media_drm_bridge_);
  }
}

void MediaDrmBridge::CreateSession(int ticket,
                                   const std::vector<const uint8_t>& init_data,
                                   const std::string& mime) const {
  JNIEnv* env = AttachCurrentThread();

  JniIntWrapper j_ticket = static_cast<jint>(ticket);
  ScopedJavaLocalRef<jbyteArray> j_init_data = ScopedJavaLocalRef(
      ToJavaByteArray(env, init_data.data(), init_data.size()));
  ScopedJavaLocalRef<jstring> j_mime =
      ScopedJavaLocalRef(ConvertUTF8ToJavaString(env, mime.c_str()));

  Java_MediaDrmBridge_createSession(env, j_media_drm_bridge_, j_ticket,
                                    j_init_data, j_mime);
}

bool MediaDrmBridge::UpdateSession(int ticket,
                                   const void* key,
                                   int key_size,
                                   const void* session_id,
                                   int session_id_size,
                                   std::string* error_msg) const {
  JNIEnv* env = AttachCurrentThread();

  ScopedJavaLocalRef<jbyteArray> j_session_id(ToJavaByteArray(
      env, static_cast<const uint8_t*>(session_id), session_id_size));
  ScopedJavaLocalRef<jbyteArray> j_response(
      ToJavaByteArray(env, static_cast<const uint8_t*>(key), key_size));

  ScopedJavaLocalRef<jobject> j_update_result(Java_MediaDrmBridge_updateSession(
      env, j_media_drm_bridge_, ticket, j_session_id, j_response));
  *error_msg = ConvertJavaStringToUTF8(
      Java_UpdateSessionResult_getErrorMessage(env, j_update_result));

  return Java_UpdateSessionResult_isSuccess(env, j_update_result) == JNI_TRUE;
}

void MediaDrmBridge::CloseSession(const std::string& session_id) const {
  JNIEnv* env = AttachCurrentThread();

  ScopedJavaLocalRef<jbyteArray> j_session_id =
      ToJavaByteArray(env, reinterpret_cast<const uint8_t*>(session_id.data()),
                      session_id.size());

  Java_MediaDrmBridge_closeSession(env, j_media_drm_bridge_, j_session_id);
}

const void* MediaDrmBridge::GetMetrics(int* size) {
  JNIEnv* env = AttachCurrentThread();

  ScopedJavaLocalRef<jbyteArray> j_metrics =
      Java_MediaDrmBridge_getMetricsInBase64(env, j_media_drm_bridge_);

  if (j_metrics.is_null()) {
    *size = 0;
    return nullptr;
  }

  jbyte* metrics_elements = env->GetByteArrayElements(j_metrics.obj(), nullptr);
  jsize metrics_size = base::android::SafeGetArrayLength(env, j_metrics);
  SB_DCHECK(metrics_elements);

  metrics_.assign(metrics_elements, metrics_elements + metrics_size);

  env->ReleaseByteArrayElements(j_metrics.obj(), metrics_elements, JNI_ABORT);
  *size = static_cast<int>(metrics_.size());

  return metrics_.data();
}

bool MediaDrmBridge::CreateMediaCryptoSession() {
  bool result = Java_MediaDrmBridge_createMediaCryptoSession(
      AttachCurrentThread(), j_media_drm_bridge_);
  if (!result && !j_media_crypto_.is_null()) {
    j_media_crypto_.Reset();
    return false;
  }

  return true;
}

void MediaDrmBridge::OnSessionMessage(
    JNIEnv* env,
    jint ticket,
    const JavaParamRef<jbyteArray>& session_id,
    jint request_type,
    const JavaParamRef<jbyteArray>& message) {
  host_->OnSessionUpdate(
      ticket, ToSbDrmSessionRequestType(static_cast<RequestType>(request_type)),
      JavaByteArrayToString(env, session_id),
      JavaByteArrayToString(env, message), kNoUrl);
}

void MediaDrmBridge::OnKeyStatusChange(
    JNIEnv* env,
    const JavaParamRef<jbyteArray>& session_id,
    const JavaParamRef<jobjectArray>& key_information) {
  std::string session_id_bytes = JavaByteArrayToString(env, session_id);

  // nullptr array indicates key status isn't supported (i.e. Android API < 23)
  jsize length =
      (key_information == nullptr) ? 0 : env->GetArrayLength(key_information);
  std::vector<SbDrmKeyId> drm_key_ids(length);
  std::vector<SbDrmKeyStatus> drm_key_statuses(length);

  static jclass mediaDrmKeyStatusClass =
      env->FindClass("android/media/MediaDrm$KeyStatus");
  SB_DCHECK(mediaDrmKeyStatusClass);

  static jmethodID getKeyIdMethod =
      env->GetMethodID(mediaDrmKeyStatusClass, "getKeyId", "()[B");
  SB_DCHECK(getKeyIdMethod);

  static jmethodID getStatusCodeMethod =
      env->GetMethodID(mediaDrmKeyStatusClass, "getStatusCode", "()I");
  SB_DCHECK(getStatusCodeMethod);

  for (jsize i = 0; i < length; ++i) {
    jobject j_key_status = env->GetObjectArrayElement(key_information, i);
    jbyteArray j_key_id = static_cast<jbyteArray>(
        env->CallObjectMethod(j_key_status, getKeyIdMethod));
    std::string key_id = JavaByteArrayToString(env, j_key_id);

    SB_DCHECK(key_id.size() <= sizeof(drm_key_ids[i].identifier));
    memcpy(drm_key_ids[i].identifier, key_id.data(), key_id.size());
    drm_key_ids[i].identifier_size = key_id.size();

    jint j_status_code = env->CallIntMethod(j_key_status, getStatusCodeMethod);
    drm_key_statuses[i] =
        ToSbDrmKeyStatus(static_cast<MediaDrmKeyStatus>(j_status_code));
  }

  host_->OnKeyStatusChange(session_id_bytes, drm_key_ids, drm_key_statuses);
}

// static
bool MediaDrmBridge::IsWidevineSupported(JNIEnv* env) {
  return Java_MediaDrmBridge_isWidevineCryptoSchemeSupported(env) == JNI_TRUE;
}

// static
bool MediaDrmBridge::IsCbcsSupported(JNIEnv* env) {
  return Java_MediaDrmBridge_isCbcsSchemeSupported(env) == JNI_TRUE;
}

}  // namespace starboard::android::shared
