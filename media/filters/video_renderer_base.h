// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// VideoRendererBase creates its own thread for the sole purpose of timing frame
// presentation.  It handles reading from the decoder and stores the results in
// a queue of decoded frames, calling OnFrameAvailable() on subclasses to notify
// when a frame is ready to display.
//
// The media filter methods Initialize(), Stop(), SetPlaybackRate() and Seek()
// should be serialized, which they commonly are the pipeline thread.
// As long as VideoRendererBase is initialized, GetCurrentFrame() is safe to
// call from any thread, at any time, including inside of OnFrameAvailable().

#ifndef MEDIA_FILTERS_VIDEO_RENDERER_BASE_H_
#define MEDIA_FILTERS_VIDEO_RENDERER_BASE_H_

#include <deque>

#include "base/memory/scoped_ptr.h"
#include "base/synchronization/condition_variable.h"
#include "base/synchronization/lock.h"
#include "base/threading/platform_thread.h"
#include "media/base/filters.h"
#include "media/base/pipeline_status.h"
#include "media/base/video_frame.h"

namespace media {

// TODO(scherkus): to avoid subclasses, consider using a peer/delegate interface
// and pass in a reference to the constructor.
class MEDIA_EXPORT VideoRendererBase
    : public VideoRenderer,
      public base::PlatformThread::Delegate {
 public:
  VideoRendererBase();
  virtual ~VideoRendererBase();

  // Filter implementation.
  virtual void Play(const base::Closure& callback) OVERRIDE;
  virtual void Pause(const base::Closure& callback) OVERRIDE;
  virtual void Flush(const base::Closure& callback) OVERRIDE;
  virtual void Stop(const base::Closure& callback) OVERRIDE;
  virtual void SetPlaybackRate(float playback_rate) OVERRIDE;
  virtual void Seek(base::TimeDelta time, const FilterStatusCB& cb) OVERRIDE;

  // VideoRenderer implementation.
  virtual void Initialize(VideoDecoder* decoder,
                          const base::Closure& callback,
                          const StatisticsCallback& stats_callback) OVERRIDE;
  virtual bool HasEnded() OVERRIDE;

  // PlatformThread::Delegate implementation.
  virtual void ThreadMain() OVERRIDE;

  // Clients of this class (painter/compositor) should use GetCurrentFrame()
  // obtain ownership of VideoFrame, it should always relinquish the ownership
  // by use PutCurrentFrame(). Current frame is not guaranteed to be non-NULL.
  // It expects clients to use color-fill the background if current frame
  // is NULL. This could happen before pipeline is pre-rolled or during
  // pause/flush/seek.
  void GetCurrentFrame(scoped_refptr<VideoFrame>* frame_out);
  void PutCurrentFrame(scoped_refptr<VideoFrame> frame);

 protected:
  // Subclass interface.  Called before any other initialization in the base
  // class takes place.
  //
  // Implementors typically use the media format of |decoder| to create their
  // output surfaces.
  virtual bool OnInitialize(VideoDecoder* decoder) = 0;

  // Subclass interface.  Called after all other stopping actions take place.
  //
  // Implementors should perform any necessary cleanup before calling the
  // callback.
  virtual void OnStop(const base::Closure& callback) = 0;

  // Subclass interface.  Called when a new frame is ready for display, which
  // can be accessed via GetCurrentFrame().
  //
  // Implementors should avoid doing any sort of heavy work in this method and
  // instead post a task to a common/worker thread to handle rendering.  Slowing
  // down the video thread may result in losing synchronization with audio.
  //
  // IMPORTANT: This method is called on the video renderer thread, which is
  // different from the thread OnInitialize(), OnStop(), and the rest of the
  // class executes on.
  virtual void OnFrameAvailable() = 0;

 private:
  // Callback from the video decoder delivering decoded video frames.
  void FrameReady(scoped_refptr<VideoFrame> frame);

  // Helper method that schedules an asynchronous read from the decoder as long
  // as there isn't a pending read and we have capacity.
  void AttemptRead_Locked();

  // Attempts to complete flushing and transition into the flushed state.
  void AttemptFlush_Locked();

  // Calculates the duration to sleep for based on |current_frame_|'s timestamp,
  // the next frame timestamp (may be NULL), and the provided playback rate.
  //
  // We don't use |playback_rate_| to avoid locking.
  base::TimeDelta CalculateSleepDuration(VideoFrame* next_frame,
                                         float playback_rate);

  // Safely handles entering to an error state.
  void EnterErrorState_Locked(PipelineStatus status);

  // Helper function that flushes the buffers when a Stop() or error occurs.
  void DoStopOrError_Locked();

  // Used for accessing data members.
  base::Lock lock_;

  scoped_refptr<VideoDecoder> decoder_;

  // Queue of incoming frames as well as the current frame since the last time
  // OnFrameAvailable() was called.
  typedef std::deque<scoped_refptr<VideoFrame> > VideoFrameQueue;
  VideoFrameQueue frames_queue_ready_;

  // The current frame available to subclasses for rendering via
  // GetCurrentFrame().  |current_frame_| can only be altered when
  // |pending_paint_| is false.
  scoped_refptr<VideoFrame> current_frame_;

  // The previous |current_frame_| and is returned via GetCurrentFrame() in the
  // situation where all frames were deallocated (i.e., during a flush).
  //
  // TODO(scherkus): remove this after getting rid of Get/PutCurrentFrame() in
  // favour of passing ownership of the current frame to the renderer via
  // callback.
  scoped_refptr<VideoFrame> last_available_frame_;

  // Used to signal |thread_| as frames are added to |frames_|.  Rule of thumb:
  // always check |state_| to see if it was set to STOPPED after waking up!
  base::ConditionVariable frame_available_;

  // State transition Diagram of this class:
  //       [kUninitialized] -------> [kError]
  //              |
  //              | Initialize()
  //              V        All frames returned
  //   +------[kFlushed]<----------------------[kFlushing]
  //   |          | Seek() or upon                  ^
  //   |          V got first frame                 |
  //   |      [kSeeking]                            | Flush()
  //   |          |                                 |
  //   |          V Got enough frames               |
  //   |      [kPrerolled]---------------------->[kPaused]
  //   |          |                Pause()          ^
  //   |          V Play()                          |
  //   |       [kPlaying]---------------------------|
  //   |          |                Pause()          ^
  //   |          V Receive EOF frame.              | Pause()
  //   |       [kEnded]-----------------------------+
  //   |                                            ^
  //   |                                            |
  //   +-----> [kStopped]                   [Any state other than]
  //                                        [kUninitialized/kError]

  // Simple state tracking variable.
  enum State {
    kUninitialized,
    kPrerolled,
    kPaused,
    kFlushing,
    kFlushed,
    kSeeking,
    kPlaying,
    kEnded,
    kStopped,
    kError,
  };
  State state_;

  // Video thread handle.
  base::PlatformThreadHandle thread_;

  // Previous time returned from the pipeline.
  base::TimeDelta previous_time_;

  // Keep track of various pending operations:
  //   - |pending_read_| is true when there's an active video decoding request.
  //   - |pending_paint_| is true when |current_frame_| is currently being
  //     accessed by the subclass.
  //   - |pending_paint_with_last_available_| is true when
  //     |last_available_frame_| is currently being accessed by the subclass.
  //
  // Flushing cannot complete until both |pending_read_| and |pending_paint_|
  // are false.
  bool pending_read_;
  bool pending_paint_;
  bool pending_paint_with_last_available_;

  float playback_rate_;

  // Filter callbacks.
  base::Closure flush_callback_;
  FilterStatusCB seek_cb_;
  StatisticsCallback statistics_callback_;

  base::TimeDelta seek_timestamp_;

  VideoDecoder::ReadCB read_cb_;

  DISALLOW_COPY_AND_ASSIGN(VideoRendererBase);
};

}  // namespace media

#endif  // MEDIA_FILTERS_VIDEO_RENDERER_BASE_H_
