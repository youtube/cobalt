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

#ifndef COBALT_MEDIA_STREAM_MICROPHONE_AUDIO_SOURCE_H_
#define COBALT_MEDIA_STREAM_MICROPHONE_AUDIO_SOURCE_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "cobalt/media_stream/media_stream_audio_source.h"
#include "cobalt/speech/microphone_manager.h"

namespace cobalt {
namespace media_stream {

class MicrophoneAudioSource : public MediaStreamAudioSource,
                              public base::RefCounted<MicrophoneAudioSource> {
 public:
  explicit MicrophoneAudioSource(const speech::Microphone::Options& options);

 private:
  MicrophoneAudioSource(const MicrophoneAudioSource&) = delete;
  MicrophoneAudioSource& operator=(const MicrophoneAudioSource&) = delete;
  ~MicrophoneAudioSource() = default;

  bool EnsureSourceIsStarted() override;

  void EnsureSourceIsStopped() override;

  // MicrophoneManager callbacks.
  scoped_ptr<cobalt::speech::Microphone> CreateMicrophone(
      const cobalt::speech::Microphone::Options& options,
      int buffer_size_bytes);

  void OnDataReceived(
      scoped_ptr<MediaStreamAudioTrack::ShellAudioBus> audio_bus);

  void OnDataCompletion();

  void OnMicError(speech::MicrophoneManager::MicrophoneError error,
                  const std::string& error_message);

  speech::MicrophoneManager microphone_manager_;

  friend class base::RefCounted<MicrophoneAudioSource>;
};

}  // namespace media_stream
}  // namespace cobalt

#endif  // COBALT_MEDIA_STREAM_MICROPHONE_AUDIO_SOURCE_H_
