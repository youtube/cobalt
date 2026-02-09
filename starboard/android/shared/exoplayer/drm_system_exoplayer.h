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

#ifndef STARBOARD_ANDROID_SHARED_DRM_SYSTEM_EXOPLAYER_H_
#define STARBOARD_ANDROID_SHARED_DRM_SYSTEM_EXOPLAYER_H_

#include <jni.h>

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "base/android/jni_android.h"
#include "starboard/android/shared/media_common.h"
#include "starboard/android/shared/media_drm_bridge.h"
#include "starboard/shared/starboard/drm/drm_system_internal.h"
#include "starboard/shared/starboard/thread_checker.h"

namespace starboard {

class DrmSystemExoPlayer : public ::SbDrmSystemPrivate {
 public:
  DrmSystemExoPlayer(
      std::string_view key_system,
      void* context,
      SbDrmSessionUpdateRequestFunc update_request_callback,
      SbDrmSessionUpdatedFunc session_updated_callback,
      SbDrmSessionKeyStatusesChangedFunc key_statuses_changed_callback);

  ~DrmSystemExoPlayer() override;
  // SbDrmSystemPrivate override begins
  void GenerateSessionUpdateRequest(int ticket,
                                    const char* type,
                                    const void* initialization_data,
                                    int initialization_data_size) override;
  void UpdateSession(int ticket,
                     const void* key,
                     int key_size,
                     const void* session_id,
                     int session_id_size) override;
  void CloseSession(const void* session_id_data, int session_id_size) override;
  DecryptStatus Decrypt(InputBuffer* buffer) override;

  bool IsServerCertificateUpdatable() override;
  void UpdateServerCertificate(int ticket,
                               const void* certificate,
                               int certificate_size) override;
  const void* GetMetrics(int* size) override;
  // SbDrmSystemPrivate override ends.

  void ExecuteKeyRequest(
      JNIEnv* env,
      const base::android::JavaParamRef<jbyteArray>& j_session_id,
      const base::android::JavaParamRef<jbyteArray>& j_message);
  void ExecuteProvisionRequest(
      JNIEnv* env,
      const base::android::JavaParamRef<jbyteArray>& j_session_id,
      const base::android::JavaParamRef<jbyteArray>& j_message) {}
  std::vector<uint8_t> GetInitializationData();

  base::android::ScopedJavaGlobalRef<jobject> get_exoplayer_drm_bridge() const {
    return j_exoplayer_drm_bridge_;
  }

  base::android::ScopedJavaLocalRef<jobject> GetDrmSessionManager() const;

 private:
  const std::string key_system_;
  void* context_;
  SbDrmSessionUpdateRequestFunc update_request_callback_;
  SbDrmSessionUpdatedFunc session_updated_callback_;
  // TODO: Update key statuses to Cobalt.
  SbDrmSessionKeyStatusesChangedFunc key_statuses_changed_callback_;

  std::mutex mutex_;
  std::atomic_bool init_data_available_ = false;
  std::condition_variable init_data_available_cv_;  // Guarded by |mutex_|..

  std::vector<uint8_t> initialization_data_;

  std::vector<uint8_t> metrics_;

  base::android::ScopedJavaGlobalRef<jobject> j_exoplayer_drm_bridge_;

  int ticket_;
};

}  // namespace starboard

#endif  // STARBOARD_ANDROID_SHARED_DRM_SYSTEM_EXOPLAYER_H_
