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

#ifndef STARBOARD_ANDROID_SHARED_VIDEO_DECODER_CACHE_H_
#define STARBOARD_ANDROID_SHARED_VIDEO_DECODER_CACHE_H_

#include <jni.h>

#include <list>
#include <memory>
#include <mutex>

#include "base/android/scoped_java_ref.h"
#include "starboard/android/shared/media_decoder.h"
#include "starboard/media.h"
#include "starboard/player.h"

namespace starboard::android::shared {

class VideoDecoderCache {
 public:
  static VideoDecoderCache* GetInstance();

  VideoDecoderCache();
  ~VideoDecoderCache() = default;

  void Put(std::unique_ptr<MediaDecoder> decoder,
           SbMediaVideoCodec codec,
           SbPlayerOutputMode output_mode);

  std::unique_ptr<MediaDecoder> Get(SbMediaVideoCodec codec,
                                    SbPlayerOutputMode output_mode);

 private:
  struct CacheEntry {
    std::unique_ptr<MediaDecoder> decoder;
    SbMediaVideoCodec codec;
    SbPlayerOutputMode output_mode;
  };

  static constexpr int kMaxCacheSize = 4;
  std::mutex mutex_;
  std::list<CacheEntry> cache_;

  base::android::ScopedJavaGlobalRef<jobject> dummy_surface_texture_;
  base::android::ScopedJavaGlobalRef<jobject> dummy_surface_;
};

}  // namespace starboard::android::shared

#endif  // STARBOARD_ANDROID_SHARED_VIDEO_DECODER_CACHE_H_
