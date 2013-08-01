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

#ifndef MEDIA_AUDIO_SHELL_AUDIO_SINK_H_
#define MEDIA_AUDIO_SHELL_AUDIO_SINK_H_

#include "base/threading/thread.h"
#include "media/base/audio_renderer_sink.h"
#include "media/audio/shell_audio_streamer.h"
#include "media/base/shell_buffer_factory.h"
#include "media/base/shell_filter_graph_log.h"

namespace media {

// This class is used to manage the complexity of audio settings as the audio
// settings are determined by the original audio data (stereo, 5.1, etc. )
// and by the decoder (Some decoders decode mono into stereo) and hardware
// (some hardware requires audio data to be interleaved but others might
// require it to be non-interleaved.
class AudioSinkSettings {
 public:
  AudioSinkSettings() {}
  
  void Reset(const ShellAudioStreamer::Config& config,
             const AudioParameters& audio_parameters);
  const ShellAudioStreamer::Config& config() const;
  const AudioParameters& audio_parameters() const;

  int channels() const;
  int per_channel_frames(AudioBus* audio_bus) const;

 private:
  ShellAudioStreamer::Config config_;
  AudioParameters audio_parameters_;
};

// platform-specific implementation of an audio endpoint. In PS3 terms this
// object owns a libmixer Channel Strip.
class MEDIA_EXPORT ShellAudioSink
    : NON_EXPORTED_BASE(public AudioRendererSink),
      NON_EXPORTED_BASE(public ShellAudioStream) {
 public:
  ShellAudioSink(ShellAudioStreamer* audio_streamer);
  virtual ~ShellAudioSink();

  // static factory method
  static ShellAudioSink* Create(ShellAudioStreamer* audio_streamer);

  // AudioRendererSink implementation
  virtual void Initialize(const AudioParameters& params,
                          RenderCallback* callback,
                          ShellFilterGraphLog* filter_graph_log) OVERRIDE;
  virtual void Start() OVERRIDE;
  virtual void Stop() OVERRIDE;
  virtual void Pause(bool flush) OVERRIDE;
  virtual void Play() OVERRIDE;
  virtual void SetPlaybackRate(float rate);
  virtual bool SetVolume(double volume) OVERRIDE;
  virtual void ResumeAfterUnderflow(bool buffer_more_audio) OVERRIDE;

  // ShellAudioStream implementation
  virtual bool PauseRequested() const;
  virtual bool PullFrames(uint32_t* offset_in_frame,
                          uint32_t* total_frames) OVERRIDE;
  virtual void ConsumeFrames(uint32_t frame_played) OVERRIDE;
  virtual const media::AudioParameters& GetAudioParameters() const OVERRIDE;
  virtual media::AudioBus* GetAudioBus() OVERRIDE;

  // useful for jitter tracking
  void SetClockBiasMs(int64 time_ms);

 private:
  // Copy audio data inside audio_bus_ into target and keep the existing data
  // at the same offset. The size of target is guaranteed to be larger than
  // audio_bus_.
  void CopyAudioBus(scoped_ptr<media::AudioBus>* target);
  // Allocate memory for the sink_buffer_ for at least 'target_size' frames
  // and create the sink audio bus on top of it. If there is an existing
  // sink_buffer_ and its size is larger than or equal to the new size,
  // nothing will be done. If we fail to allocate the new buffer then the
  // existing one will be kept. It will also take care of copying the existing
  // data to the new audio bus.
  // Return true when a new audio bus is created or the size of the existing
  // audio bus is larger than or equal to target size.
  bool CreateSinkAudioBus(uint32 target_frames_per_channel);
  // Config the audio bus that will be sent to the AudioRenderer. It reueses
  // the memory occupied by the sink audio bus (audio_bus_).
  void SetupRenderAudioBus();

  AudioParameters audio_parameters_;
  RenderCallback* render_callback_;
  ShellFilterGraphLog* filter_graph_log_;

  scoped_ptr<media::AudioBus> audio_bus_;
  uint8* sink_buffer_;
  // Used as a paremeter when calling render_callback_->Render().
  // We can only construct it through a static Create method that does a heap
  // allocate so it is a member variable to avoid a heap allocation each
  // frame.
  scoped_ptr<AudioBus> renderer_audio_bus_;

  bool pause_requested_;
  bool rebuffering_;
  // Number of frames to rebuffer before calling SinkFull
  int rebuffer_num_frames_;

  // number of samples have been loaded into audio_bus from the Renderer
  // (and may have been played and since been overwritten by newer samples)
  uint64_t render_frame_cursor_;
  // advanced by ConsumeSamples() as the Streamer reports playback advancing
  uint64_t output_frame_cursor_;

  // For jitter logging we only keep track of rendered frames, so if we
  // seek or have another audio discontinuity the rendered frame count
  // will become different than the audio clock. Store a bias here so
  // that if the rendered frame count becomes reset we have some hope
  // of tracking the audio clock still.
  uint64_t clock_bias_frames_;

  scoped_refptr<ShellBufferFactory> buffer_factory_;
  ShellAudioStreamer* audio_streamer_;

  AudioSinkSettings settings_;
};

}  // namespace media

#endif  // MEDIA_AUDIO_SHELL_AUDIO_SINK_H_
