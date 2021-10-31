// Copyright 2018 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_MEDIA_STREAM_AUDIO_PARAMETERS_H_
#define COBALT_MEDIA_STREAM_AUDIO_PARAMETERS_H_

#include <string>

#include "base/logging.h"

namespace cobalt {
namespace media_stream {

class AudioParameters {
 public:
  // Even though a constructed object is not valid, and there are no setters,
  // it is still possible to construct a dummy object and use the assignment
  // operator later.
  AudioParameters() = default;

  AudioParameters(int channel_count, int sample_rate, int bits_per_sample)
      : channel_count_(channel_count),
        sample_rate_(sample_rate),
        bits_per_sample_(bits_per_sample) {
    DCHECK(IsValid());
  }

  AudioParameters(const AudioParameters&) = default;
  AudioParameters& operator=(const AudioParameters&) = default;

  int channel_count() const { return channel_count_; }
  int sample_rate() const { return sample_rate_; }
  int bits_per_sample() const { return bits_per_sample_; }

  std::string AsHumanReadableString() const;

  bool IsValid() const {
    return (channel_count_ > 0) && (bits_per_sample_ > 0) && (sample_rate_ > 0);
  }

  int GetBitsPerSecond() const {
    return sample_rate_ * channel_count_ * bits_per_sample_;
  }

 private:
  int channel_count_;
  int sample_rate_;
  int bits_per_sample_;
};

inline bool operator==(const AudioParameters& lhs, const AudioParameters& rhs) {
  return ((lhs.channel_count() == rhs.channel_count()) &&
          (lhs.sample_rate() == rhs.sample_rate()) &&
          (lhs.bits_per_sample() == rhs.bits_per_sample()));
}

inline bool operator!=(const AudioParameters& lhs, const AudioParameters& rhs) {
  return !(lhs == rhs);
}

inline std::ostream& operator<<(std::ostream& os,
                                const AudioParameters& params) {
  os << params.AsHumanReadableString();
  return os;
}

}  // namespace media_stream
}  // namespace cobalt

#endif  // COBALT_MEDIA_STREAM_AUDIO_PARAMETERS_H_
