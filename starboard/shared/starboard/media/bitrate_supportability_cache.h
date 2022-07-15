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

#ifndef STARBOARD_SHARED_STARBOARD_MEDIA_BITRATE_SUPPORTABILITY_CACHE_H_
#define STARBOARD_SHARED_STARBOARD_MEDIA_BITRATE_SUPPORTABILITY_CACHE_H_

#include "starboard/media.h"
#include "starboard/shared/internal_only.h"
#include "starboard/shared/starboard/media/mime_supportability_cache.h"
#include "starboard/shared/starboard/media/parsed_mime_info.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace media {

// TODO: add unit tests for BitrateSupportabilityCache
class BitrateSupportabilityCache {
 public:
  static BitrateSupportabilityCache* GetInstance();

  // When cache is not enabled, GetBitrateSupportability() will always return
  // kSupportabilityUnknown, and SetSupportedBitrate() will do nothing.
  bool IsEnabled() const { return is_enabled_; }
  void SetCacheEnabled(bool enabled) { is_enabled_ = enabled; }

  // Get bitrate supportability.
  Supportability GetBitrateSupportability(const ParsedMimeInfo& mime_info);
  // Set supported bitrate range for the |codec|. Note that if supported bitrate
  // range is not set, MimeSupportabilityCache::GetMimeSupportability() will
  // always return kSupportabilityUnknown.
  void SetSupportedBitrate(SbMediaAudioCodec codec, int min, int max);
  void SetSupportedBitrate(SbMediaVideoCodec codec, int min, int max);

  // Clear all cached supported bitrate ranges.
  void ClearCache();

 private:
  // Class can only be instanced via the singleton
  BitrateSupportabilityCache() {}
  ~BitrateSupportabilityCache() {}

  BitrateSupportabilityCache(const BitrateSupportabilityCache&) = delete;
  BitrateSupportabilityCache& operator=(const BitrateSupportabilityCache&) =
      delete;

  std::atomic_bool is_enabled_{false};
};

}  // namespace media
}  // namespace starboard
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_STARBOARD_MEDIA_BITRATE_SUPPORTABILITY_CACHE_H_
