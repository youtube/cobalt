/*
 * Copyright 2013 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "media/audio/shell_audio_sink.h"

#include "base/bind.h"
#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "media/base/audio_bus.h"
#include "media/base/shell_media_statistics.h"
#include "media/filters/shell_audio_renderer.h"
#include "media/mp4/aac.h"

namespace media {

void AudioSinkSettings::Reset(const ShellAudioStreamer::Config& config,
                              const AudioParameters& audio_parameters) {
  config_ = config;
  audio_parameters_ = audio_parameters;
}

const ShellAudioStreamer::Config& AudioSinkSettings::config() const {
  return config_;
}

const AudioParameters& AudioSinkSettings::audio_parameters() const {
  return audio_parameters_;
}

int AudioSinkSettings::channels() const {
  return audio_parameters_.channels();
}

int AudioSinkSettings::per_channel_frames(AudioBus* audio_bus) const {
  return audio_bus->frames() * sizeof(float) /
      (config_.interleaved() ? channels() : 1) /
      (audio_parameters_.bits_per_sample() / 8);
}

// static
ShellAudioSink* ShellAudioSink::Create(ShellAudioStreamer* audio_streamer) {
  return new ShellAudioSink(audio_streamer);
}

ShellAudioSink::ShellAudioSink(ShellAudioStreamer* audio_streamer)
    : render_callback_(NULL)
    , pause_requested_(true)
    , rebuffering_(true)
    , rebuffer_num_frames_(0)
    , render_frame_cursor_(0)
    , output_frame_cursor_(0)
    , clock_bias_frames_(0)
    , audio_streamer_(audio_streamer) {
  buffer_factory_ = ShellBufferFactory::Instance();
}

ShellAudioSink::~ShellAudioSink() {
  DCHECK(!audio_streamer_->HasStream(this));
}

void ShellAudioSink::Initialize(const AudioParameters& params,
                                RenderCallback* callback) {
  TRACE_EVENT0("media_stack", "ShellAudioSink::Initialize()");
  DCHECK(!render_callback_);
  DCHECK(params.bits_per_sample() == 16 || params.bits_per_sample() == 32);

  render_callback_ = callback;
  audio_parameters_ = params;

  streamer_config_ = audio_streamer_->GetConfig();
  settings_.Reset(streamer_config_, params);

  audio_bus_ = AudioBus::Create(
      settings_.channels(),
      streamer_config_.sink_buffer_size_in_frames_per_channel(),
      audio_parameters_.bits_per_sample() / 8, streamer_config_.interleaved());
  if (!audio_bus_) {
    NOTREACHED() << "couldn't create sink buffer";
    render_callback_->OnRenderError();
    return;
  }

  rebuffer_num_frames_ =
      streamer_config_.initial_rebuffering_frames_per_channel();
  renderer_audio_bus_ = AudioBus::CreateWrapper(audio_bus_->channels());
}

void ShellAudioSink::Start() {
  TRACE_EVENT0("media_stack", "ShellAudioSink::Start()");

  if (!audio_streamer_->HasStream(this)) {
    pause_requested_ = true;
    rebuffering_ = true;
    audio_streamer_->StopBackgroundMusic();
    audio_streamer_->AddStream(this);
    DCHECK(audio_streamer_->HasStream(this));
  }
}

void ShellAudioSink::Stop() {
  TRACE_EVENT0("media_stack", "ShellAudioSink::Stop()");

  if (audio_streamer_->HasStream(this)) {
    audio_streamer_->RemoveStream(this);
    pause_requested_ = true;
    rebuffering_ = true;
    render_frame_cursor_ = 0;
    output_frame_cursor_ = 0;
  }
}

void ShellAudioSink::Pause(bool flush) {
  TRACE_EVENT0("media_stack", "ShellAudioSink::Pause()");
  // clear consumption of data on the mixer.
  pause_requested_ = true;
  if (flush) {
    TRACE_EVENT0("media_stack", "ShellAudioSink::Pause() flushing.");
    // remove and re-add the stream to flush
    audio_streamer_->RemoveStream(this);
    rebuffering_ = true;
    render_frame_cursor_ = 0;
    output_frame_cursor_ = 0;
    audio_streamer_->AddStream(this);
  }
}

void ShellAudioSink::Play() {
  TRACE_EVENT0("media_stack", "ShellAudioSink::Play()");
  // clear flag on mixer callback, will start to consume more data
  pause_requested_ = false;
}

void ShellAudioSink::SetPlaybackRate(float rate) {
  // if playback rate is 0.0 pause, if it's 1.0 go, if it's other
  // numbers issue a warning
  if (rate == 0.0f) {
    Pause(false);
  } else if (rate == 1.0f) {
    Play();
  } else {
    DLOG(WARNING) << "audio sink got unsupported playback rate: " << rate;
    Play();
  }
}

bool ShellAudioSink::SetVolume(double volume) {
  return audio_streamer_->SetVolume(this, volume);
}

void ShellAudioSink::ResumeAfterUnderflow(bool buffer_more_audio) {
  // only rebuffer when paused, we access state variables non atomically
  DCHECK(pause_requested_);
  DCHECK(rebuffering_);

  if (!buffer_more_audio)
    return;

  rebuffer_num_frames_ =
      std::min<int>(rebuffer_num_frames_ * 2,
                    settings_.per_channel_frames(audio_bus_.get()));
}

bool ShellAudioSink::PauseRequested() const {
  return pause_requested_ || rebuffering_;
}

bool ShellAudioSink::PullFrames(uint32_t* offset_in_frame,
                                uint32_t* total_frames) {
  TRACE_EVENT0("media_stack", "ShellAudioSink::PullFrames()");
  // with a valid render callback
  DCHECK(render_callback_);

  uint32_t dummy_offset_in_frame, dummy_total_frames;
  if (!offset_in_frame) offset_in_frame = &dummy_offset_in_frame;
  if (!total_frames) total_frames = &dummy_total_frames;

  *total_frames = render_frame_cursor_ - output_frame_cursor_;
  uint32 free_frames =
      settings_.per_channel_frames(audio_bus_.get()) - *total_frames;
  // Number of ms of buffered playback remaining
  uint32_t buffered_time =
      (*total_frames * 1000 / audio_parameters_.sample_rate());
  if (free_frames >= mp4::AAC::kSamplesPerFrame) {
    SetupRenderAudioBus();

    int frames_rendered = render_callback_->Render(renderer_audio_bus_.get(),
                                                   buffered_time);
    // 0 indicates the read is still pending. Positive number is # of frames
    // rendered, negative number indicates an error.
    if (frames_rendered > 0) {
      // +ve value indicates number of samples in a successful read
      // TODO(***REMOVED***) : We cannot guarantee this on PS3 because of the
      // resampler. Check if it is possible to move the resample into the
      // streamer.
      // DCHECK_EQ(frames_rendered, mp4::AAC::kSamplesPerFrame);
      render_frame_cursor_ += frames_rendered;
      *total_frames += frames_rendered;
      free_frames -= frames_rendered;
    }
  } else {
    // We don't need new data from the renderer, but this will ping the
    // renderer and update the timer
#if defined(__LB_WIIU__) || defined(__LB_ANDROID__) || defined(__LB_PS4__)
    // TODO(***REMOVED***) : Either remove this or make it generic across all
    // platforms.
    render_callback_->Render(NULL, buffered_time);
#endif
  }

  bool buffer_full = free_frames < mp4::AAC::kSamplesPerFrame;
  bool rebuffer_threshold_reached = *total_frames >= rebuffer_num_frames_;
  if (rebuffering_ && (buffer_full || rebuffer_threshold_reached)) {
    render_callback_->SinkFull();
    rebuffering_ = false;
  }

#if defined(__LB_LINUX__) || defined(__LB_WIIU__) || \
    defined(__LB_ANDROID__) || defined(__LB_PS4__)
  const size_t kUnderflowThreshold = mp4::AAC::kSamplesPerFrame / 2;
  if (*total_frames < kUnderflowThreshold) {
    if (!rebuffering_) {
      rebuffering_ = true;
      render_callback_->SinkUnderflow();
      UPDATE_MEDIA_STATISTICS(STAT_TYPE_AUDIO_UNDERFLOW, 0);
    }
  }
  *offset_in_frame =
      output_frame_cursor_ % settings_.per_channel_frames(audio_bus_.get());
  return !PauseRequested();
#else
  rebuffering_ = true;
  *offset_in_frame =
      output_frame_cursor_ % settings_.per_channel_frames(audio_bus_.get());
  if (pause_requested_) {
    return false;
  }
  return true;
#endif
}

void ShellAudioSink::ConsumeFrames(uint32_t frame_played) {
  TRACE_EVENT1("media_stack", "ShellAudioSink::ConsumeFrames()", "audio_clock",
               ((output_frame_cursor_ + clock_bias_frames_) * 1000) /
                   audio_parameters_.sample_rate());
  // Called by the Streamer thread to indicate where the hardware renderer
  // is in playback
  if (frame_played > 0) {
    // advance our output cursor by the number of frames we're returning
    // update audio clock, used for jitter calculations
    output_frame_cursor_ += frame_played;
    DCHECK_LE(output_frame_cursor_, render_frame_cursor_);
  }
}

media::AudioBus* ShellAudioSink::GetAudioBus() {
  return audio_bus_.get();
}

const media::AudioParameters& ShellAudioSink::GetAudioParameters() const {
  return audio_parameters_;
}

void ShellAudioSink::SetClockBiasMs(int64 time_ms) {
  // only prudent to do this when paused
  DCHECK(pause_requested_);
  clock_bias_frames_ = (time_ms * audio_parameters_.sample_rate()) / 1000;
}

void ShellAudioSink::SetupRenderAudioBus() {
  // check for buffer wraparound, hopefully rare
  int render_frame_position =
      render_frame_cursor_ % settings_.per_channel_frames(audio_bus_.get());
  int requested_frames = mp4::AAC::kSamplesPerFrame;
  if (render_frame_position + requested_frames >
      settings_.per_channel_frames(audio_bus_.get())) {
    requested_frames =
        settings_.per_channel_frames(audio_bus_.get()) - render_frame_position;
  }
  // calculate the offset into the buffer where we'd like to store these data
  if (streamer_config_.interleaved()) {
    uint8* channel_data = reinterpret_cast<uint8*>(audio_bus_->channel(0));
    uint8* channel_offset = channel_data + render_frame_position *
        audio_parameters_.bits_per_sample() / 8 * settings_.channels();
    // setup the AudioBus to pass to the renderer
    renderer_audio_bus_->SetChannelData(
        0, reinterpret_cast<float*>(channel_offset));
    renderer_audio_bus_->set_frames(
        requested_frames * audio_parameters_.bits_per_sample() / 8 /
        sizeof(float) * settings_.channels());
  } else {
    for (int i = 0; i < audio_bus_->channels(); ++i) {
      uint8* channel_data = reinterpret_cast<uint8*>(audio_bus_->channel(i));
      uint8* channel_offset = channel_data + render_frame_position *
          audio_parameters_.bits_per_sample() / 8;
      renderer_audio_bus_->SetChannelData(
          i, reinterpret_cast<float*>(channel_offset));
    }
    renderer_audio_bus_->set_frames(
        requested_frames * audio_parameters_.bits_per_sample() / 8 /
        sizeof(float));
  }
}

}  // namespace media
