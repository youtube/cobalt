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

#include "starboard/android/shared/drm_system.h"

#include <jni.h>

#include <memory>
#include <utility>

#include "base/android/jni_array.h"
#include "base/android/jni_int_wrapper.h"
#include "base/android/jni_string.h"
#include "starboard/android/shared/media_common.h"
#include "starboard/common/instance_counter.h"
#include "starboard/common/thread.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "cobalt/android/jni_headers/MediaDrmBridge_jni.h"

namespace {

using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;
using base::android::ToJavaByteArray;
using starboard::android::shared::DrmSystem;

const char kNoUrl[] = "";

// Using all capital names to be consistent with other Android media statuses.
// They are defined in the same order as in their Java counterparts.  Their
// values should be kept in consistent with their Java counterparts defined in
// android.media.MediaDrm.KeyStatus.
const jint MEDIA_DRM_KEY_STATUS_EXPIRED = 1;
const jint MEDIA_DRM_KEY_STATUS_INTERNAL_ERROR = 4;
const jint MEDIA_DRM_KEY_STATUS_OUTPUT_NOT_ALLOWED = 2;
const jint MEDIA_DRM_KEY_STATUS_PENDING = 3;
const jint MEDIA_DRM_KEY_STATUS_USABLE = 0;

// They must have the same values as defined in MediaDrm.KeyRequest.
const jint REQUEST_TYPE_INITIAL = 0;
const jint REQUEST_TYPE_RENEWAL = 1;
const jint REQUEST_TYPE_RELEASE = 2;

DECLARE_INSTANCE_COUNTER(AndroidDrmSystem)

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

std::string JByteArrayToString(
    JNIEnv* env,
    const base::android::JavaRef<jbyteArray>& j_array) {
  std::string out_string;
  ::base::android::JavaByteArrayToString(env, j_array, &out_string);
  return out_string;
}

std::string JByteArrayToString(JNIEnv* env, const jbyteArray& j_array) {
  return JByteArrayToString(env, ScopedJavaLocalRef<jbyteArray>(env, j_array));
}
}  // namespace

namespace starboard::android::shared {

void DrmSystem::OnMediaDrmSessionMessage(
    JNIEnv* env,
    jint ticket,
    const JavaParamRef<jbyteArray>& sessionId,
    jint requestType,
    const JavaParamRef<jbyteArray>& message) {
  std::string session_id = JByteArrayToString(env, sessionId);
  std::string message_str = JByteArrayToString(env, message);

  update_request_callback_(
      this, context_, ticket, kSbDrmStatusSuccess,
      SbDrmSessionRequestTypeFromMediaDrmKeyRequestType(requestType),
      /*error_message=*/nullptr, session_id.data(), session_id.size(),
      message_str.data(), message_str.size(), kNoUrl);
}

void DrmSystem::OnMediaDrmKeyStatusChange(
    JNIEnv* env,
    const JavaParamRef<jbyteArray>& sessionId,
    const JavaParamRef<jobjectArray>& keyInformation) {
  std::string session_id = JByteArrayToString(env, sessionId);

  // NULL array indicates key status isn't supported (i.e. Android API < 23)
  jsize length =
      (keyInformation == NULL) ? 0 : env->GetArrayLength(keyInformation);
  std::vector<SbDrmKeyId> drm_key_ids(length);
  std::vector<SbDrmKeyStatus> drm_key_statuses(length);

  for (jsize i = 0; i < length; ++i) {
    jobject j_key_status = env->GetObjectArrayElement(keyInformation, i);
    jclass mediaDrmKeyStatusClass =
        env->FindClass("android/media/MediaDrm$KeyStatus");
    jmethodID getKeyIdMethod =
        env->GetMethodID(mediaDrmKeyStatusClass, "getKeyId", "()[B");

    std::string key_id =
        JByteArrayToString(env, static_cast<jbyteArray>(env->CallObjectMethod(
                                    j_key_status, getKeyIdMethod)));

    SB_DCHECK(key_id.size() <= sizeof(drm_key_ids[i].identifier));
    memcpy(drm_key_ids[i].identifier, key_id.data(), key_id.size());
    drm_key_ids[i].identifier_size = key_id.size();

    jmethodID getStatusCodeMethod =
        env->GetMethodID(mediaDrmKeyStatusClass, "getStatusCode", "()I");
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

  CallDrmSessionKeyStatusesChangedCallback(session_id.data(), session_id.size(),
                                           drm_key_ids, drm_key_statuses);
}
}  // namespace starboard::android::shared

// Declare the function as static instead of putting it in the above anonymous
// namespace so it can be picked up by `std::vector<SbDrmKeyId>::operator==()`
// as functions in anonymous namespace doesn't participate in argument dependent
// lookup.
static bool operator==(const SbDrmKeyId& left, const SbDrmKeyId& right) {
  if (left.identifier_size != right.identifier_size) {
    return false;
  }
  return memcmp(left.identifier, right.identifier, left.identifier_size) == 0;
}

namespace starboard {
namespace android {
namespace shared {

DrmSystem::DrmSystem(
    const char* key_system,
    void* context,
    SbDrmSessionUpdateRequestFunc update_request_callback,
    SbDrmSessionUpdatedFunc session_updated_callback,
    SbDrmSessionKeyStatusesChangedFunc key_statuses_changed_callback)
    : Thread("DrmSystemThread"),
      key_system_(key_system),
      context_(context),
      update_request_callback_(update_request_callback),
      session_updated_callback_(session_updated_callback),
      key_statuses_changed_callback_(key_statuses_changed_callback),
      j_media_drm_bridge_(NULL),
      j_media_crypto_(NULL),
      hdcp_lost_(false) {
  ON_INSTANCE_CREATED(AndroidDrmSystem);

  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef j_key_system(ConvertUTF8ToJavaString(env, key_system));
  j_media_drm_bridge_ = ScopedJavaGlobalRef(Java_MediaDrmBridge_create(
      env, j_key_system, reinterpret_cast<jlong>(this)));

  if (j_media_drm_bridge_.is_null()) {
    SB_LOG(ERROR) << "Failed to create MediaDrmBridge.";
    return;
  }

  j_media_crypto_ = ScopedJavaGlobalRef(
      Java_MediaDrmBridge_getMediaCrypto(env, j_media_drm_bridge_));

  if (j_media_crypto_.is_null()) {
    SB_LOG(ERROR) << "Failed to create MediaCrypto.";
    return;
  }

  Start();
}

void DrmSystem::Run() {
  JNIEnv* env = AttachCurrentThread();
  bool result =
      Java_MediaDrmBridge_createMediaCryptoSession(env, j_media_drm_bridge_);
  if (result) {
    created_media_crypto_session_.store(true);
  }
  if (!result && !j_media_crypto_.is_null()) {
    j_media_crypto_.Reset();
    return;
  }

  ScopedLock scoped_lock(mutex_);
  if (!deferred_session_update_requests_.empty()) {
    for (const auto& update_request : deferred_session_update_requests_) {
      update_request->Generate(j_media_drm_bridge_);
    }
    deferred_session_update_requests_.clear();
  }
}

DrmSystem::~DrmSystem() {
  ON_INSTANCE_RELEASED(AndroidDrmSystem);
  Join();

  if (!j_media_drm_bridge_.is_null()) {
    JNIEnv* env = AttachCurrentThread();
    Java_MediaDrmBridge_destroy(env, j_media_drm_bridge_);
  }
}

DrmSystem::SessionUpdateRequest::SessionUpdateRequest(
    int ticket,
    const char* type,
    const void* initialization_data,
    int initialization_data_size) {
  JNIEnv* env = AttachCurrentThread();
  j_ticket_ = static_cast<jint>(ticket);
  j_init_data_ = ScopedJavaGlobalRef(
      ToJavaByteArray(env, static_cast<const uint8_t*>(initialization_data),
                      initialization_data_size));
  j_mime_ = ScopedJavaGlobalRef(ConvertUTF8ToJavaString(env, type));
}

void DrmSystem::SessionUpdateRequest::Generate(
    ScopedJavaGlobalRef<jobject> j_media_drm_bridge) const {
  JNIEnv* env = AttachCurrentThread();
  Java_MediaDrmBridge_createSession(env, j_media_drm_bridge, j_ticket_,
                                    j_init_data_, j_mime_);
}

void DrmSystem::GenerateSessionUpdateRequest(int ticket,
                                             const char* type,
                                             const void* initialization_data,
                                             int initialization_data_size) {
  std::unique_ptr<SessionUpdateRequest> session_update_request(
      new SessionUpdateRequest(ticket, type, initialization_data,
                               initialization_data_size));
  if (created_media_crypto_session_.load()) {
    session_update_request->Generate(j_media_drm_bridge_);
  } else {
    // Defer generating the update request.
    ScopedLock scoped_lock(mutex_);
    deferred_session_update_requests_.push_back(
        std::move(session_update_request));
  }
  // |update_request_callback_| will be called by Java calling into
  // |onSessionMessage|.
}

void DrmSystem::UpdateSession(int ticket,
                              const void* key,
                              int key_size,
                              const void* session_id,
                              int session_id_size) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jbyteArray> j_session_id(ToJavaByteArray(
      env, static_cast<const uint8_t*>(session_id), session_id_size));
  ScopedJavaLocalRef<jbyteArray> j_response(
      ToJavaByteArray(env, static_cast<const uint8_t*>(key), key_size));

  JniIntWrapper j_ticket = JniIntWrapper(ticket);
  ScopedJavaLocalRef<jobject> update_result(Java_MediaDrmBridge_updateSession(
      env, j_media_drm_bridge_, j_ticket, j_session_id, j_response));
  jboolean update_success =
      Java_UpdateSessionResult_isSuccess(env, update_result);
  ScopedJavaLocalRef<jstring> error_msg_java(
      Java_UpdateSessionResult_getErrorMessage(env, update_result));
  std::string error_msg = ConvertJavaStringToUTF8(env, error_msg_java);
  session_updated_callback_(this, context_, ticket,
                            update_success == JNI_TRUE
                                ? kSbDrmStatusSuccess
                                : kSbDrmStatusUnknownError,
                            error_msg.c_str(), session_id, session_id_size);
}

void DrmSystem::CloseSession(const void* session_id, int session_id_size) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jbyteArray> j_session_id = ToJavaByteArray(
      env, static_cast<const uint8_t*>(session_id), session_id_size);
  std::string session_id_as_string(
      static_cast<const char*>(session_id),
      static_cast<const char*>(session_id) + session_id_size);

  {
    ScopedLock scoped_lock(mutex_);
    auto iter = cached_drm_key_ids_.find(session_id_as_string);
    if (iter != cached_drm_key_ids_.end()) {
      cached_drm_key_ids_.erase(iter);
    }
  }
  Java_MediaDrmBridge_closeSession(env, j_media_drm_bridge_, j_session_id);
}

DrmSystem::DecryptStatus DrmSystem::Decrypt(InputBuffer* buffer) {
  SB_DCHECK(buffer);
  SB_DCHECK(buffer->drm_info());
  SB_DCHECK(j_media_crypto_);
  // The actual decryption will take place by calling |queueSecureInputBuffer|
  // in the decoders.  Our existence implies that there is enough information
  // to perform the decryption.
  // TODO: Returns kRetry when |UpdateSession| is not called at all to allow the
  //       player worker to handle the retry logic.
  return kSuccess;
}

const void* DrmSystem::GetMetrics(int* size) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jbyteArray> j_metrics =
      Java_MediaDrmBridge_getMetricsInBase64(env, j_media_drm_bridge_);

  if (j_metrics.is_null()) {
    *size = 0;
    return nullptr;
  }

  jbyte* metrics_elements = env->GetByteArrayElements(j_metrics.obj(), NULL);
  jsize metrics_size = base::android::SafeGetArrayLength(env, j_metrics);
  SB_DCHECK(metrics_elements);

  metrics_.assign(metrics_elements, metrics_elements + metrics_size);

  *size = static_cast<int>(metrics_.size());
  return metrics_.data();
}

void DrmSystem::CallDrmSessionKeyStatusesChangedCallback(
    const void* session_id,
    int session_id_size,
    const std::vector<SbDrmKeyId>& drm_key_ids,
    const std::vector<SbDrmKeyStatus>& drm_key_statuses) {
  SB_DCHECK(drm_key_ids.size() == drm_key_statuses.size());

  std::string session_id_as_string(
      static_cast<const char*>(session_id),
      static_cast<const char*>(session_id) + session_id_size);

  {
    ScopedLock scoped_lock(mutex_);
    if (cached_drm_key_ids_[session_id_as_string] != drm_key_ids) {
      cached_drm_key_ids_[session_id_as_string] = drm_key_ids;
      if (hdcp_lost_) {
        CallKeyStatusesChangedCallbackWithKeyStatusRestricted_Locked();
        return;
      }
    }
  }

  key_statuses_changed_callback_(this, context_, session_id, session_id_size,
                                 static_cast<int>(drm_key_ids.size()),
                                 drm_key_ids.data(), drm_key_statuses.data());
}

void DrmSystem::OnInsufficientOutputProtection() {
  // HDCP has lost, update the statuses of all keys in all known sessions to be
  // restricted.
  ScopedLock scoped_lock(mutex_);
  if (hdcp_lost_) {
    return;
  }
  hdcp_lost_ = true;
  CallKeyStatusesChangedCallbackWithKeyStatusRestricted_Locked();
}

void DrmSystem::CallKeyStatusesChangedCallbackWithKeyStatusRestricted_Locked() {
  mutex_.DCheckAcquired();

  for (auto& iter : cached_drm_key_ids_) {
    const std::string& session_id = iter.first;
    const std::vector<SbDrmKeyId>& drm_key_ids = iter.second;
    std::vector<SbDrmKeyStatus> drm_key_statuses(drm_key_ids.size(),
                                                 kSbDrmKeyStatusRestricted);

    key_statuses_changed_callback_(this, context_, session_id.data(),
                                   session_id.size(),
                                   static_cast<int>(drm_key_ids.size()),
                                   drm_key_ids.data(), drm_key_statuses.data());
  }
}

bool DrmSystem::IsWidevineSupported() {
  JNIEnv* env = AttachCurrentThread();
  return Java_MediaDrmBridge_isWidevineCryptoSchemeSupported(env) == JNI_TRUE;
}

bool DrmSystem::IsCbcsSupported() {
  JNIEnv* env = AttachCurrentThread();
  return Java_MediaDrmBridge_isCbcsSchemeSupported(env) == JNI_TRUE;
}

}  // namespace shared
}  // namespace android
}  // namespace starboard
