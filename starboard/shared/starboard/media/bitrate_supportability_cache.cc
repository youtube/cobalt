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

#include "starboard/shared/starboard/media/bitrate_supportability_cache.h"

#include <map>

#include "starboard/common/log.h"
#include "starboard/common/mutex.h"
#include "starboard/log.h"
#include "starboard/media.h"
#include "starboard/once.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace media {

namespace {

template <typename T>
class BitrateSupportabilityContainer {
 public:
  Supportability GetSupportability(T codec, int bitrate) {
    // Reject invalid parameters.
    if (bitrate < 0) {
      return kSupportabilityNotSupported;
    }
    // Bitrate 0 is always supported.
    if (bitrate == 0) {
      return kSupportabilitySupported;
    }

    ScopedLock scoped_lock(mutex_);
    auto iter = supported_bitrate_ranges_.find(codec);
    if (iter == supported_bitrate_ranges_.end()) {
      return kSupportabilityUnknown;
    }
    const Range& range = iter->second;
    if (bitrate < range.minimum || bitrate > range.maximum) {
      return kSupportabilityNotSupported;
    }
    return kSupportabilitySupported;
  }

  void SetSupportedBitrate(T codec, int min, int max) {
    SB_DCHECK(min >= 0 && max >= min);

    ScopedLock scoped_lock(mutex_);
    supported_bitrate_ranges_[codec] = Range(min, max);
  }
  void ClearContainer() {
    ScopedLock scoped_lock(mutex_);
    supported_bitrate_ranges_.clear();
  }

 private:
  struct Range {
    Range() : minimum(0), maximum(0) {}
    Range(int min, int max) : minimum(min), maximum(max) {}
    int minimum;
    int maximum;
  };

  Mutex mutex_;
  std::map<T, Range> supported_bitrate_ranges_;
};

template <typename T>
SB_ONCE_INITIALIZE_FUNCTION(BitrateSupportabilityContainer<T>, GetContainer);

}  // namespace

// static
SB_ONCE_INITIALIZE_FUNCTION(BitrateSupportabilityCache,
                            BitrateSupportabilityCache::GetInstance);

Supportability BitrateSupportabilityCache::GetBitrateSupportability(
    const ParsedMimeInfo& mime_info) {
  SB_DCHECK(mime_info.is_valid());

  if (!is_enabled_) {
    return kSupportabilityUnknown;
  }

  Supportability audio_supportability = kSupportabilitySupported;
  if (mime_info.has_audio_info()) {
    audio_supportability = GetContainer<SbMediaAudioCodec>()->GetSupportability(
        mime_info.audio_info().codec, mime_info.audio_info().bitrate);
  }

  Supportability video_supportability = kSupportabilitySupported;
  if (mime_info.has_video_info()) {
    video_supportability = GetContainer<SbMediaVideoCodec>()->GetSupportability(
        mime_info.video_info().codec, mime_info.video_info().bitrate);
  }

  if (audio_supportability == kSupportabilityNotSupported ||
      video_supportability == kSupportabilityNotSupported) {
    return kSupportabilityNotSupported;
  }
  if (audio_supportability == kSupportabilityUnknown ||
      video_supportability == kSupportabilityUnknown) {
    return kSupportabilityUnknown;
  }
  return kSupportabilitySupported;
}

void BitrateSupportabilityCache::SetSupportedBitrate(SbMediaAudioCodec codec,
                                                     int min,
                                                     int max) {
  SB_DCHECK(min >= 0 && min <= max) << "Invalid bitrate range.";

  if (!is_enabled_) {
    return;
  }
  GetContainer<SbMediaAudioCodec>()->SetSupportedBitrate(codec, min, max);
}

void BitrateSupportabilityCache::SetSupportedBitrate(SbMediaVideoCodec codec,
                                                     int min,
                                                     int max) {
  SB_DCHECK(min >= 0 && min <= max) << "Invalid bitrate range.";

  if (!is_enabled_) {
    return;
  }
  GetContainer<SbMediaVideoCodec>()->SetSupportedBitrate(codec, min, max);
}

void BitrateSupportabilityCache::ClearCache() {
  GetContainer<SbMediaAudioCodec>()->ClearContainer();
  GetContainer<SbMediaVideoCodec>()->ClearContainer();
}

}  // namespace media
}  // namespace starboard
}  // namespace shared
}  // namespace starboard
