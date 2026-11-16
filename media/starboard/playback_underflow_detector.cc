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

#include "media/starboard/playback_underflow_detector.h"

#include <algorithm>
#include <sstream>

#include "base/check.h"
#include "base/notreached.h"
#include "base/strings/to_string.h"
#include "media/base/decoder_buffer.h"

namespace media {

PlaybackUnderflowDetector::PlaybackUnderflowDetector(const Config& config)
    : config_(config) {
  CHECK(!config_.resume_threshold.is_negative());
  CHECK(!config_.underflow_margin.is_negative());
  CHECK(!config_.video_underflow_delay.is_negative());
  CHECK(!config_.saturation_delay.is_negative());
  CHECK_LT(config_.underflow_margin, config_.resume_threshold);
}

void PlaybackUnderflowDetector::Reset(bool has_audio, bool has_video) {
  audio_ = StreamState{.present = has_audio};
  video_ = StreamState{.present = has_video};
  ClearUnderflow();
}

void PlaybackUnderflowDetector::ClearUnderflow() {
  in_underflow_ = false;
  CancelVideoDeferral();
}

void PlaybackUnderflowDetector::CancelVideoDeferral() {
  video_starved_since_.reset();
}

void PlaybackUnderflowDetector::OnBuffersWritten(
    DemuxerStream::Type type,
    const DemuxerStream::DecoderBufferVector& buffers) {
  for (const auto& buffer : buffers) {
    if (buffer->end_of_stream()) {
      OnEndOfStreamWritten(type);
    } else {
      OnBufferWritten(type, buffer->timestamp() + std::max(buffer->duration(),
                                                           base::TimeDelta()));
    }
  }
}

void PlaybackUnderflowDetector::OnBufferWritten(DemuxerStream::Type type,
                                                base::TimeDelta end_timestamp) {
  StreamState* stream = GetStreamState(type);
  if (!stream) {
    return;
  }
  if (stream->written_end == kNoTimestamp ||
      end_timestamp > stream->written_end) {
    stream->written_end = end_timestamp;
  }
}

void PlaybackUnderflowDetector::OnEndOfStreamWritten(DemuxerStream::Type type) {
  StreamState* stream = GetStreamState(type);
  if (stream) {
    stream->eos_written = true;
  }
}

PlaybackUnderflowDetector::Transition PlaybackUnderflowDetector::Update(
    base::TimeTicks now,
    base::TimeDelta media_time,
    const ReadState& audio_read,
    const ReadState& video_read) {
  const std::optional<base::TimeDelta> audio_ahead =
      GetWrittenAhead(audio_, media_time);
  const std::optional<base::TimeDelta> video_ahead =
      GetWrittenAhead(video_, media_time);

  if (!in_underflow_) {
    if (IsStarved(audio_ahead, audio_read.read_pending)) {
      return StartUnderflow();
    }
    // Only cancel a pending video underflow delay once video has caught up. A
    // momentarily idle read (e.g. between two small appends) while video is
    // still behind must not restart the delay, or a trickle of video data
    // could keep video frozen indefinitely.
    if (!video_ahead || *video_ahead >= config_.underflow_margin) {
      CancelVideoDeferral();
      return Transition::kNone;
    }
    if (!video_starved_since_) {
      // Video is behind. As for audio, only a stream waiting on the demuxer
      // counts as starved.
      if (!video_read.read_pending) {
        return Transition::kNone;
      }
      video_starved_since_ = now;
    }
    // Video-only starvation while audio has data: tolerate it for a while, as
    // RendererImpl does, so short video hiccups don't interrupt audio. Audio
    // keeps the clock going while it has written data ahead, even after end
    // of stream has been written. Without that (no audio, or audio has played
    // out) nothing else keeps the clock going, so it is an underflow right
    // away.
    if (HasAudioDataAhead(media_time) &&
        now - *video_starved_since_ < config_.video_underflow_delay) {
      return Transition::kNone;
    }
    return StartUnderflow();
  }

  if (!IsReadyToResume(audio_ahead, audio_read, now) ||
      !IsReadyToResume(video_ahead, video_read, now)) {
    return Transition::kNone;
  }
  in_underflow_ = false;
  return Transition::kUnderflowEnded;
}

bool PlaybackUnderflowDetector::IsStarved(std::optional<base::TimeDelta> ahead,
                                          bool read_pending) const {
  // Absent streams and streams that wrote end of stream never starve. Only a
  // stream waiting on the demuxer counts: the renderer may also hold back
  // reads on purpose (e.g. to limit how far audio is written ahead).
  return ahead && *ahead < config_.underflow_margin && read_pending;
}

bool PlaybackUnderflowDetector::HasAudioDataAhead(
    base::TimeDelta media_time) const {
  return audio_.present && audio_.written_end != kNoTimestamp &&
         audio_.written_end - media_time >= config_.underflow_margin;
}

bool PlaybackUnderflowDetector::IsReadyToResume(
    std::optional<base::TimeDelta> ahead,
    const ReadState& read,
    base::TimeTicks now) const {
  if (!ahead || *ahead >= config_.resume_threshold) {
    return true;
  }
  // The SbPlayer stopped asking for more data: the pipeline can't take more
  // and won't reach |resume_threshold|, so waiting longer won't help. This
  // doesn't require any data ahead, or playback could stay paused forever. A
  // new underflow only starts once the stream waits on the demuxer again.
  // A null |last_write_time| (nothing written yet) intentionally counts as
  // saturated: with no read pending, the SbPlayer isn't asking for data.
  return !read.read_pending &&
         now - read.last_write_time >= config_.saturation_delay;
}

PlaybackUnderflowDetector::Transition
PlaybackUnderflowDetector::StartUnderflow() {
  in_underflow_ = true;
  CancelVideoDeferral();
  return Transition::kUnderflowStarted;
}

std::string PlaybackUnderflowDetector::GetDebugString() const {
  std::ostringstream ss;
  ss << "audio_written_end=" << audio_.written_end
     << ", audio_eos_written=" << base::ToString(audio_.eos_written)
     << ", video_written_end=" << video_.written_end
     << ", video_eos_written=" << base::ToString(video_.eos_written);
  ss << ", video_starved_since=";
  if (video_starved_since_) {
    ss << *video_starved_since_;
  } else {
    ss << "none";
  }
  return ss.str();
}

PlaybackUnderflowDetector::StreamState*
PlaybackUnderflowDetector::GetStreamState(DemuxerStream::Type type) {
  switch (type) {
    case DemuxerStream::AUDIO:
      return &audio_;
    case DemuxerStream::VIDEO:
      return &video_;
    default:
      NOTREACHED();
  }
}

// static
std::optional<base::TimeDelta> PlaybackUnderflowDetector::GetWrittenAhead(
    const StreamState& stream,
    base::TimeDelta media_time) {
  if (!stream.present || stream.eos_written) {
    return std::nullopt;
  }
  if (stream.written_end == kNoTimestamp) {
    return base::TimeDelta();
  }
  return stream.written_end - media_time;
}

}  // namespace media
