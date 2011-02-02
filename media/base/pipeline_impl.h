// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Implementation of Pipeline.

#ifndef MEDIA_BASE_PIPELINE_IMPL_H_
#define MEDIA_BASE_PIPELINE_IMPL_H_

#include <set>
#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/message_loop.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "base/threading/thread.h"
#include "base/time.h"
#include "media/base/clock.h"
#include "media/base/composite_filter.h"
#include "media/base/filter_host.h"
#include "media/base/pipeline.h"

namespace media {


// PipelineImpl runs the media pipeline.  Filters are created and called on the
// message loop injected into this object. PipelineImpl works like a state
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
class PipelineImpl : public Pipeline, public FilterHost {
 public:
  explicit PipelineImpl(MessageLoop* message_loop);

  // Pipeline implementation.
  virtual void Init(PipelineCallback* ended_callback,
                    PipelineCallback* error_callback,
                    PipelineCallback* network_callback);
  virtual bool Start(FilterCollection* filter_collection,
                     const std::string& uri,
                     PipelineCallback* start_callback);
  virtual void Stop(PipelineCallback* stop_callback);
  virtual void Seek(base::TimeDelta time, PipelineCallback* seek_callback);
  virtual bool IsRunning() const;
  virtual bool IsInitialized() const;
  virtual bool IsNetworkActive() const;
  virtual bool IsRendered(const std::string& major_mime_type) const;
  virtual float GetPlaybackRate() const;
  virtual void SetPlaybackRate(float playback_rate);
  virtual float GetVolume() const;
  virtual void SetVolume(float volume);
  virtual base::TimeDelta GetCurrentTime() const;
  virtual base::TimeDelta GetBufferedTime();
  virtual base::TimeDelta GetMediaDuration() const;
  virtual int64 GetBufferedBytes() const;
  virtual int64 GetTotalBytes() const;
  virtual void GetVideoSize(size_t* width_out, size_t* height_out) const;
  virtual bool IsStreaming() const;
  virtual bool IsLoaded() const;
  virtual PipelineError GetError() const;

 private:
  // Pipeline states, as described above.
  enum State {
    kCreated,
    kInitDataSource,
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

  virtual ~PipelineImpl();

  // Reset the state of the pipeline object to the initial state.  This method
  // is used by the constructor, and the Stop() method.
  void ResetState();

  // Updates |state_|. All state transitions should use this call.
  void set_state(State next_state);

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

  // FilterHost implementation.
  virtual void SetError(PipelineError error);
  virtual base::TimeDelta GetTime() const;
  virtual base::TimeDelta GetDuration() const;
  virtual void SetTime(base::TimeDelta time);
  virtual void SetDuration(base::TimeDelta duration);
  virtual void SetBufferedTime(base::TimeDelta buffered_time);
  virtual void SetTotalBytes(int64 total_bytes);
  virtual void SetBufferedBytes(int64 buffered_bytes);
  virtual void SetVideoSize(size_t width, size_t height);
  virtual void SetStreaming(bool streamed);
  virtual void SetLoaded(bool loaded);
  virtual void SetNetworkActivity(bool network_activity);
  virtual void NotifyEnded();
  virtual void DisableAudioRenderer();
  virtual void SetCurrentReadPosition(int64 offset);
  virtual int64 GetCurrentReadPosition();

  // Method called during initialization to insert a mime type into the
  // |rendered_mime_types_| set.
  void InsertRenderedMimeType(const std::string& major_mime_type);

  // Method called during initialization to determine if we rendered anything.
  bool HasRenderedMimeTypes() const;

  // Callback executed by filters upon completing initialization.
  void OnFilterInitialize();

  // Callback executed by filters upon completing Play(), Pause(), Seek(),
  // or Stop().
  void OnFilterStateTransition();

  // Callback executed by filters when completing teardown operations.
  void OnTeardownStateTransition();

  // The following "task" methods correspond to the public methods, but these
  // methods are run as the result of posting a task to the PipelineInternal's
  // message loop.
  void StartTask(FilterCollection* filter_collection,
                 const std::string& url,
                 PipelineCallback* start_callback);

  // InitializeTask() performs initialization in multiple passes. It is executed
  // as a result of calling Start() or InitializationComplete() that advances
  // initialization to the next state. It works as a hub of state transition for
  // initialization.
  void InitializeTask();

  // Stops and destroys all filters, placing the pipeline in the kStopped state.
  void StopTask(PipelineCallback* stop_callback);

  // Carries out stopping and destroying all filters, placing the pipeline in
  // the kError state.
  void ErrorChangedTask(PipelineError error);

  // Carries out notifying filters that the playback rate has changed.
  void PlaybackRateChangedTask(float playback_rate);

  // Carries out notifying filters that the volume has changed.
  void VolumeChangedTask(float volume);

  // Carries out notifying filters that we are seeking to a new timestamp.
  void SeekTask(base::TimeDelta time, PipelineCallback* seek_callback);

  // Carries out handling a notification from a filter that it has ended.
  void NotifyEndedTask();

  // Carries out handling a notification of network event.
  void NotifyNetworkEventTask();

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

  // PrepareFilter() creates the filter's thread and injects a FilterHost and
  // MessageLoop.
  bool PrepareFilter(scoped_refptr<Filter> filter);

  // The following initialize methods are used to select a specific type of
  // Filter object from FilterCollection and initialize it asynchronously.
  void InitializeDataSource();
  void InitializeDemuxer(const scoped_refptr<DataSource>& data_source);

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

  // Helper to find the demuxer of |major_mime_type| from Demuxer.
  scoped_refptr<DemuxerStream> FindDemuxerStream(
      const scoped_refptr<Demuxer>& demuxer,
      std::string major_mime_type);

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

  // Message loop used to execute pipeline tasks.
  MessageLoop* message_loop_;

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

  // Duration of the media in microseconds.  Set by filters.
  base::TimeDelta duration_;

  // Amount of available buffered data in microseconds.  Set by filters.
  base::TimeDelta buffered_time_;

  // Amount of available buffered data.  Set by filters.
  int64 buffered_bytes_;

  // Total size of the media.  Set by filters.
  int64 total_bytes_;

  // Video width and height.  Set by filters.
  size_t video_width_;
  size_t video_height_;

  // Sets by the filters to indicate whether the data source is a streaming
  // source.
  bool streaming_;

  // Sets by the filters to indicate whether the data source is a fully
  // loaded source.
  bool loaded_;

  // Sets by the filters to indicate whether network is active.
  bool network_activity_;

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
  PipelineError error_;

  // Vector of major mime types that have been rendered by this pipeline.
  typedef std::set<std::string> RenderedMimeTypesSet;
  RenderedMimeTypesSet rendered_mime_types_;

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

  // URL for the data source as passed in by Start().
  std::string url_;

  // Callbacks for various pipeline operations.
  scoped_ptr<PipelineCallback> seek_callback_;
  scoped_ptr<PipelineCallback> stop_callback_;
  scoped_ptr<PipelineCallback> ended_callback_;
  scoped_ptr<PipelineCallback> error_callback_;
  scoped_ptr<PipelineCallback> network_callback_;

  // Reference to the filter(s) that constitute the pipeline.
  scoped_refptr<Filter> pipeline_filter_;

  // Renderer references used for setting the volume and determining
  // when playback has finished.
  scoped_refptr<AudioRenderer> audio_renderer_;
  scoped_refptr<VideoRenderer> video_renderer_;

  // Helper class that stores filter references during pipeline
  // initialization.
  class PipelineInitState;
  scoped_ptr<PipelineInitState> pipeline_init_state_;

  FRIEND_TEST_ALL_PREFIXES(PipelineImplTest, GetBufferedTime);
  FRIEND_TEST_ALL_PREFIXES(PipelineImplTest, AudioStreamShorterThanVideo);

  DISALLOW_COPY_AND_ASSIGN(PipelineImpl);
};

}  // namespace media

#endif  // MEDIA_BASE_PIPELINE_IMPL_H_
