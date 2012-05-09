// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_PIPELINE_H_
#define MEDIA_BASE_PIPELINE_H_

#include <string>

#include "base/gtest_prod_util.h"
#include "base/synchronization/condition_variable.h"
#include "base/synchronization/lock.h"
#include "media/base/demuxer.h"
#include "media/base/download_rate_monitor.h"
#include "media/base/filter_host.h"
#include "media/base/media_export.h"
#include "media/base/pipeline_status.h"
#include "ui/gfx/size.h"

class MessageLoop;

namespace base {
class MessageLoopProxy;
class TimeDelta;
}

namespace media {

class AudioDecoder;
class AudioRenderer;
class Clock;
class Filter;
class FilterCollection;
class MediaLog;
class VideoDecoder;
class VideoRenderer;

enum NetworkEvent {
  DOWNLOAD_CONTINUED,
  DOWNLOAD_PAUSED,
  CAN_PLAY_THROUGH
};

// Callback that executes when a network event occurs.
// The parameter specifies the type of event that is being signalled.
typedef base::Callback<void(NetworkEvent)> NetworkEventCB;

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
//   [ *Created ]                                    [ Stopped ]
//         | Start()                                      ^
//         V                       SetError()             |
//   [ InitXXX (for each filter) ] -------->[ Stopping (for each filter) ]
//         |                                              ^
//         V                                              | if Stop
//   [ Seeking (for each filter) ] <--------[ Flushing (for each filter) ]
//         |                         if Seek              ^
//         V                                              |
//   [ Starting (for each filter) ]                       |
//         |                                              |
//         V      Seek()/Stop()                           |
//   [ Started ] -------------------------> [ Pausing (for each filter) ]
//         |                                              ^
//         |   NotifyEnded()             Seek()/Stop()    |
//         `-------------> [ Ended ] ---------------------'
//                                                        ^  SetError()
//                                                        |
//                                         [ Any State Other Than InitXXX ]

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
      public FilterHost,
      public DemuxerHost {
 public:
  // Constructs a media pipeline that will execute on |message_loop|.
  Pipeline(MessageLoop* message_loop, MediaLog* media_log);

  // Build a pipeline to using the given filter collection to construct a filter
  // chain.
  //
  // Pipeline initialization is an inherently asynchronous process.  Clients can
  // either poll the IsInitialized() method (discouraged) or optionally pass in
  // |start_cb|, which will be executed when initialization completes.
  //
  // The following permanent callbacks will be executed as follows:
  //   |start_cb_| will be executed when Start is done (successfully or not).
  //   |network_cb_| will be executed whenever there's a network activity.
  //   |ended_cb| will be executed whenever the media reaches the end.
  //   |error_cb_| will be executed whenever an error occurs but hasn't
  //               been reported already through another callback.
  //
  // These callbacks are only executed after Start() has been called and until
  // Stop() has completed.
  //
  // It is an error to call this method after the pipeline has already started.
  //
  // TODO(scherkus): remove IsInitialized() and force clients to use callbacks.
  void Start(scoped_ptr<FilterCollection> filter_collection,
             const PipelineStatusCB& ended_cb,
             const PipelineStatusCB& error_cb,
             const NetworkEventCB& network_cb,
             const PipelineStatusCB& start_cb);

  // Asynchronously stops the pipeline and resets it to an uninitialized state.
  //
  // If provided, |stop_cb| will be executed when the pipeline has been
  // completely torn down and reset to an uninitialized state.  It is acceptable
  // to call Start() again once the callback has finished executing.
  //
  // Stop() must be called before destroying the pipeline.  Clients can
  // determine whether Stop() must be called by checking IsRunning().
  //
  // It is an error to call this method if the pipeline has not started.
  //
  // TODO(scherkus): ideally clients would destroy the pipeline after calling
  // Stop() and create a new pipeline as needed.
  void Stop(const base::Closure& stop_cb);

  // Attempt to seek to the position specified by time.  |seek_cb| will be
  // executed when the all filters in the pipeline have processed the seek.
  //
  // Clients are expected to call GetCurrentTime() to check whether the seek
  // succeeded.
  //
  // It is an error to call this method if the pipeline has not started.
  void Seek(base::TimeDelta time, const PipelineStatusCB& seek_cb);

  // Returns true if the pipeline has been started via Start().  If IsRunning()
  // returns true, it is expected that Stop() will be called before destroying
  // the pipeline.
  bool IsRunning() const;

  // Returns true if the pipeline has been started and fully initialized to a
  // point where playback controls will be respected.  Note that it is possible
  // for a pipeline to be started but not initialized (i.e., an error occurred).
  bool IsInitialized() const;

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

  // Gets the current pipeline time. For a pipeline "time" progresses from 0 to
  // the end of the media.
  base::TimeDelta GetCurrentTime() const;

  // Get the approximate amount of playable data buffered so far in micro-
  // seconds.
  base::TimeDelta GetBufferedTime();

  // Get the duration of the media in microseconds.  If the duration has not
  // been determined yet, then returns 0.
  base::TimeDelta GetMediaDuration() const;

  // Get the total number of bytes that are buffered on the client and ready to
  // be played.
  int64 GetBufferedBytes() const;

  // Get the total size of the media file.  If the size has not yet been
  // determined or can not be determined, this value is 0.
  int64 GetTotalBytes() const;

  // Gets the natural size of the video output in pixel units.  If there is no
  // video or the video has not been rendered yet, the width and height will
  // be 0.
  void GetNaturalVideoSize(gfx::Size* out_size) const;

  // If this method returns true, that means the data source is a streaming
  // data source. Seeking may not be possible.
  bool IsStreaming() const;

  // If this method returns true, that means the data source is local and
  // the network is not needed.
  bool IsLocalSource() const;

  // Gets the current pipeline statistics.
  PipelineStatistics GetStatistics() const;

  void SetClockForTesting(Clock* clock);

 private:
  FRIEND_TEST_ALL_PREFIXES(PipelineTest, GetBufferedTime);
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
    kInitVideoDecoder,
    kInitVideoRenderer,
    kPausing,
    kSeeking,
    kFlushing,
    kStarting,
    kStarted,
    kEnded,
    kStopping,
    kStopped,
    kError,
  };

  // Reset the state of the pipeline object to the initial state.  This method
  // is used by the constructor, and the Stop() method.
  void ResetState();

  // Updates |state_|. All state transitions should use this call.
  void SetState(State next_state);

  // Simple method used to make sure the pipeline is running normally.
  bool IsPipelineOk();

  // Helper method to tell whether we are stopped or in error.
  bool IsPipelineStopped();

  // Helper method to tell whether we are in transition to stop state.
  bool IsPipelineTearingDown();

  // We could also be delayed by a transition during seek is performed.
  bool IsPipelineStopPending();

  // Helper method to tell whether we are in transition to seek state.
  bool IsPipelineSeeking();

  // Helper method to execute callback from Start() and reset
  // |filter_collection_|. Called when initialization completes
  // normally or when pipeline is stopped or error occurs during
  // initialization.
  void FinishInitialization();

  // Returns true if the given state is one that transitions to a new state
  // after iterating through each filter.
  static bool TransientState(State state);

  // Given the current state, returns the next state.
  State FindNextState(State current);

  // DataSourceHost (by way of DemuxerHost) implementation.
  virtual void SetTotalBytes(int64 total_bytes) OVERRIDE;
  virtual void SetBufferedBytes(int64 buffered_bytes) OVERRIDE;
  virtual void SetNetworkActivity(bool is_downloading_data) OVERRIDE;

  // DemuxerHost implementaion.
  virtual void SetDuration(base::TimeDelta duration) OVERRIDE;
  virtual void SetCurrentReadPosition(int64 offset) OVERRIDE;
  virtual void OnDemuxerError(PipelineStatus error) OVERRIDE;

  // FilterHost implementation.
  virtual void SetError(PipelineStatus error) OVERRIDE;
  virtual base::TimeDelta GetTime() const OVERRIDE;
  virtual base::TimeDelta GetDuration() const OVERRIDE;
  virtual void SetNaturalVideoSize(const gfx::Size& size) OVERRIDE;
  virtual void NotifyEnded() OVERRIDE;
  virtual void DisableAudioRenderer() OVERRIDE;

  // Callbacks executed by filters upon completing initialization.
  void OnFilterInitialize(PipelineStatus status);

  // Callback executed by filters upon completing Play(), Pause(), or Stop().
  void OnFilterStateTransition();

  // Callback executed by filters upon completing Seek().
  void OnFilterStateTransitionWithStatus(PipelineStatus status);

  // Callback executed by filters when completing teardown operations.
  void OnTeardownStateTransition();

  // Callback executed by filters to update statistics.
  void OnUpdateStatistics(const PipelineStatistics& stats);

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
                 const NetworkEventCB& network_cb,
                 const PipelineStatusCB& start_cb);

  // InitializeTask() performs initialization in multiple passes. It is executed
  // as a result of calling Start() or InitializationComplete() that advances
  // initialization to the next state. It works as a hub of state transition for
  // initialization.  One stage communicates its status to the next through
  // |last_stage_status|.
  void InitializeTask(PipelineStatus last_stage_status);

  // Stops and destroys all filters, placing the pipeline in the kStopped state.
  void StopTask(const base::Closure& stop_cb);

  // Carries out stopping and destroying all filters, placing the pipeline in
  // the kError state.
  void ErrorChangedTask(PipelineStatus error);

  // Carries out notifying filters that the playback rate has changed.
  void PlaybackRateChangedTask(float playback_rate);

  // Carries out notifying filters that the volume has changed.
  void VolumeChangedTask(float volume);

  // Carries out notifying filters that we are seeking to a new timestamp.
  void SeekTask(base::TimeDelta time, const PipelineStatusCB& seek_cb);

  // Carries out handling a notification from a filter that it has ended.
  void NotifyEndedTask();

  // Carries out handling a notification of network event.
  void NotifyNetworkEventTask(NetworkEvent type);

  // Carries out disabling the audio renderer.
  void DisableAudioRendererTask();

  // Carries out advancing to the next filter during Play()/Pause()/Seek().
  void FilterStateTransitionTask();

  // Carries out advancing to the next teardown operation.
  void TeardownStateTransitionTask();

  // Carries out stopping filter threads, deleting filters, running
  // appropriate callbacks, and setting the appropriate pipeline state
  // depending on whether we performing Stop() or SetError().
  // Called after all filters have been stopped.
  void FinishDestroyingFiltersTask();

  // Internal methods used in the implementation of the pipeline thread.  All
  // of these methods are only called on the pipeline thread.

  // The following initialize methods are used to select a specific type of
  // object from FilterCollection and initialize it asynchronously.
  void InitializeDemuxer();
  void OnDemuxerInitialized(PipelineStatus status);

  // Returns true if the asynchronous action of creating decoder has started.
  // Returns false if this method did nothing because the corresponding
  // audio/video stream does not exist.
  bool InitializeAudioDecoder(const scoped_refptr<Demuxer>& demuxer);
  bool InitializeVideoDecoder(const scoped_refptr<Demuxer>& demuxer);

  // Initializes a renderer and connects it with decoder. Returns true if the
  // asynchronous action of creating renderer has started. Returns
  // false if this method did nothing because the corresponding audio/video
  // stream does not exist.
  bool InitializeAudioRenderer(const scoped_refptr<AudioDecoder>& decoder);
  bool InitializeVideoRenderer(const scoped_refptr<VideoDecoder>& decoder);

  // Kicks off destroying filters. Called by StopTask() and ErrorChangedTask().
  // When we start to tear down the pipeline, we will consider two cases:
  // 1. when pipeline has not been initialized, we will transit to stopping
  // state first.
  // 2. when pipeline has been initialized, we will first transit to pausing
  // => flushing => stopping => stopped state.
  // This will remove the race condition during stop between filters.
  void TearDownPipeline();

  // Compute the current time. Assumes that the lock has been acquired by the
  // caller.
  base::TimeDelta GetCurrentTime_Locked() const;

  // Initiates a Stop() on |demuxer_| & |pipeline_filter_|. |callback|
  // is called once both objects have been stopped.
  void DoStop(const base::Closure& callback);

  // Called when |demuxer_| has stopped. This method calls Stop()
  // on |pipeline_filter_|.
  void OnDemuxerStopDone(const base::Closure& callback);

  // Initiates a Seek() on the |demuxer_| & |pipeline_filter_|.
  void DoSeek(base::TimeDelta seek_timestamp);

  // Called when |demuxer_| finishes seeking. If seeking was successful,
  // then Seek() is called on |pipeline_filter_|.
  void OnDemuxerSeekDone(base::TimeDelta seek_timestamp,
                         PipelineStatus status);

  void OnAudioUnderflow();

  // Called when |download_rate_monitor_| believes that the media can
  // be played through without needing to pause to buffer.
  void OnCanPlayThrough();

  // Carries out the notification that the media can be played through without
  // needing to pause to buffer.
  void NotifyCanPlayThrough();

  void StartClockIfWaitingForTimeUpdate_Locked();

  // Report pipeline |status| through |cb| avoiding duplicate error reporting.
  void ReportStatus(const PipelineStatusCB& cb, PipelineStatus status);

  // Message loop used to execute pipeline tasks.
  scoped_refptr<base::MessageLoopProxy> message_loop_;

  // MediaLog to which to log events.
  scoped_refptr<MediaLog> media_log_;

  // Lock used to serialize access for the following data members.
  mutable base::Lock lock_;

  // Whether or not the pipeline is running.
  bool running_;

  // Whether or not the pipeline is in transition for a seek operation.
  bool seek_pending_;

  // Whether or not the pipeline is pending a stop operation.
  bool stop_pending_;

  // Whether or not the pipeline is perform a stop operation.
  bool tearing_down_;

  // Whether or not an error triggered the teardown.
  bool error_caused_teardown_;

  // Whether or not a playback rate change should be done once seeking is done.
  bool playback_rate_change_pending_;

  // Amount of available buffered data.  Set by filters.
  int64 buffered_bytes_;

  // Total size of the media.  Set by filters.
  int64 total_bytes_;

  // Video's natural width and height.  Set by filters.
  gfx::Size natural_size_;

  // Set by the demuxer to indicate whether the data source is a streaming
  // source.
  bool streaming_;

  // Indicates whether the data source is local, such as a local media file
  // from disk or a local webcam stream.
  bool local_source_;

  // Current volume level (from 0.0f to 1.0f).  This value is set immediately
  // via SetVolume() and a task is dispatched on the message loop to notify the
  // filters.
  float volume_;

  // Current playback rate (>= 0.0f).  This value is set immediately via
  // SetPlaybackRate() and a task is dispatched on the message loop to notify
  // the filters.
  float playback_rate_;

  // Playback rate to set when the current seek has finished.
  float pending_playback_rate_;

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

  // For kSeeking we need to remember where we're seeking between filter
  // replies.
  base::TimeDelta seek_timestamp_;

  // For GetCurrentBytes()/SetCurrentBytes() we need to know what byte we are
  // currently reading.
  int64 current_bytes_;

  // Set to true in DisableAudioRendererTask().
  bool audio_disabled_;

  // Keep track of the maximum buffered position so the buffering appears
  // smooth.
  // TODO(vrk): This is a hack.
  base::TimeDelta max_buffered_time_;

  // Filter collection as passed in by Start().
  scoped_ptr<FilterCollection> filter_collection_;

  // Callbacks for various pipeline operations.
  PipelineStatusCB seek_cb_;
  base::Closure stop_cb_;
  PipelineStatusCB ended_cb_;
  PipelineStatusCB error_cb_;
  NetworkEventCB network_cb_;

  // Reference to the filter(s) that constitute the pipeline.
  scoped_refptr<Filter> pipeline_filter_;

  // Decoder reference used for signalling imminent shutdown.
  // This is a HACK necessary because WebMediaPlayerImpl::Destroy() holds the
  // renderer thread loop hostage for until PipelineImpl::Stop() calls its
  // callback.
  // This reference should only be used for this hack and no other purposes.
  // http://crbug.com/110228 tracks removing this hack.
  scoped_refptr<VideoDecoder> video_decoder_;

  // Renderer references used for setting the volume and determining
  // when playback has finished.
  scoped_refptr<AudioRenderer> audio_renderer_;
  scoped_refptr<VideoRenderer> video_renderer_;

  // Demuxer reference used for setting the preload value.
  scoped_refptr<Demuxer> demuxer_;

  // Helper class that stores filter references during pipeline
  // initialization.
  struct PipelineInitState;
  scoped_ptr<PipelineInitState> pipeline_init_state_;

  // Statistics.
  PipelineStatistics statistics_;
  // Time of pipeline creation; is non-zero only until the pipeline first
  // reaches "kStarted", at which point it is used & zeroed out.
  base::Time creation_time_;

  // Approximates the rate at which the media is being downloaded.
  DownloadRateMonitor download_rate_monitor_;

  // True if the pipeline is actively downloading bytes, false otherwise.
  bool is_downloading_data_;

  DISALLOW_COPY_AND_ASSIGN(Pipeline);
};

}  // namespace media

#endif  // MEDIA_BASE_PIPELINE_H_
