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

#ifndef MEDIA_BASE_PIPELINE_H_
#define MEDIA_BASE_PIPELINE_H_

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop_proxy.h"
#include "base/time.h"
#include "media/base/decryptor.h"
#include "media/base/filter_collection.h"
#include "media/base/media_export.h"
#include "media/base/pipeline_status.h"
#include "media/base/ranges.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size.h"

typedef void* PipelineWindow;

namespace media {

class MediaLog;

typedef base::Callback<void(PipelineStatus, const std::string&)> ErrorCB;

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
  //   |decryptor_ready_cb| can be used if Pipeline needs to be notified when
  //                        the Decryptor is ready.
  //   |ended_cb| will be executed whenever the media reaches the end.
  //   |error_cb| will be executed whenever an error occurs but hasn't
  //              been reported already through another callback.
  //   |buffering_state_cb| Optional callback that will be executed whenever the
  //                        pipeline's buffering state changes.
  //   |duration_change_cb| optional callback that will be executed whenever the
  //                        presentation duration changes.
  // It is an error to call this method after the pipeline has already started.
  virtual void Start(scoped_ptr<FilterCollection> filter_collection,
                     const SetDecryptorReadyCB& decryptor_ready_cb,
                     const PipelineStatusCB& ended_cb,
                     const ErrorCB& error_cb,
                     const PipelineStatusCB& seek_cb,
                     const BufferingStateCB& buffering_state_cb,
                     const base::Closure& duration_change_cb) = 0;

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
  virtual base::TimeDelta GetMediaTime() const = 0;

  // Get approximate time ranges of buffered media.
  virtual Ranges<base::TimeDelta> GetBufferedTimeRanges() = 0;

  // Get the duration of the media in microseconds.  If the duration has not
  // been determined yet, then returns 0.
  virtual base::TimeDelta GetMediaDuration() const = 0;

  // Get the total size of the media file.  If the size has not yet been
  // determined or can not be determined, this value is 0.
  virtual int64 GetTotalBytes() const = 0;

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

#endif  // MEDIA_BASE_PIPELINE_H_
