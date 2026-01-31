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

#include <GLES2/gl2.h>

#include "base/android/jni_android.h"
#include "base/no_destructor.h"
#include "starboard/android/shared/jni_env_ext.h"
#include "starboard/android/shared/jni_utils.h"
#include "starboard/common/log.h"
#include "starboard/common/media.h"
#include "starboard/common/player.h"

namespace starboard::android::shared {

namespace {
using base::android::ScopedJavaGlobalRef;
using ::starboard::shared::starboard::player::JobThread;

constexpr int kMaxCacheSize = 2;

ScopedJavaGlobalRef<jobject> CreateDummySurfaceTexture(int texture_id) {
  JniEnvExt* env = JniEnvExt::Get();
  jobject local_surface_texture = env->NewObjectOrAbort(
      "dev/cobalt/media/VideoSurfaceTexture", "(I)V", texture_id);
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
  static base::NoDestructor<VideoDecoderCache> instance;
  return instance.get();
}

VideoDecoderCache::VideoDecoderCache() = default;

VideoDecoderCache::~VideoDecoderCache() {
  if (job_thread_) {
    std::lock_guard lock(mutex_);
    for (const auto& entry : cache_) {
      job_thread_->Schedule([id_to_delete = entry.texture_id]() {
        glDeleteTextures(1, &id_to_delete);
      });
    }
    // We don't bother destroying EGL context here as this singleton lives
    // forever.
    job_thread_.reset();
  }
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

bool VideoDecoderCache::InitializeEgl() {
  egl_context_.display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
  if (egl_context_.display == EGL_NO_DISPLAY) {
    SB_LOG(ERROR) << "eglGetDisplay failed";
    return false;
  }

  if (!eglInitialize(egl_context_.display, nullptr, nullptr)) {
    SB_LOG(ERROR) << "eglInitialize failed";
    return false;
  }

  const EGLint config_attribs[] = {EGL_RENDERABLE_TYPE,
                                   EGL_OPENGL_ES2_BIT,
                                   EGL_RED_SIZE,
                                   8,
                                   EGL_GREEN_SIZE,
                                   8,
                                   EGL_BLUE_SIZE,
                                   8,
                                   EGL_ALPHA_SIZE,
                                   8,
                                   EGL_NONE};
  EGLConfig config;
  EGLint num_configs;
  if (!eglChooseConfig(egl_context_.display, config_attribs, &config, 1,
                       &num_configs)) {
    SB_LOG(ERROR) << "eglChooseConfig failed";
    return false;
  }

  const EGLint context_attribs[] = {EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE};
  egl_context_.context = eglCreateContext(egl_context_.display, config,
                                          EGL_NO_CONTEXT, context_attribs);
  if (egl_context_.context == EGL_NO_CONTEXT) {
    SB_LOG(ERROR) << "eglCreateContext failed";
    return false;
  }

  // To support a SurfaceTexture, we need a valid GLES environment on the thread
  // where the SurfaceTexture is created. The SurfaceTexture will be attached to
  // the texture generated in this context.
  // We use a Pbuffer surface to make the context current without requiring a
  // native window.
  const EGLint pbuffer_attribs[] = {EGL_WIDTH, 1, EGL_HEIGHT, 1, EGL_NONE};
  egl_context_.surface =
      eglCreatePbufferSurface(egl_context_.display, config, pbuffer_attribs);
  if (egl_context_.surface == EGL_NO_SURFACE) {
    SB_LOG(ERROR) << "eglCreatePbufferSurface failed";
    return false;
  }

  if (!eglMakeCurrent(egl_context_.display, egl_context_.surface,
                      egl_context_.surface, egl_context_.context)) {
    SB_LOG(ERROR) << "eglMakeCurrent failed";
    return false;
  }
  return true;
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

  {
    std::lock_guard lock(mutex_);
    if (!job_thread_) {
      job_thread_ = std::make_unique<JobThread>("VideoDecCache");
    }
  }

  ScopedJavaGlobalRef<jobject> dummy_surface_texture;
  ScopedJavaGlobalRef<jobject> dummy_surface;
  std::string error_message;
  GLuint texture_id = 0;

  // We must wait for the dummy surface to be created and the texture to be
  // generated. The caller (VideoDecoder::TeardownCodec) expects that after
  // Put() returns, the MediaCodec is no longer using the original surface
  // (which may be destroyed immediately after).
  job_thread_->ScheduleAndWait([&]() {
    if (egl_context_.display == EGL_NO_DISPLAY) {
      if (!InitializeEgl()) {
        error_message = "InitializeEgl failed";
        return;
      }
    }

    if (egl_context_.context == EGL_NO_CONTEXT) {
      error_message = "Failed to initialize EGL";
      return;
    }

    glGenTextures(1, &texture_id);
    if (texture_id == 0) {
      error_message = "Failed to generate GL texture";
      return;
    }

    dummy_surface_texture = CreateDummySurfaceTexture(texture_id);
    dummy_surface = CreateDummySurface(dummy_surface_texture);
  });

  if (!error_message.empty()) {
    return "error:" + error_message;
  }

  if (!decoder->SetOutputSurface(dummy_surface.obj())) {
    // If we fail to set the dummy surface, we must clean up the texture we just
    // created, as it won't be stored in the cache for later cleanup.
    job_thread_->Schedule(
        [id_to_delete = texture_id]() { glDeleteTextures(1, &id_to_delete); });
    return "error:Failed to set dummy surface, destroying decoder.";
  }

  std::lock_guard lock(mutex_);
  SB_LOG(INFO) << "Caching video decoder for key=" << key
               << ", num_cached=" << cache_.size();

  if (cache_.size() >= kMaxCacheSize) {
    auto& entry_to_evict = cache_.front();
    GLuint texture_id_to_delete = entry_to_evict.texture_id;
    // We must destroy the MediaDecoder (by popping it from the cache) *before*
    // scheduling the texture deletion. This ensures that the MediaCodec is
    // fully stopped and detached from the surface before the underlying GL
    // texture is destroyed, avoiding a race condition where the codec might
    // attempt to use a deleted texture.
    cache_.pop_front();
    job_thread_->Schedule([id_to_delete = texture_id_to_delete]() {
      glDeleteTextures(1, &id_to_delete);
    });
  }
  cache_.push_back({key, std::move(decoder), std::move(dummy_surface_texture),
                    std::move(dummy_surface), texture_id});
  return "";
}

std::unique_ptr<MediaDecoder> VideoDecoderCache::Get(const CacheKey& key) {
  std::lock_guard lock(mutex_);
  for (auto it = cache_.begin(); it != cache_.end(); ++it) {
    if (it->key == key) {
      SB_LOG(INFO) << "Getting cached video decoder for key=" << key;
      std::unique_ptr<MediaDecoder> decoder = std::move(it->decoder);
      decoder->SetSurfaceSwitchCallback(
          [job_thread = job_thread_.get(), texture_id = it->texture_id,
           dummy_surface_texture = std::move(it->dummy_surface_texture),
           dummy_surface = std::move(it->dummy_surface)]() mutable {
            job_thread->Schedule([id_to_delete = texture_id]() {
              glDeleteTextures(1, &id_to_delete);
            });
            // Java objects (dummy_surface and dummy_surface_texture) will be
            // released when this lambda is destroyed.
          });
      cache_.erase(it);
      return decoder;
    }
  }
  return nullptr;
}

}  // namespace starboard::android::shared
