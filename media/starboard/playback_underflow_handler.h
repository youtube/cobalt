// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#ifndef MEDIA_STARBOARD_PLAYBACK_UNDERFLOW_HANDLER_H_
#define MEDIA_STARBOARD_PLAYBACK_UNDERFLOW_HANDLER_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "media/base/buffering_state.h"
#include "media/base/demuxer_stream.h"
#include "media/base/media_export.h"

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace media {

// Handles mid-playback underflow for a renderer driving an SbPlayer.
//
// SbPlayer has no rebuffering state, so when one stream runs out of data the
// media clock keeps advancing on the other one and the web app is never told.
// When enabled, the handler periodically checks, while presenting, whether
// playback has run past the end of the data written to the SbPlayer. On
// underflow it pauses the SbPlayer (stopping the media clock) and reports
// BUFFERING_HAVE_NOTHING; once playback can continue it resumes the SbPlayer
// and reports BUFFERING_HAVE_ENOUGH. Like RendererImpl, a video-only underflow
// while audio still has data is tolerated for a few seconds first. See
// PlaybackUnderflowDetector for the exact rules. When disabled, the handler
// does nothing.
//
// The owner forwards lifecycle events (start, presenting, end of stream,
// flush, written buffers) and implements Delegate. Must be created, used and
// destroyed on the task runner passed to Create().
class MEDIA_EXPORT PlaybackUnderflowHandler {
 public:
  class Delegate {
   public:
    // Current playback position.
    virtual base::TimeDelta GetCurrentMediaTime() = 0;
    // Whether the renderer is waiting on the demuxer for more |type| data.
    virtual bool IsReadPending(DemuxerStream::Type type) const = 0;
    // Whether the app has paused playback (requested playback rate 0).
    virtual bool IsPausedByApp() const = 0;
    // Whether the renderer is playing with a valid SbPlayer and no pending
    // flush, i.e. whether checking for underflow makes sense now.
    virtual bool IsPlaybackActive() const = 0;
    // Pauses the SbPlayer, stopping the media clock, without changing the
    // playback rate requested by the app.
    virtual void PauseSbPlayer() = 0;
    // Applies the playback rate requested by the app to the SbPlayer. If the
    // app paused in the meantime, the SbPlayer stays paused.
    virtual void ResumeSbPlayer() = 0;
    // Updates the renderer's buffering state and notifies its client.
    virtual void ReportBufferingState(BufferingState state,
                                      BufferingStateChangeReason reason) = 0;

   protected:
    virtual ~Delegate() = default;
  };

  // Returns a handler that pauses and resumes playback on underflow when
  // |enabled|, or a handler that does nothing otherwise. |delegate| must
  // outlive the returned handler.
  static std::unique_ptr<PlaybackUnderflowHandler> Create(
      bool enabled,
      Delegate* delegate,
      scoped_refptr<base::SequencedTaskRunner> task_runner);

  virtual ~PlaybackUnderflowHandler() = default;

  // Lifecycle events from the renderer.
  virtual void OnStartPlayingFrom(bool has_audio, bool has_video) = 0;
  // The SbPlayer reached kSbPlayerStatePresenting and BUFFERING_HAVE_ENOUGH
  // has been reported. Starts monitoring. If playback is still paused for
  // underflow, resumes it. This is defensive: it can only happen if the
  // SbPlayer is ever re-prerolled without a Flush().
  virtual void OnPresenting() = 0;
  // The SbPlayer reached kSbPlayerStateEndOfStream. Stops monitoring. If
  // playback is still paused for underflow (defensive), leaves underflow.
  virtual void OnEndOfStream() = 0;
  virtual void OnFlush() = 0;
  virtual void OnBuffersWritten(
      DemuxerStream::Type type,
      const DemuxerStream::DecoderBufferVector& buffers) = 0;

  // Whether playback is currently paused for underflow. While true, the
  // renderer must keep the SbPlayer paused; the app's playback rate is applied
  // when playback resumes.
  virtual bool IsPausedForUnderflow() const = 0;
};

}  // namespace media

#endif  // MEDIA_STARBOARD_PLAYBACK_UNDERFLOW_HANDLER_H_
