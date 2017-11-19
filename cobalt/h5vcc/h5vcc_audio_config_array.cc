// Copyright 2015 Google Inc. All Rights Reserved.
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

#include "cobalt/h5vcc/h5vcc_audio_config_array.h"

#if !defined(COBALT_MEDIA_SOURCE_2016)
#include "media/audio/shell_audio_streamer.h"
#endif  // !defined(COBALT_MEDIA_SOURCE_2016)

namespace cobalt {
namespace h5vcc {

H5vccAudioConfigArray::H5vccAudioConfigArray() : audio_config_updated_(false) {}

void H5vccAudioConfigArray::TraceMembers(script::Tracer* tracer) {
  tracer->TraceItems(audio_configs_);
}

void H5vccAudioConfigArray::MaybeRefreshAudioConfigs() {
#if !defined(COBALT_MEDIA_SOURCE_2016)
  using ::media::ShellAudioStreamer;

  if (audio_config_updated_) {
    return;
  }
  // Some platforms may not use the filter based media stack and don't have a
  // ShellAudioStreamer implementation.
  if (!ShellAudioStreamer::Instance()) {
    return;
  }
  std::vector<ShellAudioStreamer::OutputDevice> output_devices =
      ShellAudioStreamer::Instance()->GetOutputDevices();
  audio_configs_.reserve(output_devices.size());
  for (size_t i = 0; i < output_devices.size(); ++i) {
    audio_configs_.push_back(new H5vccAudioConfig(
        output_devices[i].connector, output_devices[i].latency_ms,
        output_devices[i].coding_type, output_devices[i].number_of_channels,
        output_devices[i].sampling_frequency));
  }
  audio_config_updated_ = true;
#endif  // !defined(COBALT_MEDIA_SOURCE_2016)
}

}  // namespace h5vcc
}  // namespace cobalt
