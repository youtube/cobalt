// Copyright 2024 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_MEDIA_BASE_PIPELINE_HOLDER_H_
#define COBALT_MEDIA_BASE_PIPELINE_HOLDER_H_

#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "cobalt/media/base/chrome_media.h"
#include "cobalt/media/base/pipeline.h"

namespace cobalt {
namespace media {

class PipelineHolder {
 public:
  PipelineHolder() = default;

  bool is_m114() const { return pipeline_m114_ != nullptr; }

  void set(Pipeline<ChromeMediaM96>* pipeline) {
    DCHECK(pipeline);
    DCHECK(!pipeline_m96_);
    DCHECK(!pipeline_m114_);
    pipeline_m96_ = pipeline;
  }

  void set(Pipeline<ChromeMediaM114>* pipeline) {
    DCHECK(pipeline);
    DCHECK(!pipeline_m96_);
    DCHECK(!pipeline_m114_);
    pipeline_m114_ = pipeline;
  }

  template <typename SbPlayerPipelincPtrType>
  SbPlayerPipelincPtrType As();

  template <>
  Pipeline<ChromeMediaM96>* As<Pipeline<ChromeMediaM96>*>() {
    DCHECK(pipeline_m96_);
    return pipeline_m96_.get();
  }

  template <>
  Pipeline<ChromeMediaM114>* As<Pipeline<ChromeMediaM114>*>() {
    DCHECK(pipeline_m114_);
    return pipeline_m114_.get();
  }

#define DECLARE_MEDIA_PIPELINE_GETTER(value_type, function_name) \
  value_type function_name() const {                             \
    if (pipeline_m96_) {                                         \
      return pipeline_m96_->function_name();                     \
    }                                                            \
    DCHECK(pipeline_m114_);                                      \
    return pipeline_m114_->function_name();                      \
  }

#define DECLARE_MEDIA_PIPELINE_SETTER(value_type, function_name) \
  void function_name(value_type value) const {                   \
    if (pipeline_m96_) {                                         \
      pipeline_m96_->function_name(value);                       \
      return;                                                    \
    }                                                            \
    DCHECK(pipeline_m114_);                                      \
    pipeline_m114_->function_name(value);                        \
  }

  DECLARE_MEDIA_PIPELINE_GETTER(bool, HasAudio)
  DECLARE_MEDIA_PIPELINE_GETTER(bool, HasVideo)
  DECLARE_MEDIA_PIPELINE_GETTER(float, GetPlaybackRate)
  DECLARE_MEDIA_PIPELINE_SETTER(float, SetPlaybackRate)
  DECLARE_MEDIA_PIPELINE_SETTER(float, SetVolume)
  DECLARE_MEDIA_PIPELINE_GETTER(base::TimeDelta, GetMediaTime)
  DECLARE_MEDIA_PIPELINE_GETTER(base::TimeDelta, GetMediaDuration)

  template <typename ChromeMedia>
  void Start(typename ChromeMedia::Demuxer* demuxer,
             const SetDrmSystemReadyCB& set_drm_system_ready_cb,
             const typename ChromeMedia::PipelineStatusCB& ended_cb,
             const typename Pipeline<ChromeMedia>::ErrorCB& error_cb,
             const typename Pipeline<ChromeMedia>::SeekCB& seek_cb,
             const BufferingStateCB& buffering_state_cb,
             const base::Closure& duration_change_cb,
             const base::Closure& output_mode_change_cb,
             const base::Closure& content_size_change_cb,
             const std::string& max_video_capabilities,
             const int max_video_input_size) {
    DCHECK(As<Pipeline<ChromeMedia>*>());
    As<Pipeline<ChromeMedia>*>()->Start(
        demuxer, set_drm_system_ready_cb, ended_cb, error_cb, seek_cb,
        buffering_state_cb, duration_change_cb, output_mode_change_cb,
        content_size_change_cb, max_video_capabilities, max_video_input_size);
  }

  template <typename ChromeMedia>
  void Seek(base::TimeDelta time,
            const typename Pipeline<ChromeMedia>::SeekCB& seek_cb) {
    DCHECK(As<Pipeline<ChromeMedia>*>());
    As<Pipeline<ChromeMedia>*>()->Seek(time, seek_cb);
  }

  void Suspend() {
    if (pipeline_m96_) {
      pipeline_m96_->Suspend();
      return;
    }
    DCHECK(pipeline_m114_);
    pipeline_m114_->Suspend();
  }

  void Resume(PipelineWindow window) {
    if (pipeline_m96_) {
      pipeline_m96_->Resume(window);
      return;
    }
    DCHECK(pipeline_m114_);
    pipeline_m114_->Resume(window);
  }

  void Stop(const base::Closure& stop_cb) {
    if (pipeline_m96_) {
      pipeline_m96_->Stop(stop_cb);
      return;
    }
    DCHECK(pipeline_m114_);
    pipeline_m114_->Stop(stop_cb);
  }

  void UpdateBufferedTimeRanges(const AddRangeCB& add_range_cb) {
    if (pipeline_m96_) {
      pipeline_m96_->UpdateBufferedTimeRanges(add_range_cb);
      return;
    }
    DCHECK(pipeline_m114_);
    pipeline_m114_->UpdateBufferedTimeRanges(add_range_cb);
  }

  void GetNaturalVideoSize(gfx::Size* out_size) const {
    if (pipeline_m96_) {
      pipeline_m96_->GetNaturalVideoSize(out_size);
      return;
    }
    DCHECK(pipeline_m114_);
    pipeline_m114_->GetNaturalVideoSize(out_size);
  }

  DECLARE_MEDIA_PIPELINE_GETTER(std::vector<std::string>, GetAudioConnectors)
  DECLARE_MEDIA_PIPELINE_GETTER(bool, DidLoadingProgress)
  DECLARE_MEDIA_PIPELINE_GETTER(VideoStatistics, GetVideoStatistics)
  DECLARE_MEDIA_PIPELINE_GETTER(SetBoundsCB, GetSetBoundsCB)

  void SetPreferredOutputModeToDecodeToTexture() {
    if (pipeline_m96_) {
      pipeline_m96_->SetPreferredOutputModeToDecodeToTexture();
      return;
    }
    DCHECK(pipeline_m114_);
    pipeline_m114_->SetPreferredOutputModeToDecodeToTexture();
  }

#undef DECLARE_MEDIA_PIPELINE_GETTER
#undef DECLARE_MEDIA_PIPELINE_SETTER

 private:
  scoped_refptr<Pipeline<ChromeMediaM96>> pipeline_m96_;
  scoped_refptr<Pipeline<ChromeMediaM114>> pipeline_m114_;

  DISALLOW_COPY_AND_ASSIGN(PipelineHolder);
};

}  // namespace media
}  // namespace cobalt

#endif  // COBALT_MEDIA_BASE_PIPELINE_HOLDER_H_
