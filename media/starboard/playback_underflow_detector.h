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

#ifndef MEDIA_STARBOARD_PLAYBACK_UNDERFLOW_DETECTOR_H_
#define MEDIA_STARBOARD_PLAYBACK_UNDERFLOW_DETECTOR_H_

#include <optional>
#include <string>

#include "base/time/time.h"
#include "media/base/demuxer_stream.h"
#include "media/base/media_export.h"
#include "media/base/timestamp_constants.h"

namespace media {

// Detects mid-playback underflow from the data written to an SbPlayer.
//
// SbPlayer has no rebuffering state: when one stream runs out of data while
// the other still has data (e.g. video starves while audio is buffered), the
// media clock keeps advancing on the remaining stream. This class tracks how
// far the written data of each stream extends and decides when playback has
// run out of it (underflow started) and when playback can continue (underflow
// ended).
//
// A stream is starved when less than |underflow_margin| of its written data
// is left ahead of the playback position while it waits on the demuxer. The
// margin allows for platforms whose clock stops slightly before the end of the
// written data (e.g. Starboard's audio renderer stops the clock itself when
// its buffer runs low).
//
// Like RendererImpl, a video-only starvation while audio still has data is
// tolerated for |video_underflow_delay| before it counts as an underflow, so
// short video hiccups don't interrupt audio; this includes audio that has
// written end of stream but still has data ahead. The delay starts when video
// falls behind while waiting on the demuxer, and is only cancelled once video
// has caught up again (has |underflow_margin| of data ahead), so a trickle of
// video data can't keep postponing it. Audio starvation, or video starvation
// without audio, starts an underflow right away.
//
// An underflow ends when every stream has |resume_threshold| of written data
// ahead, has written end of stream, or is saturated: no read pending and no
// data written for |saturation_delay|, i.e. the SbPlayer stopped asking for
// more. The last case covers pipelines that can't hold |resume_threshold| of
// data (or, while paused, stop accepting data before the stream catches up
// with the playback position), which would otherwise never resume. It doesn't
// require any data ahead: no more data would arrive, and a new underflow can
// only start once the stream waits on the demuxer again.
//
// It only decides; the owner is responsible for acting on the transitions
// (pausing/resuming the SbPlayer and reporting the buffering state), and for
// calling Update() periodically. It is not thread-safe and must be used on a
// single sequence.
class MEDIA_EXPORT PlaybackUnderflowDetector {
 public:
  struct Config {
    base::TimeDelta resume_threshold;
    base::TimeDelta underflow_margin;
    base::TimeDelta video_underflow_delay;
    base::TimeDelta saturation_delay;
  };

  // The demuxer read state of a stream, provided by the owner on Update().
  struct ReadState {
    // Whether the stream is waiting on the demuxer for more data.
    bool read_pending = false;
    // When data was last written for the stream, i.e. when the last demuxer
    // read completed. Null if nothing has been written.
    base::TimeTicks last_write_time;
  };

  enum class Transition {
    kNone,
    kUnderflowStarted,
    kUnderflowEnded,
  };

  explicit PlaybackUnderflowDetector(const Config& config);

  PlaybackUnderflowDetector(const PlaybackUnderflowDetector&) = delete;
  PlaybackUnderflowDetector& operator=(const PlaybackUnderflowDetector&) =
      delete;

  // Clears all written-data state and leaves underflow. Call on flush and
  // before playback (re)starts.
  void Reset(bool has_audio, bool has_video);

  // Leaves underflow without touching the written-data state, e.g. if the
  // SbPlayer presents again while in underflow.
  void ClearUnderflow();

  // Cancels a pending video-only underflow delay, e.g. when playback is paused
  // and Update() isn't called, so the time spent paused doesn't count towards
  // |video_underflow_delay|.
  void CancelVideoDeferral();

  // Records |buffers| written to the SbPlayer for |type|. Convenience wrapper
  // around OnBufferWritten() and OnEndOfStreamWritten().
  void OnBuffersWritten(DemuxerStream::Type type,
                        const DemuxerStream::DecoderBufferVector& buffers);
  // Records that a non end-of-stream buffer ending at |end_timestamp|
  // (timestamp + duration) has been written for |type|.
  void OnBufferWritten(DemuxerStream::Type type, base::TimeDelta end_timestamp);
  // Records that end of stream has been written for |type|.
  void OnEndOfStreamWritten(DemuxerStream::Type type);

  // Evaluates the current state at |now| against |media_time|, given the
  // read state of each stream. Returns the transition, if any, and updates
  // in_underflow() accordingly.
  Transition Update(base::TimeTicks now,
                    base::TimeDelta media_time,
                    const ReadState& audio_read,
                    const ReadState& video_read);

  bool in_underflow() const { return in_underflow_; }

  // Returns a human-readable summary of the written-data state, for logging.
  std::string GetDebugString() const;

 private:
  struct StreamState {
    bool present = false;
    // End timestamp (timestamp + duration) of the furthest written buffer.
    base::TimeDelta written_end = kNoTimestamp;
    bool eos_written = false;
  };

  StreamState* GetStreamState(DemuxerStream::Type type);

  // Returns how far the written data of |stream| extends beyond |media_time|,
  // or std::nullopt if the stream is absent or end of stream was written.
  static std::optional<base::TimeDelta> GetWrittenAhead(
      const StreamState& stream,
      base::TimeDelta media_time);

  bool IsStarved(std::optional<base::TimeDelta> ahead, bool read_pending) const;
  // Whether audio has at least |underflow_margin| of written data ahead of
  // |media_time|, whether or not end of stream has been written.
  bool HasAudioDataAhead(base::TimeDelta media_time) const;
  bool IsReadyToResume(std::optional<base::TimeDelta> ahead,
                       const ReadState& read,
                       base::TimeTicks now) const;
  Transition StartUnderflow();

  const Config config_;
  StreamState audio_;
  StreamState video_;
  bool in_underflow_ = false;
  // When the video-only underflow delay started; unset if video has caught up.
  std::optional<base::TimeTicks> video_starved_since_;
};

}  // namespace media

#endif  // MEDIA_STARBOARD_PLAYBACK_UNDERFLOW_DETECTOR_H_
