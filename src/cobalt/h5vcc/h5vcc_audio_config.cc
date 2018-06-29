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

#include "cobalt/h5vcc/h5vcc_audio_config.h"

namespace cobalt {
namespace h5vcc {

H5vccAudioConfig::H5vccAudioConfig(const std::string& connector,
                                   uint32 latency_ms,
                                   const std::string coding_type,
                                   uint32 number_of_channels,
                                   uint32 sampling_frequency)
    : connector_(connector),
      latency_ms_(latency_ms),
      coding_type_(coding_type),
      number_of_channels_(number_of_channels),
      sampling_frequency_(sampling_frequency) {}

}  // namespace h5vcc
}  // namespace cobalt
