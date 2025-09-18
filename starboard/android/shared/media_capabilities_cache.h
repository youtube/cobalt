// Copyright 2022 The Cobalt Authors. All Rights Reserved.
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

#ifndef STARBOARD_ANDROID_SHARED_MEDIA_CAPABILITIES_CACHE_H_
#define STARBOARD_ANDROID_SHARED_MEDIA_CAPABILITIES_CACHE_H_

#include <jni.h>
#include <atomic>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include <mutex>

#include "base/android/jni_android.h"
#include "starboard/media.h"
#include "starboard/shared/internal_only.h"

namespace starboard::android::shared {

class MediaCapabilitiesCache {
 public:
  virtual ~MediaCapabilitiesCache() = default;

  static MediaCapabilitiesCache* GetInstance() { return nullptr; }

  virtual bool IsWidevineSupported() = 0;
  virtual bool IsCbcsSchemeSupported() = 0;

  virtual bool IsHDRTransferCharacteristicsSupported(
      SbMediaTransferId transfer_id) = 0;

  virtual bool IsPassthroughSupported(SbMediaAudioCodec codec) = 0;

  virtual bool GetAudioConfiguration(
      int index,
      SbMediaAudioConfiguration* configuration) = 0;

  virtual bool HasAudioDecoderFor(const std::string& mime_type,
                                  int bitrate) = 0;

  virtual bool HasVideoDecoderFor(const std::string& mime_type,
                                  bool must_support_secure,
                                  bool must_support_hdr,
                                  bool must_support_tunnel_mode,
                                  int frame_width,
                                  int frame_height,
                                  int bitrate,
                                  int fps) = 0;

  virtual std::string FindAudioDecoder(const std::string& mime_type,
                                       int bitrate) = 0;

  virtual std::string FindVideoDecoder(const std::string& mime_type,
                                       bool must_support_secure,
                                       bool must_support_hdr,
                                       bool require_software_codec,
                                       bool must_support_tunnel_mode,
                                       int frame_width,
                                       int frame_height,
                                       int bitrate,
                                       int fps) = 0;

  virtual bool IsEnabled() const = 0;
  virtual void SetCacheEnabled(bool enabled) = 0;
  virtual void ClearCache() = 0;

 protected:
  MediaCapabilitiesCache() = default;
  MediaCapabilitiesCache(const MediaCapabilitiesCache&) = delete;
  MediaCapabilitiesCache& operator=(const MediaCapabilitiesCache&) = delete;
};

}  // namespace starboard::android::shared

#endif  // STARBOARD_ANDROID_SHARED_MEDIA_CAPABILITIES_CACHE_H_
