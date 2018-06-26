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
#include "base/string_util_starboard.h"
#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/base/tokens.h"
#include "cobalt/dom/blob.h"
#include "cobalt/dom/dom_exception.h"
#include "cobalt/dom/dom_settings.h"
#include "cobalt/media_capture/blob_event.h"
#include "cobalt/media_stream/audio_parameters.h"
#include "cobalt/media_stream/media_stream_audio_track.h"
#include "cobalt/media_stream/media_stream_track.h"
#include "cobalt/media_stream/media_track_settings.h"
#include "cobalt/script/array_buffer.h"

namespace {

// See https://tools.ietf.org/html/rfc2586 for MIME type
const char kLinear16MimeType[] = "audio/L16";

const int32 kMinimumTimeSliceInMilliseconds = 1;
const int32 kSchedulingLatencyBufferMilliseconds = 20;

// Returns the number of bytes needed to store |time_span| duration
// of audio that has a bitrate of |bits_per_second|.
int64 GetRecommendedBufferSizeInBytes(base::TimeDelta time_span,
                                      int64 bits_per_second) {
  DCHECK_GE(time_span, base::TimeDelta::FromSeconds(0));
  // Increase buffer slightly to account for the fact that scheduling our
  // tasks might be a little bit noisy.
  double buffer_window_span_seconds = time_span.InSecondsF();
  int64 recommended_buffer_size = static_cast<int64>(
      std::ceil(buffer_window_span_seconds * bits_per_second / 8));
  DCHECK_GT(recommended_buffer_size, 0);
  return recommended_buffer_size;
}
}  // namespace

namespace cobalt {
namespace media_capture {

void MediaRecorder::Start(int32 timeslice,
                          script::ExceptionState* exception_state) {
  DCHECK(stream_);
  DCHECK(thread_checker_.CalledOnValidThread());
  // Following the spec at
  // https://www.w3.org/TR/mediastream-recording/#mediarecorder-methods:

  // Step #1, not needed for Cobalt.

  // Step #2, done by Cobalt bindings.

  // Step #3
  if (recording_state_ != kRecordingStateInactive) {
    dom::DOMException::Raise(dom::DOMException::kInvalidStateErr,
                             "Recording state must be inactive.",
                             exception_state);
    return;
  }

  // Step #4, not needed for Cobalt.

  // Step 5.
  recording_state_ = kRecordingStateRecording;

  // Step 5.1.
  DispatchEvent(new dom::Event(base::Tokens::start()));

  // Steps 5.2-5.3, not needed for Cobalt.

  // Step #5.4, create a new Blob, and start collecting data.

  // If timeslice is not undefined, then once a minimum of timeslice
  // milliseconds of data have been collected, or some minimum time slice
  // imposed by the UA, whichever is greater, start gathering data into a new
  // Blob blob, and queue a task, using the DOM manipulation task source, that
  // fires a blob event named |dataavailable| at target.
  // Avoid rounding down to 0 milliseconds.
  int effective_time_slice_milliseconds =
      std::max(kMinimumTimeSliceInMilliseconds, timeslice);
  DCHECK_GT(effective_time_slice_milliseconds, 0);

  // This is the frequency we will callback to Javascript.
  timeslice_ =
      base::TimeDelta::FromMilliseconds(effective_time_slice_milliseconds);

  slice_origin_timestamp_ = base::TimeTicks::Now();

  script::Sequence<scoped_refptr<media_stream::MediaStreamTrack>>&
      audio_tracks = stream_->GetAudioTracks();

  size_t number_audio_tracks = audio_tracks.size();
  if (number_audio_tracks == 0) {
    LOG(WARNING) << "Audio Tracks are empty.";
    return;
  }
  LOG_IF(WARNING, number_audio_tracks > 1)
      << "Only recording the first audio track.";

  auto* first_audio_track =
      base::polymorphic_downcast<media_stream::MediaStreamAudioTrack*>(
          audio_tracks.begin()->get());
  DCHECK(first_audio_track);
  first_audio_track->AddSink(this);

  // Step #5.5 is implemented in |MediaRecorder::OnReadyStateChanged|.

  // Step #6, return undefined.
}

void MediaRecorder::Stop(script::ExceptionState* exception_state) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (recording_state_ == kRecordingStateInactive) {
    dom::DOMException::Raise(dom::DOMException::kInvalidStateErr,
                             "Recording state must NOT be inactive.",
                             exception_state);
    return;
  }
  StopRecording();
}

void MediaRecorder::OnData(const ShellAudioBus& audio_bus,
                           base::TimeTicks reference_time) {
  // The source is always int16 data from the microphone.
  DCHECK_EQ(audio_bus.sample_type(), ShellAudioBus::kInt16);
  DCHECK_EQ(audio_bus.channels(), 1);
  const char* data =
      reinterpret_cast<const char*>(audio_bus.interleaved_data());
  size_t data_size = audio_bus.GetSampleSizeInBytes() * audio_bus.frames();
  base::TimeTicks now = base::TimeTicks::Now();
  bool last_in_slice = now > slice_origin_timestamp_ + timeslice_;

  if (last_in_slice) {
    DLOG(INFO) << "Slice finished.";
    // The next slice's timestamp is now.
    slice_origin_timestamp_ = now;
  }

  WriteData(data, data_size, last_in_slice, reference_time);
}

void MediaRecorder::OnSetFormat(const media_stream::AudioParameters& params) {
  bits_per_second_ = params.GetBitsPerSecond();

  // Add some padding to the end of the buffer to account for jitter in
  // scheduling, etc.
  // This allows us to potentially avoid unnecessary resizing.
  base::TimeDelta recommended_time_slice =
      timeslice_ +
      base::TimeDelta::FromMilliseconds(kSchedulingLatencyBufferMilliseconds);
  int64 buffer_size_hint =
      GetRecommendedBufferSizeInBytes(recommended_time_slice, bits_per_second_);
  buffer_.reserve(static_cast<size_t>(buffer_size_hint));
}

void MediaRecorder::OnReadyStateChanged(
    media_stream::MediaStreamTrack::ReadyState new_state) {
  // Step 5.5 from start(), defined at:
  // https://www.w3.org/TR/mediastream-recording/#mediarecorder-methods
  if (new_state == media_stream::MediaStreamTrack::kReadyStateEnded) {
    StopRecording();
    stream_ = nullptr;
  }
}

MediaRecorder::MediaRecorder(
    script::EnvironmentSettings* settings,
    const scoped_refptr<media_stream::MediaStream>& stream,
    const MediaRecorderOptions& options)
    : settings_(settings),
      stream_(stream),
      javascript_message_loop_(base::MessageLoopProxy::current()) {
  DCHECK(settings);
  // Per W3C spec, the default value of this is platform-specific,
  // so Linear16 was chosen. Spec url:
  // https://www.w3.org/TR/mediastream-recording/#dom-mediarecorder-mediarecorder
  mime_type_ =
      options.has_mime_type() ? options.mime_type() : kLinear16MimeType;

  blob_options_.set_type(mime_type_);
}

void MediaRecorder::StopRecording() {
  DCHECK(stream_);
  DCHECK_NE(recording_state_, kRecordingStateInactive);

  recording_state_ = kRecordingStateInactive;
  UnsubscribeFromTrack();

  WriteData(nullptr, 0, true, base::TimeTicks::Now());

  timeslice_ = base::TimeDelta::FromSeconds(0);
  DispatchEvent(new dom::Event(base::Tokens::stop()));
}

void MediaRecorder::UnsubscribeFromTrack() {
  DCHECK(stream_);
  script::Sequence<scoped_refptr<media_stream::MediaStreamTrack>>&
      audio_tracks = stream_->GetAudioTracks();
  size_t number_audio_tracks = audio_tracks.size();
  if (number_audio_tracks == 0) {
    LOG(WARNING) << "Audio Tracks are empty.";
    return;
  }
  auto* first_audio_track =
      base::polymorphic_downcast<media_stream::MediaStreamAudioTrack*>(
          audio_tracks.begin()->get());
  DCHECK(first_audio_track);

  first_audio_track->RemoveSink(this);
}

void MediaRecorder::DoOnDataCallback(scoped_ptr<std::vector<uint8>> data,
                                     base::TimeTicks timecode) {
  if (javascript_message_loop_ != base::MessageLoopProxy::current()) {
    javascript_message_loop_->PostTask(
        FROM_HERE, base::Bind(&MediaRecorder::DoOnDataCallback, this,
                              base::Passed(&data), timecode));
    return;
  }
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(data);

  if (data->empty()) {
    DLOG(WARNING) << "No data was recorded.";
    return;
  }

  DCHECK_LE(data->size(), kuint32max);

  auto array_buffer = script::ArrayBuffer::New(
      base::polymorphic_downcast<dom::DOMSettings*>(settings_)
          ->global_environment(),
      data->data(), data->size());
  data->clear();

  auto blob =
      make_scoped_refptr(new dom::Blob(settings_, array_buffer, blob_options_));

  DispatchEvent(new media_capture::BlobEvent(base::Tokens::dataavailable(),
                                             blob, timecode.ToInternalValue()));
}

void MediaRecorder::WriteData(const char* data, size_t length,
                              bool last_in_slice, base::TimeTicks timecode) {
  buffer_.insert(buffer_.end(), data, data + length);

  if (!last_in_slice) {
    return;
  }

  auto buffer_to_send(make_scoped_ptr(new std::vector<uint8>()));
  buffer_to_send->swap(buffer_);
  // Use the previous buffer size as a proxy for the next buffer size.
  buffer_.reserve(buffer_to_send->size());
  DoOnDataCallback(buffer_to_send.Pass(), timecode);
}

bool MediaRecorder::IsTypeSupported(const base::StringPiece mime_type) {
  base::StringPiece mime_type_container = mime_type;
  size_t pos = mime_type.find_first_of(';');
  if (pos != base::StringPiece::npos) {
    mime_type_container = base::StringPiece(mime_type.begin(), pos);
  }
  const base::StringPiece linear16_mime_type(kLinear16MimeType);
  auto match_iterator =
      std::search(mime_type_container.begin(), mime_type_container.end(),
                  linear16_mime_type.begin(), linear16_mime_type.end(),
                  base::CaseInsensitiveCompareASCII<char>());
  return match_iterator == mime_type_container.begin();
}

}  // namespace media_capture
}  // namespace cobalt
