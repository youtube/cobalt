// Copyright (c) 2008-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Implementation of Pipeline.

#ifndef MEDIA_BASE_PIPELINE_IMPL_H_
#define MEDIA_BASE_PIPELINE_IMPL_H_

#include <set>
#include <string>
#include <vector>

#include "base/message_loop.h"
#include "base/ref_counted.h"
#include "base/thread.h"
#include "base/time.h"
#include "media/base/clock_impl.h"
#include "media/base/filter_host.h"
#include "media/base/pipeline.h"

namespace media {


// PipelineImpl runs the media pipeline.  Filters are created and called on the
// message loop injected into this object. PipelineImpl works like a state
// machine to perform asynchronous initialization, pausing, seeking and playing.
//
// Here's a state diagram that describes the lifetime of this object.
//
//   [ *Created ]
//         | Start()
//         V
//   [ InitXXX (for each filter) ]
//         |
//         V
//   [ Seeking (for each filter) ] <----------------------.
//         |                                              |
//         V                                              |
//   [ Starting (for each filter) ]                       |
//         |                                              |
//         V      Seek()                                  |
//   [ Started ] --------> [ Pausing (for each filter) ] -'
//         |                                              |
//         |   NotifyEnded()                Seek()        |
//         `-------------> [ Ended ] ---------------------'
//
//                  SetError()
//   [ Any State ] -------------> [ Error ]
//         |          Stop()
//         '--------------------> [ Stopped ]
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
  virtual bool Start(FilterFactory* filter_factory,
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
  virtual base::TimeDelta GetBufferedTime() const;
  virtual base::TimeDelta GetDuration() const;
  virtual int64 GetBufferedBytes() const;
  virtual int64 GetTotalBytes() const;
  virtual void GetVideoSize(size_t* width_out, size_t* height_out) const;
  virtual bool IsStreaming() const;
  virtual bool IsLoaded() const;
  virtual PipelineError GetError() const;

  // Sets a permanent callback owned by the pipeline that will be executed when
  // the media reaches the end.
  virtual void SetPipelineEndedCallback(PipelineCallback* ended_callback);

  // |error_callback_| will be executed upon an error in the pipeline. If
  // |error_callback_| is NULL, it is ignored. The pipeline takes ownership
  // of |error_callback|.
  virtual void SetPipelineErrorCallback(PipelineCallback* error_callback);

  // |network_callback_| will be executed when there's a network event.
  virtual void SetNetworkEventCallback(PipelineCallback* network_callback);

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
    kStarting,
    kStarted,
    kEnded,
    kStopped,
    kError,
  };

  virtual ~PipelineImpl();

  // Reset the state of the pipeline object to the initial state.  This method
  // is used by the constructor, and the Stop() method.
  void ResetState();

  // Simple method used to make sure the pipeline is running normally.
  bool IsPipelineOk();

  // Helper method to tell whether we are in the state of initializing.
  bool IsPipelineInitializing();

  // Returns true if the given state is one that transitions to the started
  // state.
  static bool StateTransitionsToStarted(State state);

  // Given the current state, returns the next state.
  static State FindNextState(State current);

  // FilterHost implementation.
  virtual void SetError(PipelineError error);
  virtual base::TimeDelta GetTime() const;
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
  virtual void BroadcastMessage(FilterMessage message);

  // Method called during initialization to insert a mime type into the
  // |rendered_mime_types_| set.
  void InsertRenderedMimeType(const std::string& major_mime_type);

  // Method called during initialization to determine if we rendered anything.
  bool HasRenderedMimeTypes() const;

  // Callback executed by filters upon completing initialization.
  void OnFilterInitialize();

  // Callback executed by filters upon completing Play(), Pause() or Seek().
  void OnFilterStateTransition();

  // The following "task" methods correspond to the public methods, but these
  // methods are run as the result of posting a task to the PipelineInternal's
  // message loop.
  void StartTask(FilterFactory* filter_factory,
                 const std::string& url,
                 PipelineCallback* start_callback);

  // InitializeTask() performs initialization in multiple passes. It is executed
  // as a result of calling Start() or InitializationComplete() that advances
  // initialization to the next state. It works as a hub of state transition for
  // initialization.
  void InitializeTask();

  // Stops and destroys all filters, placing the pipeline in the kStopped state
  // and setting the error code to PIPELINE_STOPPED.
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

  // Carries out message broadcasting on the message loop.
  void BroadcastMessageTask(FilterMessage message);

  // Carries out advancing to the next filter during Play()/Pause()/Seek().
  void FilterStateTransitionTask();

  // Internal methods used in the implementation of the pipeline thread.  All
  // of these methods are only called on the pipeline thread.

  // The following template functions make use of the fact that media filter
  // derived interfaces are self-describing in the sense that they all contain
  // the static method filter_type() which returns a FilterType enum that
  // uniquely identifies the filter's interface.  In addition, filters that are
  // specific to audio or video also support a static method major_mime_type()
  // which returns a string of "audio/" or "video/".
  //
  // Uses the FilterFactory to create a new filter of the Filter class, and
  // initializes it using the Source object.  The source may be another filter
  // or it could be a string in the case of a DataSource.
  //
  // The CreateFilter() method actually does much more than simply creating the
  // filter.  It also creates the filter's thread and injects a FilterHost and
  // MessageLoop.  Finally, it calls the filter's type-specific Initialize()
  // method to initialize the filter.  If the required filter cannot be created,
  // PIPELINE_ERROR_REQUIRED_FILTER_MISSING is raised, initialization is halted
  // and this object will remain in the "Error" state.
  template <class Filter, class Source>
  void CreateFilter(FilterFactory* filter_factory,
                    Source source,
                    const MediaFormat& source_media_format);

  // Creates a Filter and initializes it with the given |source|.  If a Filter
  // could not be created or an error occurred, this method returns NULL and the
  // pipeline's |error_| member will contain a specific error code.  Note that
  // the Source could be a filter or a DemuxerStream, but it must support the
  // GetMediaFormat() method.
  template <class Filter, class Source>
  void CreateFilter(FilterFactory* filter_factory, Source* source) {
    CreateFilter<Filter, Source*>(filter_factory,
                                  source,
                                  source->media_format());
  }

  // Creates a DataSource (the first filter in a pipeline).
  void CreateDataSource();

  // Creates a Demuxer.
  void CreateDemuxer();

  // Creates a decoder of type Decoder. Returns true if the asynchronous action
  // of creating decoder has started. Returns false if this method did nothing
  // because the corresponding audio/video stream does not exist.
  template <class Decoder>
  bool CreateDecoder();

  // Creates a renderer of type Renderer and connects it with Decoder. Returns
  // true if the asynchronous action of creating renderer has started. Returns
  // false if this method did nothing because the corresponding audio/video
  // stream does not exist.
  template <class Decoder, class Renderer>
  bool CreateRenderer();

  // Examine the list of existing filters to find one that supports the
  // specified Filter interface. If one exists, the |filter_out| will contain
  // the filter, |*filter_out| will be NULL.
  template <class Filter>
  void GetFilter(scoped_refptr<Filter>* filter_out) const;

  // Stops every filters, filter host and filter thread and releases all
  // references to them.
  void DestroyFilters();

  // Message loop used to execute pipeline tasks.
  MessageLoop* message_loop_;

  // Lock used to serialize access for the following data members.
  mutable Lock lock_;

  // Whether or not the pipeline is running.
  bool running_;

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
  ClockImpl clock_;

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

  // For kPausing, kSeeking and kStarting, we need to track how many filters
  // have completed transitioning to the destination state.  When
  // |remaining_transitions_| reaches 0 the pipeline can transition out
  // of the current state.
  size_t remaining_transitions_;

  // For kSeeking we need to remember where we're seeking between filter
  // replies.
  base::TimeDelta seek_timestamp_;

  // Filter factory as passed in by Start().
  scoped_refptr<FilterFactory> filter_factory_;

  // URL for the data source as passed in by Start().
  std::string url_;

  // Callbacks for various pipeline operations.
  scoped_ptr<PipelineCallback> seek_callback_;
  scoped_ptr<PipelineCallback> stop_callback_;
  scoped_ptr<PipelineCallback> ended_callback_;
  scoped_ptr<PipelineCallback> error_callback_;
  scoped_ptr<PipelineCallback> network_callback_;

  // Vector of our filters and map maintaining the relationship between the
  // FilterType and the filter itself.
  typedef std::vector<scoped_refptr<MediaFilter> > FilterVector;
  FilterVector filters_;

  typedef std::map<FilterType, scoped_refptr<MediaFilter> > FilterTypeMap;
  FilterTypeMap filter_types_;

  // Vector of threads owned by the pipeline and being used by filters.
  typedef std::vector<base::Thread*> FilterThreadVector;
  FilterThreadVector filter_threads_;

  DISALLOW_COPY_AND_ASSIGN(PipelineImpl);
};

}  // namespace media

#endif  // MEDIA_BASE_PIPELINE_IMPL_H_
