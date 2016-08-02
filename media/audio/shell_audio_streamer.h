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

#ifndef MEDIA_AUDIO_SHELL_AUDIO_STREAMER_H_
#define MEDIA_AUDIO_SHELL_AUDIO_STREAMER_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "media/mp4/aac.h"

namespace media {

class AudioBus;
class AudioParameters;

// Abstract class for adding an audio stream to the audio streamer.
// Your class can implement this interface and then call AddStream(this) to
// attach itself to the hardware audio streamer.
class ShellAudioStream {
 public:
  // Checks if "Pause" has been requested on this stream. The streamer will
  // halt playback the next time it updates.
  virtual bool PauseRequested() const = 0;
  // This function serves several purposes:
  // 1. Once the audio stream is added to the streamer. This function will be
  // called periodically so the stream (the AudioSink) can pull data from
  // upper level, even when it is paused.
  // 2. It will return true to indicate that it is playing, false to pause.
  // The frame referred in this function is not an AAC frame but a PCM frame. It
  // contains a group of samples start at the same timestamp, each of them are
  // from different channels of a multi-channel audio stream.
  // NOTE: This function can be called on a low level audio mixer thread and
  // is LATENCY-SENSITIVE. Avoid locks and other high-latency operations!
  virtual bool PullFrames(uint32_t* offset_in_frame,
                          uint32_t* total_frames) = 0;
  // This function tells the stream that `frame_played` of audio frames have
  // been played and can be removed from the buffer. The stream can also use
  // this to calculate the time elapsed. The stream shouldn't pull any data
  // in this function, PullFrames is the only point to pull data.
  virtual void ConsumeFrames(uint32_t frame_played) = 0;
  // Get the AudioParameters for this stream
  virtual const AudioParameters& GetAudioParameters() const = 0;
  // Get the internal buffer of this audio stream as an AudioBus.
  virtual AudioBus* GetAudioBus() = 0;
};

// The class contains stub functions for platform specific audio playback.
// Classes inherited from it have to implement all the pure virtual functions
// and provide implementations for the static functions.
class ShellAudioStreamer {
 public:
  class Config {
   public:
    static const uint32 kInvalidSampleRate = 0;

    enum StorageMode { INTERLEAVED, PLANAR };

    Config() : valid_(false) {}

    // Initialize the Config settings, see the comment on individual member
    // below for more details.
    Config(StorageMode storage_mode,
           uint32 initial_rebuffering_frames_per_channel,
           uint32 sink_buffer_size_in_frames_per_channel,
           uint32 max_hardware_channels,
           uint32 bytes_per_sample,
           uint32 native_output_sample_rate = kInvalidSampleRate)
        : valid_(true),
          interleaved_(storage_mode == INTERLEAVED),
          initial_rebuffering_frames_per_channel_(
              initial_rebuffering_frames_per_channel),
          sink_buffer_size_in_frames_per_channel_(
              sink_buffer_size_in_frames_per_channel),
          max_hardware_channels_(max_hardware_channels),
          bytes_per_sample_(bytes_per_sample),
          native_output_sample_rate_(native_output_sample_rate) {
      const size_t kFramesPerAccessUnit = mp4::AAC::kFramesPerAccessUnit;
      DCHECK_LE(initial_rebuffering_frames_per_channel,
                sink_buffer_size_in_frames_per_channel);
      DCHECK_EQ(initial_rebuffering_frames_per_channel % kFramesPerAccessUnit,
                0);
      DCHECK_EQ(sink_buffer_size_in_frames_per_channel % kFramesPerAccessUnit,
                0);
    }

    bool interleaved() const {
      AssertValid();
      return interleaved_;
    }
    uint32 initial_rebuffering_frames_per_channel() const {
      AssertValid();
      return initial_rebuffering_frames_per_channel_;
    }
    uint32 sink_buffer_size_in_frames_per_channel() const {
      AssertValid();
      return sink_buffer_size_in_frames_per_channel_;
    }
    uint32 max_hardware_channels() const {
      AssertValid();
      return max_hardware_channels_;
    }
    uint32 bytes_per_sample() const {
      AssertValid();
      return bytes_per_sample_;
    }
    uint32 native_output_sample_rate() const {
      AssertValid();
      return native_output_sample_rate_;
    }

   private:
    void AssertValid() const { DCHECK(valid_); }

    bool valid_;

    // Is the data in audio bus interleaved and stored as one channel.
    bool interleaved_;
    // The following parameter controls the sink rebuffering.
    // See ShellAudioSink::ResumeAfterUnderflow for more details.
    uint32 initial_rebuffering_frames_per_channel_;
    uint32 sink_buffer_size_in_frames_per_channel_;
    // Max channels the current audio hardware can render. This can be changed
    // during the running of the application as the user can plug/unplug
    // different devices. So it represent the current status on the time of
    // query.
    uint32 max_hardware_channels_;
    uint32 bytes_per_sample_;
    uint32 native_output_sample_rate_;
  };

  struct OutputDevice {
    std::string connector;
    uint32 latency_ms;
    std::string coding_type;
    uint32 number_of_channels;
    uint32 sampling_frequency;
  };

  ShellAudioStreamer() {}
  virtual ~ShellAudioStreamer(){};

  // The only instance of the platform specific audio streamer. It becomes
  // valid after calling Initialize and become NULL after calling Terminate.
  static ShellAudioStreamer* Instance();
  static void Initialize();
  static void Terminate();

  virtual Config GetConfig() const = 0;
  virtual bool AddStream(ShellAudioStream* stream) = 0;
  virtual void RemoveStream(ShellAudioStream* stream) = 0;
  virtual bool HasStream(ShellAudioStream* stream) const = 0;
  virtual bool SetVolume(ShellAudioStream* stream, double volume) = 0;
  // Some consoles have background music tracks playing even when other apps
  // are running. This function can be used to stop the background music.
  virtual void StopBackgroundMusic() {}
  // Returns the available audio output devices.  This function is for
  // informational purpose and is currently only used to create
  // h5vcc::AudioConfig.
  virtual std::vector<OutputDevice> GetOutputDevices() {
    return std::vector<OutputDevice>();
  }

  DISALLOW_COPY_AND_ASSIGN(ShellAudioStreamer);
};

}  // namespace media

#endif  // MEDIA_AUDIO_SHELL_AUDIO_STREAMER_H_
