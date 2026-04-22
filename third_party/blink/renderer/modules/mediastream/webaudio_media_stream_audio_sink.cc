// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediastream/webaudio_media_stream_audio_sink.h"

#include <memory>
#include <string>

#include "base/logging.h"
#include "base/trace_event/trace_event.h"
#include "media/base/audio_fifo.h"
#include "media/base/audio_parameters.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/renderer/platform/media/web_audio_source_provider_client.h"

namespace {
static const size_t kMaxNumberOfAudioFifoBuffers = 100;
}

namespace blink {

// Size of the buffer that WebAudio processes each time, it is the same value
// as AudioNode::ProcessingSizeInFrames in WebKit.
// static
const int WebAudioMediaStreamAudioSink::kWebAudioRenderBufferSize = 128;

WebAudioMediaStreamAudioSink::WebAudioMediaStreamAudioSink(
    MediaStreamComponent* component,
    int context_sample_rate)
    : is_enabled_(false), component_(component), track_stopped_(false) {
  // Get the native audio output hardware sample-rate for the sink.
  // We need to check if there is a valid frame since the unittests
  // do not have one and they will inject their own |sink_params_| for testing.
  WebLocalFrame* const web_frame = WebLocalFrame::FrameForCurrentContext();
  if (web_frame) {
    sink_params_.Reset(media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                       media::ChannelLayoutConfig::Stereo(),
                       context_sample_rate, kWebAudioRenderBufferSize);
  }
  // Connect the source provider to the track as a sink.
  WebMediaStreamAudioSink::AddToAudioTrack(
      this, WebMediaStreamTrack(component_.Get()));
}

WebAudioMediaStreamAudioSink::~WebAudioMediaStreamAudioSink() {
  if (audio_converter_.get())
    audio_converter_->RemoveInput(this);

  // If the track is still active, it is necessary to notify the track before
  // the source provider goes away.
  if (!track_stopped_) {
    WebMediaStreamAudioSink::RemoveFromAudioTrack(
        this, WebMediaStreamTrack(component_.Get()));
  }
}

void WebAudioMediaStreamAudioSink::OnSetFormat(
    const media::AudioParameters& params) {
  DCHECK(params.IsValid());

  base::AutoLock auto_lock(lock_);
  LOG(INFO) << "KJ: WebAudio Sink - OnSetFormat: Input=" << params.AsHumanReadableString()
               << ", Sink=" << sink_params_.AsHumanReadableString();
  DCHECK(sink_params_.IsValid());

  source_params_ = params;
  pre_roll_frames_ = params.sample_rate() * 120 / 1000; // 120ms cushion
  max_allowed_frames_ = params.sample_rate() * 400 / 1000; // 400ms max latency
  is_pre_rolling_ = true;
  LOG(INFO) << "KJ: WebAudio Sink: Pre-roll cushion set to " << pre_roll_frames_
            << " frames, max latency " << max_allowed_frames_ << " frames";

  // Create the audio converter with |disable_fifo| as false so that the
  // converter will request source_params.frames_per_buffer() each time.
  // This will not increase the complexity as there is only one client to
  // the converter.
  audio_converter_ =
      std::make_unique<media::AudioConverter>(params, sink_params_, false);
  audio_converter_->AddInput(this);
  fifo_ = std::make_unique<media::AudioFifo>(
      params.channels(),
      kMaxNumberOfAudioFifoBuffers * params.frames_per_buffer());
}

void WebAudioMediaStreamAudioSink::OnReadyStateChanged(
    WebMediaStreamSource::ReadyState state) {
  NON_REENTRANT_SCOPE(ready_state_reentrancy_checker_);
  if (state == WebMediaStreamSource::kReadyStateEnded)
    track_stopped_ = true;
}

void WebAudioMediaStreamAudioSink::OnData(
    const media::AudioBus& audio_bus,
    base::TimeTicks estimated_capture_time) {
  NON_REENTRANT_SCOPE(capture_reentrancy_checker_);
  DCHECK(!estimated_capture_time.is_null());
  TRACE_EVENT2(TRACE_DISABLED_BY_DEFAULT("mediastream"),
               "WebAudioMediaStreamAudioSink::OnData", "this",
               static_cast<void*>(this), "frames", audio_bus.frames());

  base::AutoLock auto_lock(lock_);
  if (!fifo_.get())
    return;

  if (!is_enabled_) {
    // Before consumer starts, allow filling the FIFO up to pre-roll frames
    // so we have a cushion ready for the first pull.
    if (fifo_->frames() < pre_roll_frames_) {
      fifo_->Push(&audio_bus);
      LOG(INFO) << "KJ: WebAudio Sink: Pre-filling FIFO: level=" << fifo_->frames();
    }
    return;
  }

  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("mediastream"),
               "WebAudioMediaStreamAudioSink::OnData under lock");

  DCHECK_EQ(audio_bus.channels(), source_params_.channels());
  DCHECK_EQ(audio_bus.frames(), source_params_.frames_per_buffer());

  if (fifo_->frames() + audio_bus.frames() <= fifo_->max_frames()) {
    fifo_->Push(&audio_bus);
    total_frames_pushed_ += audio_bus.frames();

    if (is_enabled_ && fifo_->frames() > max_allowed_frames_) {
      LOG(WARNING) << "KJ: WebAudio Sink: FIFO OVERFLOW (latency too high): level=" << fifo_->frames()
                   << ", clearing and re-entering pre-roll";
      fifo_->Clear();
      is_pre_rolling_ = true;
    }

    LOG(INFO) << "KJ: WebAudio Sink: FIFO Input: frames=" << audio_bus.frames()
              << ", level_after=" << fifo_->frames()
              << ", total_pushed=" << total_frames_pushed_
              << ", capture_ts=" << estimated_capture_time
              << ", arrival_ts=" << base::TimeTicks::Now();
  } else {
    // This can happen if the data in FIFO is too slowly consumed or
    // WebAudio stops consuming data.
    LOG(WARNING) << "KJ: WebAudio Sink: FIFO FULL! level=" << fifo_->frames()
                 << ", max=" << fifo_->max_frames();
    TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("mediastream"),
                 "WebAudioMediaStreamAudioSink::OnData FIFO full");
  }
}

void WebAudioMediaStreamAudioSink::SetClient(
    WebAudioSourceProviderClient* client) {
  NOTREACHED();
}

void WebAudioMediaStreamAudioSink::ProvideInput(
    const WebVector<float*>& audio_data,
    int number_of_frames) {
  NON_REENTRANT_SCOPE(provide_input_reentrancy_checker_);
  DCHECK_EQ(number_of_frames, kWebAudioRenderBufferSize);

  TRACE_EVENT2(TRACE_DISABLED_BY_DEFAULT("mediastream"),
               "WebAudioMediaStreamAudioSink::ProvideInput", "this",
               static_cast<void*>(this), "frames", number_of_frames);

  if (!output_wrapper_ ||
      static_cast<size_t>(output_wrapper_->channels()) != audio_data.size()) {
    output_wrapper_ =
        media::AudioBus::CreateWrapper(static_cast<int>(audio_data.size()));
  }

  output_wrapper_->set_frames(number_of_frames);
  for (size_t i = 0; i < audio_data.size(); ++i)
    output_wrapper_->SetChannelData(static_cast<int>(i), audio_data[i]);

  base::AutoLock auto_lock(lock_);
  LOG(INFO) << "KJ: WebAudio Sink - Pulling " << number_of_frames
               << " frames for JS @ " << sink_params_.sample_rate() << "Hz";
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("mediastream"),
               "WebAudioMediaStreamAudioSink::ProvideInput under lock");

  if (!audio_converter_)
    return;

  is_enabled_ = true;
  audio_converter_->Convert(output_wrapper_.get());
}

// |lock_| needs to be acquired before this function is called. It's called by
// AudioConverter which in turn is called by the above ProvideInput() function.
// Thus thread safety analysis is disabled here and |lock_| acquire manually
// asserted.
double WebAudioMediaStreamAudioSink::ProvideInput(
    media::AudioBus* audio_bus,
    uint32_t frames_delayed,
    const media::AudioGlitchInfo& glitch_info) NO_THREAD_SAFETY_ANALYSIS {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("mediastream"),
               "WebAudioMediaStreamAudioSink::ProvideInput 2");

  lock_.AssertAcquired();
  const int frames_to_consume = audio_bus->frames();
  base::TimeTicks now = base::TimeTicks::Now();

  if (is_pre_rolling_) {
    if (fifo_->frames() < pre_roll_frames_) {
      audio_bus->Zero();
      return 1.0;
    }
    is_pre_rolling_ = false;
    LOG(INFO) << "KJ: WebAudio Sink: Pre-roll complete. FIFO level=" << fifo_->frames();
  }

  if (fifo_->frames() >= audio_bus->frames()) {
    total_frames_read_since_last_log_ += frames_to_consume;

    if (last_rate_log_time_.is_null()) {
      last_rate_log_time_ = now;
    } else if (now - last_rate_log_time_ >= base::Seconds(1)) {
      double delta_seconds = (now - last_rate_log_time_).InSecondsF();
      double rate = total_frames_read_since_last_log_ / delta_seconds;
      LOG(INFO) << "KJ: WebAudio Sink: Consumption Rate: rate=" << rate
                << ", target=" << source_params_.sample_rate()
                << ", total_pushed=" << total_frames_pushed_
                << ", total_consumed=" << total_frames_consumed_
                << ", diff=" << (total_frames_pushed_ - total_frames_consumed_);
      last_rate_log_time_ = now;
      total_frames_read_since_last_log_ = 0;
    }
    total_frames_consumed_ += frames_to_consume;

    fifo_->Consume(audio_bus, 0, audio_bus->frames());
  } else {
    LOG(WARNING) << "KJ: WebAudio Sink: FIFO STARVATION: requested=" << frames_to_consume
                 << ", available=" << fifo_->frames()
                 << ", ts=" << now;
    is_pre_rolling_ = true;
    audio_bus->Zero();
    TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("mediastream"),
                 "WebAudioMediaStreamAudioSink::ProvideInput underrun",
                 "frames missing", frames_to_consume - fifo_->frames());
    DVLOG(1) << "WARNING: Underrun, FIFO has data " << fifo_->frames()
             << " samples but " << frames_to_consume << " samples are needed";
  }

  return 1.0;
}

void WebAudioMediaStreamAudioSink::SetSinkParamsForTesting(
    const media::AudioParameters& sink_params) {
  base::AutoLock auto_lock(lock_);
  sink_params_ = sink_params;
}

}  // namespace blink
