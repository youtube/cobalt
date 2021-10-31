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

#include "cobalt/media/sandbox/zzuf_fuzzer.h"

#include "base/logging.h"

namespace cobalt {
namespace media {
namespace sandbox {

ZzufFuzzer::ZzufFuzzer(const std::vector<uint8>& original_content,
                       double min_ratio, double max_ratio,
                       int initial_seed /*= kSeedForOriginalContent*/)
    : seed_(initial_seed) {
  original_content_.assign(original_content.begin(), original_content.end());
  DCHECK(!original_content_.empty());

  _zz_fd_init();
  zzuf_set_ratio(min_ratio, max_ratio);

  UpdateFuzzedContent();
}

ZzufFuzzer::~ZzufFuzzer() { _zz_fd_fini(); }

void ZzufFuzzer::AdvanceSeed() {
  ++seed_;
  UpdateFuzzedContent();
}

void ZzufFuzzer::UpdateFuzzedContent() {
  // Because zzuf is not actually doing any io, the fd just serves as an index
  // to its fd to fuzz context map.
  static const int kFd = 0;

  if (seed_ == kSeedForOriginalContent) {
    fuzzed_content_ = original_content_;
    return;
  }

  fuzzed_content_ = original_content_;

  zzuf_set_seed(seed_);

  _zz_register(kFd);
  _zz_fuzz(kFd, &fuzzed_content_[0], fuzzed_content_.size());
  _zz_unregister(kFd);
}

}  // namespace sandbox
}  // namespace media
}  // namespace cobalt
