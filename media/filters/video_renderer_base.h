// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_VIDEO_RENDERER_BASE_H_
#define MEDIA_FILTERS_VIDEO_RENDERER_BASE_H_

#include <deque>

#include "base/memory/ref_counted.h"
#include "base/synchronization/condition_variable.h"
#include "base/synchronization/lock.h"
#include "base/threading/platform_thread.h"
#include "media/base/filters.h"
#include "media/base/pipeline_status.h"
#include "media/base/video_decoder.h"
#include "media/base/video_frame.h"

namespace media {

// VideoRendererBase creates its own thread for the sole purpose of timing frame
// presentation.  It handles reading from the decoder and stores the results in
// a queue of decoded frames and executing a callback when a frame is ready for
// rendering.
class MEDIA_EXPORT VideoRendererBase
    : public VideoRenderer,
      public base::PlatformThread::Delegate {
 public:
  typedef base::Callback<void(bool)> SetOpaqueCB;

  // |paint_cb| is executed on the video frame timing thread whenever a new
  // frame is available for painting via GetCurrentFrame().
  //
  // |set_opaque_cb| is executed when the renderer is initialized to inform
  // the player whether the decoder's output will be opaque or not.
  //
  // Implementors should avoid doing any sort of heavy work in this method and
  // instead post a task to a common/worker thread to handle rendering.  Slowing
  // down the video thread may result in losing synchronization with audio.
  //
  // Setting |drop_frames_| to true causes the renderer to drop expired frames.
  //
  // TODO(scherkus): pass the VideoFrame* to this callback and remove
  // Get/PutCurrentFrame() http://crbug.com/108435
  VideoRendererBase(const base::Closure& paint_cb,
                    const SetOpaqueCB& set_opaque_cb,
                    bool drop_frames);
  virtual ~VideoRendererBase();

  // Filter implementation.
  virtual void Play(const base::Closure& callback) OVERRIDE;
  virtual void Pause(const base::Closure& callback) OVERRIDE;
  virtual void Flush(const base::Closure& callback) OVERRIDE;
  virtual void Stop(const base::Closure& callback) OVERRIDE;
  virtual void SetPlaybackRate(float playback_rate) OVERRIDE;
  virtual void Seek(base::TimeDelta time, const PipelineStatusCB& cb) OVERRIDE;

  // VideoRenderer implementation.
  virtual void Initialize(const scoped_refptr<VideoDecoder>& decoder,
                          const PipelineStatusCB& status_cb,
                          const StatisticsCB& statistics_cb,
                          const TimeCB& time_cb) OVERRIDE;
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

 private:
  // Callback from the video decoder delivering decoded video frames and
  // reporting video decoder status.
  void FrameReady(VideoDecoder::DecoderStatus status,
                  scoped_refptr<VideoFrame> frame);

  // Helper method that schedules an asynchronous read from the decoder as long
  // as there isn't a pending read and we have capacity.
  void AttemptRead_Locked();

  // Called when the VideoDecoder Flush() completes.
  void OnDecoderFlushDone();

  // Attempts to complete flushing and transition into the flushed state.
  void AttemptFlush_Locked();

  // Calculates the duration to sleep for based on |current_frame_|'s timestamp,
  // the next frame timestamp (may be NULL), and the provided playback rate.
  //
  // We don't use |playback_rate_| to avoid locking.
  base::TimeDelta CalculateSleepDuration(
      const scoped_refptr<VideoFrame>& next_frame,
      float playback_rate);

  // Safely handles entering to an error state.
  void EnterErrorState_Locked(PipelineStatus status);

  // Helper function that flushes the buffers when a Stop() or error occurs.
  void DoStopOrError_Locked();

  // Return the number of frames currently held by this class.
  int NumFrames_Locked() const;

  // Used for accessing data members.
  base::Lock lock_;

  scoped_refptr<VideoDecoder> decoder_;

  // Queue of incoming frames as well as the current frame since the last time
  // OnFrameAvailable() was called.
  typedef std::deque<scoped_refptr<VideoFrame> > VideoFrameQueue;
  VideoFrameQueue ready_frames_;

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
  //   +------[kFlushed]<-----[kFlushing]<--- OnDecoderFlushDone()
  //   |          | Seek() or upon                  ^
  //   |          V got first frame           [kFlushingDecoder]
  //   |      [kSeeking]                            ^
  //   |          |                                 | Flush()
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
    kFlushingDecoder,
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

  bool drop_frames_;

  float playback_rate_;

  // Filter callbacks.
  base::Closure flush_cb_;
  PipelineStatusCB seek_cb_;
  StatisticsCB statistics_cb_;
  TimeCB time_cb_;

  base::TimeDelta seek_timestamp_;

  // Embedder callback for notifying a new frame is available for painting.
  base::Closure paint_cb_;

  // Callback to execute to inform the player if the video decoder's output is
  // opaque.
  SetOpaqueCB set_opaque_cb_;

  DISALLOW_COPY_AND_ASSIGN(VideoRendererBase);
};

}  // namespace media

#endif  // MEDIA_FILTERS_VIDEO_RENDERER_BASE_H_
