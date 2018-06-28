// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_H5VCC_H5VCC_AUDIO_CONFIG_H_
#define COBALT_H5VCC_H5VCC_AUDIO_CONFIG_H_

#include <string>

#include "cobalt/script/wrappable.h"

namespace cobalt {
namespace h5vcc {

class H5vccAudioConfig : public script::Wrappable {
 public:
  // Don't use ShellAudioStreamer::OutputDevice here so we needn't expose the
  // whole ShellAudioStreamer interface.
  H5vccAudioConfig(const std::string& connector, uint32 latency_ms,
                   const std::string coding_type, uint32 number_of_channels,
                   uint32 sampling_frequency);

  const std::string& connector() const { return connector_; }
  uint32 latency_ms() const { return latency_ms_; }
  const std::string& coding_type() const { return coding_type_; }
  uint32 number_of_channels() const { return number_of_channels_; }
  uint32 sampling_frequency() const { return sampling_frequency_; }

  DEFINE_WRAPPABLE_TYPE(H5vccAudioConfig);

 private:
  const std::string connector_;
  uint32 latency_ms_;
  const std::string coding_type_;
  uint32 number_of_channels_;
  uint32 sampling_frequency_;

  DISALLOW_COPY_AND_ASSIGN(H5vccAudioConfig);
};

}  // namespace h5vcc
}  // namespace cobalt

#endif  // COBALT_H5VCC_H5VCC_AUDIO_CONFIG_H_
