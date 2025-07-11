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

#include <string>
#include <string_view>
#include <vector>

#include "base/android/jni_android.h"
#include "base/android/jni_int_wrapper.h"
#include "base/memory/raw_ref.h"
#include "starboard/drm.h"

namespace starboard::android::shared {

using base::android::JavaParamRef;
using base::android::JavaRef;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;

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

  struct Status {
    enum Type {
      kSuccess = 0,
      kOperationError = 1,
      kNotProvisionedError = 2,
    };

    const Type type;
    const std::string error_message;

    bool ok() const { return type == kSuccess; }
  };

  MediaDrmBridge(raw_ref<MediaDrmBridge::Host> host,
                 const char* key_system,
                 bool use_app_provisioning);
  ~MediaDrmBridge();

  MediaDrmBridge(const MediaDrmBridge&) = delete;
  MediaDrmBridge& operator=(const MediaDrmBridge&) = delete;

  bool is_valid() const {
    return !j_media_drm_bridge_.is_null() && !j_media_crypto_.is_null();
  }

  jobject GetMediaCrypto() const { return j_media_crypto_.obj(); }

  void CreateSession(int ticket,
                     const std::vector<const uint8_t>& init_data,
                     const std::string& mime) const;

  Status CreateSessionNoProvisioning(
      int ticket,
      const std::vector<const uint8_t>& init_data,
      const std::string& mime) const;
  void GenerateProvisionRequest() const;
  Status ProvideProvisionResponse(const void* response,
                                  int response_size) const;

  Status UpdateSession(int ticket,
                       const void* key,
                       int key_size,
                       const void* session_id,
                       int session_id_size,
                       std::string* error_msg) const;
  void CloseSession(const std::string& session_id) const;
  const void* GetMetrics(int* size);
  bool CreateMediaCryptoSession();

  void OnSessionMessage(JNIEnv* env,
                        jint ticket,
                        const JavaParamRef<jbyteArray>& session_id,
                        jint request_type,
                        const JavaParamRef<jbyteArray>& message);
  void OnKeyStatusChange(JNIEnv* env,
                         const JavaParamRef<jbyteArray>& session_id,
                         const JavaParamRef<jobjectArray>& key_information);
  void OnProvisioningRequestMessage(JNIEnv* env,
                                    const JavaParamRef<jbyteArray>& message);

  static bool IsWidevineSupported(JNIEnv* env);
  static bool IsCbcsSupported(JNIEnv* env);

 private:
  const raw_ref<MediaDrmBridge::Host> host_;
  std::vector<uint8_t> metrics_;

  ScopedJavaGlobalRef<jobject> j_media_drm_bridge_;
  ScopedJavaGlobalRef<jobject> j_media_crypto_;
};

}  // namespace starboard::android::shared

#endif  // STARBOARD_ANDROID_SHARED_MEDIA_DRM_BRIDGE_H_
