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

#ifndef COBALT_H5VCC_H5VCC_AUDIO_CONFIG_ARRAY_H_
#define COBALT_H5VCC_H5VCC_AUDIO_CONFIG_ARRAY_H_

#include <string>
#include <vector>

#include "cobalt/h5vcc/h5vcc_audio_config.h"
#include "cobalt/script/wrappable.h"

namespace cobalt {
namespace h5vcc {

class H5vccAudioConfigArray : public script::Wrappable {
 public:
  H5vccAudioConfigArray();

  uint32 length() {
    MaybeRefreshAudioConfigs();
    return static_cast<uint32>(audio_configs_.size());
  }

  scoped_refptr<H5vccAudioConfig> Item(uint32 index) {
    MaybeRefreshAudioConfigs();
    if (index < length()) {
      return audio_configs_[index];
    }
    return NULL;
  }

  DEFINE_WRAPPABLE_TYPE(H5vccAudioConfigArray);
  void TraceMembers(script::Tracer* tracer) override;

 private:
  void MaybeRefreshAudioConfigs();

  bool audio_config_updated_;
  std::vector<scoped_refptr<H5vccAudioConfig> > audio_configs_;

  DISALLOW_COPY_AND_ASSIGN(H5vccAudioConfigArray);
};

}  // namespace h5vcc
}  // namespace cobalt

#endif  // COBALT_H5VCC_H5VCC_AUDIO_CONFIG_ARRAY_H_
