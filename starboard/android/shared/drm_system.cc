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

#include "starboard/android/shared/jni_env_ext.h"
#include "starboard/android/shared/jni_utils.h"
#include "starboard/android/shared/media_common.h"
#include "starboard/common/instance_counter.h"

namespace {

using starboard::android::shared::DrmSystem;
using starboard::android::shared::JniEnvExt;
using starboard::android::shared::ScopedLocalJavaRef;

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

}  // namespace

// This has to be defined outside the above anonymous namespace to be picked up
// by the comparison of std::vector<SbDrmKeyId>.
bool operator==(const SbDrmKeyId& left, const SbDrmKeyId& right) {
  if (left.identifier_size != right.identifier_size) {
    return false;
  }
  return memcmp(left.identifier, right.identifier, left.identifier_size) == 0;
}

extern "C" SB_EXPORT_PLATFORM void
Java_dev_cobalt_media_MediaDrmBridge_nativeOnSessionMessage(
    JNIEnv* env,
    jobject unused_this,
    jlong native_media_drm_bridge,
    jint ticket,
    jbyteArray j_session_id,
    jint request_type,
    jbyteArray j_message) {
  jbyte* session_id_elements = env->GetByteArrayElements(j_session_id, NULL);
  jsize session_id_size = env->GetArrayLength(j_session_id);

  jbyte* message_elements = env->GetByteArrayElements(j_message, NULL);
  jsize message_size = env->GetArrayLength(j_message);

  SB_DCHECK(session_id_elements);
  SB_DCHECK(message_elements);

  DrmSystem* drm_system = reinterpret_cast<DrmSystem*>(native_media_drm_bridge);
  SB_DCHECK(drm_system);
  drm_system->CallUpdateRequestCallback(
      ticket, SbDrmSessionRequestTypeFromMediaDrmKeyRequestType(request_type),
      session_id_elements, session_id_size, message_elements, message_size,
      kNoUrl);
  env->ReleaseByteArrayElements(j_session_id, session_id_elements, JNI_ABORT);
  env->ReleaseByteArrayElements(j_message, message_elements, JNI_ABORT);
}

extern "C" SB_EXPORT_PLATFORM void
Java_dev_cobalt_media_MediaDrmBridge_nativeOnKeyStatusChange(
    JniEnvExt* env,
    jobject unused_this,
    jlong native_media_drm_bridge,
    jbyteArray j_session_id,
    jobjectArray j_key_status_array) {
  jbyte* session_id_elements = env->GetByteArrayElements(j_session_id, NULL);
  jsize session_id_size = env->GetArrayLength(j_session_id);

  SB_DCHECK(session_id_elements);

  // NULL array indicates key status isn't supported (i.e. Android API < 23)
  jsize length = (j_key_status_array == NULL)
                     ? 0
                     : env->GetArrayLength(j_key_status_array);
  std::vector<SbDrmKeyId> drm_key_ids(length);
  std::vector<SbDrmKeyStatus> drm_key_statuses(length);

  for (jsize i = 0; i < length; ++i) {
    jobject j_key_status =
        env->GetObjectArrayElementOrAbort(j_key_status_array, i);
    jbyteArray j_key_id = static_cast<jbyteArray>(
        env->CallObjectMethodOrAbort(j_key_status, "getKeyId", "()[B"));

    jbyte* key_id_elements = env->GetByteArrayElements(j_key_id, NULL);
    jsize key_id_size = env->GetArrayLength(j_key_id);
    SB_DCHECK(key_id_elements);

    SB_DCHECK(key_id_size <= sizeof(drm_key_ids[i].identifier));
    memcpy(drm_key_ids[i].identifier, key_id_elements, key_id_size);
    env->ReleaseByteArrayElements(j_key_id, key_id_elements, JNI_ABORT);
    drm_key_ids[i].identifier_size = key_id_size;

    jint j_status_code =
        env->CallIntMethodOrAbort(j_key_status, "getStatusCode", "()I");
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

  DrmSystem* drm_system = reinterpret_cast<DrmSystem*>(native_media_drm_bridge);
  SB_DCHECK(drm_system);
  drm_system->CallDrmSessionKeyStatusesChangedCallback(
      session_id_elements, session_id_size, drm_key_ids, drm_key_statuses);

  env->ReleaseByteArrayElements(j_session_id, session_id_elements, JNI_ABORT);
}

namespace starboard {
namespace android {
namespace shared {

namespace {

jbyteArray ByteArrayFromRaw(const void* data, int size) {
  return JniEnvExt::Get()->NewByteArrayFromRaw(static_cast<const jbyte*>(data),
                                               size);
}

}  // namespace

DrmSystem::DrmSystem(
    const char* key_system,
    void* context,
    SbDrmSessionUpdateRequestFunc update_request_callback,
    SbDrmSessionUpdatedFunc session_updated_callback,
    SbDrmSessionKeyStatusesChangedFunc key_statuses_changed_callback)
    : key_system_(key_system),
      context_(context),
      update_request_callback_(update_request_callback),
      session_updated_callback_(session_updated_callback),
      key_statuses_changed_callback_(key_statuses_changed_callback),
      j_media_drm_bridge_(NULL),
      j_media_crypto_(NULL),
      hdcp_lost_(false) {
  ON_INSTANCE_CREATED(AndroidDrmSystem);

  JniEnvExt* env = JniEnvExt::Get();
  ScopedLocalJavaRef<jstring> j_key_system(
      env->NewStringStandardUTFOrAbort(key_system));
  j_media_drm_bridge_ = env->CallStaticObjectMethodOrAbort(
      "dev/cobalt/media/MediaDrmBridge", "create",
      "(Ljava/lang/String;J)Ldev/cobalt/media/MediaDrmBridge;",
      j_key_system.Get(), reinterpret_cast<jlong>(this));
  if (!j_media_drm_bridge_) {
    SB_LOG(ERROR) << "Failed to create MediaDrmBridge.";
    return;
  }
  j_media_drm_bridge_ = env->ConvertLocalRefToGlobalRef(j_media_drm_bridge_);
  j_media_crypto_ = env->CallObjectMethodOrAbort(
      j_media_drm_bridge_, "getMediaCrypto", "()Landroid/media/MediaCrypto;");
  if (!j_media_crypto_) {
    SB_LOG(ERROR) << "Failed to create MediaCrypto.";
    return;
  }
  j_media_crypto_ = env->ConvertLocalRefToGlobalRef(j_media_crypto_);
}

DrmSystem::~DrmSystem() {
  ON_INSTANCE_RELEASED(AndroidDrmSystem);

  JniEnvExt* env = JniEnvExt::Get();
  if (j_media_crypto_) {
    env->DeleteGlobalRef(j_media_crypto_);
    j_media_crypto_ = NULL;
  }
  if (j_media_drm_bridge_) {
    env->CallVoidMethodOrAbort(j_media_drm_bridge_, "destroy", "()V");
    env->DeleteGlobalRef(j_media_drm_bridge_);
    j_media_drm_bridge_ = NULL;
  }
}

void DrmSystem::GenerateSessionUpdateRequest(int ticket,
                                             const char* type,
                                             const void* initialization_data,
                                             int initialization_data_size) {
  ScopedLocalJavaRef<jbyteArray> j_init_data(
      ByteArrayFromRaw(initialization_data, initialization_data_size));
  JniEnvExt* env = JniEnvExt::Get();
  ScopedLocalJavaRef<jstring> j_mime(env->NewStringStandardUTFOrAbort(type));
  env->CallVoidMethodOrAbort(
      j_media_drm_bridge_, "createSession", "(I[BLjava/lang/String;)V",
      static_cast<jint>(ticket), j_init_data.Get(), j_mime.Get());
  // |update_request_callback_| will be called by Java calling into
  // |onSessionMessage|.
}

void DrmSystem::UpdateSession(int ticket,
                              const void* key,
                              int key_size,
                              const void* session_id,
                              int session_id_size) {
  ScopedLocalJavaRef<jbyteArray> j_session_id(
      ByteArrayFromRaw(session_id, session_id_size));
  ScopedLocalJavaRef<jbyteArray> j_response(ByteArrayFromRaw(key, key_size));

  auto env = JniEnvExt::Get();
  ScopedLocalJavaRef<jobject> update_result(env->CallObjectMethodOrAbort(
      j_media_drm_bridge_, "updateSession",
      "(I[B[B)Ldev/cobalt/media/MediaDrmBridge$UpdateSessionResult;",
      static_cast<jint>(ticket), j_session_id.Get(), j_response.Get()));
  jboolean update_success =
      env->CallBooleanMethodOrAbort(update_result.Get(), "isSuccess", "()Z");
  ScopedLocalJavaRef<jstring> error_msg_java(env->CallObjectMethodOrAbort(
      update_result.Get(), "getErrorMessage", "()Ljava/lang/String;"));
  std::string error_msg =
      env->GetStringStandardUTFOrAbort(error_msg_java.Get());
  session_updated_callback_(this, context_, ticket,
                            update_success == JNI_TRUE
                                ? kSbDrmStatusSuccess
                                : kSbDrmStatusUnknownError,
                            error_msg.c_str(), session_id, session_id_size);
}

void DrmSystem::CloseSession(const void* session_id, int session_id_size) {
  JniEnvExt* env = JniEnvExt::Get();
  ScopedLocalJavaRef<jbyteArray> j_session_id(
      ByteArrayFromRaw(session_id, session_id_size));
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
  env->CallVoidMethodOrAbort(j_media_drm_bridge_, "closeSession", "([B)V",
                             j_session_id.Get());
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
  JniEnvExt* env = JniEnvExt::Get();
  jbyteArray j_metrics = static_cast<jbyteArray>(env->CallObjectMethodOrAbort(
      j_media_drm_bridge_, "getMetricsInBase64", "()[B"));

  if (!j_metrics) {
    *size = 0;
    return nullptr;
  }

  jbyte* metrics_elements = env->GetByteArrayElements(j_metrics, NULL);
  jsize metrics_size = env->GetArrayLength(j_metrics);
  SB_DCHECK(metrics_elements);

  metrics_.assign(metrics_elements, metrics_elements + metrics_size);

  env->ReleaseByteArrayElements(j_metrics, metrics_elements, JNI_ABORT);
  *size = static_cast<int>(metrics_.size());
  return metrics_.data();
}

void DrmSystem::CallUpdateRequestCallback(int ticket,
                                          SbDrmSessionRequestType request_type,
                                          const void* session_id,
                                          int session_id_size,
                                          const void* content,
                                          int content_size,
                                          const char* url) {
  update_request_callback_(this, context_, ticket, kSbDrmStatusSuccess,
                           request_type, NULL, session_id, session_id_size,
                           content, content_size, url);
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

}  // namespace shared
}  // namespace android
}  // namespace starboard
