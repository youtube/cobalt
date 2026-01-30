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

#include <atomic>
#include <chrono>

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "starboard/android/shared/starboard_bridge.h"
#include "starboard/common/log.h"

#include "cobalt/android/jni_headers/ExoPlayerDrmBridge_jni.h"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#include "cobalt/android/jni_headers/ExoPlayerManager_jni.h"
#pragma GCC diagnostic pop

namespace starboard {
namespace {
using base::android::AttachCurrentThread;
using base::android::JavaByteArrayToString;
using base::android::JavaParamRef;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;

constexpr int kWaitForInitializedTimeoutUsec = 250'000;  // 250 ms.
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

DrmSystemExoPlayer::~DrmSystemExoPlayer() {
  Java_ExoPlayerDrmBridge_release(AttachCurrentThread(),
                                  j_exoplayer_drm_bridge_);
}

void DrmSystemExoPlayer::GenerateSessionUpdateRequest(
    int ticket,
    const char* type,
    const void* initialization_data,
    int initialization_data_size) {
  SB_LOG(INFO) << "CALLED GENERATESESSIONUPDATEREQUEST() WITH TYPE " << type;
  // Store initialization data to create ExoPlayer formats.
  initialization_data_ = std::vector<uint8_t>(
      reinterpret_cast<const uint8_t*>(initialization_data),
      reinterpret_cast<const uint8_t*>(initialization_data) +
          initialization_data_size);
  init_data_available_.store(true);
  init_data_available_cv_.notify_one();
}

void DrmSystemExoPlayer::UpdateSession(int ticket,
                                       const void* key,
                                       int key_size,
                                       const void* session_id,
                                       int session_id_size) {
  SB_LOG(INFO) << "CALLED UPDATESESSION()";
}
void DrmSystemExoPlayer::CloseSession(const void* session_id_data,
                                      int session_id_size) {}

SbDrmSystemPrivate::DecryptStatus DrmSystemExoPlayer::Decrypt(
    InputBuffer* buffer) {
  // All buffers are decrypted by the player.
  return kSuccess;
}

bool DrmSystemExoPlayer::IsServerCertificateUpdatable() {
  return false;
}

void DrmSystemExoPlayer::UpdateServerCertificate(int ticket,
                                                 const void* certificate,
                                                 int certificate_size) {
  SB_LOG(INFO) << "CALLED UpdateServerCertificate()";
}

const void* DrmSystemExoPlayer::GetMetrics(int* size) {
  SB_LOG(INFO) << "CALLED GetMetrics()";

  JNIEnv* env = AttachCurrentThread();

  ScopedJavaLocalRef<jbyteArray> j_metrics =
      Java_ExoPlayerDrmBridge_getMetricsInBase64(env, j_exoplayer_drm_bridge_);

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

void DrmSystemExoPlayer::ExecuteKeyRequest(
    JNIEnv* env,
    const JavaParamRef<jbyteArray>& j_session_id,
    const JavaParamRef<jbyteArray>& j_message) {
  std::string session_id = JavaByteArrayToString(env, j_session_id);
  std::string message = JavaByteArrayToString(env, j_message);
  update_request_callback_(
      this, context_, kSbDrmTicketInvalid, kSbDrmStatusSuccess,
      kSbDrmSessionRequestTypeLicenseRequest, nullptr, session_id.data(),
      session_id.size(), message.data(), message.size(), kNoUrl);
}

std::vector<uint8_t> DrmSystemExoPlayer::GetInitializationData() {
  bool received_init_data;
  {
    std::unique_lock<std::mutex> lock(mutex_);
    received_init_data = init_data_available_cv_.wait_for(
        lock, std::chrono::microseconds(kWaitForInitializedTimeoutUsec),
        [this] { return init_data_available_.load(); });
  }

  if (!received_init_data) {
    SB_LOG(INFO) << "TIMED OUT GETTING INIT DATA";
    return std::vector<uint8_t>();
  }

  return initialization_data_;
}

}  // namespace starboard
