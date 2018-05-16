// Copyright 2018 Google Inc. All Rights Reserved.
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

#include "cobalt/media_capture/media_recorder.h"

#include <algorithm>
#include <cmath>

#include "base/message_loop.h"
#include "base/string_piece.h"
#include "cobalt/dom/array_buffer.h"
#include "cobalt/dom/blob.h"
#include "cobalt/dom/dom_exception.h"
#include "cobalt/media_stream/media_stream_track.h"
#include "cobalt/media_stream/media_track_settings.h"

namespace {

// See https://tools.ietf.org/html/rfc2586 for MIME type
const char kLinear16MimeType[] = "audio/L16";

const int32 kMinimumTimeSliceInMilliseconds = 1;

// Read Microphone input every few milliseconds, so that
// the input buffer doesn't fill up.
const int32 kDefaultMicrophoneReadThresholdMilliseconds = 50;

const double kSchedulingLatencyBufferSeconds = 0.20;

}  // namespace

namespace cobalt {
namespace media_capture {

void MediaRecorder::Start(int32 timeslice,
                          script::ExceptionState* exception_state) {
  DCHECK(thread_checker_.CalledOnValidThread());
  // Following the spec at
  // https://www.w3.org/TR/mediastream-recording/#mediarecorder-methods:

  // Step #1, not needed for Cobalt.

  // Step #2, done by Cobalt bindings.

  // Step #3
  if (recording_state_ != kRecordingStateInactive) {
    dom::DOMException::Raise(dom::DOMException::kInvalidStateErr,
                             "Internal error: Unable to get DOM settings.",
                             exception_state);
    return;
  }

  // Step #4-5.3, not needed for Cobalt.
  recording_state_ = kRecordingStateRecording;

  // Step #5.4, create a new Blob, and start collecting data.

  // If timeslice is not undefined, then once a minimum of timeslice
  // milliseconds of data have been collected, or some minimum time slice
  // imposed by the UA, whichever is greater, start gathering data into a new
  // Blob blob, and queue a task, using the DOM manipulation task source, that
  // fires a blob event named |dataavailable| at target.

  // We need to drain the media frequently, so try to read atleast once every
  // |read_frequency_| interval.
  int32 effective_time_slice_milliseconds =
      std::min(kDefaultMicrophoneReadThresholdMilliseconds, timeslice);
  // Avoid rounding down to 0 milliseconds.
  effective_time_slice_milliseconds = std::max(
      kMinimumTimeSliceInMilliseconds, effective_time_slice_milliseconds);
  read_frequency_ =
      base::TimeDelta::FromMilliseconds(effective_time_slice_milliseconds);

  // This is the frequency we will callback to Javascript.
  callback_frequency_ = base::TimeDelta::FromMilliseconds(timeslice);

  int64 buffer_size_hint = GetRecommendedBufferSize(callback_frequency_);
  recorded_buffer_.HintTypicalSize(static_cast<size_t>(buffer_size_hint));

  ResetLastCallbackTime();
  ReadStreamAndDoCallback();

  stream_reader_callback_.Reset(
      base::Bind(&MediaRecorder::ReadStreamAndDoCallback, this));
  MessageLoop::current()->PostTask(FROM_HERE,
                                   stream_reader_callback_.callback());

  // Step #5.5, not needed.

  // Step #6, return undefined.
}

void MediaRecorder::Stop(script::ExceptionState* exception_state) {
  DCHECK(thread_checker_.CalledOnValidThread());
  UNREFERENCED_PARAMETER(exception_state);
  NOTREACHED();
}

MediaRecorder::MediaRecorder(
    script::EnvironmentSettings* settings,
    const scoped_refptr<media_stream::MediaStream>& stream,
    const MediaRecorderOptions& options)
    : settings_(settings), stream_(stream) {
  // Per W3C spec, the default value of this is platform-specific,
  // so Linear16 was chosen. Spec url:
  // https://www.w3.org/TR/mediastream-recording/#dom-mediarecorder-mediarecorder
  mime_type_ =
      options.has_mime_type() ? options.mime_type() : kLinear16MimeType;
}

void MediaRecorder::ReadStreamAndDoCallback() {
  DCHECK(stream_);
  DCHECK(thread_checker_.CalledOnValidThread());

  size_t number_audio_tracks = stream_->GetAudioTracks().size();
  if (number_audio_tracks == 0) {
    LOG(WARNING) << "Audio Tracks are empty.";
    return;
  }
  LOG_IF(WARNING, number_audio_tracks > 1)
      << "Only recording the first audio track.";

  base::TimeTicks current_time = base::TimeTicks::Now();
  base::TimeDelta time_difference = last_callback_time_ - current_time;

  int64 recommended_buffer_size = GetRecommendedBufferSize(time_difference);
  base::StringPiece writeable_buffer = recorded_buffer_.GetWriteCursor(
      static_cast<size_t>(recommended_buffer_size));

  media_stream::MediaStreamTrack* track =
      stream_->GetAudioTracks().begin()->get();

  int64 bytes_read = track->Read(writeable_buffer);

  if (bytes_read < 0) {
    // An error occured, so do not post another read.
    DoOnDataCallback();
    return;
  }

  DCHECK_LE(bytes_read, static_cast<int64>(writeable_buffer.size()));
  recorded_buffer_.IncrementCursorPosition(static_cast<size_t>(bytes_read));

  if (current_time >= GetNextCallbackTime()) {
    DoOnDataCallback();
    ResetLastCallbackTime();
  }

  // Note that GetNextCallbackTime() should not be cached, since
  // ResetLastCallbackTime() above can change its value.
  base::TimeDelta time_until_expiration = GetNextCallbackTime() - current_time;

  // Consider the scenario where |time_until_expiration| is 4ms, and
  // read_frequency is 10ms.  In this case, just do the read 4 milliseconds
  // later, and then do the callback.
  base::TimeDelta delay_until_next_read =
      std::min(time_until_expiration, read_frequency_);

  MessageLoop::current()->PostDelayedTask(
      FROM_HERE, stream_reader_callback_.callback(), delay_until_next_read);
}

void MediaRecorder::DoOnDataCallback() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (recorded_buffer_.GetWrittenChunk().empty()) {
    DLOG(WARNING) << "No data was recorded.";
    return;
  }

  base::StringPiece written_data = recorded_buffer_.GetWrittenChunk();
  DCHECK_LE(written_data.size(), kuint32max);
  uint32 number_of_written_bytes = static_cast<uint32>(written_data.size());

  auto array_buffer = make_scoped_refptr(new dom::ArrayBuffer(
      settings_, reinterpret_cast<const uint8*>(written_data.data()),
      number_of_written_bytes));
  recorded_buffer_.Reset();

  auto blob = make_scoped_refptr(new dom::Blob(settings_, array_buffer));
  // TODO: Post a task to fire BlobEvent (constructed out of |blob| and
  // |array_buffer| at target.
}

void MediaRecorder::ResetLastCallbackTime() {
  DCHECK(thread_checker_.CalledOnValidThread());
  last_callback_time_ = base::TimeTicks::Now();
}

void MediaRecorder::CalculateStreamBitrate() {
  DCHECK(thread_checker_.CalledOnValidThread());
  media_stream::MediaStreamTrack* track =
      stream_->GetAudioTracks().begin()->get();
  const media_stream::MediaTrackSettings& settings = track->GetSettings();
  DCHECK_GT(settings.sample_rate(), 0);
  DCHECK_GT(settings.sample_size(), 0);
  DCHECK_GT(settings.channel_count(), 0);
  bitrate_bps_ = settings.sample_rate() * settings.sample_size() *
                 settings.channel_count();
  DCHECK_GT(bitrate_bps_, 0);
}

int64 MediaRecorder::GetRecommendedBufferSize(base::TimeDelta time_span) const {
  DCHECK_GE(time_span, base::TimeDelta::FromSeconds(0));
  // Increase buffer slightly to account for the fact that scheduling our
  // tasks might be a little bit noisy.
  double buffer_window_span_seconds =
      time_span.InSecondsF() + kSchedulingLatencyBufferSeconds;
  int64 recommended_buffer_size =
      static_cast<int64>(std::ceil(buffer_window_span_seconds * bitrate_bps_));
  DCHECK_GT(recommended_buffer_size, 0);
  return recommended_buffer_size;
}

bool MediaRecorder::IsTypeSupported(const base::StringPiece mime_type) {
  return mime_type == kLinear16MimeType;
}

base::StringPiece MediaRecorder::Buffer::GetWriteCursor(
    size_t number_of_bytes) {
  size_t minimim_required_size = current_position_ + number_of_bytes;
  if (minimim_required_size > buffer_.size()) {
    buffer_.resize(minimim_required_size);
  }
  return base::StringPiece(reinterpret_cast<const char*>(buffer_.data()),
                           number_of_bytes);
}

void MediaRecorder::Buffer::IncrementCursorPosition(size_t number_of_bytes) {
  size_t new_position = current_position_ + number_of_bytes;
  DCHECK_LE(new_position, buffer_.size());
  current_position_ = new_position;
}

base::StringPiece MediaRecorder::Buffer::GetWrittenChunk() const {
  return base::StringPiece(reinterpret_cast<const char*>(buffer_.data()),
                           current_position_);
}

void MediaRecorder::Buffer::Reset() {
  current_position_ = 0;
  buffer_.resize(0);
}

void MediaRecorder::Buffer::HintTypicalSize(size_t number_of_bytes) {
  // Cap the hint size to be 1 Megabyte.
  const size_t kMaxBufferSizeHintInBytes = 1024 * 1024;
  buffer_.reserve(std::min(number_of_bytes, kMaxBufferSizeHintInBytes));
}

}  // namespace media_capture
}  // namespace cobalt
