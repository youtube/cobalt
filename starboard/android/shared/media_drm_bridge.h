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
#include <vector>

#include "base/android/jni_android.h"
#include "base/android/jni_int_wrapper.h"
#include "base/memory/raw_ref.h"
#include "starboard/drm.h"

namespace starboard::android::shared {

class MediaDrmBridge {
 public:
  class Host {
   public:
    virtual void OnSessionUpdate(int ticket,
                                 SbDrmSessionRequestType request_type,
                                 std::string_view session_id,
                                 std::string_view content) = 0;
    virtual void OnKeyStatusChange(
        std::string_view session_id,
        const std::vector<SbDrmKeyId>& drm_key_ids,
        const std::vector<SbDrmKeyStatus>& drm_key_statuses) = 0;

   protected:
    ~Host() = default;
  };

  struct OperationResult {
    // This value should match MediaDrmBridge::OperationResult::Status in
    // cobalt/android/apk/app/src/main/java/dev/cobalt/media/MediaDrmBridge.java
    enum Status {
      kSuccess = 0,
      kOperationFailed = 1,
    };

    const Status status;
    const std::string error_message;

    bool ok() const { return status == kSuccess; }
  };

  MediaDrmBridge(raw_ref<MediaDrmBridge::Host> host,
                 std::string_view key_system);
  ~MediaDrmBridge();

  MediaDrmBridge(const MediaDrmBridge&) = delete;
  MediaDrmBridge& operator=(const MediaDrmBridge&) = delete;

  bool is_valid() const {
    return !j_media_drm_bridge_.is_null() && !j_media_crypto_.is_null();
  }

  jobject GetMediaCrypto() const { return j_media_crypto_.obj(); }

  void CreateSession(int ticket,
                     std::string_view init_data,
                     std::string_view mime) const;
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
  const raw_ref<MediaDrmBridge::Host> host_;
  std::vector<uint8_t> metrics_;

  base::android::ScopedJavaGlobalRef<jobject> j_media_drm_bridge_;
  base::android::ScopedJavaGlobalRef<jobject> j_media_crypto_;
};

std::ostream& operator<<(std::ostream& os,
                         const MediaDrmBridge::OperationResult& result);

}  // namespace starboard::android::shared

#endif  // STARBOARD_ANDROID_SHARED_MEDIA_DRM_BRIDGE_H_
