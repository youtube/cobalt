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

// platform-specific implementation of an audio endpoint.
class MEDIA_EXPORT ShellAudioSink : NON_EXPORTED_BASE(public AudioRendererSink),
                                    NON_EXPORTED_BASE(public ShellAudioStream) {
 public:
  ShellAudioSink(ShellAudioStreamer* audio_streamer);
  virtual ~ShellAudioSink();

  // static factory method
  static ShellAudioSink* Create(ShellAudioStreamer* audio_streamer);

  // AudioRendererSink implementation
  void Initialize(const AudioParameters& params,
                  RenderCallback* callback) override;
  void Start() override;
  void Stop() override;
  void Pause(bool flush) override;
  void Play() override;
  bool SetVolume(double volume) override;
  void ResumeAfterUnderflow(bool buffer_more_audio) override;

  // ShellAudioStream implementation
  bool PauseRequested() const override;
  bool PullFrames(uint32_t* offset_in_frame, uint32_t* total_frames) override;
  void ConsumeFrames(uint32_t frame_played) override;
  const AudioParameters& GetAudioParameters() const override;
  AudioBus* GetAudioBus() override;

 private:
  // Config the audio bus that will be sent to the AudioRenderer. It reueses
  // the memory occupied by the sink audio bus (audio_bus_).
  void SetupRenderAudioBus();

  AudioParameters audio_parameters_;
  RenderCallback* render_callback_;

  scoped_ptr<AudioBus> audio_bus_;

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

  scoped_refptr<ShellBufferFactory> buffer_factory_;
  ShellAudioStreamer* audio_streamer_;
  ShellAudioStreamer::Config streamer_config_;

  AudioSinkSettings settings_;
};

}  // namespace media

#endif  // MEDIA_AUDIO_SHELL_AUDIO_SINK_H_
