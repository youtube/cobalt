// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_MEDIA_AUDIO_COBALT_AUDIO_CAPTURER_SOURCE_H
#define COBALT_MEDIA_AUDIO_COBALT_AUDIO_CAPTURER_SOURCE_H

#include "media/base/audio_capturer_source.h"

#include <atomic>

#include "media/base/media_export.h"
#include "starboard/microphone.h"

namespace media {

class MEDIA_EXPORT CobaltAudioCapturerSource final
    : public AudioCapturerSource {
 public:
  CobaltAudioCapturerSource() = default;
  ~CobaltAudioCapturerSource() final = default;

  CobaltAudioCapturerSource(const CobaltAudioCapturerSource&) = delete;
  CobaltAudioCapturerSource& operator=(const CobaltAudioCapturerSource&) =
      delete;

  // AudioCapturerSource implementation.
  void Initialize(const AudioParameters& params,
                  CaptureCallback* callback) final;
  void Start() final;
  void Stop() final;
  void SetVolume(double volume) final;
  void SetAutomaticGainControl(bool enable) final;
  void SetOutputDeviceForAec(const std::string& output_device_id) final;

 private:
  AudioParameters params_;
  CaptureCallback* callback_ = nullptr;

  SbMicrophone microphone_{kSbMicrophoneInvalid};
  // Minimum requested bytes per microphone read.
  int min_microphone_read_in_bytes_{0};
  std::string label_{};
};

}  // namespace media

#endif  // COBALT_MEDIA_AUDIO_COBALT_AUDIO_CAPTURER_SOURCE_H
