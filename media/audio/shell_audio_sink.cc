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
#include "base/logging.h"
#include "media/base/audio_bus.h"
#include "media/base/shell_filter_graph_log_constants.h"
#include "media/filters/shell_audio_renderer.h"

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
    , filter_graph_log_(NULL)
    , sink_buffer_(NULL)
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
  buffer_factory_->Reclaim(sink_buffer_);
  sink_buffer_ = NULL;
}

void ShellAudioSink::Initialize(const AudioParameters& params,
                                RenderCallback* callback,
                                ShellFilterGraphLog* filter_graph_log) {
  DCHECK(!render_callback_);
  DCHECK(params.bits_per_sample() == 16 || params.bits_per_sample() == 32);
  DCHECK(filter_graph_log);
  filter_graph_log_ = filter_graph_log;
  filter_graph_log_->LogEvent(kObjectIdAudioSink, kEventInitialize);

  render_callback_ = callback;
  audio_parameters_ = params;

  streamer_config_ = audio_streamer_->GetConfig();
  settings_.Reset(streamer_config_, params);

  // something is very wrong if we cannot create sink audio bus on creation
  if (!CreateSinkAudioBus(streamer_config_.initial_frames_per_channel())) {
    NOTREACHED() << "couldn't create sink buffer";
    render_callback_->OnRenderError();
    return;
  }

  rebuffer_num_frames_ =
      streamer_config_.initial_rebuffering_frames_per_channel();
  renderer_audio_bus_ = AudioBus::CreateWrapper(audio_bus_->channels());
}

void ShellAudioSink::Start() {
  filter_graph_log_->LogEvent(kObjectIdAudioSink, kEventStart);

  if (!audio_streamer_->HasStream(this)) {
    pause_requested_ = true;
    rebuffering_ = true;
    audio_streamer_->AddStream(this);
    DCHECK(audio_streamer_->HasStream(this));
  }
}

void ShellAudioSink::Stop() {
  filter_graph_log_->LogEvent(kObjectIdAudioSink, kEventStop);

  if (audio_streamer_->HasStream(this)) {
    audio_streamer_->RemoveStream(this);
    pause_requested_ = true;
    rebuffering_ = true;
    render_frame_cursor_ = 0;
    output_frame_cursor_ = 0;
  }
}

void ShellAudioSink::Pause(bool flush) {
  // clear consumption of data on the mixer.
  filter_graph_log_->LogEvent(kObjectIdAudioSink, kEventPause);
  pause_requested_ = true;
  if (flush) {
    // remove and re-add the stream to flush
    audio_streamer_->RemoveStream(this);
    filter_graph_log_->LogEvent(kObjectIdAudioSink, kEventFlush);
    rebuffering_ = true;
    render_frame_cursor_ = 0;
    output_frame_cursor_ = 0;
    audio_streamer_->AddStream(this);
  }
}

void ShellAudioSink::Play() {
  filter_graph_log_->LogEvent(kObjectIdAudioSink, kEventPlay);
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
  // only rebuffer when paused, we access state variables nonatomically
  DCHECK(pause_requested_);
  DCHECK(rebuffering_);

  if (!buffer_more_audio)
    return;

  // The rebuffering system is determinated by three factors: the current
  // rebuffering frame (R), the current buffer capacity in frame (C) and the
  // maximum buffer capacity in frame (M). We always have R <= C <= M.
  // For example, we can have a 1M buffer but will trigger SinkFull when we
  // have 512K audio frame in the buffer. In this case R = 512K while C = 1M.
  // When we are going to buffer more audio, we first increase R. If R is
  // larger than C, we have to re-allocate the buffer to C. C is capped at M.
  // Some platforms requires that the audio bus not being changed during
  // playback so we have to set C to M on these platforms.
  CreateSinkAudioBus(rebuffer_num_frames_ * 2);
  rebuffer_num_frames_ =
      std::min<int>(rebuffer_num_frames_ * 2,
                    settings_.per_channel_frames(audio_bus_.get()));
  DLOG(INFO) << "sink rebuffering, buffer now "
      << settings_.per_channel_frames(audio_bus_.get()) << " frames, "
      << audio_bus_->frames() * audio_bus_->channels() * sizeof(float)
      << " bytes";
}

bool ShellAudioSink::PauseRequested() const {
  return pause_requested_ || rebuffering_;
}

bool ShellAudioSink::PullFrames(uint32_t* offset_in_frame,
                                uint32_t* total_frames) {
  // with a valid render callback
  DCHECK(render_callback_);
  filter_graph_log_->LogEvent(kObjectIdAudioSink, kEventRender);

  uint32_t dummy_offset_in_frame, dummy_total_frames;
  if (!offset_in_frame) offset_in_frame = &dummy_offset_in_frame;
  if (!total_frames) total_frames = &dummy_total_frames;

  *total_frames = render_frame_cursor_ - output_frame_cursor_;
  uint32 free_frames =
      settings_.per_channel_frames(audio_bus_.get()) - *total_frames;
  // Number of ms of buffered playback remaining
  uint32_t buffered_time =
      (*total_frames * 1000 / audio_parameters_.sample_rate());
  if (free_frames >= streamer_config_.renderer_request_frames()) {
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
      // DCHECK_EQ(frames_rendered,
      //           streamer_config_.renderer_request_frames());
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

  bool buffer_full = free_frames < streamer_config_.renderer_request_frames();
  bool rebuffer_threshold_reached = *total_frames >= rebuffer_num_frames_;
  if (rebuffering_ && (buffer_full || rebuffer_threshold_reached)) {
    render_callback_->SinkFull();
    rebuffering_ = false;
  }

#if defined(__LB_LINUX__) || defined(__LB_WIIU__) || \
    defined(__LB_ANDROID__) || defined(__LB_PS4__)
  if (*total_frames < streamer_config_.underflow_threshold()) {
    if (!rebuffering_) {
      rebuffering_ = true;
      render_callback_->SinkUnderflow();
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
  // Called by the Streamer thread to indicate where the hardware renderer
  // is in playback
  if (frame_played > 0) {
    // advance our output cursor by the number of frames we're returning
    // update audio clock, used for jitter calculations
    filter_graph_log_->LogEvent(
        kObjectIdAudioSink, kEventAudioClock,
        ((output_frame_cursor_ + clock_bias_frames_) * 1000) /
         audio_parameters_.sample_rate());
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

void ShellAudioSink::CopyAudioBus(scoped_ptr<media::AudioBus>* target) {
  DCHECK(target);
  DCHECK_EQ(audio_bus_->channels(), (*target)->channels());
  DCHECK_LT(audio_bus_->frames(), (*target)->frames());
  // calculate how many frames were left in the buffer before underrun
  uint32 unplayed_frames = render_frame_cursor_ - output_frame_cursor_;
  uint32 frames_to_bytes = audio_parameters_.bits_per_sample() / 8;
  if (streamer_config_.interleaved()) {
    frames_to_bytes *= settings_.channels();
  }
  // check to see if we need to copy frames from the old buffer
  if (unplayed_frames > 0) {
    for (int i = 0; i < audio_bus_->channels(); ++i) {
      uint32 start_offset =
          output_frame_cursor_ % settings_.per_channel_frames(audio_bus_.get());
      uint32 frames_to_copy = std::min<uint32>(
          unplayed_frames,
          settings_.per_channel_frames(audio_bus_.get()) - start_offset);
      float* src_frame_pointer = audio_bus_->channel(i) +
          start_offset * frames_to_bytes / sizeof(float);
      float* target_frame_pointer = (*target)->channel(i);
      memcpy(target_frame_pointer, src_frame_pointer,
             frames_to_copy * frames_to_bytes);
      if (unplayed_frames > frames_to_copy) {
        src_frame_pointer = audio_bus_->channel(i);
        target_frame_pointer +=
            frames_to_copy * frames_to_bytes / sizeof(float);
        frames_to_copy = unplayed_frames - frames_to_copy;
        memcpy(target_frame_pointer, src_frame_pointer,
               frames_to_copy * frames_to_bytes);
      }
      DLOG(INFO) << "copying " << unplayed_frames << " audio frames, "
          << unplayed_frames * frames_to_bytes << " bytes, to new buffer.";
    }
  }
}

bool ShellAudioSink::CreateSinkAudioBus(uint32 target_frames_per_channel) {
  if (target_frames_per_channel > streamer_config_.max_frames_per_channel())
    target_frames_per_channel = streamer_config_.max_frames_per_channel();

  uint32 target_memory_size = audio_parameters_.bits_per_sample() / 8 *
      settings_.channels() * target_frames_per_channel;
  uint32 current_memory_size = 0;
  if (audio_bus_) {
    current_memory_size = audio_bus_->frames() * audio_bus_->channels() *
        sizeof(float);
  }
  if (target_memory_size <= current_memory_size)
    return true;
  uint8* new_buffer = buffer_factory_->AllocateNow(target_memory_size);
  scoped_ptr<media::AudioBus> new_audio_bus;
  if (!new_buffer)
    return false;

  // AudioBus treats everything in float so we have to convert.
  uint32 float_frame_per_channel = target_frames_per_channel *
      audio_parameters_.bits_per_sample() / 8 / sizeof(float);
  if (streamer_config_.interleaved()) {
    new_audio_bus = AudioBus::WrapMemory(
        1, settings_.channels() * float_frame_per_channel, new_buffer);
  } else {
    new_audio_bus = AudioBus::WrapMemory(
        settings_.channels(), float_frame_per_channel, new_buffer);
  }

  if (!audio_bus_) {
    sink_buffer_ = new_buffer;
    audio_bus_.swap(new_audio_bus);
    return true;
  }

  bool stream_added = audio_streamer_->HasStream(this);
  if (stream_added) audio_streamer_->RemoveStream(this);

  CopyAudioBus(&new_audio_bus);
  clock_bias_frames_ += output_frame_cursor_;
  // TODO(***REMOVED***) : The decoder/renderer relies on that the size of the
  // buffer passed to them is exactly 1024 frames to work properly. The
  // following operation will break this as output_frame_cursor_ maybe not be
  // exactly multiple times of 1024. This problem can be avoided by either fix
  // the code or ensure that the audio bus begins with the maximum buffer size
  // possible.
  render_frame_cursor_ -= output_frame_cursor_;
  output_frame_cursor_ = 0;

  buffer_factory_->Reclaim(sink_buffer_);
  sink_buffer_ = new_buffer;
  audio_bus_.swap(new_audio_bus);
  if (stream_added) audio_streamer_->AddStream(this);
  return true;
}

void ShellAudioSink::SetupRenderAudioBus() {
  // check for buffer wraparound, hopefully rare
  int render_frame_position =
      render_frame_cursor_ % settings_.per_channel_frames(audio_bus_.get());
  int requested_frames = streamer_config_.renderer_request_frames();
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
