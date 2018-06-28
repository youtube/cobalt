// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_MEDIA_SANDBOX_ZZUF_FUZZER_H_
#define COBALT_MEDIA_SANDBOX_ZZUF_FUZZER_H_

#include <vector>

#include "base/basictypes.h"

extern "C" {
#include "third_party/zzuf/src/common/fd.h"
#include "third_party/zzuf/src/common/fuzz.h"
}

namespace cobalt {
namespace media {
namespace sandbox {

// This class provide fuzzed content based on the original content and a seed
// that can be advanced.  By default the first fuzzed content it returns is the
// same as the original content.
class ZzufFuzzer {
 public:
  static const int kSeedForOriginalContent = -1;

  // |initial_seed| can be used to "fast-forward" to the particular seed that
  // generates the data to cause an issue.  During normal fuzzing this should be
  // left to its default argument to ensure that it covers the original data.
  ZzufFuzzer(const std::vector<uint8>& original_content, double min_ratio,
             double max_ratio, int initial_seed = kSeedForOriginalContent);
  ~ZzufFuzzer();

  int seed() const { return seed_; }
  const std::vector<uint8_t>& GetOriginalContent() const {
    return original_content_;
  }
  const std::vector<uint8_t>& GetFuzzedContent() const {
    return fuzzed_content_;
  }

  void AdvanceSeed();

 private:
  void UpdateFuzzedContent();

  int seed_;
  std::vector<uint8_t> original_content_;
  std::vector<uint8_t> fuzzed_content_;
};

}  // namespace sandbox
}  // namespace media
}  // namespace cobalt

#endif  // COBALT_MEDIA_SANDBOX_ZZUF_FUZZER_H_
