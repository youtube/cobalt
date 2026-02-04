//
// Copyright 2020 Comcast Cable Communications Management, LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// SPDX-License-Identifier: Apache-2.0
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

#include "third_party/starboard/rdk/shared/audio_sink/gstreamer_audio_sink_type.h"

namespace third_party {
namespace starboard {
namespace rdk {
namespace shared {
namespace audio_sink {

// static
GStreamerAudioSinkType* GStreamerAudioSinkType::CreateInstance() {
  return new GStreamerAudioSinkType();
}

// static
void GStreamerAudioSinkType::DestroyInstance(GStreamerAudioSinkType* instance) {
  delete instance;
}

}  // namespace audio_sink
}  // namespace shared
}  // namespace rdk
}  // namespace starboard
}  // namespace third_party
