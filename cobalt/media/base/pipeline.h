// Copyright 2016 Google Inc. All Rights Reserved.
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

#ifndef COBALT_MEDIA_BASE_PIPELINE_H_
#define COBALT_MEDIA_BASE_PIPELINE_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop_proxy.h"
#include "base/time.h"
#include "cobalt/media/base/demuxer.h"
#include "cobalt/media/base/media_export.h"
#include "cobalt/media/base/pipeline_status.h"
#include "cobalt/media/base/ranges.h"
#include "starboard/drm.h"
#include "starboard/window.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size.h"

namespace cobalt {
namespace media {

class MediaLog;

typedef SbWindow PipelineWindow;

// Callback to notify that a DRM system is ready.
typedef base::Callback<void(SbDrmSystem)> DrmSystemReadyCB;

// Callback to set an DrmSystemReadyCB.
typedef base::Callback<void(const DrmSystemReadyCB&)> SetDrmSystemReadyCB;

// Pipeline contains the common interface for media pipelines.  It provides
// functions to perform asynchronous initialization, pausing, seeking and
// playing.
class MEDIA_EXPORT Pipeline : public base::RefCountedThreadSafe<Pipeline> {
 public:
  // Return true if the punch through box should be rendered.  Return false if
  // no punch through box should be rendered.
  typedef base::Callback<bool(const gfx::Rect&)> SetBoundsCB;

  // Buffering states the pipeline transitions between during playback.
  // kHaveMetadata:
  //   Indicates that the following things are known:
  //   content duration, natural size, start time, and whether the content has
  //   audio and/or video in supported formats.
  // kPrerollCompleted:
  //   All renderers have buffered enough data to satisfy preroll and are ready
  //   to start playback.
  enum BufferingState {
    kHaveMetadata,
    kPrerollCompleted,
  };

  typedef base::Callback<void(BufferingState)> BufferingStateCB;
#if SB_HAS(PLAYER_WITH_URL)
  typedef base::Callback<void(EmeInitDataType, const std::vector<uint8_t>&)>
      OnEncryptedMediaInitDataEncounteredCB;
#endif  // SB_HAS(PLAYER_WITH_URL)

  static scoped_refptr<Pipeline> Create(
      PipelineWindow window,
      const scoped_refptr<base::MessageLoopProxy>& message_loop,
      MediaLog* media_log);

  virtual ~Pipeline() {}

  virtual void Suspend() {}
  virtual void Resume() {}

  // Build a pipeline to using the given filter collection to construct a filter
  // chain, executing |seek_cb| when the initial seek/preroll has completed.
  //
  // |filter_collection| must be a complete collection containing a demuxer,
  // audio/video decoders, and audio/video renderers. Failing to do so will
  // result in a crash.
  //
  // The following permanent callbacks will be executed as follows up until
  // Stop() has completed:
  //   |set_drm_system_ready_cb| can be used if Pipeline needs to be notified
  //                             when the |SbDrmSystem| is ready.
  //   |ended_cb| will be executed whenever the media reaches the end.
  //   |error_cb| will be executed whenever an error occurs but hasn't
  //              been reported already through another callback.
  //   |buffering_state_cb| Optional callback that will be executed whenever the
  //                        pipeline's buffering state changes.
  //   |duration_change_cb| optional callback that will be executed whenever the
  //                        presentation duration changes.
  // It is an error to call this method after the pipeline has already started.
  virtual void Start(Demuxer* demuxer,
                     const SetDrmSystemReadyCB& set_drm_system_ready_cb,
#if SB_HAS(PLAYER_WITH_URL)
                     const OnEncryptedMediaInitDataEncounteredCB&
                         encrypted_media_init_data_encountered_cb,
                     const std::string& source_url,
#endif  // SB_HAS(PLAYER_WITH_URL)
                     const PipelineStatusCB& ended_cb,
                     const PipelineStatusCB& error_cb,
                     const PipelineStatusCB& seek_cb,
                     const BufferingStateCB& buffering_state_cb,
                     const base::Closure& duration_change_cb,
                     const base::Closure& output_mode_change_cb,
                     const base::Closure& content_size_change_cb) = 0;

  // Asynchronously stops the pipeline, executing |stop_cb| when the pipeline
  // teardown has completed.
  //
  // Stop() must complete before destroying the pipeline. It it permissible to
  // call Stop() at any point during the lifetime of the pipeline.
  virtual void Stop(const base::Closure& stop_cb) = 0;

  // Attempt to seek to the position specified by time.  |seek_cb| will be
  // executed when the all filters in the pipeline have processed the seek.
  //
  // Clients are expected to call GetMediaTime() to check whether the seek
  // succeeded.
  //
  // It is an error to call this method if the pipeline has not started.
  virtual void Seek(base::TimeDelta time, const PipelineStatusCB& seek_cb) = 0;

  // Returns true if the media has audio.
  virtual bool HasAudio() const = 0;

  // Returns true if the media has video.
  virtual bool HasVideo() const = 0;

  // Gets the current playback rate of the pipeline.  When the pipeline is
  // started, the playback rate will be 0.0f.  A rate of 1.0f indicates
  // that the pipeline is rendering the media at the standard rate.  Valid
  // values for playback rate are >= 0.0f.
  virtual float GetPlaybackRate() const = 0;

  // Attempt to adjust the playback rate. Setting a playback rate of 0.0f pauses
  // all rendering of the media.  A rate of 1.0f indicates a normal playback
  // rate.  Values for the playback rate must be greater than or equal to 0.0f.
  virtual void SetPlaybackRate(float playback_rate) = 0;

  // Gets the current volume setting being used by the audio renderer.  When
  // the pipeline is started, this value will be 1.0f.  Valid values range
  // from 0.0f to 1.0f.
  virtual float GetVolume() const = 0;

  // Attempt to set the volume of the audio renderer.  Valid values for volume
  // range from 0.0f (muted) to 1.0f (full volume).  This value affects all
  // channels proportionately for multi-channel audio streams.
  virtual void SetVolume(float volume) = 0;

  // Returns the current media playback time, which progresses from 0 until
  // GetMediaDuration().
  virtual base::TimeDelta GetMediaTime() = 0;

  // Get approximate time ranges of buffered media.
  virtual Ranges<base::TimeDelta> GetBufferedTimeRanges() = 0;

  // Get the duration of the media in microseconds.  If the duration has not
  // been determined yet, then return 0.
  virtual base::TimeDelta GetMediaDuration() const = 0;

#if SB_HAS(PLAYER_WITH_URL)
  // Get the start date of the media in microseconds since 1601. If the start
  // date has not been determined yet, then return 0.
  virtual base::TimeDelta GetMediaStartDate() const = 0;
#endif  // SB_HAS(PLAYER_WITH_URL)

  // Gets the natural size of the video output in pixel units.  If there is no
  // video or the video has not been rendered yet, the width and height will
  // be 0.
  virtual void GetNaturalVideoSize(gfx::Size* out_size) const = 0;

  // Return true if loading progress has been made since the last time this
  // method was called.
  virtual bool DidLoadingProgress() const = 0;

  // Gets the current pipeline statistics.
  virtual PipelineStatistics GetStatistics() const = 0;

  // Get the SetBoundsCB used to set the bounds of the video frame.
  virtual SetBoundsCB GetSetBoundsCB() { return SetBoundsCB(); }

  // Updates the player's preference for decode-to-texture versus punch through.
  virtual void SetDecodeToTextureOutputMode(bool /*enabled*/) {}
};

}  // namespace media
}  // namespace cobalt

#endif  // COBALT_MEDIA_BASE_PIPELINE_H_
