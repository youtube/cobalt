// Copyright 2017 Google Inc. All Rights Reserved.
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

#include "starboard/shared/starboard/media/media_util.h"

#include "starboard/log.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace media {

bool IsAudioOutputSupported(SbMediaAudioCodingType coding_type, int channels) {
  int count = SbMediaGetAudioOutputCount();

  for (int output_index = 0; output_index < count; ++output_index) {
    SbMediaAudioConfiguration configuration;
    if (!SbMediaGetAudioConfiguration(output_index, &configuration)) {
      continue;
    }

    if (configuration.coding_type == coding_type &&
        configuration.number_of_channels >= channels) {
      return true;
    }
  }

  return false;
}

SbMediaTransferId GetTransferIdFromString(const std::string& eotf) {
  if (eotf == "bt709") {
    return kSbMediaTransferIdBt709;
  } else if (eotf == "smpte2084") {
    return kSbMediaTransferIdSmpteSt2084;
  } else if (eotf == "arib-std-b67") {
    return kSbMediaTransferIdAribStdB67;
  }
  return kSbMediaTransferIdUnknown;
}

int GetBytesPerSample(SbMediaAudioSampleType sample_type) {
  switch (sample_type) {
    case kSbMediaAudioSampleTypeInt16Deprecated:
      return 2;
    case kSbMediaAudioSampleTypeFloat32:
      return 4;
  }

  SB_NOTREACHED();
  return 4;
}

}  // namespace media
}  // namespace starboard
}  // namespace shared
}  // namespace starboard
