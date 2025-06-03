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
#include "base/memory/raw_ptr.h"
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
    virtual void CallUpdateRequestCallback(int ticket,
                                           SbDrmSessionRequestType request_type,
                                           const void* session_id,
                                           int session_id_size,
                                           const void* content,
                                           int content_size,
                                           const char* url) = 0;
    virtual void CallDrmSessionKeyStatusesChangedCallback(
        const void* session_id,
        int session_id_size,
        const std::vector<SbDrmKeyId>& drm_key_ids,
        const std::vector<SbDrmKeyStatus>& drm_key_statuses) = 0;

   protected:
    ~Host() = default;
  };

  MediaDrmBridge(MediaDrmBridge::Host* host, const char* key_system);
  ~MediaDrmBridge();

  MediaDrmBridge(const MediaDrmBridge&) = delete;
  MediaDrmBridge& operator=(const MediaDrmBridge&) = delete;

  bool is_valid() const {
    return !j_media_drm_bridge_.is_null() && !j_media_crypto_.is_null();
  }

  Host* host() const { return host_.get(); }

  jobject GetMediaCrypto() const { return j_media_crypto_.obj(); }

  void CreateSession(int ticket,
                     const std::vector<const uint8_t>& init_data,
                     const std::string& mime) const;
  // Updates the session. Returns true on success.
  bool UpdateSession(int ticket,
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
                        const JavaParamRef<jbyteArray>& sessionId,
                        jint requestType,
                        const JavaParamRef<jbyteArray>& message);
  void OnKeyStatusChange(JNIEnv* env,
                         const JavaParamRef<jbyteArray>& sessionId,
                         const JavaParamRef<jobjectArray>& keyInformation);

  static bool IsWidevineSupported(JNIEnv* env);
  static bool IsCbcsSupported(JNIEnv* env);

 private:
  const raw_ptr<MediaDrmBridge::Host> host_;
  std::vector<uint8_t> metrics_;

  ScopedJavaGlobalRef<jobject> j_media_drm_bridge_;
  ScopedJavaGlobalRef<jobject> j_media_crypto_;
};

}  // namespace starboard::android::shared

#endif  // STARBOARD_ANDROID_SHARED_MEDIA_DRM_BRIDGE_H_
