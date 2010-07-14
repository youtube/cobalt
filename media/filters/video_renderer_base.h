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

#include "base/condition_variable.h"
#include "base/lock.h"
#include "base/scoped_ptr.h"
#include "media/base/filters.h"
#include "media/base/video_frame.h"

namespace media {

// TODO(scherkus): to avoid subclasses, consider using a peer/delegate interface
// and pass in a reference to the constructor.
class VideoRendererBase : public VideoRenderer,
                          public PlatformThread::Delegate {
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

  // MediaFilter implementation.
  virtual void Play(FilterCallback* callback);
  virtual void Pause(FilterCallback* callback);
  virtual void Stop(FilterCallback* callback);
  virtual void SetPlaybackRate(float playback_rate);
  virtual void Seek(base::TimeDelta time, FilterCallback* callback);

  // VideoRenderer implementation.
  virtual void Initialize(VideoDecoder* decoder, FilterCallback* callback);
  virtual bool HasEnded();

  // PlatformThread::Delegate implementation.
  virtual void ThreadMain();

  // Assigns the current frame, which will never be NULL as long as this filter
  // is initialized.
  void GetCurrentFrame(scoped_refptr<VideoFrame>* frame_out);

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

  virtual VideoDecoder* GetDecoder() {
    return decoder_.get();
  }

  int width() { return width_; }
  int height() { return height_; }
  VideoFrame::Format surface_format() { return surface_format_; }
  VideoFrame::SurfaceType surface_type() { return surface_type_; }

  // TODO(jiesun): move this to gles_video_render.cc.
  inline bool uses_egl_image() {
    return surface_type_ == media::VideoFrame::TYPE_EGL_IMAGE;
  }

 private:
  // Callback from video decoder to deliver decoded video frames and decrements
  // |pending_reads_|.
  void OnFillBufferDone(scoped_refptr<VideoFrame> frame);

  // Helper method that schedules an asynchronous read from the decoder and
  // increments |pending_reads_|.
  //
  // Safe to call from any thread.
  void ScheduleRead_Locked();

  // Calculates the duration to sleep for based on |current_frame_|'s timestamp,
  // the next frame timestamp (may be NULL), and the provided playback rate.
  //
  // We don't use |playback_rate_| to avoid locking.
  base::TimeDelta CalculateSleepDuration(VideoFrame* next_frame,
                                         float playback_rate);

  // Used for accessing data members.
  Lock lock_;

  scoped_refptr<VideoDecoder> decoder_;

  int width_;
  int height_;
  VideoFrame::Format surface_format_;
  VideoFrame::SurfaceType surface_type_;

  // Queue of incoming frames as well as the current frame since the last time
  // OnFrameAvailable() was called.
  typedef std::deque< scoped_refptr<VideoFrame> > VideoFrameQueue;
  VideoFrameQueue frames_;
  scoped_refptr<VideoFrame> current_frame_;

  // Used to signal |thread_| as frames are added to |frames_|.  Rule of thumb:
  // always check |state_| to see if it was set to STOPPED after waking up!
  ConditionVariable frame_available_;

  // Simple state tracking variable.
  enum State {
    kUninitialized,
    kPaused,
    kSeeking,
    kPlaying,
    kEnded,
    kStopped,
    kError,
  };
  State state_;

  // Video thread handle.
  PlatformThreadHandle thread_;

  // Previous time returned from the pipeline.
  base::TimeDelta previous_time_;

  // Keeps track of our pending reads.  We *must* have no pending reads before
  // executing the pause callback, otherwise we breach the contract that all
  // filters are idling.
  //
  // We use size_t since we compare against std::deque::size().
  size_t pending_reads_;

  float playback_rate_;

  // Filter callbacks.
  scoped_ptr<FilterCallback> pause_callback_;
  scoped_ptr<FilterCallback> seek_callback_;

  DISALLOW_COPY_AND_ASSIGN(VideoRendererBase);
};

}  // namespace media

#endif  // MEDIA_FILTERS_VIDEO_RENDERER_BASE_H_
