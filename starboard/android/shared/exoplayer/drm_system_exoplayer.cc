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

#include "starboard/android/shared/exoplayer/drm_system_exoplayer.h"

#include <chrono>
#include <vector>

#include "base/android/jni_array.h"
#include "starboard/android/shared/drm_common.h"
#include "starboard/android/shared/starboard_bridge.h"
#include "starboard/common/log.h"

#include "cobalt/android/jni_headers/ExoPlayerDrmBridge_jni.h"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#include "cobalt/android/jni_headers/ExoPlayerManager_jni.h"
#pragma GCC diagnostic pop

namespace starboard {
namespace {
using android::shared::RequestType;
using base::android::AttachCurrentThread;
using base::android::JavaByteArrayToString;
using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;
using base::android::ToJavaByteArray;
using starboard::android::shared::ToSbDrmKeyStatus;
using starboard::android::shared::ToSbDrmSessionRequestType;

constexpr int kWaitForInitializedTimeoutUsec = 5000000;  // 5 s.
constexpr char kNoUrl[] = "";

std::string JavaByteArrayToString(
    JNIEnv* env,
    const base::android::JavaRef<jbyteArray>& j_byte_array) {
  std::string out;
  JavaByteArrayToString(env, j_byte_array, &out);
  return out;
}
}  // namespace

DrmSystemExoPlayer::DrmSystemExoPlayer(
    std::string_view key_system,
    void* context,
    SbDrmSessionUpdateRequestFunc update_request_callback,
    SbDrmSessionUpdatedFunc session_updated_callback,
    SbDrmSessionKeyStatusesChangedFunc key_statuses_changed_callback)
    : key_system_(key_system),
      context_(context),
      update_request_callback_(update_request_callback),
      session_updated_callback_(session_updated_callback),
      key_statuses_changed_callback_(key_statuses_changed_callback) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> j_exoplayer_manager(
      StarboardBridge::GetInstance()->GetExoPlayerManager(env));
  if (!j_exoplayer_manager) {
    SB_LOG(ERROR) << "Failed to fetch ExoPlayerManager";
    return;
  }

  ScopedJavaLocalRef<jobject> j_exoplayer_drm_bridge =
      Java_ExoPlayerManager_createExoPlayerDrmBridge(
          env, j_exoplayer_manager, reinterpret_cast<jlong>(this));
  if (!j_exoplayer_drm_bridge) {
    SB_LOG(ERROR) << "Could not create Java ExoPlayerBridge";
    return;
  }

  j_exoplayer_drm_bridge_.Reset(j_exoplayer_drm_bridge);
}

base::android::ScopedJavaLocalRef<jobject>
DrmSystemExoPlayer::GetDrmSessionManager() const {
  if (!j_exoplayer_drm_bridge_) {
    return ScopedJavaLocalRef<jobject>();
  }
  JNIEnv* env = AttachCurrentThread();
  return Java_ExoPlayerDrmBridge_getDrmSessionManager(env,
                                                      j_exoplayer_drm_bridge_);
}

DrmSystemExoPlayer::~DrmSystemExoPlayer() {
  Java_ExoPlayerDrmBridge_release(AttachCurrentThread(),
                                  j_exoplayer_drm_bridge_);
}

void DrmSystemExoPlayer::GenerateSessionUpdateRequest(
    int ticket,
    const char* type,
    const void* initialization_data,
    int initialization_data_size) {
  std::lock_guard<std::mutex> lock(mutex_);
  ticket_ = ticket;
  initialization_data_ = std::vector<uint8_t>(
      reinterpret_cast<const uint8_t*>(initialization_data),
      reinterpret_cast<const uint8_t*>(initialization_data) +
          initialization_data_size);
  init_data_available_ = true;
  init_data_available_cv_.notify_one();
}

void DrmSystemExoPlayer::UpdateSession(int ticket,
                                       const void* key,
                                       int key_size,
                                       const void* session_id,
                                       int session_id_size) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jbyteArray> j_response =
      ToJavaByteArray(env, reinterpret_cast<const uint8_t*>(key), key_size);

  {
    std::lock_guard<std::mutex> lock(mutex_);
    session_id_to_ticket_[std::string(reinterpret_cast<const char*>(session_id),
                                      session_id_size)] = ticket;
  }

  Java_ExoPlayerDrmBridge_setKeyRequestResponse(env, j_exoplayer_drm_bridge_,
                                                j_response);
  session_updated_callback_(this, context_, ticket, kSbDrmStatusSuccess, "",
                            session_id, session_id_size);
}

// The ExoPlayer will close the session when it no longer needs it.
void DrmSystemExoPlayer::CloseSession(const void* session_id_data,
                                      int session_id_size) {}

SbDrmSystemPrivate::DecryptStatus DrmSystemExoPlayer::Decrypt(
    InputBuffer* buffer) {
  // All buffers are decrypted by the player.
  return kSuccess;
}

// TODO: Implement certificate update handling.
bool DrmSystemExoPlayer::IsServerCertificateUpdatable() {
  return false;
}

void DrmSystemExoPlayer::UpdateServerCertificate(int ticket,
                                                 const void* certificate,
                                                 int certificate_size) {}

const void* DrmSystemExoPlayer::GetMetrics(int* size) {
  JNIEnv* env = AttachCurrentThread();

  ScopedJavaLocalRef<jbyteArray> j_metrics =
      Java_ExoPlayerDrmBridge_getMetricsInBase64(env, j_exoplayer_drm_bridge_);

  if (j_metrics.is_null()) {
    *size = 0;
    return nullptr;
  }

  jbyte* metrics_elements = env->GetByteArrayElements(j_metrics.obj(), nullptr);
  if (!metrics_elements) {
    *size = 0;
    return nullptr;
  }
  jsize metrics_size = base::android::SafeGetArrayLength(env, j_metrics);

  {
    std::lock_guard<std::mutex> lock(mutex_);
    metrics_.assign(metrics_elements, metrics_elements + metrics_size);
    *size = static_cast<int>(metrics_.size());
  }

  env->ReleaseByteArrayElements(j_metrics.obj(), metrics_elements, JNI_ABORT);

  return metrics_.data();
}

void DrmSystemExoPlayer::OnKeyRequest(
    JNIEnv* env,
    jint request_type,
    const JavaParamRef<jbyteArray>& j_message,
    const JavaParamRef<jbyteArray>& j_session_id) {
  std::string session_id = JavaByteArrayToString(env, j_session_id);
  std::string message = JavaByteArrayToString(env, j_message);

  int ticket;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = session_id_to_ticket_.find(session_id);
    if (it != session_id_to_ticket_.end()) {
      ticket = it->second;
    } else {
      ticket = ticket_;
    }
  }

  update_request_callback_(
      this, context_, ticket, kSbDrmStatusSuccess,
      ToSbDrmSessionRequestType(static_cast<RequestType>(request_type)),
      nullptr, session_id.data(), session_id.size(), message.data(),
      message.size(), kNoUrl);
}

void DrmSystemExoPlayer::OnProvisionRequest(
    JNIEnv* env,
    const JavaParamRef<jbyteArray>& j_session_id,
    const JavaParamRef<jbyteArray>& j_message) const {
  SB_NOTREACHED() << "App provisioning is unsupported for ExoPlayer.";
}

void DrmSystemExoPlayer::OnKeyStatusChanged(
    JNIEnv* env,
    const JavaParamRef<jbyteArray>& session_id,
    const JavaParamRef<jobjectArray>& key_information) {
  std::string session_id_bytes = JavaByteArrayToString(env, session_id);

  jsize length =
      (key_information == nullptr) ? 0 : env->GetArrayLength(key_information);
  std::vector<SbDrmKeyId> drm_key_ids(length);
  std::vector<SbDrmKeyStatus> drm_key_statuses(length);

  for (jsize i = 0; i < length; ++i) {
    ScopedJavaLocalRef<jobject> j_key_status(
        env, env->GetObjectArrayElement(key_information.obj(), i));
    ScopedJavaLocalRef<jbyteArray> j_key_id =
        Java_ExoPlayerKeyStatus_getKeyId(env, j_key_status);
    std::string key_id = JavaByteArrayToString(env, j_key_id);

    SB_CHECK_LE(key_id.size(), sizeof(drm_key_ids[i].identifier));
    memcpy(drm_key_ids[i].identifier, key_id.data(), key_id.size());
    drm_key_ids[i].identifier_size = key_id.size();

    drm_key_statuses[i] = ToSbDrmKeyStatus(
        Java_ExoPlayerKeyStatus_getStatusCode(env, j_key_status));
  }

  key_statuses_changed_callback_(this, context_, session_id_bytes.data(),
                                 session_id_bytes.size(),
                                 static_cast<int>(drm_key_ids.size()),
                                 drm_key_ids.data(), drm_key_statuses.data());
}

std::vector<uint8_t> DrmSystemExoPlayer::GetInitializationData() {
  std::unique_lock<std::mutex> lock(mutex_);
  bool received_init_data = init_data_available_cv_.wait_for(
      lock, std::chrono::microseconds(kWaitForInitializedTimeoutUsec),
      [this] { return init_data_available_; });

  if (!received_init_data) {
    return std::vector<uint8_t>();
  }

  return initialization_data_;
}

}  // namespace starboard
