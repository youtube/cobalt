// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The pipeline is the public API clients use for playing back media.  Clients
// provide a filter collection containing the filters they want the pipeline to
// use to render media.

#ifndef MEDIA_BASE_PIPELINE_H_
#define MEDIA_BASE_PIPELINE_H_

#include <string>

#include "media/base/filters.h"
#include "media/base/pipeline_status.h"

namespace base {
class TimeDelta;
}

namespace media {

struct PipelineStatistics {
  PipelineStatistics() :
      audio_bytes_decoded(0),
      video_bytes_decoded(0),
      video_frames_decoded(0),
      video_frames_dropped(0) {
  }

  uint32 audio_bytes_decoded;  // Should be uint64?
  uint32 video_bytes_decoded;  // Should be uint64?
  uint32 video_frames_decoded;
  uint32 video_frames_dropped;
};

enum NetworkEvent {
  DOWNLOAD_CONTINUED,
  DOWNLOAD_PAUSED,
  CAN_PLAY_THROUGH
};

class FilterCollection;

class MEDIA_EXPORT Pipeline : public base::RefCountedThreadSafe<Pipeline> {
 public:
  // Callback that executes when a network event occurrs.
  // The parameter specifies the type of event that is being signaled.
  typedef base::Callback<void(NetworkEvent)> NetworkEventCB;

  // Initializes pipeline. Pipeline takes ownership of all callbacks passed
  // into this method.
  // |ended_callback| will be executed when the media reaches the end.
  // |error_callback_| will be executed upon an error in the pipeline.
  // |network_callback_| will be executed when there's a network event.
  virtual void Init(const PipelineStatusCB& ended_callback,
                    const PipelineStatusCB& error_callback,
                    const NetworkEventCB& network_callback) = 0;

  // Build a pipeline to render the given URL using the given filter collection
  // to construct a filter chain.  Returns true if successful, false otherwise
  // (i.e., pipeline already started).  Note that a return value of true
  // only indicates that the initialization process has started successfully.
  // Pipeline initialization is an inherently asynchronous process.  Clients can
  // either poll the IsInitialized() method (discouraged) or use the
  // |start_callback| as described below.
  //
  // This method is asynchronous and can execute a callback when completed.
  // If the caller provides a |start_callback|, it will be called when the
  // pipeline initialization completes.
  virtual bool Start(FilterCollection* filter_collection,
                     const std::string& url,
                     const PipelineStatusCB& start_callback) = 0;

  // Asynchronously stops the pipeline and resets it to an uninitialized state.
  // If provided, |stop_callback| will be executed when the pipeline has been
  // completely torn down and reset to an uninitialized state.  It is acceptable
  // to call Start() again once the callback has finished executing.
  //
  // Stop() must be called before destroying the pipeline.  Clients can
  // determine whether Stop() must be called by checking IsRunning().
  //
  // TODO(scherkus): ideally clients would destroy the pipeline after calling
  // Stop() and create a new pipeline as needed.
  virtual void Stop(const PipelineStatusCB& stop_callback) = 0;

  // Attempt to seek to the position specified by time.  |seek_callback| will be
  // executed when the all filters in the pipeline have processed the seek.
  //
  // Clients are expected to call GetCurrentTime() to check whether the seek
  // succeeded.
  virtual void Seek(base::TimeDelta time,
                    const PipelineStatusCB& seek_callback) = 0;

  // Returns true if the pipeline has been started via Start().  If IsRunning()
  // returns true, it is expected that Stop() will be called before destroying
  // the pipeline.
  virtual bool IsRunning() const = 0;

  // Returns true if the pipeline has been started and fully initialized to a
  // point where playback controls will be respected.  Note that it is possible
  // for a pipeline to be started but not initialized (i.e., an error occurred).
  virtual bool IsInitialized() const = 0;

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
  //
  // TODO(scherkus): What about maximum rate?  Does HTML5 specify a max?
  virtual void SetPlaybackRate(float playback_rate) = 0;

  // Gets the current volume setting being used by the audio renderer.  When
  // the pipeline is started, this value will be 1.0f.  Valid values range
  // from 0.0f to 1.0f.
  virtual float GetVolume() const = 0;

  // Attempt to set the volume of the audio renderer.  Valid values for volume
  // range from 0.0f (muted) to 1.0f (full volume).  This value affects all
  // channels proportionately for multi-channel audio streams.
  virtual void SetVolume(float volume) = 0;

  // Set the preload value for the pipeline.
  virtual void SetPreload(Preload preload) = 0;

  // Gets the current pipeline time. For a pipeline "time" progresses from 0 to
  // the end of the media.
  virtual base::TimeDelta GetCurrentTime() const = 0;

  // Get the approximate amount of playable data buffered so far in micro-
  // seconds.
  virtual base::TimeDelta GetBufferedTime() = 0;

  // Get the duration of the media in microseconds.  If the duration has not
  // been determined yet, then returns 0.
  virtual base::TimeDelta GetMediaDuration() const = 0;

  // Get the total number of bytes that are buffered on the client and ready to
  // be played.
  virtual int64 GetBufferedBytes() const = 0;

  // Get the total size of the media file.  If the size has not yet been
  // determined or can not be determined, this value is 0.
  virtual int64 GetTotalBytes() const = 0;

  // Gets the natural size of the video output in pixel units.  If there is no
  // video or the video has not been rendered yet, the width and height will
  // be 0.
  virtual void GetNaturalVideoSize(gfx::Size* out_size) const = 0;

  // If this method returns true, that means the data source is a streaming
  // data source. Seeking may not be possible.
  virtual bool IsStreaming() const = 0;

  // If this method returns true, that means the data source has fully loaded
  // the media and that the network is no longer needed.
  virtual bool IsLoaded() const = 0;

  // Gets the current pipeline statistics.
  virtual PipelineStatistics GetStatistics() const = 0;

 protected:
  // Only allow ourselves to be deleted by reference counting.
  friend class base::RefCountedThreadSafe<Pipeline>;
  virtual ~Pipeline() {}
};

}  // namespace media

#endif  // MEDIA_BASE_PIPELINE_H_
