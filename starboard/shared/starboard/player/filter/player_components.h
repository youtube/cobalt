// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

#ifndef STARBOARD_SHARED_STARBOARD_PLAYER_FILTER_PLAYER_COMPONENTS_H_
#define STARBOARD_SHARED_STARBOARD_PLAYER_FILTER_PLAYER_COMPONENTS_H_

#include <memory>
#include <string>
#include <vector>

#include "build/build_config.h"
#include "starboard/common/check_op.h"
#include "starboard/common/log.h"
#include "starboard/common/ref_counted.h"
#include "starboard/common/result.h"
#include "starboard/decode_target.h"
#include "starboard/drm.h"
#include "starboard/media.h"
#include "starboard/player.h"
#include "starboard/shared/internal_only.h"
#include "starboard/shared/starboard/media/media_util.h"
#include "starboard/shared/starboard/player/filter/audio_decoder_internal.h"
#include "starboard/shared/starboard/player/filter/audio_renderer_internal.h"
#include "starboard/shared/starboard/player/filter/audio_renderer_sink.h"
#include "starboard/shared/starboard/player/filter/media_time_provider.h"
#include "starboard/shared/starboard/player/filter/video_decoder_internal.h"
#include "starboard/shared/starboard/player/filter/video_render_algorithm.h"
#include "starboard/shared/starboard/player/filter/video_renderer_internal.h"
#include "starboard/shared/starboard/player/filter/video_renderer_sink.h"

namespace starboard {

// This class holds necessary media stack components required by
// by |FilterBasedPlayerWorkerHandler| to function.  It owns the components, and
// the returned value of each function won't change over the lifetime of this
// object, so it is safe to cache the returned objects.
class PlayerComponents {
 public:
  // This class creates PlayerComponents.
  class Factory {
   public:
    class CreationParameters {
     public:
      explicit CreationParameters(const AudioStreamInfo& audio_stream_info,
                                  SbDrmSystem drm_system = kSbDrmSystemInvalid);
      CreationParameters(const VideoStreamInfo& video_stream_info,
                         SbPlayer player,
                         SbPlayerOutputMode output_mode,
                         int max_video_input_size,
                         SbDecodeTargetGraphicsContextProvider*
                             decode_target_graphics_context_provider,
                         SbDrmSystem drm_system = kSbDrmSystemInvalid);
      CreationParameters(const AudioStreamInfo& audio_stream_info,
                         const VideoStreamInfo& video_stream_info,
                         SbPlayer player,
                         SbPlayerOutputMode output_mode,
                         int max_video_input_size,
                         SbDecodeTargetGraphicsContextProvider*
                             decode_target_graphics_context_provider,
                         SbDrmSystem drm_system = kSbDrmSystemInvalid);
      CreationParameters(const CreationParameters& that) = default;
      void operator=(const CreationParameters& that) = delete;

      void reset_audio_codec() {
        audio_stream_info_.codec = kSbMediaAudioCodecNone;
      }
      void reset_video_codec() {
        video_stream_info_.codec = kSbMediaVideoCodecNone;
      }

      SbMediaAudioCodec audio_codec() const { return audio_stream_info_.codec; }

      const AudioStreamInfo& audio_stream_info() const {
        SB_DCHECK_NE(audio_stream_info_.codec, kSbMediaAudioCodecNone);
        return audio_stream_info_;
      }

      SbMediaVideoCodec video_codec() const { return video_stream_info_.codec; }

      const std::string& audio_mime() const {
        SB_DCHECK_NE(audio_stream_info_.codec, kSbMediaAudioCodecNone);
        return audio_stream_info_.mime;
      }

      const VideoStreamInfo& video_stream_info() const {
        SB_DCHECK_NE(video_stream_info_.codec, kSbMediaVideoCodecNone);
        return video_stream_info_;
      }

      const std::string& video_mime() const {
        SB_DCHECK_NE(video_stream_info_.codec, kSbMediaVideoCodecNone);
        return video_stream_info_.mime;
      }

      const std::string& max_video_capabilities() const {
        SB_DCHECK_NE(video_stream_info_.codec, kSbMediaVideoCodecNone);
        return video_stream_info_.max_video_capabilities;
      }

      SbPlayer player() const { return player_; }
      SbPlayerOutputMode output_mode() const { return output_mode_; }
      int max_video_input_size() const { return max_video_input_size_; }
      SbDecodeTargetGraphicsContextProvider*
      decode_target_graphics_context_provider() const {
        SB_DCHECK_NE(video_stream_info_.codec, kSbMediaVideoCodecNone);
        return decode_target_graphics_context_provider_;
      }

      SbDrmSystem drm_system() const { return drm_system_; }

     private:
      // |audio_stream_info_.codec| can be set to kSbMediaAudioCodecNone for
      // audioless video.
      AudioStreamInfo audio_stream_info_;

      // The following members are only used by the video stream, and only need
      // to be set when |video_stream_info_.codec| isn't kSbMediaVideoCodecNone.
      // |video_stream_info_.codec| can be set to kSbMediaVideoCodecNone for
      // audio only video.
      VideoStreamInfo video_stream_info_;
      SbPlayer player_ = kSbPlayerInvalid;
      SbPlayerOutputMode output_mode_ = kSbPlayerOutputModeInvalid;
      int max_video_input_size_ = 0;
      SbDecodeTargetGraphicsContextProvider*
          decode_target_graphics_context_provider_ = nullptr;

      // The following member are used by both the audio stream and the video
      // stream, when they are encrypted.
      SbDrmSystem drm_system_ = kSbDrmSystemInvalid;
    };

    virtual ~Factory() {}

    // TODO: Consider making it return Factory*.
    // Individual platform should implement this function to allow the creation
    // of a Factory instance.
    static std::unique_ptr<Factory> Create();

    // Individual implementations must implement this function to indicate which
    // output modes they support.
    static bool OutputModeSupported(SbPlayerOutputMode output_mode,
                                    SbMediaVideoCodec codec,
                                    SbDrmSystem drm_system);

    virtual NonNullResult<std::unique_ptr<PlayerComponents>> CreateComponents(
        const CreationParameters& creation_parameters);

    struct AudioComponents {
      std::unique_ptr<AudioDecoder> decoder;
      std::unique_ptr<AudioRendererSink> renderer_sink;
    };
    struct VideoComponents {
      std::unique_ptr<VideoDecoder> decoder;
      std::unique_ptr<VideoRenderAlgorithm> render_algorithm;
      scoped_refptr<VideoRendererSink> renderer_sink;
    };
    struct MediaComponents {
      AudioComponents audio;
      VideoComponents video;
    };

#if BUILDFLAG(COBALT_IS_RELEASE_BUILD)
   private:
#endif  // BUILDFLAG(COBALT_IS_RELEASE_BUILD)

    // Note that the following function is exposed in non-Gold build to allow
    // unit tests to run.
    virtual Result<MediaComponents> CreateSubComponents(
        const CreationParameters& creation_parameters) = 0;

   protected:
    Factory() {}

    AudioComponents CreateStubAudioComponents(
        const CreationParameters& creation_parameters);

    VideoComponents CreateStubVideoComponents(
        const CreationParameters& creation_parameters);

    // Check AudioRenderer ctor for more details on the parameters.
    virtual void GetAudioRendererParams(
        const CreationParameters& creation_parameters,
        int* max_cached_frames,
        int* min_frames_per_append) const;

   private:
    Factory(const Factory&) = delete;
    void operator=(const Factory&) = delete;
  };

  PlayerComponents() = default;
  virtual ~PlayerComponents() {}

  virtual MediaTimeProvider* GetMediaTimeProvider() = 0;
  virtual AudioRenderer* GetAudioRenderer() = 0;
  virtual VideoRenderer* GetVideoRenderer() = 0;

 private:
  PlayerComponents(const PlayerComponents&) = delete;
  void operator=(const PlayerComponents&) = delete;
};

}  // namespace starboard

#endif  // STARBOARD_SHARED_STARBOARD_PLAYER_FILTER_PLAYER_COMPONENTS_H_
