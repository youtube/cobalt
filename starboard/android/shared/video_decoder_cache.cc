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

#include "starboard/android/shared/video_decoder_cache.h"

#include "base/android/jni_android.h"
#include "starboard/android/shared/jni_env_ext.h"
#include "starboard/android/shared/jni_utils.h"
#include "starboard/common/log.h"
#include "starboard/common/media.h"
#include "starboard/common/player.h"

namespace starboard::android::shared {

namespace {
using base::android::ScopedJavaGlobalRef;

ScopedJavaGlobalRef<jobject> CreateDummySurfaceTexture() {
  JniEnvExt* env = JniEnvExt::Get();
  jobject local_surface_texture =
      env->NewObjectOrAbort("dev/cobalt/media/VideoSurfaceTexture", "(I)V", 0);
  ScopedJavaGlobalRef<jobject> global_ref;
  global_ref.Reset(env, local_surface_texture);
  env->DeleteLocalRef(local_surface_texture);
  SB_CHECK(global_ref.obj());
  return global_ref;
}

ScopedJavaGlobalRef<jobject> CreateDummySurface(
    const ScopedJavaGlobalRef<jobject>& surface_texture) {
  JniEnvExt* env = JniEnvExt::Get();
  jobject local_surface = env->NewObjectOrAbort(
      "android/view/Surface", "(Landroid/graphics/SurfaceTexture;)V",
      surface_texture.obj());
  ScopedJavaGlobalRef<jobject> global_ref;
  global_ref.Reset(env, local_surface);
  env->DeleteLocalRef(local_surface);
  SB_CHECK(global_ref.obj());
  return global_ref;
}

}  // namespace

// static
VideoDecoderCache* VideoDecoderCache::GetInstance() {
  static VideoDecoderCache* instance = new VideoDecoderCache();
  return instance;
}

VideoDecoderCache::VideoDecoderCache() = default;

bool VideoDecoderCache::CacheKey::operator==(const CacheKey& other) const {
  return codec == other.codec && output_mode == other.output_mode;
}

std::ostream& operator<<(std::ostream& os,
                         const VideoDecoderCache::CacheKey& key) {
  return os << "{codec=" << GetMediaVideoCodecName(key.codec)
            << ", output_mode=" << GetPlayerOutputModeName(key.output_mode)
            << "}";
}

std::string VideoDecoderCache::Put(const CacheKey& key,
                                   std::unique_ptr<MediaDecoder> decoder) {
  if (!decoder) {
    return "Invalid: decoder is null";
  }

  auto dummy_surface_texture = CreateDummySurfaceTexture();
  auto dummy_surface = CreateDummySurface(dummy_surface_texture);

  if (!decoder->SetOutputSurface(dummy_surface.obj())) {
    return "Failed to set dummy surface, destroying decoder.";
  }

  SB_LOG(INFO) << "Caching video decoder for key=" << key;

  std::lock_guard lock(mutex_);
  if (cache_.size() >= kMaxCacheSize) {
    cache_.pop_front();
  }
  cache_.push_back({key, std::move(decoder), std::move(dummy_surface_texture),
                    std::move(dummy_surface)});
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
