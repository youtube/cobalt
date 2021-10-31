// Copyright 2018 The Cobalt Authors. All Rights Reserved.
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
#include <memory>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util_starboard.h"
#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/base/tokens.h"
#include "cobalt/dom/blob.h"
#include "cobalt/dom/dom_exception.h"
#include "cobalt/dom/dom_settings.h"
#include "cobalt/media_capture/blob_event.h"
#include "cobalt/media_capture/encoders/flac_audio_encoder.h"
#include "cobalt/media_capture/encoders/linear16_audio_encoder.h"
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

using cobalt::media_capture::encoders::AudioEncoder;
using cobalt::media_capture::encoders::FlacAudioEncoder;
using cobalt::media_capture::encoders::Linear16AudioEncoder;

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

std::unique_ptr<AudioEncoder> CreateAudioEncoder(
    base::StringPiece requested_mime_type) {
  if (Linear16AudioEncoder::IsLinear16MIMEType(requested_mime_type)) {
    return std::unique_ptr<AudioEncoder>(new Linear16AudioEncoder());
  }
  if (FlacAudioEncoder::IsFlacMIMEType(requested_mime_type)) {
    return std::unique_ptr<AudioEncoder>(new FlacAudioEncoder());
  }
  return std::unique_ptr<AudioEncoder>(nullptr);
}

}  // namespace

namespace cobalt {
namespace media_capture {

void MediaRecorder::Start(int32 timeslice,
                          script::ExceptionState* exception_state) {
  DCHECK(stream_);
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
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
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (recording_state_ == kRecordingStateInactive) {
    dom::DOMException::Raise(dom::DOMException::kInvalidStateErr,
                             "Recording state must NOT be inactive.",
                             exception_state);
    return;
  }
  StopRecording();
}

void MediaRecorder::OnData(const AudioBus& audio_bus,
                           base::TimeTicks reference_time) {
  // The source is always int16 data from the microphone.
  DCHECK_EQ(audio_bus.sample_type(), AudioBus::kInt16);
  DCHECK_EQ(audio_bus.channels(), size_t(1));
  DCHECK(audio_encoder_);
  audio_encoder_->Encode(audio_bus, reference_time);
}

void MediaRecorder::OnEncodedDataAvailable(const uint8* data, size_t data_size,
                                           base::TimeTicks timecode) {
  auto data_to_send =
      base::WrapUnique(new std::vector<uint8>(data, data + data_size));
  javascript_message_loop_->PostTask(
      FROM_HERE, base::Bind(&MediaRecorder::CalculateLastInSliceAndWriteData,
                            weak_this_, base::Passed(&data_to_send), timecode));
}

void MediaRecorder::OnSetFormat(const media_stream::AudioParameters& params) {
  if (!audio_encoder_) {
    audio_encoder_ = CreateAudioEncoder(mime_type_);
    audio_encoder_->AddListener(this);
  }
  DCHECK(audio_encoder_);
  audio_encoder_->OnSetFormat(params);

  int64 bits_per_second =
      audio_encoder_->GetEstimatedOutputBitsPerSecond(params);
  if (bits_per_second <= 0) {
    return;
  }

  // Add some padding to the end of the buffer to account for jitter in
  // scheduling, etc.
  // This allows us to potentially avoid unnecessary resizing.
  // If the timeslice is using the default maximum long value, do not reserve
  // space for buffer as we don't know how much is needed.
  if (!timeslice_unspecified_) {
    base::TimeDelta recommended_time_slice =
        timeslice_ +
        base::TimeDelta::FromMilliseconds(kSchedulingLatencyBufferMilliseconds);
    int64 buffer_size_hint = GetRecommendedBufferSizeInBytes(
        recommended_time_slice, bits_per_second);
    buffer_.reserve(static_cast<size_t>(buffer_size_hint));
  }
}

void MediaRecorder::OnReadyStateChanged(
    media_stream::MediaStreamTrack::ReadyState new_state) {
  // Step 5.5 from start(), defined at:
  // https://www.w3.org/TR/mediastream-recording/#mediarecorder-methods
  if (new_state == media_stream::MediaStreamTrack::kReadyStateEnded) {
    if (audio_encoder_) {
      audio_encoder_->Finish(base::TimeTicks::Now());
      audio_encoder_.reset();
    }
    StopRecording();
    stream_ = nullptr;
  }
}

MediaRecorder::MediaRecorder(
    script::EnvironmentSettings* settings,
    const scoped_refptr<media_stream::MediaStream>& stream,
    const MediaRecorderOptions& options,
    script::ExceptionState* exception_state)
    : dom::EventTarget(settings),
      settings_(settings),
      stream_(stream),
      javascript_message_loop_(base::MessageLoop::current()->task_runner()),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)),
      weak_this_(weak_ptr_factory_.GetWeakPtr()) {
  DCHECK(settings);
  // Per W3C spec, the default value of this is platform-specific,
  // so Linear16 was chosen. Spec url:
  // https://www.w3.org/TR/mediastream-recording/#dom-mediarecorder-mediarecorder

  if (options.has_mime_type()) {
    if (!IsTypeSupported(options.mime_type())) {
      dom::DOMException::Raise(dom::DOMException::kNotSupportedErr,
                               exception_state);
      return;
    }
  }

  std::string desired_mime_type =
      options.has_mime_type() ? options.mime_type() : kLinear16MimeType;

  mime_type_ = desired_mime_type;
  DCHECK(IsTypeSupported(mime_type_));
  blob_options_.set_type(mime_type_);
}

MediaRecorder::MediaRecorder(
    script::EnvironmentSettings* settings,
    const scoped_refptr<media_stream::MediaStream>& stream,
    script::ExceptionState* exception_state)
    : MediaRecorder(settings, stream, MediaRecorderOptions(), exception_state) {
}

MediaRecorder::~MediaRecorder() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (recording_state_ != kRecordingStateInactive) {
    UnsubscribeFromTrack();
  }
}

void MediaRecorder::StopRecording() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(stream_);
  DCHECK_NE(recording_state_, kRecordingStateInactive);

  recording_state_ = kRecordingStateInactive;
  UnsubscribeFromTrack();

  std::unique_ptr<std::vector<uint8>> empty;
  base::TimeTicks now = base::TimeTicks::Now();
  WriteData(std::move(empty), true, now);

  timeslice_ = base::TimeDelta::FromSeconds(0);
  timeslice_unspecified_ = false;
  DispatchEvent(new dom::Event(base::Tokens::stop()));
}

void MediaRecorder::UnsubscribeFromTrack() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
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

void MediaRecorder::DoOnDataCallback(base::TimeTicks timecode) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (buffer_.empty()) {
    DLOG(WARNING) << "No data was recorded.";
    return;
  }

  DCHECK_LE(buffer_.size(), kuint32max);

  auto array_buffer = script::ArrayBuffer::New(
      base::polymorphic_downcast<dom::DOMSettings*>(settings_)
          ->global_environment(),
      buffer_.data(), buffer_.size());

  auto blob = base::WrapRefCounted(
      new dom::Blob(settings_, array_buffer, blob_options_));

  DispatchEvent(new media_capture::BlobEvent(base::Tokens::dataavailable(),
                                             blob, timecode.ToInternalValue()));
}

void MediaRecorder::WriteData(std::unique_ptr<std::vector<uint8>> data,
                              bool last_in_slice, base::TimeTicks timecode) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (data) {
    buffer_.insert(buffer_.end(), data->begin(), data->end());
  }

  if (!last_in_slice) {
    return;
  }

  DoOnDataCallback(timecode);
  buffer_.clear();
}

void MediaRecorder::CalculateLastInSliceAndWriteData(
    std::unique_ptr<std::vector<uint8>> data, base::TimeTicks timecode) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  base::TimeTicks now = base::TimeTicks::Now();
  bool last_in_slice = now > slice_origin_timestamp_ + timeslice_;

  if (last_in_slice) {
    DLOG(INFO) << "Slice finished.";
    // The next slice's timestamp is now.
    slice_origin_timestamp_ = now;
  }
  WriteData(std::move(data), last_in_slice, timecode);
}

bool MediaRecorder::IsTypeSupported(const base::StringPiece mime_type) {
  return Linear16AudioEncoder::IsLinear16MIMEType(mime_type) ||
         FlacAudioEncoder::IsFlacMIMEType(mime_type);
}

}  // namespace media_capture
}  // namespace cobalt
