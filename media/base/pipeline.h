// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_PIPELINE_H_
#define MEDIA_BASE_PIPELINE_H_

#include <string>

#include "base/gtest_prod_util.h"
#include "base/synchronization/condition_variable.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_checker.h"
#include "media/base/audio_renderer.h"
#include "media/base/demuxer.h"
#include "media/base/media_export.h"
#include "media/base/pipeline_status.h"
#include "media/base/ranges.h"
#include "media/base/serial_runner.h"
#include "ui/gfx/size.h"

class MessageLoop;

namespace base {
class MessageLoopProxy;
class TimeDelta;
}

namespace media {

class AudioDecoder;
class Clock;
class FilterCollection;
class MediaLog;
class VideoRenderer;

// Adapter for using asynchronous Pipeline methods in code that wants to run
// synchronously.  To use, construct an instance of this class and pass the
// |Callback()| to the Pipeline method requiring a callback.  Then Wait() for
// the callback to get fired and call status() to see what the callback's
// argument was.  This object is for one-time use; call |Callback()| exactly
// once.
class MEDIA_EXPORT PipelineStatusNotification {
 public:
  PipelineStatusNotification();
  ~PipelineStatusNotification();

  // See class-level comment for usage.
  PipelineStatusCB Callback();
  void Wait();
  PipelineStatus status();

 private:
  void Notify(media::PipelineStatus status);

  base::Lock lock_;
  base::ConditionVariable cv_;
  media::PipelineStatus status_;
  bool notified_;

  DISALLOW_COPY_AND_ASSIGN(PipelineStatusNotification);
};

// Pipeline runs the media pipeline.  Filters are created and called on the
// message loop injected into this object. Pipeline works like a state
// machine to perform asynchronous initialization, pausing, seeking and playing.
//
// Here's a state diagram that describes the lifetime of this object.
//
//   [ *Created ]                       [ Any State ]
//         | Start()                         | Stop() / SetError()
//         V                                 V
//   [ InitXXX (for each filter) ]      [ Stopping ]
//         |                                 |
//         V                                 V
//   [ InitPreroll ]                    [ Stopped ]
//         |
//         V
//   [ Starting ] <-- [ Seeking ]
//         |               ^
//         V               |
//   [ Started ] ----------'
//                 Seek()
//
// Initialization is a series of state transitions from "Created" through each
// filter initialization state.  When all filter initialization states have
// completed, we are implicitly in a "Paused" state.  At that point we simulate
// a Seek() to the beginning of the media to give filters a chance to preroll.
// From then on the normal Seek() transitions are carried out and we start
// playing the media.
//
// If any error ever happens, this object will transition to the "Error" state
// from any state. If Stop() is ever called, this object will transition to
// "Stopped" state.
class MEDIA_EXPORT Pipeline
    : public base::RefCountedThreadSafe<Pipeline>,
      public DemuxerHost {
 public:
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

  // Constructs a media pipeline that will execute on |message_loop|.
  Pipeline(const scoped_refptr<base::MessageLoopProxy>& message_loop,
           MediaLog* media_log);

  // Build a pipeline to using the given filter collection to construct a filter
  // chain, executing |seek_cb| when the initial seek/preroll has completed.
  //
  // |filter_collection| must be a complete collection containing a demuxer,
  // audio/video decoders, and audio/video renderers. Failing to do so will
  // result in a crash.
  //
  // The following permanent callbacks will be executed as follows up until
  // Stop() has completed:
  //   |ended_cb| will be executed whenever the media reaches the end.
  //   |error_cb| will be executed whenever an error occurs but hasn't
  //              been reported already through another callback.
  //   |buffering_state_cb| Optional callback that will be executed whenever the
  //                    pipeline's buffering state changes.
  // It is an error to call this method after the pipeline has already started.
  void Start(scoped_ptr<FilterCollection> filter_collection,
             const PipelineStatusCB& ended_cb,
             const PipelineStatusCB& error_cb,
             const PipelineStatusCB& seek_cb,
             const BufferingStateCB& buffering_state_cb);

  // Asynchronously stops the pipeline, executing |stop_cb| when the pipeline
  // teardown has completed.
  //
  // Stop() must complete before destroying the pipeline. It it permissible to
  // call Stop() at any point during the lifetime of the pipeline.
  void Stop(const base::Closure& stop_cb);

  // Attempt to seek to the position specified by time.  |seek_cb| will be
  // executed when the all filters in the pipeline have processed the seek.
  //
  // Clients are expected to call GetMediaTime() to check whether the seek
  // succeeded.
  //
  // It is an error to call this method if the pipeline has not started.
  void Seek(base::TimeDelta time, const PipelineStatusCB& seek_cb);

  // Returns true if the pipeline has been started via Start().  If IsRunning()
  // returns true, it is expected that Stop() will be called before destroying
  // the pipeline.
  bool IsRunning() const;

  // Returns true if the media has audio.
  bool HasAudio() const;

  // Returns true if the media has video.
  bool HasVideo() const;

  // Gets the current playback rate of the pipeline.  When the pipeline is
  // started, the playback rate will be 0.0f.  A rate of 1.0f indicates
  // that the pipeline is rendering the media at the standard rate.  Valid
  // values for playback rate are >= 0.0f.
  float GetPlaybackRate() const;

  // Attempt to adjust the playback rate. Setting a playback rate of 0.0f pauses
  // all rendering of the media.  A rate of 1.0f indicates a normal playback
  // rate.  Values for the playback rate must be greater than or equal to 0.0f.
  //
  // TODO(scherkus): What about maximum rate?  Does HTML5 specify a max?
  void SetPlaybackRate(float playback_rate);

  // Gets the current volume setting being used by the audio renderer.  When
  // the pipeline is started, this value will be 1.0f.  Valid values range
  // from 0.0f to 1.0f.
  float GetVolume() const;

  // Attempt to set the volume of the audio renderer.  Valid values for volume
  // range from 0.0f (muted) to 1.0f (full volume).  This value affects all
  // channels proportionately for multi-channel audio streams.
  void SetVolume(float volume);

  // Returns the current media playback time, which progresses from 0 until
  // GetMediaDuration().
  base::TimeDelta GetMediaTime() const;

  // Get approximate time ranges of buffered media.
  Ranges<base::TimeDelta> GetBufferedTimeRanges();

  // Get the duration of the media in microseconds.  If the duration has not
  // been determined yet, then returns 0.
  base::TimeDelta GetMediaDuration() const;

  // Get the total size of the media file.  If the size has not yet been
  // determined or can not be determined, this value is 0.
  int64 GetTotalBytes() const;

  // Gets the natural size of the video output in pixel units.  If there is no
  // video or the video has not been rendered yet, the width and height will
  // be 0.
  void GetNaturalVideoSize(gfx::Size* out_size) const;

  // Return true if loading progress has been made since the last time this
  // method was called.
  bool DidLoadingProgress() const;

  // Gets the current pipeline statistics.
  PipelineStatistics GetStatistics() const;

  void SetClockForTesting(Clock* clock);
  void SetErrorForTesting(PipelineStatus status);

 private:
  FRIEND_TEST_ALL_PREFIXES(PipelineTest, GetBufferedTimeRanges);
  FRIEND_TEST_ALL_PREFIXES(PipelineTest, DisableAudioRenderer);
  FRIEND_TEST_ALL_PREFIXES(PipelineTest, DisableAudioRendererDuringInit);
  FRIEND_TEST_ALL_PREFIXES(PipelineTest, EndedCallback);
  FRIEND_TEST_ALL_PREFIXES(PipelineTest, AudioStreamShorterThanVideo);
  friend class MediaLog;

  // Only allow ourselves to be deleted by reference counting.
  friend class base::RefCountedThreadSafe<Pipeline>;
  virtual ~Pipeline();

  // Pipeline states, as described above.
  enum State {
    kCreated,
    kInitDemuxer,
    kInitAudioDecoder,
    kInitAudioRenderer,
    kInitVideoRenderer,
    kInitPrerolling,
    kSeeking,
    kStarting,
    kStarted,
    kStopping,
    kStopped,
  };

  // Updates |state_|. All state transitions should use this call.
  void SetState(State next_state);

  static const char* GetStateString(State state);
  State GetNextState() const;

  // Helper method that runs & resets |seek_cb_| and resets |seek_timestamp_|
  // and |seek_pending_|.
  void FinishSeek();

  // DataSourceHost (by way of DemuxerHost) implementation.
  virtual void SetTotalBytes(int64 total_bytes) OVERRIDE;
  virtual void AddBufferedByteRange(int64 start, int64 end) OVERRIDE;
  virtual void AddBufferedTimeRange(base::TimeDelta start,
                                    base::TimeDelta end) OVERRIDE;

  // DemuxerHost implementaion.
  virtual void SetDuration(base::TimeDelta duration) OVERRIDE;
  virtual void OnDemuxerError(PipelineStatus error) OVERRIDE;

  // Initiates teardown sequence in response to a runtime error.
  //
  // Safe to call from any thread.
  void SetError(PipelineStatus error);

  // Callback executed when the natural size of the video has changed.
  void OnNaturalVideoSizeChanged(const gfx::Size& size);

  // Callbacks executed when a renderer has ended.
  void OnAudioRendererEnded();
  void OnVideoRendererEnded();

  // Callback executed by filters to update statistics.
  void OnUpdateStatistics(const PipelineStatistics& stats);

  // Callback executed by audio renderer when it has been disabled.
  void OnAudioDisabled();

  // Callback executed by audio renderer to update clock time.
  void OnAudioTimeUpdate(base::TimeDelta time, base::TimeDelta max_time);

  // Callback executed by video renderer to update clock time.
  void OnVideoTimeUpdate(base::TimeDelta max_time);

  // The following "task" methods correspond to the public methods, but these
  // methods are run as the result of posting a task to the PipelineInternal's
  // message loop.
  void StartTask(scoped_ptr<FilterCollection> filter_collection,
                 const PipelineStatusCB& ended_cb,
                 const PipelineStatusCB& error_cb,
                 const PipelineStatusCB& seek_cb,
                 const BufferingStateCB& buffering_state_cb);

  // Stops and destroys all filters, placing the pipeline in the kStopped state.
  void StopTask(const base::Closure& stop_cb);

  // Carries out stopping and destroying all filters, placing the pipeline in
  // the kStopped state.
  void ErrorChangedTask(PipelineStatus error);

  // Carries out notifying filters that the playback rate has changed.
  void PlaybackRateChangedTask(float playback_rate);

  // Carries out notifying filters that the volume has changed.
  void VolumeChangedTask(float volume);

  // Carries out notifying filters that we are seeking to a new timestamp.
  void SeekTask(base::TimeDelta time, const PipelineStatusCB& seek_cb);

  // Handles audio/video ended logic and running |ended_cb_|.
  void DoAudioRendererEnded();
  void DoVideoRendererEnded();
  void RunEndedCallbackIfNeeded();

  // Carries out disabling the audio renderer.
  void AudioDisabledTask();

  // Kicks off initialization for each media object, executing |done_cb| with
  // the result when completed.
  void InitializeDemuxer(const PipelineStatusCB& done_cb);
  void InitializeAudioDecoder(const PipelineStatusCB& done_cb);
  void InitializeAudioRenderer(const PipelineStatusCB& done_cb);
  void InitializeVideoRenderer(const PipelineStatusCB& done_cb);

  // Kicks off destroying filters. Called by StopTask() and ErrorChangedTask().
  // When we start to tear down the pipeline, we will consider two cases:
  // 1. when pipeline has not been initialized, we will transit to stopping
  // state first.
  // 2. when pipeline has been initialized, we will first transit to pausing
  // => flushing => stopping => stopped state.
  // This will remove the race condition during stop between filters.
  void TearDownPipeline();

  // Compute the time corresponding to a byte offset.
  base::TimeDelta TimeForByteOffset_Locked(int64 byte_offset) const;

  void OnStateTransition(PipelineStatus status);
  void StateTransitionTask(PipelineStatus status);

  // Initiates an asynchronous preroll call sequence executing |done_cb|
  // with the final status when completed.
  void DoInitialPreroll(const PipelineStatusCB& done_cb);

  // Initiates an asynchronous pause-flush-seek-preroll call sequence
  // executing |done_cb| with the final status when completed.
  //
  // TODO(scherkus): Prerolling should be separate from seeking so we can report
  // finer grained ready states (HAVE_CURRENT_DATA vs. HAVE_FUTURE_DATA)
  // indepentent from seeking.
  void DoSeek(base::TimeDelta seek_timestamp, const PipelineStatusCB& done_cb);

  // Updates playback rate and volume and initiates an asynchronous play call
  // sequence executing |done_cb| with the final status when completed.
  void DoPlay(const PipelineStatusCB& done_cb);

  // Initiates an asynchronous pause-flush-stop call sequence executing
  // |done_cb| when completed.
  void DoStop(const PipelineStatusCB& done_cb);
  void OnStopCompleted(PipelineStatus status);

  void OnAudioUnderflow();

  void StartClockIfWaitingForTimeUpdate_Locked();

  // Message loop used to execute pipeline tasks.
  scoped_refptr<base::MessageLoopProxy> message_loop_;

  // MediaLog to which to log events.
  scoped_refptr<MediaLog> media_log_;

  // Lock used to serialize access for the following data members.
  mutable base::Lock lock_;

  // Whether or not the pipeline is running.
  bool running_;

  // Amount of available buffered data.  Set by filters.
  Ranges<int64> buffered_byte_ranges_;
  Ranges<base::TimeDelta> buffered_time_ranges_;

  // True when AddBufferedByteRange() has been called more recently than
  // DidLoadingProgress().
  mutable bool did_loading_progress_;

  // Total size of the media.  Set by filters.
  int64 total_bytes_;

  // Video's natural width and height.  Set by filters.
  gfx::Size natural_size_;

  // Current volume level (from 0.0f to 1.0f).  This value is set immediately
  // via SetVolume() and a task is dispatched on the message loop to notify the
  // filters.
  float volume_;

  // Current playback rate (>= 0.0f).  This value is set immediately via
  // SetPlaybackRate() and a task is dispatched on the message loop to notify
  // the filters.
  float playback_rate_;

  // Reference clock.  Keeps track of current playback time.  Uses system
  // clock and linear interpolation, but can have its time manually set
  // by filters.
  scoped_ptr<Clock> clock_;

  // If this value is set to true, then |clock_| is paused and we are waiting
  // for an update of the clock greater than or equal to the elapsed time to
  // start the clock.
  bool waiting_for_clock_update_;

  // Status of the pipeline.  Initialized to PIPELINE_OK which indicates that
  // the pipeline is operating correctly. Any other value indicates that the
  // pipeline is stopped or is stopping.  Clients can call the Stop() method to
  // reset the pipeline state, and restore this to PIPELINE_OK.
  PipelineStatus status_;

  // Whether the media contains rendered audio and video streams.
  // TODO(fischman,scherkus): replace these with checks for
  // {audio,video}_decoder_ once extraction of {Audio,Video}Decoder from the
  // Filter heirarchy is done.
  bool has_audio_;
  bool has_video_;

  // The following data members are only accessed by tasks posted to
  // |message_loop_|.

  // Member that tracks the current state.
  State state_;

  // Whether we've received the audio/video ended events.
  bool audio_ended_;
  bool video_ended_;

  // Set to true in DisableAudioRendererTask().
  bool audio_disabled_;

  scoped_ptr<FilterCollection> filter_collection_;

  // Temporary callback used for Start() and Seek().
  PipelineStatusCB seek_cb_;

  // Temporary callback used for Stop().
  base::Closure stop_cb_;

  // Permanent callbacks passed in via Start().
  PipelineStatusCB ended_cb_;
  PipelineStatusCB error_cb_;
  BufferingStateCB buffering_state_cb_;

  // Audio renderer reference used for setting the volume and determining
  // when playback has finished.
  scoped_refptr<AudioRenderer> audio_renderer_;

  // Video Renderer reference used for determining when playback has finished
  // and for signalling imminent shutdown.
  // The signalling imminent shutdown is a HACK necessary because
  // WebMediaPlayerImpl::Destroy() holds the render thread loop hostage
  // until PipelineImpl::Stop() calls its callback.
  // http://crbug.com/110228 tracks removing this hack.
  scoped_refptr<VideoRenderer> video_renderer_;

  // Demuxer reference used for setting the preload value.
  scoped_refptr<Demuxer> demuxer_;

  // Audio decoder reference used during initialization.
  //
  // TODO(scherkus): Remove after renderers do initialization, see
  // http://crbug.com/145635
  scoped_refptr<AudioDecoder> audio_decoder_;

  PipelineStatistics statistics_;

  // Time of pipeline creation; is non-zero only until the pipeline first
  // reaches "kStarted", at which point it is used & zeroed out.
  base::Time creation_time_;

  scoped_ptr<SerialRunner> pending_callbacks_;

  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(Pipeline);
};

}  // namespace media

#endif  // MEDIA_BASE_PIPELINE_H_
