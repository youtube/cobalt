// Copyright 2024 The Cobalt Authors. All Rights Reserved.
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

#include "media/starboard/starboard_audio_device_factory.h"
#include "media/starboard/starboard_audio_renderer_sink.h"

#include "base/logging.h"

namespace media {

StarboardAudioDeviceFactory::StarboardAudioDeviceFactory() {
  LOG(INFO) << "Register StarboardAudioDeviceFactory";
}

StarboardAudioDeviceFactory::~StarboardAudioDeviceFactory() {
  LOG(INFO) << "Unregister StarboardAudioDeviceFactory";
}

scoped_refptr<media::AudioRendererSink>
StarboardAudioDeviceFactory::NewAudioRendererSink(
    blink::WebAudioDeviceSourceType source_type,
    const blink::LocalFrameToken& frame_token,
    const media::AudioSinkParameters& params) {
  return base::MakeRefCounted<media::StarboardAudioRendererSink>();
}

OutputDeviceInfo StarboardAudioDeviceFactory::GetOutputDeviceInfo(
    const blink::LocalFrameToken& frame_token,
    const std::string& device_id) {
  LOG(ERROR) << "GetOutputDeviceInfo - NOT IMPL";
  return OutputDeviceInfo(
      std::string(), OUTPUT_DEVICE_STATUS_OK,
      AudioParameters(AudioParameters::AUDIO_PCM_LOW_LATENCY,
                      ChannelLayoutConfig::Stereo(), 48000, 480));
}
}  // namespace media
