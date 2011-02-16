// Copyright (c) 2010 The Chromium Authors. All rights reserved.
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

#include "base/scoped_ptr.h"
#include "base/synchronization/condition_variable.h"
#include "base/synchronization/lock.h"
#include "base/threading/platform_thread.h"
#include "media/base/filters.h"
#include "media/base/video_frame.h"

namespace media {

// TODO(scherkus): to avoid subclasses, consider using a peer/delegate interface
// and pass in a reference to the constructor.
class VideoRendererBase : public VideoRenderer,
                          public base::PlatformThread::Delegate {
 public:
  VideoRendererBase();
  virtual ~VideoRendererBase();

  // Helper method to parse out video-related information from a MediaFormat.
  // Returns true all the required parameters are existent in |media_format|.
  // |surface_type_out|, |surface_format_out|, |width_out|, |height_out| can
  // be NULL where the result is not needed.
  static bool ParseMediaFormat(
      const MediaFormat& media_format,
      VideoFrame::SurfaceType* surface_type_out,
      VideoFrame::Format* surface_format_out,
      int* width_out, int* height_out);

  // Filter implementation.
  virtual void Play(FilterCallback* callback);
  virtual void Pause(FilterCallback* callback);
  virtual void Flush(FilterCallback* callback);
  virtual void Stop(FilterCallback* callback);
  virtual void SetPlaybackRate(float playback_rate);
  virtual void Seek(base::TimeDelta time, FilterCallback* callback);

  // VideoRenderer implementation.
  virtual void Initialize(VideoDecoder* decoder,
                          FilterCallback* callback,
                          StatisticsCallback* stats_callback);
  virtual bool HasEnded();

  // PlatformThread::Delegate implementation.
  virtual void ThreadMain();

  // Clients of this class (painter/compositor) should use GetCurrentFrame()
  // obtain ownership of VideoFrame, it should always relinquish the ownership
  // by use PutCurrentFrame(). Current frame is not guaranteed to be non-NULL.
  // It expects clients to use color-fill the background if current frame
  // is NULL. This could happen when before pipeline is pre-rolled or during
  // pause/flush/seek.
  void GetCurrentFrame(scoped_refptr<VideoFrame>* frame_out);
  void PutCurrentFrame(scoped_refptr<VideoFrame> frame);

 protected:
  // Subclass interface.  Called before any other initialization in the base
  // class takes place.
  //
  // Implementors typically use the media format of |decoder| to create their
  // output surfaces.  Implementors should NOT call InitializationComplete().
  virtual bool OnInitialize(VideoDecoder* decoder) = 0;

  // Subclass interface.  Called after all other stopping actions take place.
  //
  // Implementors should perform any necessary cleanup before calling the
  // callback.
  virtual void OnStop(FilterCallback* callback) = 0;

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

  virtual VideoDecoder* GetDecoder();

  int width() { return width_; }
  int height() { return height_; }
  VideoFrame::Format surface_format() { return surface_format_; }
  VideoFrame::SurfaceType surface_type() { return surface_type_; }

  void ReadInput(scoped_refptr<VideoFrame> frame);

 private:
  // Callback from video decoder to deliver decoded video frames and decrements
  // |pending_reads_|.
  void ConsumeVideoFrame(scoped_refptr<VideoFrame> frame);

  // Helper method that schedules an asynchronous read from the decoder and
  // increments |pending_reads_|.
  //
  // Safe to call from any thread.
  void ScheduleRead_Locked();

  // Helper function to finished "flush" operation
  void OnFlushDone();

  // Helper method that flushes all video frame in "ready queue" including
  // current frame into "done queue".
  void FlushBuffers();

  // Calculates the duration to sleep for based on |current_frame_|'s timestamp,
  // the next frame timestamp (may be NULL), and the provided playback rate.
  //
  // We don't use |playback_rate_| to avoid locking.
  base::TimeDelta CalculateSleepDuration(VideoFrame* next_frame,
                                         float playback_rate);

  // Used for accessing data members.
  base::Lock lock_;

  scoped_refptr<VideoDecoder> decoder_;

  int width_;
  int height_;
  VideoFrame::Format surface_format_;
  VideoFrame::SurfaceType surface_type_;

  // Queue of incoming frames as well as the current frame since the last time
  // OnFrameAvailable() was called.
  typedef std::deque< scoped_refptr<VideoFrame> > VideoFrameQueue;
  VideoFrameQueue frames_queue_ready_;
  VideoFrameQueue frames_queue_done_;
  scoped_refptr<VideoFrame> current_frame_;
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

  // Keeps track of our pending buffers. We *must* have no pending reads
  // before executing the flush callback; We decrement it each time we receive
  // a buffer and increment it each time we send a buffer out. therefore if
  // decoder provides buffer, |pending_reads_| is always non-positive and if
  // renderer provides buffer, |pending_reads_| is always non-negative.
  int pending_reads_;
  bool pending_paint_;
  bool pending_paint_with_last_available_;

  float playback_rate_;

  // Filter callbacks.
  scoped_ptr<FilterCallback> flush_callback_;
  scoped_ptr<FilterCallback> seek_callback_;
  scoped_ptr<StatisticsCallback> statistics_callback_;

  base::TimeDelta seek_timestamp_;

  DISALLOW_COPY_AND_ASSIGN(VideoRendererBase);
};

}  // namespace media

#endif  // MEDIA_FILTERS_VIDEO_RENDERER_BASE_H_
