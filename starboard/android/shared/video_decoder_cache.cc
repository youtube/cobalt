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

namespace starboard::android::shared {

// static
VideoDecoderCache* VideoDecoderCache::GetInstance() {
  static VideoDecoderCache* instance = new VideoDecoderCache();
  return instance;
}

VideoDecoderCache::VideoDecoderCache() {
  base::android::AttachCurrentThread();
  JniEnvExt* env = JniEnvExt::Get();
  jobject local_surface_texture =
      env->NewObjectOrAbort("dev/cobalt/media/VideoSurfaceTexture", "(I)V", 0);
  dummy_surface_texture_.Reset(env, local_surface_texture);
  env->DeleteLocalRef(local_surface_texture);

  if (dummy_surface_texture_.obj()) {
    jobject local_surface = env->NewObjectOrAbort(
        "android/view/Surface", "(Landroid/graphics/SurfaceTexture;)V",
        dummy_surface_texture_.obj());
    dummy_surface_.Reset(env, local_surface);
    env->DeleteLocalRef(local_surface);
  }
}

void VideoDecoderCache::Put(std::unique_ptr<MediaDecoder> decoder,
                            SbMediaVideoCodec codec,
                            SbPlayerOutputMode output_mode) {
  if (!decoder) {
    return;
  }

  if (dummy_surface_) {
    if (!decoder->SetOutputSurface(dummy_surface_.obj())) {
      SB_LOG(WARNING)
          << "Failed to switch to dummy surface, destroying decoder.";
      return;
    }
  }

  SB_LOG(INFO) << "Caching video decoder for " << GetMediaVideoCodecName(codec);
  std::lock_guard lock(mutex_);
  if (cache_.size() >= kMaxCacheSize) {
    cache_.pop_front();
  }
  cache_.push_back({std::move(decoder), codec, output_mode});
}

std::unique_ptr<MediaDecoder> VideoDecoderCache::Get(
    SbMediaVideoCodec codec,
    SbPlayerOutputMode output_mode) {
  std::lock_guard lock(mutex_);
  for (auto it = cache_.begin(); it != cache_.end(); ++it) {
    if (it->codec == codec && it->output_mode == output_mode) {
      std::unique_ptr<MediaDecoder> decoder = std::move(it->decoder);
      cache_.erase(it);
      return decoder;
    }
  }
  return nullptr;
}

}  // namespace starboard::android::shared
