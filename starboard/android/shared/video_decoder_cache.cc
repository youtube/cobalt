// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/android/shared/video_decoder_cache.h"

#include "base/no_destructor.h"
#include "starboard/common/log.h"
#include "starboard/common/media.h"
#include "starboard/common/player.h"

namespace starboard::android::shared {

namespace {
constexpr int kMaxCacheSize = 2;
}  // namespace

// static
VideoDecoderCache* VideoDecoderCache::GetInstance() {
  static base::NoDestructor<VideoDecoderCache> instance;
  return instance.get();
}

VideoDecoderCache::VideoDecoderCache() = default;

VideoDecoderCache::~VideoDecoderCache() {
  // VideoDecoderCache is a singleton, so it shouldn't be destroyed.
  // But if it is, the unique_ptrs in cache_ will be destroyed, which destroys
  // the MediaDecoders.
}

bool VideoDecoderCache::CacheKey::operator==(const CacheKey& other) const {
  return codec == other.codec && output_mode == other.output_mode &&
         frame_width == other.frame_width &&
         frame_height == other.frame_height && max_width == other.max_width &&
         max_height == other.max_height && fps == other.fps;
}

std::ostream& operator<<(std::ostream& os,
                         const VideoDecoderCache::CacheKey& key) {
  os << "{codec=" << GetMediaVideoCodecName(key.codec)
     << ", output_mode=" << GetPlayerOutputModeName(key.output_mode)
     << ", frame_width=" << key.frame_width
     << ", frame_height=" << key.frame_height << ", max_width=";
  if (key.max_width) {
    os << *key.max_width;
  } else {
    os << "nullopt";
  }
  os << ", max_height=";
  if (key.max_height) {
    os << *key.max_height;
  } else {
    os << "nullopt";
  }
  os << ", fps=" << key.fps << "}";
  return os;
}

std::string VideoDecoderCache::Put(const CacheKey& key,
                                   std::unique_ptr<MediaDecoder> decoder) {
  if (!decoder) {
    return "error:Invalid: decoder is null";
  }

  if (decoder->is_secure()) {
    return "policy: we don't cache secure decoders";
  }

  // We caches decoder only for short.
  const bool is_cacheable_size = [&] {
    if (key.frame_width == 1080 && key.frame_height == 1920) {
      return true;
    }
    if (key.frame_width == 720 && key.frame_height == 1280) {
      return true;
    }
    return false;
  }();
  if (!is_cacheable_size) {
    return "policy: we don't cache the given resolution";
  }

  // With the new stop() approach, the decoder is already stopped by
  // MediaDecoder::Suspend() (which calls media_codec_bridge->Stop()).
  // We just need to store it.

  std::lock_guard lock(mutex_);
  SB_LOG(INFO) << "Caching video decoder for key=" << key
               << ", num_cached=" << cache_.size();

  if (cache_.size() >= kMaxCacheSize) {
    cache_.pop_front();
  }
  cache_.push_back({key, std::move(decoder)});
  return "";
}

std::unique_ptr<MediaDecoder> VideoDecoderCache::Get(const CacheKey& key) {
  std::lock_guard lock(mutex_);
  for (auto it = cache_.begin(); it != cache_.end(); ++it) {
    if (it->key == key) {
      SB_LOG(INFO) << "Getting cached video decoder for key=" << key;
      std::unique_ptr<MediaDecoder> decoder = std::move(it->decoder);
      cache_.erase(it);
      return decoder;
    }
  }
  return nullptr;
}

}  // namespace starboard::android::shared
