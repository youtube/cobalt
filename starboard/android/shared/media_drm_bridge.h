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

#ifndef STARBOARD_ANDROID_SHARED_MEDIA_DRM_BRIDGE_H_
#define STARBOARD_ANDROID_SHARED_MEDIA_DRM_BRIDGE_H_

#include <jni.h>

#include <ostream>
#include <string>
#include <string_view>
#include <vector>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ref.h"
#include "starboard/common/pass_key.h"
#include "starboard/drm.h"

namespace starboard {

// A "status" for MediaDrmBridge operations.
// GENERATED_JAVA_ENUM_PACKAGE: dev.cobalt.media
enum DrmOperationStatus {
  DRM_OPERATION_STATUS_SUCCESS,
  DRM_OPERATION_STATUS_OPERATION_FAILED,
  DRM_OPERATION_STATUS_NOT_PROVISIONED,
};

class MediaDrmBridge {
 public:
  class Host {
   public:
    virtual void OnSessionUpdate(int ticket,
                                 SbDrmSessionRequestType request_type,
                                 std::string_view session_id,
                                 std::string_view content) = 0;
    virtual void OnProvisioningRequest(std::string_view content) = 0;
    virtual void OnKeyStatusChange(
        std::string_view session_id,
        const std::vector<SbDrmKeyId>& drm_key_ids,
        const std::vector<SbDrmKeyStatus>& drm_key_statuses) = 0;

   protected:
    ~Host() = default;
  };

  struct OperationResult {
    DrmOperationStatus status = DRM_OPERATION_STATUS_SUCCESS;
    std::string error_message;

    bool ok() const { return status == DRM_OPERATION_STATUS_SUCCESS; }
  };

  static std::unique_ptr<MediaDrmBridge> Create(
      base::raw_ref<MediaDrmBridge::Host> host,
      std::string_view key_system,
      bool enable_app_provisioning);

  MediaDrmBridge(PassKey<MediaDrmBridge>,
                 base::raw_ref<MediaDrmBridge::Host> host);
  ~MediaDrmBridge();

  MediaDrmBridge(const MediaDrmBridge&) = delete;
  MediaDrmBridge& operator=(const MediaDrmBridge&) = delete;

  jobject GetMediaCrypto() const { return j_media_crypto_.obj(); }

  void CreateSession(int ticket,
                     std::string_view init_data,
                     std::string_view mime) const;

  OperationResult CreateSessionWithAppProvisioning(int ticket,
                                                   std::string_view init_data,
                                                   std::string_view mime) const;
  std::string GenerateProvisionRequest() const;
  OperationResult ProvideProvisionResponse(std::string_view response) const;

  OperationResult UpdateSession(int ticket,
                                std::string_view key,
                                std::string_view session_id) const;
  void CloseSession(std::string_view session_id) const;
  const void* GetMetrics(int* size);
  bool CreateMediaCryptoSession();

  void OnSessionMessage(
      JNIEnv* env,
      jint ticket,
      const base::android::JavaParamRef<jbyteArray>& session_id,
      jint request_type,
      const base::android::JavaParamRef<jbyteArray>& message);
  void OnKeyStatusChange(
      JNIEnv* env,
      const base::android::JavaParamRef<jbyteArray>& session_id,
      const base::android::JavaParamRef<jobjectArray>& key_information);

  static bool IsWidevineSupported(JNIEnv* env);
  static bool IsCbcsSupported(JNIEnv* env);

 private:
  bool Initialize(std::string_view key_system, bool enable_app_provisioning);

  const base::raw_ref<MediaDrmBridge::Host> host_;
  std::vector<uint8_t> metrics_;

  // The factory method guarantees that |j_media_drm_bridge_| is non-null.
  // Instances are only returned if initialization succeeds; therefore, this
  // member is guaranteed to be valid for the lifetime of the object.
  base::android::ScopedJavaGlobalRef<jobject> j_media_drm_bridge_;

  // |j_media_crypto_| is non-null after initialization, but may be reset
  // via CreateMediaCryptoSession() if a session creation failure occurs.
  base::android::ScopedJavaGlobalRef<jobject> j_media_crypto_;
};

std::ostream& operator<<(std::ostream& os, DrmOperationStatus);
std::ostream& operator<<(std::ostream& os,
                         const MediaDrmBridge::OperationResult& result);

}  // namespace starboard

#endif  // STARBOARD_ANDROID_SHARED_MEDIA_DRM_BRIDGE_H_
