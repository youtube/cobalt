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
constexpr jint MEDIA_DRM_KEY_STATUS_EXPIRED = 1;
constexpr jint MEDIA_DRM_KEY_STATUS_INTERNAL_ERROR = 4;
constexpr jint MEDIA_DRM_KEY_STATUS_OUTPUT_NOT_ALLOWED = 2;
constexpr jint MEDIA_DRM_KEY_STATUS_PENDING = 3;
constexpr jint MEDIA_DRM_KEY_STATUS_USABLE = 0;

// They must have the same values as defined in MediaDrm.KeyRequest.
constexpr jint REQUEST_TYPE_INITIAL = 0;
constexpr jint REQUEST_TYPE_RENEWAL = 1;
constexpr jint REQUEST_TYPE_RELEASE = 2;

SbDrmSessionRequestType SbDrmSessionRequestTypeFromMediaDrmKeyRequestType(
    jint request_type) {
  if (request_type == REQUEST_TYPE_INITIAL) {
    return kSbDrmSessionRequestTypeLicenseRequest;
  }
  if (request_type == REQUEST_TYPE_RENEWAL) {
    return kSbDrmSessionRequestTypeLicenseRenewal;
  }
  if (request_type == REQUEST_TYPE_RELEASE) {
    return kSbDrmSessionRequestTypeLicenseRelease;
  }
  SB_NOTREACHED();
  return kSbDrmSessionRequestTypeLicenseRequest;
}

}  // namespace

MediaDrmBridge::MediaDrmBridge(MediaDrmBridge::Host* host,
                               const char* key_system)
    : host_(host) {
  SB_DCHECK(host_);
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
  JniIntWrapper j_ticket = JniIntWrapper(ticket);

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

void MediaDrmBridge::OnSessionMessage(JNIEnv* env,
                                      jint ticket,
                                      const JavaParamRef<jbyteArray>& sessionId,
                                      jint requestType,
                                      const JavaParamRef<jbyteArray>& message) {
  jbyte* session_id_elements = env->GetByteArrayElements(sessionId, nullptr);
  jsize session_id_size = env->GetArrayLength(sessionId);

  jbyte* message_elements = env->GetByteArrayElements(message, nullptr);
  jsize message_size = env->GetArrayLength(message);

  SB_DCHECK(session_id_elements);
  SB_DCHECK(message_elements);

  host_->CallUpdateRequestCallback(
      ticket, SbDrmSessionRequestTypeFromMediaDrmKeyRequestType(requestType),
      session_id_elements, session_id_size, message_elements, message_size,
      kNoUrl);
  env->ReleaseByteArrayElements(sessionId, session_id_elements, JNI_ABORT);
  env->ReleaseByteArrayElements(message, message_elements, JNI_ABORT);
}

void MediaDrmBridge::OnKeyStatusChange(
    JNIEnv* env,
    const JavaParamRef<jbyteArray>& sessionId,
    const JavaParamRef<jobjectArray>& keyInformation) {
  jbyte* session_id_elements = env->GetByteArrayElements(sessionId, nullptr);
  jsize session_id_size = env->GetArrayLength(sessionId);

  SB_DCHECK(session_id_elements);

  // nullptr array indicates key status isn't supported (i.e. Android API < 23)
  jsize length =
      (keyInformation == nullptr) ? 0 : env->GetArrayLength(keyInformation);
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
    jobject j_key_status = env->GetObjectArrayElement(keyInformation, i);
    jbyteArray j_key_id = static_cast<jbyteArray>(
        env->CallObjectMethod(j_key_status, getKeyIdMethod));

    jbyte* key_id_elements = env->GetByteArrayElements(j_key_id, nullptr);
    jsize key_id_size = env->GetArrayLength(j_key_id);
    SB_DCHECK(key_id_elements);

    SB_DCHECK(key_id_size <= sizeof(drm_key_ids[i].identifier));
    memcpy(drm_key_ids[i].identifier, key_id_elements, key_id_size);
    env->ReleaseByteArrayElements(j_key_id, key_id_elements, JNI_ABORT);
    drm_key_ids[i].identifier_size = key_id_size;

    jint j_status_code = env->CallIntMethod(j_key_status, getStatusCodeMethod);
    if (j_status_code == MEDIA_DRM_KEY_STATUS_EXPIRED) {
      drm_key_statuses[i] = kSbDrmKeyStatusExpired;
    } else if (j_status_code == MEDIA_DRM_KEY_STATUS_INTERNAL_ERROR) {
      drm_key_statuses[i] = kSbDrmKeyStatusError;
    } else if (j_status_code == MEDIA_DRM_KEY_STATUS_OUTPUT_NOT_ALLOWED) {
      drm_key_statuses[i] = kSbDrmKeyStatusRestricted;
    } else if (j_status_code == MEDIA_DRM_KEY_STATUS_PENDING) {
      drm_key_statuses[i] = kSbDrmKeyStatusPending;
    } else if (j_status_code == MEDIA_DRM_KEY_STATUS_USABLE) {
      drm_key_statuses[i] = kSbDrmKeyStatusUsable;
    } else {
      SB_NOTREACHED();
      drm_key_statuses[i] = kSbDrmKeyStatusError;
    }
  }

  host_->CallDrmSessionKeyStatusesChangedCallback(
      session_id_elements, session_id_size, drm_key_ids, drm_key_statuses);
  env->ReleaseByteArrayElements(sessionId, session_id_elements, JNI_ABORT);
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
