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

#include <EGL/egl.h>
#include <jni.h>

#include <list>
#include <memory>
#include <mutex>
#include <optional>
#include <ostream>

#include "base/android/scoped_java_ref.h"
#include "starboard/android/shared/media_decoder.h"
#include "starboard/media.h"
#include "starboard/player.h"
#include "starboard/shared/starboard/player/job_thread.h"

namespace starboard::android::shared {

class VideoDecoderCache {
 public:
  static VideoDecoderCache* GetInstance();

  VideoDecoderCache();
  ~VideoDecoderCache();

  struct CacheKey {
    SbMediaVideoCodec codec;
    SbPlayerOutputMode output_mode;
    int frame_width;
    int frame_height;
    std::optional<int> max_width;
    std::optional<int> max_height;
    int fps;

    bool operator==(const CacheKey& other) const;
  };

  // Return error message, if it fails. If OK, return empty string.
  std::string Put(const CacheKey& key, std::unique_ptr<MediaDecoder> decoder);

  std::unique_ptr<MediaDecoder> Get(const CacheKey& key);

 private:
  struct CacheEntry {
    CacheKey key;
    std::unique_ptr<MediaDecoder> decoder;
    base::android::ScopedJavaGlobalRef<jobject> dummy_surface_texture;
    base::android::ScopedJavaGlobalRef<jobject> dummy_surface;
  };

  struct EglContext {
    EGLDisplay display = EGL_NO_DISPLAY;
    EGLContext context = EGL_NO_CONTEXT;
    EGLSurface surface = EGL_NO_SURFACE;
  };

  void InitializeEgl();
  void DestroyEgl();

  static constexpr int kMaxCacheSize = 4;
  std::mutex mutex_;
  std::list<CacheEntry> cache_;
  std::unique_ptr<::starboard::shared::starboard::player::JobThread> job_thread_;
  EglContext egl_context_;
};

std::ostream& operator<<(std::ostream& os,
                         const VideoDecoderCache::CacheKey& key);

}  // namespace starboard::android::shared

#endif  // STARBOARD_ANDROID_SHARED_VIDEO_DECODER_CACHE_H_
