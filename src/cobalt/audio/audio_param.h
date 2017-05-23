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

#ifndef COBALT_AUDIO_AUDIO_PARAM_H_
#define COBALT_AUDIO_AUDIO_PARAM_H_

#include "base/memory/ref_counted.h"
#include "cobalt/audio/audio_helpers.h"
#include "cobalt/script/environment_settings.h"
#include "cobalt/script/wrappable.h"

namespace cobalt {
namespace audio {

//   https://www.w3.org/TR/webaudio/#AudioParam
class AudioParam : public script::Wrappable {
 public:
  AudioParam(float default_value) { default_value_ = default_value; }

  // Web API: AudioParam
  //
  float value() const { return value_; }
  void set_value(float value) { value_ = value; }

  float default_value() const { return default_value_; }

  DEFINE_WRAPPABLE_TYPE(AudioParam);

 private:
  float value_;
  float default_value_;
  DISALLOW_COPY_AND_ASSIGN(AudioParam);
};

}  // namespace audio
}  // namespace cobalt

#endif  // COBALT_AUDIO_AUDIO_PARAM_H_
